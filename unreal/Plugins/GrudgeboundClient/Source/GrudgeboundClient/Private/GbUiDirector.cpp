#include "GbUiDirector.h"

#include "GbApiSubsystem.h"
#include "GbAuthSubsystem.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "GbWidgets.h"
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
	PrimaryActorTick.bCanEverTick = false;
}

void AGbUiDirector::BeginPlay()
{
	Super::BeginPlay();

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

	// T opens the Talisman Case (works alongside the template's Enhanced Input mappings).
	EnableInput(PC);
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AGbUiDirector::ToggleTalismanCase);
		InputComponent->BindKey(EKeys::L, IE_Pressed, this, &AGbUiDirector::OpenLastChainLink);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AGbUiDirector::ToggleRules);
		InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AGbUiDirector::ToggleSatchel);
		InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &AGbUiDirector::SignOut);
		InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AGbUiDirector::ToggleVows);
		InputComponent->BindKey(EKeys::I, IE_Pressed, this, &AGbUiDirector::ReplayIntro);
		InputComponent->BindKey(EKeys::O, IE_Pressed, this, &AGbUiDirector::ToggleObservatory);
	}

	UGbAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>();
	Auth->OnSessionChanged.AddUniqueDynamic(this, &AGbUiDirector::HandleSessionChanged);
	ShowEntry();
	Auth->ResumeSession();
}

void AGbUiDirector::HandleSessionChanged()
{
	const UGbAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>();
	if (!Auth->IsSignedIn()) ShowScreen(EScreen::SignIn);
	else if (Current == EScreen::SignIn) ShowEntry();
}

void AGbUiDirector::ShowEntry()
{
	if (!GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->IsSignedIn()) { ShowScreen(EScreen::SignIn); return; }
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
	if (!GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->IsSignedIn()) Screen = EScreen::SignIn;
	Current = Screen;
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
		? FString(TEXT("Kessa: Your points are sealed in the Rite's ledger. (L opens the proof)"))
		: FString::Printf(TEXT("Kessa: %s is sealed in the Rite's ledger. (L opens the proof)"), *Curse));
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
