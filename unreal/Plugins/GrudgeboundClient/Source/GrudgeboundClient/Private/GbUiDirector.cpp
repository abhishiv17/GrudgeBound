#include "GbUiDirector.h"

#include "GbApiSubsystem.h"
#include "GbAuthSubsystem.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "GbSettingsMenu.h"
#include "GbUserSettings.h"
#include "GbWidgets.h"
#include "GbWardGreybox.h"
#include "GbVowWidget.h"
#include "GbIntroWidget.h"
#include "GbSatchelWidget.h"
#include "GbObservatoryWidget.h"
#include "GbStorySubsystem.h"
#include "GbCoopMission.h"
#include "GameFramework/WorldSettings.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

namespace
{
	UGbApiSubsystem* ApiOf(const UWorld* World)
	{
		const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UGbApiSubsystem>() : nullptr;
	}

	UGbPlayerSubsystem* PlayerOf(const UWorld* World)
	{
		const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UGbPlayerSubsystem>() : nullptr;
	}
}

AGbUiDirector::AGbUiDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGbUiDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !PC->IsLocalController()) return;
	UGbUserSettings* Settings = UGbUserSettings::Get();

	// No walking or camera keys while a panel, the menu or a door fade has the player (the same rule as clicks).
	if (Current == EScreen::Ward && !bBuildingTransition && !PC->IsMoveInputIgnored())
	{
		const float Forward = (Settings->IsDown(PC, TEXT("MoveUp")) ? 1.f : 0.f) - (Settings->IsDown(PC, TEXT("MoveDown")) ? 1.f : 0.f);
		const float Right = (Settings->IsDown(PC, TEXT("MoveRight")) ? 1.f : 0.f) - (Settings->IsDown(PC, TEXT("MoveLeft")) ? 1.f : 0.f);
		if (Forward == 0.f && Right == 0.f) bKeyWalking = false;
		else
		{
			// Keys take over from a click: drop the path so the character does not drift back to the old target.
			if (!bKeyWalking) { PC->StopMovement(); bKeyWalking = true; }
			// Up the screen, whatever the camera heading (indoors the room's own camera).
			const float Yaw = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : SmoothedYaw;
			Pawn->AddMovementInput(FRotator(0.f, Yaw, 0.f).RotateVector(FVector(Forward, Right, 0.f)).GetSafeNormal(), 1.f);
		}

		const float Turn = (Settings->IsDown(PC, TEXT("RotateRight")) ? 1.f : 0.f) - (Settings->IsDown(PC, TEXT("RotateLeft")) ? 1.f : 0.f);
		CameraYaw += Turn * Settings->CameraRotateSpeed * DeltaSeconds;
		if (Settings->IsDown(PC, TEXT("RotateHold")))
		{
			float MouseX = 0.f, MouseY = 0.f;
			PC->GetInputMouseDelta(MouseX, MouseY);
			CameraYaw += MouseX * 2.2f * Settings->MouseRotateSensitivity * (Settings->bInvertMouseRotate ? -1.f : 1.f);
		}
		const auto Zoom = [&](FName Action) -> float
		{
			const FKey Key = Settings->KeyFor(Action);
			if (!Key.IsValid()) return 0.f;
			if (PC->WasInputKeyJustPressed(Key)) return 140.f;
			const bool bWheel = Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown;
			return !bWheel && PC->IsInputKeyDown(Key) ? 900.f * DeltaSeconds : 0.f;
		};
		const float Step = Zoom(TEXT("ZoomOut")) - Zoom(TEXT("ZoomIn"));
		if (Step != 0.f)
		{
			Settings->CameraDistance = FMath::Clamp(Settings->CameraDistance + Step, 700.f, UGbUserSettings::MaxDistance);
			bCameraDirty = true;
		}
		if (Settings->WasPressed(PC, TEXT("ResetCamera")))
		{
			CameraYaw = UGbUserSettings::DefaultYaw + 360.f * FMath::RoundToFloat((CameraYaw - UGbUserSettings::DefaultYaw) / 360.f);
			Settings->CameraDistance = UGbUserSettings::DefaultDistance;
			Settings->CameraPitch = UGbUserSettings::DefaultPitch;
			bCameraDirty = true;
		}
	}
	else bKeyWalking = false;

	// A long lens from further back at a quarter turn: the Ward reads as a small town, not a floor from above.
	USpringArmComponent* Boom = Pawn->FindComponentByClass<USpringArmComponent>();
	UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (!Boom || !Camera) return;
	const float Ease = Settings->bSmoothCamera ? 9.f : 0.f; // 0 = snap
	SmoothedYaw = FMath::FInterpTo(SmoothedYaw, CameraYaw, DeltaSeconds, Ease);
	SmoothedPitch = FMath::FInterpTo(SmoothedPitch, FMath::Clamp(Settings->CameraPitch, 28.f, 78.f), DeltaSeconds, Ease);
	Boom->SetUsingAbsoluteRotation(true);
	Boom->bUsePawnControlRotation = false;
	Boom->bDoCollisionTest = false;
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, Settings->CameraDistance, DeltaSeconds, Ease);
	Boom->SetWorldRotation(FRotator(-SmoothedPitch, SmoothedYaw, 0.f));
	Boom->bEnableCameraLag = Settings->bSmoothCamera;
	Boom->CameraLagSpeed = 7.f;
	Camera->bUsePawnControlRotation = false;
	Camera->SetRelativeRotation(FRotator::ZeroRotator);
	Camera->SetFieldOfView(50.f);
}

