#include "GbUiDirector.h"

#include "GbApiSubsystem.h"
#include "GbAudio.h"
#include "GbAuthSubsystem.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "GbSettingsMenu.h"
#include "GbUserSettings.h"
#include "GbWidgets.h"
#include "GbWardGreybox.h"
#include "GbVowWidget.h"
#include "GbIntroWidget.h"
#include "GbArrivalWidget.h"
#include "GbGreyboxMaterials.h"
#include "GbGuide.h"
#include "GbSatchelWidget.h"
#include "GbObservatoryWidget.h"
#include "GbPanelFixtures.h"
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
#include "GbExorcistLook.h"
#include "GbTideWard.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "UObject/ConstructorHelpers.h"

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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Boat(TEXT("/Game/Grudgebound/Env/Pirate/boat_row_large.boat_row_large"));
	FerryMesh = Boat.Object;
}

void AGbUiDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (bCrossing && (Pawn != CrossingPawn.Get() || PC != CrossingController.Get())) FinishCrossing(false);
	if (!Pawn || !PC->IsLocalController()) return;
    UpdateSightlines(DeltaSeconds,PC,Pawn);
	UGbUserSettings* Settings = UGbUserSettings::Get();
	UGbExorcistLook::Ensure(Pawn); // the exorcist's body instead of the template mannequin
	PlaceRival();
	UpdateGuide(DeltaSeconds, Pawn);
    if (Current!=EScreen::Arrival) if (const UGbPlayerSubsystem* P=PlayerOf(GetWorld()))
        UGbExorcistLook::Ensure(Pawn)->SetPresentation(P->GetSatchel().Profile.Look);
	if (bCrossing) TickCrossing(DeltaSeconds, PC, Pawn);

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
	}
	else bKeyWalking = false;

	// The ferry owns walking, but the passenger can still frame the crossing. Menus and panels own both.
	const AGbSettingsHost* Menu = AGbSettingsHost::Find(GetWorld());
	if (CanControlCamera(PC, Menu && Menu->IsOpen()))
	{
		const float Turn = (Settings->IsDown(PC, TEXT("RotateRight")) ? 1.f : 0.f) - (Settings->IsDown(PC, TEXT("RotateLeft")) ? 1.f : 0.f);
		CameraYaw += Turn * Settings->CameraRotateSpeed * DeltaSeconds;
		if (Settings->IsDown(PC, TEXT("RotateHold")))
		{
			float MouseX = 0.f, MouseY = 0.f;
			PC->GetInputMouseDelta(MouseX, MouseY);
			CameraYaw += MouseX * 2.2f * Settings->MouseRotateSensitivity * (Settings->bInvertMouseRotate ? -1.f : 1.f);
			if (MouseY != 0.f)
			{
				Settings->CameraPitch = FMath::Clamp(Settings->CameraPitch + MouseY * 1.4f * Settings->MouseRotateSensitivity
					* (Settings->bInvertMouseRotate ? -1.f : 1.f), UGbUserSettings::MinPitch, UGbUserSettings::MaxPitch);
				bCameraDirty = true;
			}
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

	// A long lens from further back at a quarter turn: the Ward reads as a small town, not a floor from above.
	USpringArmComponent* Boom = Pawn->FindComponentByClass<USpringArmComponent>();
	UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (!Boom || !Camera) return;
	const float Ease = Settings->bSmoothCamera ? 9.f : 0.f; // 0 = snap
	SmoothedYaw = FMath::FInterpTo(SmoothedYaw, CameraYaw, DeltaSeconds, Ease);
	SmoothedPitch = FMath::FInterpTo(SmoothedPitch, FMath::Clamp(Settings->CameraPitch, UGbUserSettings::MinPitch, UGbUserSettings::MaxPitch), DeltaSeconds, Ease);
	Boom->SetUsingAbsoluteRotation(true);
	Boom->bUsePawnControlRotation = false;
	Boom->bDoCollisionTest = false;
	// Pull back during the crossing for a wider view of the water and approaching shore.
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, Settings->CameraDistance * (bCrossing ? 1.45f : 1.f), DeltaSeconds, bCrossing ? 1.6f : Ease);
	Boom->SetWorldRotation(FRotator(-SmoothedPitch, SmoothedYaw, 0.f));
	Boom->bEnableCameraLag = Settings->bSmoothCamera;
	Boom->CameraLagSpeed = 7.f;
	Camera->bUsePawnControlRotation = false;
	Camera->SetRelativeRotation(FRotator::ZeroRotator);
	Camera->SetFieldOfView(50.f);
}

bool AGbUiDirector::CanControlCamera(const APlayerController* PC, bool bMenuOpen) const
{
	return PC && Current == EScreen::Ward && !bMenuOpen && !PC->IsLookInputIgnored()
		&& (bCrossing || (!bBuildingTransition && !PC->IsMoveInputIgnored()));
}

void AGbUiDirector::BeginPlay()
{
	Super::BeginPlay();
	if (UGbPlayerSubsystem* Player = PlayerOf(GetWorld())) Player->SetWardId(FString()); // every session starts in the configured Ward
	MoorFerry();
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
    Arrival = CreateWidget<UGbArrivalWidget>(PC, UGbArrivalWidget::StaticClass());

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
    Arrival->AddToViewport(80); SpawnMentor(); SpawnRival();

	Encounter->OnFight.AddUObject(this, &AGbUiDirector::Fight);
	Encounter->OnSlipAway.AddUObject(this, &AGbUiDirector::SlipAway);
	Battle->OnBack.AddUObject(this, &AGbUiDirector::BackToWard);
	TalismanCase->OnClose.AddUObject(this, &AGbUiDirector::ToggleTalismanCase);
	Rules->OnClose.AddUObject(this, &AGbUiDirector::ToggleRules);
	Shop->OnClose.AddUObject(this, &AGbUiDirector::ToggleShop);
	VowShrine->OnClose.AddUObject(this, &AGbUiDirector::ToggleVows);
	Intro->OnFinished.AddUObject(this, &AGbUiDirector::FinishIntro);
    Arrival->OnClose.AddUObject(this,&AGbUiDirector::CloseArrival);
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
	if (bCrossing)
	{
		if (APlayerController* PC = CrossingController.Get())
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(Mode);
		}
		return;
	}
	ShowScreen(Current);
}

EGbAudioScene AGbUiDirector::GetAudioScene() const
{
	switch (Current)
	{
	case EScreen::SignIn: case EScreen::Intro: return EGbAudioScene::Title;
	case EScreen::Encounter: case EScreen::Battle: return EGbAudioScene::Battle;
	case EScreen::Ward: return CurrentBuilding == EGbBuilding::Outside ? EGbAudioScene::Street : EGbAudioScene::Indoors;
	default: return EGbAudioScene::Panel;
	}
}

