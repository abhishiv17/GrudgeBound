#include "GbCoopMission.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"

AGbCoopController::AGbCoopController() { bShowMouseCursor = true; }
void AGbCoopController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
#if !UE_BUILD_SHIPPING
	bBot = FParse::Param(FCommandLine::Get(),TEXT("GbCoopBot"));
#endif
	if (FApp::CanEverRender()) { Hud = CreateWidget<UGbCoopHud>(this,UGbCoopHud::StaticClass()); if (Hud) Hud->AddToViewport(); }
	FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode);
}
void AGbCoopController::SetupInputComponent()
{
	Super::SetupInputComponent(); if (!InputComponent) return;
	InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AGbCoopController::UsePressed);
	InputComponent->BindKey(EKeys::E,IE_Released,this,&AGbCoopController::UseReleased);
	InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AGbCoopController::Cast);
	InputComponent->BindKey(EKeys::One,IE_Pressed,this,&AGbCoopController::VoteNames);
	InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&AGbCoopController::VoteBell);
	InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AGbCoopController::Retry);
}
void AGbCoopController::PlayerTick(float DeltaSeconds)
{
	Super::PlayerTick(DeltaSeconds); if (!IsLocalController()) return;
	if (bBot) { BotTick(DeltaSeconds); return; }
	auto* P = ::Cast<AGbCoopCharacter>(GetPawn()); if (!P || P->IsDown()) return;
	FVector Direction((IsInputKeyDown(EKeys::W)?1:0)-(IsInputKeyDown(EKeys::S)?1:0),(IsInputKeyDown(EKeys::D)?1:0)-(IsInputKeyDown(EKeys::A)?1:0),0);
	P->AddMovementInput(Direction.GetSafeNormal());
	if (IsInputKeyDown(EKeys::LeftMouseButton)) Cast();
}
void AGbCoopController::UsePressed() { ServerUse(true); }
void AGbCoopController::UseReleased() { ServerUse(false); }
void AGbCoopController::VoteNames() { ServerVote(EGbBellEnding::ReturnNames); }
void AGbCoopController::VoteBell() { ServerVote(EGbBellEnding::BreakBell); }
void AGbCoopController::Retry() { ServerRetry(); }
void AGbCoopController::Cast()
{
	const float Now=GetWorld()->GetTimeSeconds(); if (Now<NextLocalCast) return;
	FHitResult Hit; if (GetHitResultUnderCursor(ECC_Visibility,true,Hit)) { NextLocalCast=Now+.68f; ServerCast(Hit.Location); }
}
void AGbCoopController::ServerUse_Implementation(bool bHeld) { if (auto* M=GetWorld()->GetAuthGameMode<AGbCoopMode>()) M->Use(this,bHeld); }
void AGbCoopController::ServerCast_Implementation(FVector_NetQuantize Target) { if (auto* M=GetWorld()->GetAuthGameMode<AGbCoopMode>()) M->Cast(this,Target); }
void AGbCoopController::ServerVote_Implementation(EGbBellEnding Ending) { if (auto* M=GetWorld()->GetAuthGameMode<AGbCoopMode>()) M->Vote(this,Ending); }
void AGbCoopController::ServerRetry_Implementation() { if (auto* M=GetWorld()->GetAuthGameMode<AGbCoopMode>()) M->Retry(); }
void AGbCoopController::ClientLine_Implementation(const FString& Text) { PersonalLine=Text; LineUntil=GetWorld()->GetTimeSeconds()+8; }