void AGbUiDirector::BeginPlay()
{
	Super::BeginPlay();
	CameraYaw = SmoothedYaw = UGbUserSettings::DefaultYaw;
	SmoothedPitch = UGbUserSettings::Get()->CameraPitch;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	Hud = CreateWidget<UGbHudWidget>(PC, UGbHudWidget::StaticClass());
	Encounter = CreateWidget<UGbEncounterWidget>(PC, UGbEncounterWidget::StaticClass());
	Battle = CreateWidget<UGbBattleWidget>(PC, UGbBattleWidget::StaticClass());
	TalismanCase = CreateWidget<UGbTalismanCaseWidget>(PC, UGbTalismanCaseWidget::StaticClass());
	Rules = CreateWidget<UGbRulesWidget>(PC, UGbRulesWidget::StaticClass());
	Shop = CreateWidget<UGbShopWidget>(PC, UGbShopWidget::StaticClass());
	SignIn = CreateWidget<UGbSignInWidget>(PC, UGbSignInWidget::StaticClass());
	VowShrine = CreateWidget<UGbVowWidget>(PC, UGbVowWidget::StaticClass());
	Intro = CreateWidget<UGbIntroWidget>(PC, UGbIntroWidget::StaticClass());
	Satchel = CreateWidget<UGbSatchelWidget>(PC, UGbSatchelWidget::StaticClass());
	Mender = CreateWidget<UGbMenderWidget>(PC, UGbMenderWidget::StaticClass());
	Room = CreateWidget<UGbRoomWidget>(PC, UGbRoomWidget::StaticClass());
	Observatory = CreateWidget<UGbObservatoryWidget>(PC, UGbObservatoryWidget::StaticClass());

	Hud->AddToViewport(0);
	Encounter->AddToViewport(10);
	Battle->AddToViewport(20);
	TalismanCase->AddToViewport(30);
	Rules->AddToViewport(30);
	Shop->AddToViewport(30);
	SignIn->AddToViewport(100);
	VowShrine->AddToViewport(30);
	Intro->AddToViewport(80);
	Satchel->AddToViewport(30);
	Mender->AddToViewport(30); Room->AddToViewport(30);
	Observatory->AddToViewport(30);

	Encounter->OnFight.AddUObject(this, &AGbUiDirector::Fight);
	Encounter->OnSlipAway.AddUObject(this, &AGbUiDirector::SlipAway);
	Battle->OnBack.AddUObject(this, &AGbUiDirector::BackToWard);
	TalismanCase->OnClose.AddUObject(this, &AGbUiDirector::ToggleTalismanCase);
	Rules->OnClose.AddUObject(this, &AGbUiDirector::ToggleRules);
	Shop->OnClose.AddUObject(this, &AGbUiDirector::ToggleShop);
	VowShrine->OnClose.AddUObject(this, &AGbUiDirector::ToggleVows);
	Intro->OnFinished.AddUObject(this, &AGbUiDirector::FinishIntro);
	SignIn->OnEnter.AddUObject(this, &AGbUiDirector::EnterWard);
	Satchel->OnClose.AddUObject(this, &AGbUiDirector::ToggleSatchel);
	Mender->OnClose.AddUObject(this, &AGbUiDirector::CloseBuildingPanel);
	Room->OnClose.AddUObject(this, &AGbUiDirector::CloseBuildingPanel);
	Room->OnCase.AddUObject(this, &AGbUiDirector::RoomCase);
	Observatory->OnClose.AddUObject(this, &AGbUiDirector::ToggleObservatory);

	if (UGbApiSubsystem* Api = ApiOf(GetWorld()))
	{
		Api->OnBattleStarted.AddUniqueDynamic(this, &AGbUiDirector::HandleBattleStarted);
		Api->OnBattleTurn.AddUniqueDynamic(this, &AGbUiDirector::HandleBattleTurn);
		Api->OnError.AddUniqueDynamic(this, &AGbUiDirector::HandleApiError);
		Api->OnChainReceipt.AddUniqueDynamic(this, &AGbUiDirector::HandleChainReceipt);
		Api->OnRuleReceipt.AddUniqueDynamic(this, &AGbUiDirector::HandleRuleReceipt);
		Api->OnTradeReceipt.AddUniqueDynamic(this, &AGbUiDirector::HandleTradeReceipt);
		Api->OnVowReceipt.AddUniqueDynamic(this, &AGbUiDirector::HandleTradeReceipt);
	}

	// Panel keys follow the player's bindings (settings menu > Controls) and re-bind whenever those change.
	EnableInput(PC);
	BindKeys();
	UGbUserSettings::Get()->OnChanged.AddUObject(this, &AGbUiDirector::BindKeys);

	UGbAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>();
	Auth->OnSessionChanged.AddUniqueDynamic(this, &AGbUiDirector::HandleSessionChanged);
	ShowEntry();
	Auth->ResumeSession();
}