bool AGbUiDirector::CloseTopPanel()
{
	switch (Current)
	{
    case EScreen::Arrival: CloseArrival(); return true;
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
	if (bCrossing && !Auth->IsSignedIn()) FinishCrossing(false);
	if (!Auth->IsSignedIn()) ShowScreen(EScreen::SignIn);
	else if (Current == EScreen::SignIn && bEntered) ShowEntry();
}

void AGbUiDirector::UpdateTitleCamera(bool bTitle)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || bTitle == bTitleCamera) return;
	if (bTitle && !TitleCamera)
	{
		for (TActorIterator<AGbWardGreybox> It(GetWorld()); It; ++It) if (It->GetClass() == AGbWardGreybox::StaticClass()) // Lantern Ward, not the Tide Ward built on it
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
	else if (!PlayerOf(GetWorld())->GetSatchel().Profile.bValid || !PlayerOf(GetWorld())->GetSatchel().Profile.bComplete || PlayerOf(GetWorld())->GetSatchel().Profile.Mint==TEXT("pending")) OpenArrival();
    else ShowScreen(EScreen::Ward);
}

void AGbUiDirector::FinishIntro()
{
	GetGameInstance()->GetSubsystem<UGbStorySubsystem>()->CompleteIntro();
    OpenArrival();
}

void AGbUiDirector::SpawnMentor()
{
 const FVector Feet(-1030,-65,5);
 PierMentor=GetWorld()->SpawnActor<AActor>();
 USceneComponent* Root=NewObject<USceneComponent>(PierMentor); PierMentor->SetRootComponent(Root); Root->RegisterComponent(); PierMentor->SetActorLocation(Feet);
 UGbExorcistLook* Doll=NewObject<UGbExorcistLook>(PierMentor); UGbExorcistLook::FOutfit Outfit;
 Outfit.Top=GbGreybox::Color::Hex(TEXT("5A2E4F")); Outfit.TopSh=Outfit.Top*.82f; Outfit.Hair=GbGreybox::Color::Hex(TEXT("C9C3D6")); Outfit.HairHi=Outfit.Hair;
 Outfit.bSkirt=true; Outfit.Skirt=Outfit.Top; Outfit.bSatchel=false; Outfit.bCoatTails=false; Outfit.bScarfTail=false; Outfit.bTalisman=false; Outfit.bPonytail=false;
 Doll->SetOutfit(Outfit); Doll->SetupAttachment(Root); Doll->SetRelativeRotation(FRotator(0,134,0)); Doll->RegisterComponent();
 AGbBuildingDoor* Door=GetWorld()->SpawnActor<AGbBuildingDoor>(Feet+FVector(-55,80,0),FRotator::ZeroRotator);
 Door->Building=EGbBuilding::Outside; Door->Use=EGbBuildingUse::Arrival; Door->Label=TEXT("Kessa"); Door->OnConstruction(Door->GetActorTransform());
}
void AGbUiDirector::SpawnRival()
{
 RivalFigure=GetWorld()->SpawnActor<AActor>();
 USceneComponent* Root=NewObject<USceneComponent>(RivalFigure); RivalFigure->SetRootComponent(Root); Root->RegisterComponent();
 UGbExorcistLook* Doll=NewObject<UGbExorcistLook>(RivalFigure); UGbExorcistLook::FOutfit Outfit;
 // A darker mirror of the player's coat: soot grey with a rust scarf, so the rival reads at a distance.
 Outfit.Top=GbGreybox::Color::Hex(TEXT("3B3A40")); Outfit.TopSh=Outfit.Top*.8f; Outfit.Hair=GbGreybox::Color::Hex(TEXT("2A1E1A")); Outfit.HairHi=Outfit.Hair*1.2f;
 Outfit.Scarf=GbGreybox::Color::Hex(TEXT("A4472A"));
 Doll->SetOutfit(Outfit); Doll->SetupAttachment(Root); Doll->RegisterComponent();
 RivalDoor=GetWorld()->SpawnActor<AGbBuildingDoor>(FVector::ZeroVector,FRotator::ZeroRotator);
 RivalDoor->Building=EGbBuilding::Outside; RivalDoor->Use=EGbBuildingUse::Rival; RivalDoor->Label=TEXT("Dace Marrow");
 RivalDoor->OnConstruction(RivalDoor->GetActorTransform());
 RivalFigure->SetActorHiddenInGame(true); RivalDoor->SetActorHiddenInGame(true); RivalDoor->SetActorEnableCollision(false);
}
void AGbUiDirector::PlaceRival()
{
 if (!RivalFigure || !RivalDoor || bRivalPinned) return;
 const UGbPlayerSubsystem* P=PlayerOf(GetWorld()); if (!P) return;
 const FGbSatchel& Data=P->GetSatchel(); // a copy, kept alive for this call
 // Nobody waits on the pier until Kessa has welcomed the player.
 const bool bShow=Data.bValid && Data.Rival.bValid && (Data.Profile.bComplete || Data.Profile.bLegacyStarter);
 PlaceRivalAt(bShow?Data.Rival.Where:FString());
}
void AGbUiDirector::UpdateGuide(float DeltaSeconds, APawn* Pawn)
{
 GuideClock-=DeltaSeconds; if (GuideClock>0.f || bWaypointPinned) return;
 GuideClock=.25f;
 if (!Waypoint) Waypoint=GetWorld()->SpawnActor<AGbWaypoint>();
 const UGbPlayerSubsystem* P=PlayerOf(GetWorld()); if (!P || !Waypoint || !Pawn) return;
 const FGbSatchel Data=P->GetSatchel();
 FVector At; FString Via;
 const FGbJournalEntry* J=GbGuide::Tracked(Data);
 bool bShow=J && UGbUserSettings::Get()->bShowWaypoint && Current==EScreen::Ward && !bCrossing && !bBuildingTransition
  && GbGuide::Resolve(GetWorld(),J->Target,Pawn->GetActorLocation(),At,Via);
 // Close enough: the gold marker underfoot says the rest.
 if (bShow && FVector::Dist2D(At,Pawn->GetActorLocation())<220.f) bShow=false;
 Waypoint->Point(bShow,At);
 UpdateTips(Data,Pawn);
}
void AGbUiDirector::UpdateTips(const FGbSatchel& S, APawn* Pawn)
{
 UGbApiSubsystem* A=GetGameInstance()?GetGameInstance()->GetSubsystem<UGbApiSubsystem>():nullptr;
 if (!A || !S.bValid || !(S.Profile.bComplete || S.Profile.bLegacyStarter) || GetWorld()->GetTimeSeconds()<NextTipAt) return;
 const bool bWard=Current==EScreen::Ward && !bCrossing && !bBuildingTransition;
 bool bLevel=false; for (const auto& L : S.Levels) bLevel|=L.Value.Level>1;
 const bool bFind=S.Gear.Num()>0 || S.Notes.Num()>0 || S.DiscoveryFlags.Contains(TEXT("alley-parcel")) || S.DiscoveryFlags.Contains(TEXT("dock-tin"));
 // In order of urgency; one at a time, a few seconds apart.
 const TPair<const TCHAR*,bool> Moments[]={
  {TEXT("battle"),Current==EScreen::Encounter},
  {TEXT("tide"),bWard && GbGuide::RegionOf(Pawn->GetActorLocation())==GbGuide::ERegion::Tide},
  {TEXT("level"),bWard && bLevel},
  {TEXT("find"),bWard && bFind},
  {TEXT("beacon"),bWard && Waypoint && Waypoint->IsShowing()},
 };
 for (const auto& M : Moments)
 {
  if (!M.Value || A->WasTipShown(M.Key)) continue;
  for (const FGbTip& T : S.Tips) if (T.Id==M.Key && !T.bSeen) { A->ShowTip(T.Id,T.Text); NextTipAt=GetWorld()->GetTimeSeconds()+10.f; return; }
 }
}
void AGbUiDirector::PlaceRivalAt(const FString& Spot)
{
 if (!RivalFigure || !RivalDoor || Spot==RivalSpot) return;
 RivalSpot=Spot; const bool bShow=!Spot.IsEmpty();
 RivalFigure->SetActorHiddenInGame(!bShow); RivalDoor->SetActorHiddenInGame(!bShow); RivalDoor->SetActorEnableCollision(bShow);
 if (!bShow) return;
 // Feet and facing: beside Kessa on the harbour pier; by the Tide ferry (same pier, nearer the boat); on Tide's quay
 // by the marked storehouse; and, after the last round, in Lantern's plaza.
 FVector Feet=Spot==TEXT("pier")?FVector(-930,150,5):Spot==TEXT("ferry")?FVector(-1150,170,5):FVector(420,260,5);
 if (Spot==TEXT("quay")) // where Tide's north pier meets the wharf, a step off the walk from the ferry, facing it
  for (TActorIterator<AGbTideWard> Tide(GetWorld()); Tide; ++Tide) { Feet=Tide->GetLandingLocation()+FVector(-330,160,5); break; }
 const float Yaw=Spot==TEXT("quay")?0.f:Spot==TEXT("plaza")?200.f:-140.f;
 RivalFigure->SetActorLocationAndRotation(Feet,FRotator(0,Yaw,0));
 RivalDoor->SetActorLocation(Feet+FRotator(0,Yaw,0).RotateVector(FVector(70,0,0)));
}
void AGbUiDirector::OpenArrival()
{
 APlayerController* PC=GetWorld()->GetFirstPlayerController(); if (!PC || !PC->GetPawn() || !Arrival) return;
 CurrentBuilding=EGbBuilding::Outside; GbBuildings::Teleport(PC->GetPawn(),FVector(-1130,40,5));
 PC->GetPawn()->SetActorRotation(FRotator(0,-46,0));
 if (UGbPlayerSubsystem* P=PlayerOf(GetWorld())) P->SetWardId(TEXT("lantern"));
 if (!ArrivalCamera)
 {
  const FVector Eye(-1600,-520,300), Focus(-1080,-12,0); // both figures in the upper half, above the panel
  ArrivalCamera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Focus-Eye).Rotation());
  ArrivalCamera->GetCameraComponent()->SetFieldOfView(60); ArrivalCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
 }
 Arrival->Open(); ShowScreen(EScreen::Arrival); PC->SetViewTargetWithBlend(ArrivalCamera,.5f);
}
void AGbUiDirector::CloseArrival()
{
 if (APlayerController* PC=GetWorld()->GetFirstPlayerController()) PC->SetViewTargetWithBlend(PC->GetPawn(),.5f);
 ShowScreen(EScreen::Ward);
}