void AGbCoopController::BotTick(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	BotAge+=DeltaSeconds;
	if (BotAge>200) { UE_LOG(LogTemp,Error,TEXT("GB_COOP_BOT_TIMEOUT")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
	auto* P=::Cast<AGbCoopCharacter>(GetPawn()); auto* S=GetWorld()->GetGameState<AGbCoopState>(); if (!P || !S || !PlayerState) return;
	const auto& M=S->Mission; const float Now=GetWorld()->GetTimeSeconds(), ServerNow=S->GetServerWorldTimeSeconds();
	if (LastObservedPhase!=static_cast<int32>(M.Phase) || LastObservedClues!=M.Clues)
	{
		LastObservedPhase=static_cast<int32>(M.Phase); LastObservedClues=M.Clues; PersonalLine.Reset();
		UE_LOG(LogTemp,Display,TEXT("GB_COOP_CLIENT Phase=%d Clues=%d Party=%d"),LastObservedPhase,LastObservedClues,S->PlayerArray.Num());
	}
	if (M.Phase==EGbBellPhase::Complete)
	{
		CompleteTime+=DeltaSeconds;
		if (CompleteTime>3) { UE_LOG(LogTemp,Display,TEXT("GB_COOP_RESULT Complete Ending=%d Party=%d"),static_cast<int32>(M.Ending),S->PlayerArray.Num()); FPlatformMisc::RequestExit(false); }
		return;
	}
	if (M.Phase==EGbBellPhase::Defeat) { ServerRetry(); return; }
	if (M.Phase==EGbBellPhase::Decision)
	{
		if (Now>=BotUseTime) { ServerVote(EGbBellEnding::ReturnNames); BotUseTime=Now+1; } return;
	}
	if (P->IsDown()) return;
	EGbBellObject Goal=EGbBellObject::Mira;
	if (M.Phase==EGbBellPhase::Search)
	{
		for (int32 I=0; I<3; ++I) if (!(M.Clues & (1<<I))) { Goal=static_cast<EGbBellObject>(static_cast<int32>(EGbBellObject::ClueNotice)+I); break; }
	}
	if (M.Phase==EGbBellPhase::Ritual || M.Phase==EGbBellPhase::Boss) Goal=(PlayerState->GetPlayerId()%2) ? EGbBellObject::AnchorA : EGbBellObject::AnchorB;
	if (M.Phase==EGbBellPhase::Revelation) Goal=EGbBellObject::Eiden;
	FVector Destination=GbBell::Position(Goal);
	if (M.Phase==EGbBellPhase::Boss)
	{
		const FVector Boss=GbBell::Position(EGbBellObject::Collector);
		if (M.PulseAt-ServerNow<1.3f && M.PulseAt>ServerNow)
		{
			if (Now>=BotUseTime) { ServerUse(false); BotUseTime=Now+.5f; } Destination=Boss+(P->GetActorLocation()-Boss).GetSafeNormal2D()*1050;
		}
		else if (M.OpenUntil>ServerNow)
		{
			if (Now>=BotUseTime) { ServerUse(false); BotUseTime=Now+.5f; }
			if (Now>=NextLocalCast) { ServerCast(Boss); NextLocalCast=Now+.7f; }
			return;
		}
	}
	const FVector Delta=Destination-P->GetActorLocation();
	if (Delta.SizeSquared2D()>FMath::Square(125.f)) { P->AddMovementInput(Delta.GetSafeNormal2D()); }
	else if (Now>=BotUseTime) { ServerUse(true); BotUseTime=Now+.5f; }
#endif
}

void UGbCoopHud::BuildTree()
{
	UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
	UBorder* Card=MakePanel(FLinearColor(.07f,.05f,.09f,.94f),FMargin(16));
	UVerticalBox* Col=WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Sized(Col,620,0));
	Col->AddChildToVerticalBox(MakeText(TEXT("THE NINTH BELL  /  CO-OP STORY"),20,FLinearColor(.95f,.75f,.3f),true));
	ObjectiveText=MakeText(TEXT("Waiting for the party's story..."),16,FLinearColor(.96f,.9f,.75f),true); Col->AddChildToVerticalBox(ObjectiveText);
	MissionText=MakeText(TEXT(""),14,FLinearColor(.96f,.9f,.75f)); Col->AddChildToVerticalBox(MissionText);
	PartyText=MakeText(TEXT(""),13,FLinearColor(.96f,.9f,.75f)); Col->AddChildToVerticalBox(PartyText);
	Col->AddChildToVerticalBox(MakeText(TEXT("WASD move | Hold E interact/channel/revive | Mouse seal | 1/2 vote | R retry after wipe"),12,FLinearColor(.7f,.7f,.75f)));
	UCanvasPanelSlot* CardSlot=Root->AddChildToCanvas(Card); CardSlot->SetPosition(FVector2D(20,20)); CardSlot->SetAutoSize(true);
	UBorder* Banner=MakePanel(FLinearColor(.07f,.05f,.09f,.95f),FMargin(20));
	DialogueText=MakeText(TEXT(""),18,FLinearColor(.96f,.9f,.75f)); Banner->SetContent(Sized(DialogueText,960,0));
	UCanvasPanelSlot* BannerSlot=Root->AddChildToCanvas(Banner); BannerSlot->SetAnchors(FAnchors(.5f,1)); BannerSlot->SetAlignment(FVector2D(.5f,1)); BannerSlot->SetPosition(FVector2D(0,-24)); BannerSlot->SetAutoSize(true);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UGbCoopHud::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
	Super::NativeTick(Geometry,DeltaSeconds); auto* S=GetWorld()->GetGameState<AGbCoopState>(); if (!S || !ObjectiveText) return;
	const auto& M=S->Mission; const float Now=S->GetServerWorldTimeSeconds();
	ObjectiveText->SetText(FText::FromString(GbBell::Objective(M)));
	FString Info=FString::Printf(TEXT("Witnessed clues: %d/3  |  Seal channel: %d%%  |  Party: %d/4"),FMath::CountBits(static_cast<uint32>(M.Clues)),FMath::RoundToInt(M.Channel*100),S->PlayerArray.Num());
	if (M.Phase==EGbBellPhase::Boss) Info+=FString::Printf(TEXT("\nCollector: %d / %d  |  %s  |  Toll in %.1fs"),FMath::RoundToInt(M.BossHp),FMath::RoundToInt(M.BossMaxHp),M.OpenUntil>Now?TEXT("EXPOSED: CAST NOW"):TEXT("SHIELDED"),FMath::Max(0.f,M.PulseAt-Now));
	if (M.Phase==EGbBellPhase::Boss && M.PulseAt-Now<=2) Info+=TEXT("\nTOLL WARNING: LEAVE THE COURT!");
	if (M.Phase==EGbBellPhase::Decision) Info+=FString::Printf(TEXT("\nVotes: %d / %d. A tie returns the names."),M.Votes,S->PlayerArray.Num());
	MissionText->SetText(FText::FromString(Info)); FString Party;
	for (TActorIterator<AGbCoopCharacter> It(GetWorld()); It; ++It) if (auto* PS=It->GetPlayerState()) Party+=FString::Printf(TEXT("%s: %s  "),*PS->GetPlayerName(),It->IsDown()?TEXT("DOWN - hold E nearby"):*FString::Printf(TEXT("%d ink"),FMath::RoundToInt(It->Health)));
	PartyText->SetText(FText::FromString(Party)); auto* PC=Cast<AGbCoopController>(GetOwningPlayer());
	DialogueText->SetText(FText::FromString(PC && GetWorld()->GetTimeSeconds()<PC->LineUntil?PC->PersonalLine:S->Line));
}
