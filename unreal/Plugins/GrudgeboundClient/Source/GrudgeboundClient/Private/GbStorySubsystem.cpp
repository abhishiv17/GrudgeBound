#include "GbStorySubsystem.h"
#include "GbApiSubsystem.h"
#include "GbAuthSubsystem.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UGbStorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UGbPlayerSubsystem* P = Collection.InitializeDependency<UGbPlayerSubsystem>();
	UGbApiSubsystem* A = GetGameInstance()->GetSubsystem<UGbApiSubsystem>();
	GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->OnSessionChanged.AddDynamic(this, &UGbStorySubsystem::HandleSessionChanged);
	P->OnPlayerChanged.AddDynamic(this, &UGbStorySubsystem::HandlePlayer);
	A->OnTownEvents.AddDynamic(this, &UGbStorySubsystem::HandleEvents);
	A->OnWorldState.AddDynamic(this, &UGbStorySubsystem::HandleWorld);
	A->OnBattleTurn.AddDynamic(this, &UGbStorySubsystem::HandleBattle);
	A->OnChainReceipt.AddDynamic(this, &UGbStorySubsystem::HandleBattleReceipt);
	A->OnRuleVerdict.AddDynamic(this, &UGbStorySubsystem::HandleVerdict);
	A->OnRuleReceipt.AddDynamic(this, &UGbStorySubsystem::HandleRuleReceipt);
	A->OnVowSworn.AddDynamic(this, &UGbStorySubsystem::HandleVowSworn);
	A->OnShop.AddDynamic(this, &UGbStorySubsystem::HandleShop);
	HandleSessionChanged();
}

void UGbStorySubsystem::Deinitialize()
{
	GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->OnSessionChanged.RemoveDynamic(this, &UGbStorySubsystem::HandleSessionChanged);
	GetGameInstance()->GetSubsystem<UGbPlayerSubsystem>()->OnPlayerChanged.RemoveDynamic(this, &UGbStorySubsystem::HandlePlayer);
	UGbApiSubsystem* A = GetGameInstance()->GetSubsystem<UGbApiSubsystem>();
	A->OnTownEvents.RemoveDynamic(this, &UGbStorySubsystem::HandleEvents);
	A->OnWorldState.RemoveDynamic(this, &UGbStorySubsystem::HandleWorld);
	A->OnBattleTurn.RemoveDynamic(this, &UGbStorySubsystem::HandleBattle);
	A->OnChainReceipt.RemoveDynamic(this, &UGbStorySubsystem::HandleBattleReceipt);
	A->OnRuleVerdict.RemoveDynamic(this, &UGbStorySubsystem::HandleVerdict);
	A->OnRuleReceipt.RemoveDynamic(this, &UGbStorySubsystem::HandleRuleReceipt);
	A->OnVowSworn.RemoveDynamic(this, &UGbStorySubsystem::HandleVowSworn);
	A->OnShop.RemoveDynamic(this, &UGbStorySubsystem::HandleShop);
	Super::Deinitialize();
}

void UGbStorySubsystem::HandleSessionChanged()
{
	const FString Account = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->GetAccount();
	if (Account == ActiveAccount) return;
	ActiveAccount = Account; ActiveSlot.Reset(); Data = nullptr;
	PendingBinds.Reset(); PendingResponses.Reset(); AcceptedRules.Reset();
	if (!Account.IsEmpty())
	{
		ActiveSlot = GbStory::SaveSlot(Account, GetDefault<UGbSettings>()->GetServerUrl());
		Data = Cast<UGbStorySaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlot, 0));
		if (!Data) Data = Cast<UGbStorySaveGame>(UGameplayStatics::CreateSaveGameObject(UGbStorySaveGame::StaticClass()));
		UGbApiSubsystem* A = GetGameInstance()->GetSubsystem<UGbApiSubsystem>();
		HandleEvents(A->GetRecentTownEvents()); HandleWorld(A->GetWorldState()); HandlePlayer();
	}
	OnStoryChanged.Broadcast();
}

bool UGbStorySubsystem::ShouldShowIntro() const { return Data && !Data->Progress.bIntroSeen; }
FString UGbStorySubsystem::GetChapterTitle() const { return Data ? GbStory::ChapterTitle(Data->Progress) : FString(); }
FString UGbStorySubsystem::GetObjective() const { return Data ? GbStory::Objective(Data->Progress) : FString(); }