void AGbUiDirector::ReplayIntro()
{
	if (bBuildingTransition) return;
	if (Current == EScreen::Ward) { Intro->Open(); ShowScreen(EScreen::Intro); }
}

void AGbUiDirector::SignOut()
{
	if (Current == EScreen::Ward) GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->SignOut();
}

void AGbUiDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bCrossing) FinishCrossing(false);
	if (FerryBoat) FerryBoat->Destroy();
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
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !Hud) return;
	if (!bEntered || !GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->IsSignedIn()) Screen = EScreen::SignIn;
	Current = Screen;
	UpdateTitleCamera(Screen == EScreen::SignIn);
	// The opening uses Esc to skip; everywhere else Esc is the menu.
	if (AGbSettingsHost* Menu = AGbSettingsHost::Find(GetWorld())) Menu->bKeyBusy = Screen == EScreen::Intro;

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
    Arrival->SetVisibility(Screen == EScreen::Arrival ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Screen != EScreen::Observatory) Observatory->Stop();
	// The HUD stays in the street and at an encounter; a panel carries its own status line and key hints, and the
	// HUD's corner boxes would sit half under the wider panels.
	Hud->SetVisibility(Screen == EScreen::Ward || Screen == EScreen::Encounter ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    Hud->SetIndoor(CurrentBuilding!=EGbBuilding::Outside);

	// On Play the template's own input setup stays as it is; input only changes when a dialog opens or closes.
	if (!bInputTouched && Screen == EScreen::Ward) return;
	bInputTouched = true;

	PC->SetShowMouseCursor(true);
	// A key held when a dialog opens is released while the dialog has the keyboard, so the game never hears it go
	// up and keeps walking that way afterwards. Drop every held key on each switch, both ways.
	PC->FlushPressedKeys();
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
            : Screen == EScreen::Arrival ? static_cast<UUserWidget*>(Arrival.Get())
			: Screen == EScreen::Satchel ? static_cast<UUserWidget*>(Satchel.Get())
			: Screen == EScreen::Mender ? static_cast<UUserWidget*>(Mender.Get())
			: Screen == EScreen::Room ? static_cast<UUserWidget*>(Room.Get())
			: Screen == EScreen::Observatory ? static_cast<UUserWidget*>(Observatory.Get())
			: Screen == EScreen::Rules ? static_cast<UUserWidget*>(Rules.Get())
			: Screen == EScreen::Shop ? static_cast<UUserWidget*>(Shop.Get()) : static_cast<UUserWidget*>(TalismanCase.Get());
		if (Screen == EScreen::Arrival || Screen == EScreen::TalismanCase || Screen == EScreen::Rules || Screen == EScreen::Shop || Screen == EScreen::Vows || Screen == EScreen::Satchel || Screen == EScreen::Mender || Screen == EScreen::Room || Screen == EScreen::Observatory)
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
	if (bBuildingTransition) return;
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
	if (bBuildingTransition) return;
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
	if (bBuildingTransition) return;
	if (Current == EScreen::Satchel) ShowScreen(EScreen::Ward);
	else if (Current == EScreen::Shop) { Satchel->Open(); ShowScreen(EScreen::Satchel); }
	else if (Current == EScreen::Ward)
	{
		Satchel->Open(); ShowScreen(EScreen::Satchel);
	}
}