void AGbUiDirector::BindKeys()
{
	if (!InputComponent) return;
	InputComponent->KeyBindings.Reset();
	const UGbUserSettings* S = UGbUserSettings::Get();
	const TPair<const TCHAR*, void (AGbUiDirector::*)()> Keys[] = {
		{ TEXT("Case"), &AGbUiDirector::ToggleTalismanCase }, { TEXT("Proof"), &AGbUiDirector::OpenLastChainLink },
		{ TEXT("Rite"), &AGbUiDirector::ToggleRules }, { TEXT("Satchel"), &AGbUiDirector::ToggleSatchel },
		{ TEXT("SignOut"), &AGbUiDirector::SignOut }, { TEXT("Vows"), &AGbUiDirector::ToggleVows },
		{ TEXT("Story"), &AGbUiDirector::ReplayIntro }, { TEXT("Observatory"), &AGbUiDirector::ToggleObservatory },
	};
	for (const auto& K : Keys)
	{
		const FKey Key = S->KeyFor(K.Key);
		if (Key.IsValid()) InputComponent->BindKey(Key, IE_Pressed, this, K.Value);
	}
}

void AGbUiDirector::RestoreInput()
{
	bInputTouched = true;
	ShowScreen(Current);
}

bool AGbUiDirector::CloseTopPanel()
{
	switch (Current)
	{
	case EScreen::TalismanCase: case EScreen::Rules: case EScreen::Shop: case EScreen::Vows: case EScreen::Satchel:
	case EScreen::Mender: case EScreen::Room: case EScreen::Observatory:
		ShowScreen(EScreen::Ward);
		return true;
	default:
		return false;
	}
}

void AGbUiDirector::HandleSessionChanged()
{
	const UGbAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>();
	if (!Auth->IsSignedIn()) ShowScreen(EScreen::SignIn);
	else if (Current == EScreen::SignIn && bEntered) ShowEntry();
}

void AGbUiDirector::UpdateTitleCamera(bool bTitle)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || bTitle == bTitleCamera) return;
	if (bTitle && !TitleCamera)
	{
		for (TActorIterator<AGbWardGreybox> It(GetWorld()); It; ++It)
		{
			// Over the roofs toward the harbour, from the south-west.
			const FTransform Ward = It->GetActorTransform();
			TitleCamera = GetWorld()->SpawnActor<ACameraActor>(Ward.TransformPosition(FVector(-2500.f, -2300.f, 2500.f)),
				(Ward.GetRotation() * FRotator(-36.f, 42.f, 0.f).Quaternion()).Rotator());
			TitleCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
			TitleCamera->GetCameraComponent()->SetFieldOfView(50.f);
			break;
		}
	}
	if (!TitleCamera) return;
	bTitleCamera = bTitle;
	if (bTitle) PC->SetViewTarget(TitleCamera);
	else PC->SetViewTargetWithBlend(PC->GetPawn(), 1.4f, VTBlend_EaseInOut, 2.f);
}