void UGbStorySubsystem::SaveAndNotify()
{
	if (!Data || ActiveSlot.IsEmpty()) return;
	if (!UGameplayStatics::SaveGameToSlot(Data, ActiveSlot, 0)) UE_LOG(LogTemp, Warning, TEXT("Grudgebound: story progress could not be saved locally."));
	OnStoryChanged.Broadcast();
}

void UGbStorySubsystem::CompleteIntro()
{
	if (!Data || Data->Progress.bIntroSeen) return;
	Data->Progress.bIntroSeen = true; SaveAndNotify();
}

void UGbStorySubsystem::VisitBroker()
{
	if (!Data || !Data->Progress.bFirstBind) return;
	bool bChanged = !Data->Progress.bVisitedBroker;
	Data->Progress.bVisitedBroker = true;
	if (Data->Progress.bBankruptcy && !Data->Progress.bDebtAcknowledged) { Data->Progress.bDebtAcknowledged = true; bChanged = true; }
	if (bChanged) SaveAndNotify();
}

void UGbStorySubsystem::HandleEvents(const TArray<FGbTownEvent>& Events)
{
	if (!Data) return;
	bool bChanged = false;
	for (const auto& E : Events) bChanged |= GbStory::Observe(Data->Progress, E);
	if (bChanged) SaveAndNotify();
}

void UGbStorySubsystem::HandleWorld(const FGbWorldState& State)
{
	if (Data && GbStory::Observe(Data->Progress, State)) SaveAndNotify();
}

void UGbStorySubsystem::HandlePlayer()
{
	if (!Data || Data->Progress.bFirstBind) return;
	UGbPlayerSubsystem* P = GetGameInstance()->GetSubsystem<UGbPlayerSubsystem>();
	if (P->GetActiveAccount() != ActiveAccount) return;
	for (const auto& T : P->GetTalismans())
	{
		// A bought token or the starter does not prove a first bind. Recover completed binds after a lost receipt/restart.
		if (!T.BattleId.IsEmpty() && (ActiveAccount.EndsWith(TEXT(".offline")) || !T.ChainTokenId.IsEmpty()))
		{
			Data->Progress.bFirstBind = true; SaveAndNotify(); return;
		}
	}
}

void UGbStorySubsystem::HandleBattle(const FGbBattleTurn& Turn)
{
	if (!Data || !Turn.bEnded || Turn.Points <= 0 || (Turn.Outcome != EGbOutcome::Won && Turn.Outcome != EGbOutcome::Bound)) return;
	if (Turn.bHasChain)
	{
		if (Turn.Outcome == EGbOutcome::Bound) PendingBinds.Add(Turn.BattleId);
		if (GbStory::Chapter(Data->Progress) == 3) PendingResponses.Add(Turn.BattleId);
		return;
	}
	bool bChanged = GbStory::Respond(Data->Progress);
	if (Turn.Outcome == EGbOutcome::Bound && !Data->Progress.bFirstBind) { Data->Progress.bFirstBind = true; bChanged = true; }
	if (bChanged) SaveAndNotify();
}

void UGbStorySubsystem::HandleBattleReceipt(const FString& Id, const FGbChainReceipt& Receipt)
{
	const bool bBound = PendingBinds.Remove(Id) > 0;
	const bool bResponse = PendingResponses.Remove(Id) > 0;
	if (!Data || Receipt.Status != TEXT("done")) return;
	bool bChanged = bResponse && GbStory::Respond(Data->Progress);
	if (bBound && !Receipt.TokenId.IsEmpty() && !Data->Progress.bFirstBind) { Data->Progress.bFirstBind = true; bChanged = true; }
	if (bChanged) SaveAndNotify();
}

void UGbStorySubsystem::HandleVerdict(const FGbRuleVerdict& Verdict)
{
	if (Data && Verdict.bAccepted && GbStory::Chapter(Data->Progress) == 3) AcceptedRules.Add(Verdict.JobId);
}

void UGbStorySubsystem::HandleRuleReceipt(const FString& Id, const FGbChainReceipt& Receipt)
{
	const bool bAccepted = AcceptedRules.Remove(Id) > 0;
	if (Data && bAccepted && Receipt.Status == TEXT("done") && GbStory::Respond(Data->Progress)) SaveAndNotify();
}

void UGbStorySubsystem::HandleVowSworn(const FString&, const FGbChainReceipt& Receipt)
{
	if (Data && Receipt.Status == TEXT("done") && GbStory::Respond(Data->Progress)) SaveAndNotify();
}

void UGbStorySubsystem::HandleShop(const FGbShop& Shop)
{
	if (Shop.MerchantId == TEXT("broker")) VisitBroker();
}