void AGbUiDirector::ToggleShop()
{
	if (bBuildingTransition) return;
	if (Current == EScreen::Shop) { Satchel->Open(); ShowScreen(EScreen::Satchel); }
	else if (Current == EScreen::Ward && GbBuildings::Allows(CurrentBuilding, EGbBuildingUse::Broker))
	{
		Shop->Open();
		ShowScreen(EScreen::Shop);
	}
}

void AGbUiDirector::ToggleObservatory()
{
	if (bBuildingTransition) return;
	if (Current == EScreen::Observatory) ShowScreen(EScreen::Ward);
	else if (Current == EScreen::Ward) { Observatory->Open(); ShowScreen(EScreen::Observatory); }
}

void AGbUiDirector::ToggleVows()
{
	if (bBuildingTransition) return;
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

void AGbUiDirector::MoorFerry()
{
	// The boat waits at the ferry door that leads away from where the player is.
	const UGbPlayerSubsystem* Player = PlayerOf(GetWorld());
	const FString Here = Player ? Player->GetWardId() : FString();
	const AGbBuildingDoor* Pier = nullptr;
	for (TActorIterator<AGbBuildingDoor> It(GetWorld()); It; ++It) if (It->Use == EGbBuildingUse::Travel && It->TravelWard != Here) { Pier = *It; break; }
	if (!Pier) return;
	if (!FerryBoat)
	{
		if (!FerryMesh) return;
		FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
		FerryBoat = GetWorld()->SpawnActor<AStaticMeshActor>(Params);
		if (!FerryBoat) return;
		FerryBoat->SetMobility(EComponentMobility::Movable);
		FerryBoat->GetStaticMeshComponent()->SetStaticMesh(FerryMesh);
		FerryBoat->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FerryBoat->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
		FerryBoat->SetActorScale3D(FVector(0.9f));
	}
	FerryBoat->SetActorLocationAndRotation(FerryMooring(Pier->GetActorLocation()), FRotator(0.f, 4.f, 0.f));
}

void AGbUiDirector::TickCrossing(float DeltaSeconds, APlayerController* PC, APawn* Pawn)
{
	CrossTime += DeltaSeconds;
	const float T = FMath::Clamp(CrossTime / CrossingSeconds, 0.f, 1.f);
	// Push off gently, row, glide in.
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, T, 2.f);
	FVector Boat = FMath::Lerp(CrossFrom, CrossTo, Eased);
	const float Under = FMath::Sin(T * PI); // 0 at the piers, 1 in open water
	Boat.Z += FMath::Sin(CrossTime * 2.3f) * 5.f * Under;
	const FRotator Heading = (CrossTo - CrossFrom).Rotation();
	if (FerryBoat) FerryBoat->SetActorLocationAndRotation(Boat, FRotator(FMath::Sin(CrossTime * 2.3f + 1.f) * 2.f * Under, Heading.Yaw + 90.f, FMath::Sin(CrossTime * 1.7f) * 2.5f * Under));
	// The player stands in the boat, looking where it goes.
	Pawn->SetActorLocationAndRotation(Boat + FVector(0.f, 0.f, 24.f + CrossHalfHeight), FRotator(0.f, Heading.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (T < 1.f) return;

	FinishCrossing(true);
	const AGbSettingsHost* Menu = AGbSettingsHost::Find(GetWorld());
	if (!Menu || !Menu->IsOpen()) ShowScreen(EScreen::Ward);
}

void AGbUiDirector::FinishCrossing(bool bArrived)
{
	if (!bCrossing) return;
	UE_LOG(LogTemp, Display, TEXT("Grudgebound ferry: %s (%s)"), bArrived ? TEXT("arrived") : TEXT("cancelled"), *CrossWard);
	bCrossing = false;
	if (APawn* Pawn = CrossingPawn.Get())
	{
		// Collision first: without it the pawn reports no height and would be set down inside the pier.
		Pawn->SetActorEnableCollision(bCrossCollision);
		GbBuildings::Teleport(Pawn, bArrived ? CrossLanding : CrossReturn);
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
			Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(CrossMovementMode), CrossCustomMode);
	}
	if (APlayerController* PC = CrossingController.Get())
	{
		PC->StopMovement(); // discard any click path queued while aboard
		PC->SetIgnoreMoveInput(false); // release only the ferry's lock
	}
	if (bArrived) if (UGbPlayerSubsystem* Player = PlayerOf(GetWorld())) Player->SetWardId(CrossWard);
	bBuildingTransition = false;
	CrossingPawn.Reset(); CrossingController.Reset();
	MoorFerry();
}

void AGbUiDirector::UpdateSightlines(float DeltaSeconds,const APlayerController* PC,const APawn* Pawn)
{
 SightlineClock+=DeltaSeconds; if (SightlineClock<.1f || !PC->PlayerCameraManager) return; SightlineClock=0.f;
 if (!bSightlinesCached)
 {
  bSightlinesCached=true;
  for (TActorIterator<AGbWardGreybox> Ward(GetWorld());Ward;++Ward)
  {
   TArray<UInstancedStaticMeshComponent*> Parts; Ward->GetComponents(Parts);
   for (auto* Part:Parts)
   {
    const UStaticMesh* Mesh=Part->GetStaticMesh(); if (!Mesh) continue;
    const FString Name=Mesh->GetName(); if (!Name.Contains(TEXT("tree")) && !Name.Contains(TEXT("pine"))) continue;
    for (int32 I=0;I<Part->GetInstanceCount();++I)
    {
     FTreeSightline T; T.Part=Part; T.Instance=I; Part->GetInstanceTransform(I,T.Local,false);
     FTransform WorldTransform; Part->GetInstanceTransform(I,WorldTransform,true);
     T.Bounds=Mesh->GetBoundingBox().TransformBy(WorldTransform).ExpandBy(35.f); TreeSightlines.Add(T);
    }
   }
  }
 }
 const FVector Start=PC->PlayerCameraManager->GetCameraLocation(), End=Pawn->GetActorLocation()+FVector(0,0,35);
 for (FTreeSightline& T:TreeSightlines) if (UInstancedStaticMeshComponent* Part=T.Part.Get())
 {
  const bool Hide=CurrentBuilding==EGbBuilding::Outside && Current!=EScreen::SignIn && Current!=EScreen::Intro
   && FVector::DistSquared2D(T.Bounds.GetCenter(),End)<FMath::Square(800.f) && FMath::LineBoxIntersection(T.Bounds,Start,End,End-Start);
  if (Hide==T.bHidden) continue;
  FTransform Render=T.Local; if (Hide) Render.SetScale3D(FVector(.001f));
  Part->UpdateInstanceTransform(T.Instance,Render,false,true,true); T.bHidden=Hide;
 }
}

bool AGbUiDirector::IsWalking() const
{
 const APlayerController* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
 const AGbSettingsHost* Menu=AGbSettingsHost::Find(GetWorld());
 return Current==EScreen::Ward && !bBuildingTransition && !bCrossing && !(Menu && Menu->IsOpen()) && PC && !PC->IsMoveInputIgnored();
}
void AGbUiDirector::UseBuilding(AGbBuildingDoor* Door)
{
 if (!Door || !IsWalking()) return;
 APlayerController* PC=GetWorld()->GetFirstPlayerController(); APawn* Pawn=PC?PC->GetPawn():nullptr;
 if (!Pawn || FVector::DistSquared(Pawn->GetActorLocation(),Door->GetActorLocation())>FMath::Square(230.f)) return;
 if (Door->Use==EGbBuildingUse::Arrival) {OpenArrival();return;}
 if (Door->Use==EGbBuildingUse::Rival)
 {
  UGbApiSubsystem* A=ApiOf(GetWorld()); const UGbPlayerSubsystem* P=PlayerOf(GetWorld()); if (!A || !P) return;
  const FGbRival& R=P->GetSatchel().Rival;
  if (!R.bReady) {A->SayAsKessa(R.Name+TEXT(": ")+R.Line); return;}
  A->StartBattle(R.Where==TEXT("quay")?TEXT("tide"):TEXT("lantern"),P->GetPartner().SpeciesId,R.Stage);
  return;
 }
 if (Door->Use==EGbBuildingUse::Discover || (Door->Use==EGbBuildingUse::Enter && !Door->DiscoveryId.IsEmpty()))
 {
  UGbApiSubsystem* A=ApiOf(GetWorld()); const UGbPlayerSubsystem* P=PlayerOf(GetWorld()); if (!A || !P) return;
  if (!P->GetSatchel().bExplorationAvailable) {A->SayAsKessa(TEXT("Exploration is unavailable. Refresh your Satchel after reconnecting.")); return;}
  if (Door->Use==EGbBuildingUse::Discover || !P->GetSatchel().DiscoveryFlags.Contains(TEXT("storehouse-open"))) {A->Explore(Door->DiscoveryId); return;}
 }
 if (Door->Use==EGbBuildingUse::Travel)
 {
  // Carry the player across; change the HUD's Ward only after landing.
  if (CurrentBuilding!=EGbBuilding::Outside || Door->TravelWard.IsEmpty()) return;
  // The ferryman takes nobody green: Tide opens when the rival is beaten at this pier (server-decided).
  if (const UGbPlayerSubsystem* P=PlayerOf(GetWorld()))
   if (Door->TravelWard==TEXT("tide") && P->GetSatchel().bValid && !P->GetSatchel().bTideOpen)
   {
    if (UGbApiSubsystem* A=ApiOf(GetWorld())) A->SayAsKessa(TEXT("Kessa: The ferryman won't take you yet. ")+P->GetSatchel().Rival.Name+TEXT(" is waiting by the boat: beat them and the crossing is yours."));
    return;
   }
  // No cut: the boat carries the player across the water to the other pier (TickCrossing).
  const AGbBuildingDoor* Far=nullptr;
  const UGbPlayerSubsystem* Player=PlayerOf(GetWorld());
  for (TActorIterator<AGbBuildingDoor> It(GetWorld());It;++It)
   if (It->Use==EGbBuildingUse::Travel && *It!=Door && (!Player || It->TravelWard==Player->GetWardId())) { Far=*It; break; }
  if (!Far) return;
  MoorFerry();
  if (!FerryBoat) return;
  bBuildingTransition=true; bCrossing=true; CrossTime=0.f;
  CrossingPawn=Pawn; CrossingController=PC;
  CrossReturn=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetSimpleCollisionHalfHeight()+5.f);
  bCrossCollision=Pawn->GetActorEnableCollision(); CrossHalfHeight=Pawn->GetSimpleCollisionHalfHeight();
  PC->StopMovement(); PC->SetIgnoreMoveInput(true);
  CrossFrom=FerryMooring(Door->GetActorLocation());
  CrossTo=FerryMooring(Far->GetActorLocation());
  CrossLanding=Door->TargetFeet; CrossWard=Door->TravelWard;
  UE_LOG(LogTemp, Display, TEXT("Grudgebound ferry: departing for %s"), *CrossWard);
  Pawn->SetActorEnableCollision(false);
  if (ACharacter* Character=Cast<ACharacter>(Pawn))
  {
   UCharacterMovementComponent* Movement=Character->GetCharacterMovement();
   CrossMovementMode=Movement->MovementMode; CrossCustomMode=Movement->CustomMovementMode;
   Movement->StopMovementImmediately(); Movement->DisableMovement();
  }
  TickCrossing(0.f,PC,Pawn);
  return;
 }
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
	// Nobody slips away from a rival: Escape on that screen just goes to the fight.
	if (PendingBattle.RivalStage > 0) { Fight(); return; }
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
	// -GbShotPanels=<folder>: each building and pocket panel as the game shows it (over the Ward), filled
	// with the preview test's sample data, saved as <panel>.png 1.5 s apart, then quit.
	FString PanelFolder;
	if (FParse::Value(FCommandLine::Get(), TEXT("GbShotPanels="), PanelFolder))
	{
		struct FPanelShots { int32 Step = 0; FTimerHandle Timer; };
		TSharedRef<FPanelShots> Shots = MakeShared<FPanelShots>();
		TWeakObjectPtr<UWorld> World = &InWorld;
		InWorld.GetTimerManager().SetTimer(Shots->Timer, FTimerDelegate::CreateLambda([World, Shots, PanelFolder]()
		{
			UWorld* W = World.Get();
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			AGbUiDirector* Director = nullptr;
			if (W) for (TActorIterator<AGbUiDirector> It(W); It; ++It) Director = *It;
			if (!PC || !Director || !Director->Hud) return;
			const TArray<FString>& Names = FGbPanelFixtures::Shots();
			const int32 K = Shots->Step++;
			const int32 I = K / 2;
			if (I >= Names.Num()) { FPlatformMisc::RequestExit(false); return; }
			if (K % 2 == 1) { FScreenshotRequest::RequestScreenshot(PanelFolder / (Names[I] + TEXT(".png")), true, false); return; }
			const FString& Name = Names[I];
			UGbWidgetBase* Panel = Name == TEXT("TalismanCase") ? static_cast<UGbWidgetBase*>(Director->TalismanCase.Get())
				: Name == TEXT("Mender") ? static_cast<UGbWidgetBase*>(Director->Mender.Get())
				: Name.StartsWith(TEXT("Satchel")) ? static_cast<UGbWidgetBase*>(Director->Satchel.Get())
				: Name == TEXT("Broker") ? static_cast<UGbWidgetBase*>(Director->Shop.Get())
				: Name == TEXT("Rite") ? static_cast<UGbWidgetBase*>(Director->Rules.Get())
				: Name == TEXT("Vows") ? static_cast<UGbWidgetBase*>(Director->VowShrine.Get())
				: Name.StartsWith(TEXT("Observatory")) ? static_cast<UGbWidgetBase*>(Director->Observatory.Get())
				: static_cast<UGbWidgetBase*>(Director->Room.Get());
			const TArray<UUserWidget*> Others = { Director->Encounter, Director->Battle, Director->TalismanCase, Director->Rules, Director->Shop,
				Director->SignIn, Director->VowShrine, Director->Intro, Director->Satchel, Director->Mender, Director->Room, Director->Observatory };
			for (UUserWidget* Other : Others) if (Other && Other != Panel) Other->SetVisibility(ESlateVisibility::Collapsed);
			PC->SetViewTarget(PC->GetPawn());
			Director->Hud->SetVisibility(ESlateVisibility::Collapsed); // as ShowScreen does for every panel
			Panel->SetVisibility(ESlateVisibility::Visible);
			FGbPanelFixtures::Fill(Panel, Name);
		}), 1.5f, true, 8.f);
	}

	// -GbPathSpec=<file> -GbPathOut=<folder>: camera moves for trailer footage. Run with -benchmark -fps=30 so
	// every engine tick is exactly 1/30 s and each frame is saved as <folder>/<shot>/fNNNN.png. One shot per line:
	//   <name> <frames> [setup tokens] <key> <key> ...   key = x,y,z,pitch,yaw[,fov] (world cm), or @dx,dy,dz,pitch,yaw[,fov]
	//   relative to the player when the shot starts. Tokens: rival-<spot>, t (Tide quay), f (board the ferry),
	//   follow (keys stay relative to the player every frame), pawn=x,y,z,yaw, pawnto=x,y,z (walks over the shot),
	//   linear (no ease), pcam (the player's own camera instead of keys), skip=N (ticks to wait after setup, default 20).
	FString PathSpec;
	if (FParse::Value(FCommandLine::Get(), TEXT("GbPathSpec="), PathSpec))
	{
		struct FPathKey { FVector L = FVector::ZeroVector; FVector R = FVector::ZeroVector; bool bRel = false; };
		struct FPathShot { FString Name; int32 Frames = 0; TArray<FPathKey> Keys; TArray<FString> Setup; };
		struct FPathRun
		{
			TArray<FPathShot> Shots; FString Out; int32 Shot = 0; int32 Tick = 0; int32 Phase = 0; int32 Frame = 0; int32 Skip = 20;
			FVector Origin = FVector::ZeroVector; FVector PawnFrom = FVector::ZeroVector; FVector PawnTo = FVector::ZeroVector;
			bool bPawnMove = false; bool bFollow = false; bool bLinear = false; bool bPlayerCam = false;
			TWeakObjectPtr<ACameraActor> Cam; FDelegateHandle Handle;
		};
		TSharedRef<FPathRun> Run = MakeShared<FPathRun>();
		FParse::Value(FCommandLine::Get(), TEXT("GbPathOut="), Run->Out);
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *PathSpec);
		for (FString Line : Lines)
		{
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#"))) continue;
			TArray<FString> T; Line.ParseIntoArrayWS(T);
			if (T.Num() < 2) continue;
			FPathShot S; S.Name = T[0]; S.Frames = FCString::Atoi(*T[1]);
			for (int32 I = 2; I < T.Num(); ++I)
			{
				const TCHAR C = T[I][0];
				const bool bRel = C == TEXT('@');
				if (bRel || C == TEXT('-') || FChar::IsDigit(C))
				{
					TArray<FString> P; T[I].Mid(bRel ? 1 : 0).ParseIntoArray(P, TEXT(","));
					if (P.Num() < 5) continue;
					FPathKey K; K.bRel = bRel;
					K.L = FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]));
					K.R = FVector(FCString::Atof(*P[3]), FCString::Atof(*P[4]), P.Num() >= 6 ? FCString::Atof(*P[5]) : 60.f);
					S.Keys.Add(K);
				}
				else S.Setup.Add(T[I]);
			}
			Run->Shots.Add(S);
		}
		auto Spline = [](const TArray<FVector>& V, float U) -> FVector
		{
			if (V.Num() == 1) return V[0];
			const float X = FMath::Clamp(U, 0.f, 1.f) * (V.Num() - 1);
			const int32 I = FMath::Min(FMath::FloorToInt(X), V.Num() - 2);
			const float F = X - I;
			const FVector P0 = V[FMath::Max(I - 1, 0)], P1 = V[I], P2 = V[I + 1], P3 = V[FMath::Min(I + 2, V.Num() - 1)];
			return 0.5f * ((2.f * P1) + (-P0 + P2) * F + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * F * F + (-P0 + 3.f * P1 - 3.f * P2 + P3) * F * F * F);
		};
		TWeakObjectPtr<UWorld> World = &InWorld;
		Run->Handle = FWorldDelegates::OnWorldTickStart.AddLambda([World, Run, Spline](UWorld* W, ELevelTick, float)
		{
			if (W != World.Get() || !W) return;
			APlayerController* PC = W->GetFirstPlayerController();
			APawn* Me = PC ? PC->GetPawn() : nullptr;
			if (!PC || !Me) return;
			if (++Run->Tick < 240) return; // streaming and first shaders settle
			if (Run->Shot >= Run->Shots.Num())
			{
				if (Run->Phase++ > 30) { FWorldDelegates::OnWorldTickStart.Remove(Run->Handle); FPlatformMisc::RequestExit(false); }
				return;
			}
			const FPathShot& S = Run->Shots[Run->Shot];
			if (Run->Phase == 0)
			{
				for (TObjectIterator<UGbSignInWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::Collapsed);
				Run->bPawnMove = Run->bFollow = Run->bLinear = Run->bPlayerCam = false; Run->Skip = 20;
				for (const FString& Tok : S.Setup)
				{
					if (Tok == TEXT("follow")) Run->bFollow = true;
					else if (Tok == TEXT("linear")) Run->bLinear = true;
					else if (Tok == TEXT("pcam")) Run->bPlayerCam = true;
					else if (Tok.StartsWith(TEXT("skip="))) Run->Skip = FCString::Atoi(*Tok.Mid(5));
					else if (Tok.StartsWith(TEXT("rival-")))
					{
						for (TActorIterator<AGbUiDirector> Director(W); Director; ++Director) { Director->bRivalPinned = true; Director->PlaceRivalAt(Tok.Mid(6)); }
					}
					else if (Tok == TEXT("t"))
					{
						for (TActorIterator<AGbTideWard> Tide(W); Tide; ++Tide)
						{
							GbBuildings::Teleport(Me, Tide->GetLandingLocation() - FVector(900.f, 0.f, 0.f));
							if (UGbPlayerSubsystem* Player = PlayerOf(W)) Player->SetWardId(TEXT("tide"));
						}
					}
					else if (Tok == TEXT("f"))
					{
						for (TActorIterator<AGbUiDirector> Director(W); Director; ++Director)
							for (TActorIterator<AGbBuildingDoor> Door(W); Door; ++Door)
								if (Door->Use == EGbBuildingUse::Travel && Door->TravelWard == TEXT("tide"))
								{
									GbBuildings::Teleport(Me, Door->GetActorLocation());
									Director->Current = AGbUiDirector::EScreen::Ward;
									Director->UseBuilding(*Door);
								}
					}
					else if (Tok.StartsWith(TEXT("pawn=")) || Tok.StartsWith(TEXT("pawnto=")))
					{
						const bool bTo = Tok.StartsWith(TEXT("pawnto="));
						TArray<FString> P; Tok.Mid(bTo ? 7 : 5).ParseIntoArray(P, TEXT(","));
						if (P.Num() < 3) continue;
						const FVector L(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]));
						if (bTo) { Run->bPawnMove = true; Run->PawnTo = L; }
						else
						{
							GbBuildings::Teleport(Me, L);
							if (P.Num() >= 4) Me->SetActorRotation(FRotator(0.f, FCString::Atof(*P[3]), 0.f));
						}
					}
				}
				if (UGbExorcistLook* Look = UGbExorcistLook::Ensure(Me)) Look->SetPreviewWalk(Run->bPawnMove);
				Run->Phase = 1; Run->Frame = 0; Run->Tick = 240;
				return;
			}
			if (Run->Phase == 1)
			{
				if (Run->Tick < 240 + Run->Skip) return;
				Run->Origin = Me->GetActorLocation();
				Run->PawnFrom = Run->Origin;
				if (Run->bPawnMove && !Run->PawnTo.IsZero())
				{
					const FVector D = Run->PawnTo - Run->PawnFrom;
					Me->SetActorRotation(FRotator(0.f, D.Rotation().Yaw, 0.f));
				}
				if (!Run->bPlayerCam)
				{
					if (!Run->Cam.IsValid()) Run->Cam = W->SpawnActor<ACameraActor>();
					Run->Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
					PC->SetViewTarget(Run->Cam.Get());
				}
				else PC->SetViewTarget(Me);
				Run->Phase = 2;
			}
			const float X = S.Frames > 1 ? float(Run->Frame) / float(S.Frames - 1) : 0.f;
			const float U = Run->bLinear ? X : 0.5f - 0.5f * FMath::Cos(PI * X);
			if (Run->bPawnMove) Me->SetActorLocation(FMath::Lerp(Run->PawnFrom, Run->PawnTo, X), false, nullptr, ETeleportType::TeleportPhysics);
			if (!Run->bPlayerCam && Run->Cam.IsValid() && S.Keys.Num() > 0)
			{
				const FVector Base = Run->bFollow ? Me->GetActorLocation() : Run->Origin;
				TArray<FVector> Ls, Rs;
				for (const FPathKey& K : S.Keys) { Ls.Add(K.bRel ? Base + K.L : K.L); Rs.Add(K.R); }
				const FVector L = Spline(Ls, U), R = Spline(Rs, U);
				Run->Cam->SetActorLocationAndRotation(L, FRotator(R.X, R.Y, 0.f));
				Run->Cam->GetCameraComponent()->SetFieldOfView(R.Z);
			}
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s/%s/f%04d.png"), *Run->Out, *S.Name, Run->Frame), false, false);
			if (++Run->Frame >= S.Frames) { ++Run->Shot; Run->Phase = 0; }
		});
	}

	FString ShotFolder;
	if (FParse::Value(FCommandLine::Get(), TEXT("GbShot="), ShotFolder))
	{
		// Steps, 1.5 s apart: title.png (with UI), then per camera "move" and "shoot" (shotN.png, no UI), then
		// hud.png (title hidden, HUD shown), battle.png and battle-hit.png (a staged fight) and menu.png, then quit. A shot is taken a step after its camera moved.
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
				if ((K - 1) % 2 == 1) { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s/shot%d.png"), *ShotFolder, V), Shots->Cams[V].StartsWith(TEXT("arrival")), false); return; }
				// "rival-<spot>": Dace at that spot (pier, ferry, quay, plaza); combine with fixed cameras to frame it.
				// "waypoint-<discovery id>": the beacon over that find or door.
				if (Shots->Cams[V].StartsWith(TEXT("waypoint-")))
				{
					const FString Id=Shots->Cams[V].Mid(9);
					for (TActorIterator<AGbUiDirector> Director(W); Director; ++Director)
					{
						if (!Director->Waypoint) Director->Waypoint=W->SpawnActor<AGbWaypoint>();
						for (TActorIterator<AGbBuildingDoor> Door(W); Door; ++Door) if (Door->DiscoveryId==Id && Director->Waypoint) { Director->bWaypointPinned=true; Director->Waypoint->Point(true,Door->GetActorLocation()); }
					}
					return;
				}
				if (Shots->Cams[V].StartsWith(TEXT("rival-")))
				{
					for (TActorIterator<AGbUiDirector> Director(W); Director; ++Director) { Director->bRivalPinned = true; Director->PlaceRivalAt(Shots->Cams[V].Mid(6)); }
					return;
				}
				if (Shots->Cams[V].StartsWith(TEXT("arrival")))
				{
					for (TActorIterator<AGbUiDirector> Director(W); Director; ++Director)
                    {
                        Director->OpenArrival(); Director->SignIn->SetVisibility(ESlateVisibility::Collapsed); Director->Hud->SetVisibility(ESlateVisibility::Collapsed);
                        Director->Arrival->SetVisibility(ESlateVisibility::Visible); Director->Current=AGbUiDirector::EScreen::Arrival;
                        PC->SetViewTarget(Director->ArrivalCamera); FGbPanelFixtures::Fill(Director->Arrival,Shots->Cams[V]);
                    }
					return;
				}
				TArray<FString> P; Shots->Cams[V].ParseIntoArray(P, TEXT(","));
				// "f": board the ferry where the player is; follow with "p" views to watch the crossing.
				if (Shots->Cams[V] == TEXT("f") && PC->GetPawn())
				{
					for (TObjectIterator<UGbSignInWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::Collapsed);
					PC->SetViewTarget(PC->GetPawn());
					for (TActorIterator<AGbUiDirector> Director(W); Director; ++Director)
						for (TActorIterator<AGbBuildingDoor> Door(W); Door; ++Door)
							if (Door->Use == EGbBuildingUse::Travel && Door->TravelWard == TEXT("tide"))
							{
								GbBuildings::Teleport(PC->GetPawn(), Door->GetActorLocation());
								Director->Current = AGbUiDirector::EScreen::Ward;
								Director->UseBuilding(*Door);
							}
					return;
				}
				// "t": stage the player on Tide's quay (does not exercise the ferry).
				if (Shots->Cams[V] == TEXT("t") && PC->GetPawn())
				{
					for (TActorIterator<AGbTideWard> Tide(W); Tide; ++Tide)
					{
						GbBuildings::Teleport(PC->GetPawn(), Tide->GetLandingLocation() - FVector(900.f, 0.f, 0.f));
						if (UGbPlayerSubsystem* Player = PlayerOf(W)) Player->SetWardId(TEXT("tide"));
					}
					if (UGbExorcistLook* Look = UGbExorcistLook::Ensure(PC->GetPawn())) Look->SetPreviewWalk(false);
					PC->SetViewTarget(PC->GetPawn());
					return;
				}
				// "in1".."in4": stand inside a building (EGbBuilding: 1 Counting House, 2 Mender, 3 Rite Hall, 4 Room)
				// and look through its own camera, as the door does.
				if (Shots->Cams[V].StartsWith(TEXT("in")) && PC->GetPawn())
				{
					const EGbBuilding Kind = static_cast<EGbBuilding>(FCString::Atoi(*Shots->Cams[V].Mid(2)));
					for (TObjectIterator<UGbSignInWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::Collapsed);
					GbBuildings::Teleport(PC->GetPawn(), GbBuildings::Spawn(Kind));
					for (TActorIterator<AGbUiDirector> Director(W); Director; ++Director) Director->CurrentBuilding = Kind;
					for (TActorIterator<AGbBuildingRoom> Room(W); Room; ++Room) if (Room->Building == Kind) PC->SetViewTarget(*Room);
					return;
				}
				// "c" / "cw" / "cb": the character up close (standing, walking, from behind).
				if (Shots->Cams[V].StartsWith(TEXT("c")) && PC->GetPawn())
				{
					APawn* Me = PC->GetPawn();
					if (UGbExorcistLook* Look = UGbExorcistLook::Ensure(Me)) Look->SetPreviewWalk(Shots->Cams[V].Contains(TEXT("w")));
					const bool bBack = Shots->Cams[V].Contains(TEXT("b"));
					const FVector Eye = Me->GetActorLocation() + Me->GetActorRotation().RotateVector(FVector(bBack ? -520.f : 560.f, bBack ? 260.f : 280.f, 170.f));
					ACameraActor* Camera = W->SpawnActor<ACameraActor>(Eye, (Me->GetActorLocation() - FVector(0, 0, 8) - Eye).Rotation());
					Camera->GetCameraComponent()->SetConstraintAspectRatio(false); Camera->GetCameraComponent()->SetFieldOfView(38.f);
					PC->SetViewTarget(Camera);
					return;
				}
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
			// Steps after the views: 0-1 hud, 2-3 encounter poster, 4-6 battle, 7-8 menu.
			const int32 Step = Shots->MenuTab >= 0 ? K - 1 - Views * 2 : 99;
			if (Step == 2 || Step == 3)
			{
				for (TObjectIterator<UGbEncounterWidget> It; It; ++It)
				{
					if (It->GetWorld() != W) continue;
					if (Step == 3) { FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("encounter.png"), true, false); continue; }
					FGbBattleStart Start;
					Start.Wild.Id = TEXT("riot-bell"); Start.Wild.Name = TEXT("Riot Bell"); Start.Wild.Emotion = TEXT("rage"); Start.Wild.Omen = TEXT("calamity");
					Start.Wild.BornFrom = TEXT("a Ward where PvP never stops"); Start.Matchup = TEXT("disadvantage");
					Start.View.Wild.SpeciesId = TEXT("riot-bell"); Start.View.Player.SpeciesId = TEXT("weepwick"); Start.View.Player.Name = TEXT("Weepwick");
					Start.Lines = { TEXT("A wild Riot Bell tears out of the tall grass.") };
					for (TObjectIterator<UGbHudWidget> Hud; Hud; ++Hud) if (Hud->GetWorld() == W) Hud->SetVisibility(ESlateVisibility::Collapsed);
					It->Show(Start); It->SetVisibility(ESlateVisibility::Visible);
				}
				return;
			}
			switch (Step >= 4 ? Step - 2 : Step)
			{
			case 0:
				PC->SetViewTarget(PC->GetPawn());
				for (TObjectIterator<UGbSignInWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::Collapsed);
				for (TObjectIterator<UGbHudWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::HitTestInvisible);
				break;
			case 1: FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("hud.png"), true, false); break;
			case 2: case 4:
			{
				// A staged fight (no server needed): the opening, then one exchange caught mid-swing.
				FGbBattleStart Start;
				Start.View.Wild.SpeciesId = TEXT("riot-bell"); Start.View.Wild.Name = TEXT("Riot Bell"); Start.View.Wild.Emotion = TEXT("rage");
				Start.View.Wild.Omen = TEXT("calamity"); Start.View.Wild.Hp = 62; Start.View.Wild.MaxHp = 80;
				Start.View.Player.SpeciesId = TEXT("weepwick"); Start.View.Player.Name = TEXT("Weepwick"); Start.View.Player.Emotion = TEXT("sorrow");
				Start.View.Player.Omen = TEXT("whisper"); Start.View.Player.Hp = 31; Start.View.Player.MaxHp = 40;
				Start.View.Salves = 2; Start.View.GildedSeals = 1; Start.View.SealChance = 0.42f; Start.View.GildedSealChance = 0.71f; Start.View.FleeChance = 0.8f;
				Start.View.bCanAttack = Start.View.bCanWard = Start.View.bCanSeal = Start.View.bCanGildedSeal = Start.View.bCanSalve = Start.View.bCanFlee = true;
				Start.Lines = { TEXT("A wild Riot Bell tears out of the tall grass."), TEXT("Weepwick steps onto the binding circle.") };
				for (TObjectIterator<UGbBattleWidget> It; It; ++It)
				{
					if (It->GetWorld() != W) continue;
					if (Step == 4)
					{
						for (TObjectIterator<UGbEncounterWidget> E; E; ++E) if (E->GetWorld() == W) E->SetVisibility(ESlateVisibility::Collapsed);
					}
					if (Step == 4)
					{
						for (TObjectIterator<UGbHudWidget> Hud; Hud; ++Hud) if (Hud->GetWorld() == W) Hud->SetVisibility(ESlateVisibility::Collapsed);
						It->ShowStart(Start); It->SetVisibility(ESlateVisibility::Visible);
					}
					else
					{
						FGbBattleTurn Turn; Turn.Action = TEXT("attack"); Turn.View = Start.View;
						Turn.View.Wild.Hp = 48; Turn.View.Player.Hp = 22;
						Turn.Lines = { TEXT("Weepwick strikes for 14."), TEXT("Riot Bell answers for 9.") };
						It->ApplyTurn(Turn);
						FTimerHandle Mid;
						W->GetTimerManager().SetTimer(Mid, FTimerDelegate::CreateLambda([ShotFolder]() { FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("battle-hit.png"), true, false); }), 0.28f, false);
					}
				}
				break;
			}
			case 3: FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("battle.png"), true, false); break;
			case 5:
				for (TObjectIterator<UGbBattleWidget> It; It; ++It) if (It->GetWorld() == W) It->SetVisibility(ESlateVisibility::Collapsed);
				for (TObjectIterator<UGbHudWidget> Hud; Hud; ++Hud) if (Hud->GetWorld() == W) Hud->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (AGbSettingsHost* Menu = AGbSettingsHost::Find(W)) Menu->Open(Shots->MenuTab);
				break;
			case 6: FScreenshotRequest::RequestScreenshot(ShotFolder / TEXT("menu.png"), true, false); break;
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