void AGbUiDirector::EnterWard()
{
	bEntered = true;
	UGbAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>();
	if (Auth->IsSignedIn()) ShowEntry();
	else if (!Auth->IsBusy()) Auth->BeginSignIn();
}

void AGbUiDirector::ShowEntry()
{
	// Every launch starts on the title screen, signed in or not.
	if (!bEntered || !GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->IsSignedIn()) { ShowScreen(EScreen::SignIn); return; }
	GetGameInstance()->GetSubsystem<UGbStorySubsystem>()->RefreshSession();
	if (GetGameInstance()->GetSubsystem<UGbStorySubsystem>()->ShouldShowIntro())
	{
		Intro->Open(); ShowScreen(EScreen::Intro);
	}
	else ShowScreen(EScreen::Ward);
}

void AGbUiDirector::FinishIntro()
{
	GetGameInstance()->GetSubsystem<UGbStorySubsystem>()->CompleteIntro();
	ShowScreen(EScreen::Ward);
}

void AGbUiDirector::ReplayIntro()
{
	if (Current == EScreen::Ward) { Intro->Open(); ShowScreen(EScreen::Intro); }
}

void AGbUiDirector::SignOut()
{
	if (Current == EScreen::Ward) GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->SignOut();
}

void AGbUiDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(BuildingTimer);
	if (bCameraDirty) UGbUserSettings::Get()->SaveConfig();
	UGbUserSettings::Get()->OnChanged.RemoveAll(this);
	GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->OnSessionChanged.RemoveDynamic(this, &AGbUiDirector::HandleSessionChanged);
	if (UGbApiSubsystem* Api = ApiOf(GetWorld()))
	{
		Api->OnBattleStarted.RemoveDynamic(this, &AGbUiDirector::HandleBattleStarted);
		Api->OnBattleTurn.RemoveDynamic(this, &AGbUiDirector::HandleBattleTurn);
		Api->OnError.RemoveDynamic(this, &AGbUiDirector::HandleApiError);
		Api->OnChainReceipt.RemoveDynamic(this, &AGbUiDirector::HandleChainReceipt);
		Api->OnRuleReceipt.RemoveDynamic(this, &AGbUiDirector::HandleRuleReceipt);
		Api->OnTradeReceipt.RemoveDynamic(this, &AGbUiDirector::HandleTradeReceipt);
		Api->OnVowReceipt.RemoveDynamic(this, &AGbUiDirector::HandleTradeReceipt);
		// Leaving the level mid-battle: forget it locally; the server expires it.
		Api->AbandonBattle();
	}
	Super::EndPlay(Reason);
}

void AGbUiDirector::ShowScreen(EScreen Screen)
{
	if (bBuildingTransition) return;
	if (!bEntered || !GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->IsSignedIn()) Screen = EScreen::SignIn;
	Current = Screen;
	UpdateTitleCamera(Screen == EScreen::SignIn);
	// The opening uses Esc to skip; everywhere else Esc is the menu.
	if (AGbSettingsHost* Menu = AGbSettingsHost::Find(GetWorld())) Menu->bKeyBusy = Screen == EScreen::Intro;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !Hud) return;

	Encounter->SetVisibility(Screen == EScreen::Encounter ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Battle->SetVisibility(Screen == EScreen::Battle ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	TalismanCase->SetVisibility(Screen == EScreen::TalismanCase ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Rules->SetVisibility(Screen == EScreen::Rules ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Shop->SetVisibility(Screen == EScreen::Shop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SignIn->SetVisibility(Screen == EScreen::SignIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	VowShrine->SetVisibility(Screen == EScreen::Vows ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Intro->SetVisibility(Screen == EScreen::Intro ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Satchel->SetVisibility(Screen == EScreen::Satchel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Mender->SetVisibility(Screen == EScreen::Mender ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Room->SetVisibility(Screen == EScreen::Room ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Observatory->SetVisibility(Screen == EScreen::Observatory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Screen != EScreen::Observatory) Observatory->Stop();
	Hud->SetVisibility(Screen == EScreen::Battle || Screen == EScreen::Intro || Screen == EScreen::SignIn ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// On Play the template's own input setup stays as it is; input only changes when a dialog opens or closes.
	if (!bInputTouched && Screen == EScreen::Ward) return;
	bInputTouched = true;

	PC->SetShowMouseCursor(true);
	if (Screen == EScreen::Ward)
	{
		// Back to walking: the world gets clicks and keys again. SetIgnoreMoveInput is a counter, and a fight
		// opens two dialogs in a row (encounter, then battle): reset it, or walking stays blocked after a fight.
		PC->ResetIgnoreMoveInput();
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else
	{
		// A dialog is open: stop the pawn where it stands; clicks go to the dialog only.
		PC->StopMovement();
		if (!PC->IsMoveInputIgnored()) PC->SetIgnoreMoveInput(true);
		UUserWidget* Focus = Screen == EScreen::Encounter ? static_cast<UUserWidget*>(Encounter.Get())
			: Screen == EScreen::Battle ? static_cast<UUserWidget*>(Battle.Get())
			: Screen == EScreen::SignIn ? static_cast<UUserWidget*>(SignIn.Get())
			: Screen == EScreen::Vows ? static_cast<UUserWidget*>(VowShrine.Get())
			: Screen == EScreen::Intro ? static_cast<UUserWidget*>(Intro.Get())
			: Screen == EScreen::Satchel ? static_cast<UUserWidget*>(Satchel.Get())
			: Screen == EScreen::Mender ? static_cast<UUserWidget*>(Mender.Get())
			: Screen == EScreen::Room ? static_cast<UUserWidget*>(Room.Get())
			: Screen == EScreen::Observatory ? static_cast<UUserWidget*>(Observatory.Get())
			: Screen == EScreen::Rules ? static_cast<UUserWidget*>(Rules.Get())
			: Screen == EScreen::Shop ? static_cast<UUserWidget*>(Shop.Get()) : static_cast<UUserWidget*>(TalismanCase.Get());
		if (Screen == EScreen::TalismanCase || Screen == EScreen::Rules || Screen == EScreen::Shop || Screen == EScreen::Vows || Screen == EScreen::Satchel || Screen == EScreen::Mender || Screen == EScreen::Room || Screen == EScreen::Observatory)
		{
			// Keep game input so T closes the case again.
			FInputModeGameAndUI Mode;
			Mode.SetWidgetToFocus(Focus->TakeWidget());
			Mode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(Mode);
		}
		else
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Focus->TakeWidget());
			PC->SetInputMode(Mode);
		}
	}
}

void AGbUiDirector::ToggleTalismanCase()
{
	if (Current == EScreen::TalismanCase)
	{
		ShowScreen(EScreen::Ward);
	}
	else if (Current == EScreen::Ward)
	{
		TalismanCase->Refresh();
		if (UGbPlayerSubsystem* P = GetGameInstance()->GetSubsystem<UGbPlayerSubsystem>())
		{
			P->RefreshFromChain();
			if (!P->GetAccountUrl().IsEmpty()) LastChainUrl = P->GetAccountUrl(); // L: your account on the explorer.
		}
		ShowScreen(EScreen::TalismanCase);
	}
}

void AGbUiDirector::ToggleRules()
{
	if (Current == EScreen::Ward && !GbBuildings::Allows(CurrentBuilding, EGbBuildingUse::Rules))
	{
		// OWNER TEXT
		if (UGbApiSubsystem* A=ApiOf(GetWorld())) A->SayAsKessa(TEXT("Kessa keeps the Rite in the Rite Hall.")); return;
	}
	if (Current == EScreen::Rules)
	{
		ShowScreen(EScreen::Ward);
	}
	else if (Current == EScreen::Ward)
	{
		Rules->Open();
		ShowScreen(EScreen::Rules);
	}
}

void AGbUiDirector::ToggleSatchel()
{
	if (Current == EScreen::Satchel) ShowScreen(EScreen::Ward);
	else if (Current == EScreen::Shop) { Satchel->Open(); ShowScreen(EScreen::Satchel); }
	else if (Current == EScreen::Ward)
	{
		Satchel->Open(); ShowScreen(EScreen::Satchel);
	}
}

void AGbUiDirector::ToggleShop()
{
	if (Current == EScreen::Shop) { Satchel->Open(); ShowScreen(EScreen::Satchel); }
	else if (Current == EScreen::Ward && GbBuildings::Allows(CurrentBuilding, EGbBuildingUse::Broker))
	{
		Shop->Open();
		ShowScreen(EScreen::Shop);
	}
}

void AGbUiDirector::ToggleObservatory()
{
	if (Current == EScreen::Observatory) ShowScreen(EScreen::Ward);
	else if (Current == EScreen::Ward) { Observatory->Open(); ShowScreen(EScreen::Observatory); }
}

void AGbUiDirector::ToggleVows()
{
	if (Current == EScreen::Ward && !GbBuildings::Allows(CurrentBuilding, EGbBuildingUse::Vows))
	{
		// OWNER TEXT (same Rite Hall placeholder)
		if (UGbApiSubsystem* A=ApiOf(GetWorld())) A->SayAsKessa(TEXT("Kessa keeps the Rite in the Rite Hall.")); return;
	}
	if (Current == EScreen::Vows) ShowScreen(EScreen::Ward);
	else if (Current == EScreen::Ward)
	{
		VowShrine->Open(); ShowScreen(EScreen::Vows);
	}
}

void AGbUiDirector::CloseBuildingPanel() { ShowScreen(EScreen::Ward); }
void AGbUiDirector::RoomCase() { ShowScreen(EScreen::Ward); ToggleTalismanCase(); }

void AGbUiDirector::UseBuilding(AGbBuildingDoor* Door)
{
 if (!Door || Current!=EScreen::Ward || bBuildingTransition) return;
 APlayerController* PC=GetWorld()->GetFirstPlayerController(); APawn* Pawn=PC?PC->GetPawn():nullptr;
 if (!Pawn || FVector::DistSquared(Pawn->GetActorLocation(),Door->GetActorLocation())>FMath::Square(230.f)) return;
 if (Door->Use==EGbBuildingUse::Enter || Door->Use==EGbBuildingUse::Exit)
 {
  const bool Enter=Door->Use==EGbBuildingUse::Enter;
  if ((Enter && CurrentBuilding!=EGbBuilding::Outside) || (!Enter && CurrentBuilding!=Door->Building)) return;
  bBuildingTransition=true; PC->StopMovement(); PC->SetIgnoreMoveInput(true);
  if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0,1,.3f,FLinearColor::Black,false,true);
  const FVector Target=Door->TargetFeet; const EGbBuilding Destination=Enter?Door->Building:EGbBuilding::Outside;
  GetWorldTimerManager().SetTimer(BuildingTimer,FTimerDelegate::CreateWeakLambda(this,[this,Target,Destination]()
  {
   APlayerController* Controller=GetWorld()->GetFirstPlayerController(); APawn* P=Controller?Controller->GetPawn():nullptr;
   if (P)
   {
    GbBuildings::Teleport(P,Target);
    CurrentBuilding=Destination;
    AActor* View=P;
    if (Destination!=EGbBuilding::Outside) for (TActorIterator<AGbBuildingRoom> It(GetWorld());It;++It) if (It->Building==Destination) { View=*It; break; }
    Controller->SetViewTarget(View); Controller->ResetIgnoreMoveInput();
    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->StartCameraFade(1,0,.3f,FLinearColor::Black,false,false);
   }
   bBuildingTransition=false; ShowScreen(EScreen::Ward);
  }),.3f,false);
  return;
 }
 if (!GbBuildings::Allows(CurrentBuilding,Door->Use) || CurrentBuilding!=Door->Building) return;
 switch (Door->Use)
 {
 case EGbBuildingUse::Broker: ToggleShop(); break;
 case EGbBuildingUse::Mender: Mender->Open(); ShowScreen(EScreen::Mender); break;
 case EGbBuildingUse::Rules: ToggleRules(); break;
 case EGbBuildingUse::Vows: ToggleVows(); break;
 case EGbBuildingUse::Room: Room->Open(); ShowScreen(EScreen::Room); break;
 case EGbBuildingUse::Case: ToggleTalismanCase(); break;
 default: break;
 }
}

void AGbUiDirector::HandleTradeReceipt(const FString& JobId, const FGbChainReceipt& Receipt)
{
	if (Receipt.Status == TEXT("done") && !Receipt.ExplorerUrl.IsEmpty()) LastChainUrl = Receipt.ExplorerUrl;
}

void AGbUiDirector::HandleRuleReceipt(const FString& JobId, const FGbChainReceipt& Receipt)
{
	if (Receipt.Status != TEXT("done")) return;
	LastChainUrl = Receipt.ExplorerUrl;
	UE_LOG(LogTemp, Log, TEXT("Grudgebound: rule %d on NEAR %s: %s"), Receipt.RuleId, *Receipt.Network, *Receipt.ExplorerUrl);
}

void AGbUiDirector::HandleBattleStarted(const FGbBattleStart& Start)
{
	PendingBattle = Start;
	Encounter->Show(Start);
	ShowScreen(EScreen::Encounter);
}

void AGbUiDirector::Fight()
{
	Battle->ShowStart(PendingBattle);
	ShowScreen(EScreen::Battle);
}

void AGbUiDirector::SlipAway()
{
	// The server owns supplies/ink and one open battle per player. Resolve the existing flee move rather
	// than forgetting an encounter locally and stranding all pocket/healing actions until it expires.
	Battle->ShowStart(PendingBattle); ShowScreen(EScreen::Battle);
	if (UGbApiSubsystem* Api = ApiOf(GetWorld())) Api->SendAction(EGbAction::Flee);
}

void AGbUiDirector::HandleBattleTurn(const FGbBattleTurn& Turn)
{
	if (Turn.bEnded && Turn.bHasChain) CurseOfBattle.Add(Turn.BattleId, Turn.View.Wild.Name);
	if (Current != EScreen::Battle) return;
	Battle->ApplyTurn(Turn);
}

void AGbUiDirector::HandleChainReceipt(const FString& BattleId, const FGbChainReceipt& Receipt)
{
	UGbApiSubsystem* Api = ApiOf(GetWorld());
	if (!Api) return;
	FString Curse;
	CurseOfBattle.RemoveAndCopyValue(BattleId, Curse);
	if (Curse.IsEmpty()) Curse = TEXT("Your curse");
	if (Receipt.Status != TEXT("done"))
	{
		UE_LOG(LogTemp, Warning, TEXT("Grudgebound: NEAR write for battle %s failed: %s"), *BattleId, *Receipt.Error);
		Api->SayAsKessa(TEXT("Kessa: The ledger would not take it this time. Your case will show what was sealed."));
		return;
	}
	LastChainUrl = Receipt.ExplorerUrl;
	UE_LOG(LogTemp, Log, TEXT("Grudgebound: battle %s on NEAR %s: %s"), *BattleId, *Receipt.Network, *Receipt.ExplorerUrl);
	Api->SayAsKessa(Receipt.TokenId.IsEmpty()
		? FString::Printf(TEXT("Kessa: Your points are sealed in the Rite's ledger. (%s opens the proof)"), *UGbUserSettings::Get()->KeyLabel(TEXT("Proof")))
		: FString::Printf(TEXT("Kessa: %s is sealed in the Rite's ledger. (%s opens the proof)"), *Curse, *UGbUserSettings::Get()->KeyLabel(TEXT("Proof"))));
}

void AGbUiDirector::OpenLastChainLink()
{
	if (!LastChainUrl.IsEmpty()) FPlatformProcess::LaunchURL(*LastChainUrl, nullptr, nullptr);
}

void AGbUiDirector::BackToWard()
{
	if (UGbPlayerSubsystem* Player = PlayerOf(GetWorld())) Player->StartEncounterGrace();
	if (UGbApiSubsystem* Api = ApiOf(GetWorld())) Api->RefreshNow();
	if (UGbPlayerSubsystem* Player = PlayerOf(GetWorld())) Player->RefreshFromChain();
	ShowScreen(EScreen::Ward);
}

void AGbUiDirector::HandleApiError(const FString& What, int32 Status, const FString& Message)
{
	if (What == TEXT("battle/turn") && Current == EScreen::Battle)
	{
		// 404: the battle is gone on the server. 409: that move isn't allowed now; the turn isn't spent.
		const bool bOver = Status == 404;
		const FString Text = Status == 0 ? FString(TEXT("Hollowmere is unreachable. Please try again shortly.")) : Message;
		Battle->ShowProblem(Text, bOver);
	}
	else if (What == TEXT("battle/start"))
	{
		if (UGbApiSubsystem* Api = ApiOf(GetWorld()))
		{
			Api->SayAsKessa(Status == 0
				? FString(TEXT("Something stirs in the grass... but the town is silent. (Game server not reachable.)"))
				: FString::Printf(TEXT("The grass goes still. (%s)"), *Message));
		}
		if (UGbPlayerSubsystem* Player = PlayerOf(GetWorld())) Player->StartEncounterGrace();
	}
}

// ================================================================ game flow subsystem

bool UGbGameFlowSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGbGameFlowSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The menu (Esc / F1) works in the Ward and in the co-op mission alike.
	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		FActorSpawnParameters MenuParams;
		MenuParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MenuParams.ObjectFlags |= RF_Transient;
		InWorld.SpawnActor<AGbSettingsHost>(AGbSettingsHost::StaticClass(), FTransform::Identity, MenuParams);
	}

#if !UE_BUILD_SHIPPING
	// Development aid: -GbShot=<folder> saves pictures and quits. -GbShotCams="x,y,z,pitch,yaw,fov;..." are fixed
	// cameras (world cm), "p" is the player's own camera. -GbShotMenu=<tab> adds the HUD and one tab of the menu.
	FString ShotFolder;
	if (FParse::Value(FCommandLine::Get(), TEXT("GbShot="), ShotFolder))
	{
		// Steps, 1.5 s apart: title.png (with UI), then per camera "move" and "shoot" (shotN.png, no UI), then
		// hud.png (title hidden, HUD shown) and menu.png, then quit. A shot is taken a step after its camera moved.
		struct FShots { TArray<FString> Cams; int32 Step = 0; int32 MenuTab = -1; FTimerHandle Timer; };
		TSharedRef<FShots> Shots = MakeShared<FShots>();
		FString Cams;
		if (FParse::Value(FCommandLine::Get(), TEXT("GbShotCams="), Cams, false)) Cams.ParseIntoArray(Shots->Cams, TEXT(";"));
		if (Shots->Cams.Num() == 0) Shots->Cams.Add(FString());
		FParse::Value(FCommandLine::Get(), TEXT("GbShotMenu="), Shots->MenuTab);
		TWeakObjectPtr<UWorld> World = &InWorld;
		InWorld.GetTimerManager().SetTimer(Shots->Timer, FTimerDelegate::CreateLambda([World, Shots, ShotFolder]()
		{
			UWorld* W = World.Get();
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			if (!PC) return;
			const int32 Views = Shots->Cams.Num();
			const int32 K = Shots->Step++;
			if (K == 0) { FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("title.png"), true, false); return; }
			const int32 V = (K - 1) / 2;
			if (V < Views)
			{
				if ((K - 1) % 2 == 1) { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s/shot%d.png"), *ShotFolder, V), false, false); return; }
				TArray<FString> P; Shots->Cams[V].ParseIntoArray(P, TEXT(","));
				if (P.Num() >= 5)
				{
					ACameraActor* Camera = W->SpawnActor<ACameraActor>(FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])), FRotator(FCString::Atof(*P[3]), FCString::Atof(*P[4]), 0.f));
					Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
					if (P.Num() >= 6) Camera->GetCameraComponent()->SetFieldOfView(FCString::Atof(*P[5]));
					PC->SetViewTarget(Camera);
				}
				else PC->SetViewTarget(PC->GetPawn());
				return;
			}
			switch (Shots->MenuTab >= 0 ? K - 1 - Views * 2 : 4)
			{
			case 0:
				PC->SetViewTarget(PC->GetPawn());
				for (TObjectIterator<UGbSignInWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::Collapsed);
				for (TObjectIterator<UGbHudWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::HitTestInvisible);
				break;
			case 1: FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("hud.png"), true, false); break;
			case 2: if (AGbSettingsHost* Menu = AGbSettingsHost::Find(W)) Menu->Open(Shots->MenuTab); break;
			case 3: FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("menu.png"), true, false); break;
			default: FPlatformMisc::RequestExit(false);
			}
		}), 1.5f, true, 7.f);
	}
#endif

	// The opt-in co-op mission owns its local UI and networking. No solo API battles or sign-in gate run there.
	const auto ModeClass=InWorld.GetWorldSettings()->DefaultGameMode;
	if (ModeClass && ModeClass->IsChildOf(AGbCoopMode::StaticClass())) return;

	if (UGbApiSubsystem* Api = ApiOf(&InWorld))
	{
		Api->StartPolling();
	}

	if (GetDefault<UGbSettings>()->bCreateDefaultUi)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		InWorld.SpawnActor<AGbUiDirector>(AGbUiDirector::StaticClass(), FTransform::Identity, Params);
	}
}
