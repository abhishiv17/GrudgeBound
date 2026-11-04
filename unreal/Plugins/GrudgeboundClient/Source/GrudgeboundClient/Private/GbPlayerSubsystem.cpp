#include "GbPlayerSubsystem.h"

#include "GbApiSubsystem.h"
#include "GbSettings.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

const TCHAR* UGbPlayerSubsystem::StarterSpecies = TEXT("cinder-grin");
const TCHAR* UGbPlayerSubsystem::StarterOmen = TEXT("whisper");
const TCHAR* UGbPlayerSubsystem::StarterId = TEXT("tal-starter");
const TCHAR* UGbPlayerSubsystem::SlotName = TEXT("Grudgebound");

void UGbPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UGbApiSubsystem* Api = Collection.InitializeDependency<UGbApiSubsystem>();
	Load();
	if (Api)
	{
		Api->OnBattleTurn.AddDynamic(this, &UGbPlayerSubsystem::HandleBattleTurn);
		Api->OnChainReceipt.AddDynamic(this, &UGbPlayerSubsystem::HandleChainReceipt);
		Api->OnShop.AddDynamic(this, &UGbPlayerSubsystem::HandleShop);
		Api->OnTrade.AddDynamic(this, &UGbPlayerSubsystem::HandleTrade);
		Api->OnTradeReceipt.AddDynamic(this, &UGbPlayerSubsystem::HandleTradeReceipt);
		Api->OnPlayerView.AddDynamic(this, &UGbPlayerSubsystem::HandlePlayerView);
		Api->FetchPlayer();
	}
}

void UGbPlayerSubsystem::RefreshFromChain()
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UGbApiSubsystem* Api = GI->GetSubsystem<UGbApiSubsystem>()) Api->FetchPlayer();
	}
}

void UGbPlayerSubsystem::HandlePlayerView(const FGbPlayerView& View)
{
	if (!Data || !View.bChain) return;
	bOnChain = true;
	AccountUrl = View.ExplorerUrl;
	if (View.Points >= 0) Data->Points = View.Points;
	if (View.Ember >= 0) Data->Ember = View.Ember;

	// While a mint or trade is still settling, a token may be on chain before its receipt reaches us: matching it
	// now would add it twice. Points and Ember are safe to take; the talisman list waits for the next refresh.
	// (A bind whose NEAR write failed no longer counts: it settled, as a local-only talisman, hidden from the case.)
	if (!IsSettling())
	{
		TSet<FString> OnChain;
		for (const FGbChainTalisman& C : View.Talismans)
		{
			OnChain.Add(C.TokenId);
			FGbTalisman* Local = Data->Talismans.FindByPredicate([&C](const FGbTalisman& T) { return T.ChainTokenId == C.TokenId; });
			if (!Local)
			{
				// Bound on another PC, bought on the web, or given by another player.
				Local = &Data->Talismans.AddDefaulted_GetRef();
				Local->Id = FString::Printf(TEXT("tal-%d"), Data->NextTalisman++);
				Local->ChainTokenId = C.TokenId;
			}
			Local->SpeciesId = C.SpeciesId;
			Local->Omen = C.Omen;
			Local->Wins = FMath::Max(Local->Wins, C.Wins);
			Local->Purse = C.Purse;
		}
		// Gone from the chain (sold or traded away): gone from the case. The starter and unminted talismans stay.
		Data->Talismans.RemoveAll([&OnChain](const FGbTalisman& T) { return !T.ChainTokenId.IsEmpty() && !OnChain.Contains(T.ChainTokenId); });
		if (Data->Talismans.Num() == 0) MakeFresh();
		if (!Data->Talismans.ContainsByPredicate([this](const FGbTalisman& T) { return T.Id == Data->PartnerId && IsShown(T); }))
		{
			const FGbTalisman* First = Data->Talismans.FindByPredicate([this](const FGbTalisman& T) { return IsShown(T); });
			Data->PartnerId = First ? First->Id : FString(StarterId);
		}
	}
	Save();
}

bool UGbPlayerSubsystem::IsSettling()
{
	const double Now = FPlatformTime::Seconds();
	for (auto It = Settling.CreateIterator(); It; ++It)
	{
		if (Now - It.Value() > SettleTimeoutSeconds)
		{
			PendingTrades.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	return Settling.Num() > 0;
}

bool UGbPlayerSubsystem::IsShown(const FGbTalisman& T) const
{
	return !bOnChain || T.Id == StarterId || !T.ChainTokenId.IsEmpty() || (!T.BattleId.IsEmpty() && Settling.Contains(T.BattleId));
}

void UGbPlayerSubsystem::MakeFresh()
{
	Data = Cast<UGbSaveGame>(UGameplayStatics::CreateSaveGameObject(UGbSaveGame::StaticClass()));
	Data->Points = 0;
	Data->Ember = StartingEmber;
	FGbTalisman Starter;
	Starter.Id = StarterId;
	Starter.SpeciesId = StarterSpecies;
	Starter.Omen = StarterOmen;
	Data->Talismans = { Starter };
	Data->PartnerId = Starter.Id;
	Data->NextTalisman = 1;
}

void UGbPlayerSubsystem::Load()
{
	Data = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Data = Cast<UGbSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
	if (!Data || Data->Talismans.Num() == 0)
	{
		MakeFresh();
		return;
	}
	const bool bPartnerKnown = Data->Talismans.ContainsByPredicate([this](const FGbTalisman& T) { return T.Id == Data->PartnerId; });
	if (!bPartnerKnown) Data->PartnerId = Data->Talismans[0].Id;
}

void UGbPlayerSubsystem::Save()
{
	if (Data) UGameplayStatics::SaveGameToSlot(Data, SlotName, 0);
	OnPlayerChanged.Broadcast();
}

int32 UGbPlayerSubsystem::GetPoints() const
{
	return Data ? Data->Points : 0;
}

int32 UGbPlayerSubsystem::GetEmber() const
{
	return Data ? Data->Ember : StartingEmber;
}

TArray<FGbTalisman> UGbPlayerSubsystem::GetTalismans() const
{
	TArray<FGbTalisman> Out;
	if (Data)
	{
		for (const FGbTalisman& T : Data->Talismans) if (IsShown(T)) Out.Add(T);
	}
	return Out;
}

FGbTalisman UGbPlayerSubsystem::GetPartner() const
{
	if (Data)
	{
		for (const FGbTalisman& T : Data->Talismans)
		{
			if (T.Id == Data->PartnerId && IsShown(T)) return T;
		}
		for (const FGbTalisman& T : Data->Talismans)
		{
			if (IsShown(T)) return T;
		}
	}
	FGbTalisman Starter;
	Starter.Id = StarterId;
	Starter.SpeciesId = StarterSpecies;
	Starter.Omen = StarterOmen;
	return Starter;
}

void UGbPlayerSubsystem::SetPartner(const FString& TalismanId)
{
	if (!Data || Data->PartnerId == TalismanId) return;
	if (!Data->Talismans.ContainsByPredicate([&](const FGbTalisman& T) { return T.Id == TalismanId && IsShown(T); })) return;
	Data->PartnerId = TalismanId;
	Save();
}

void UGbPlayerSubsystem::GetRecord(int32& Won, int32& Bound, int32& Fled, int32& Lost) const
{
	Won = Data ? Data->Won : 0;
	Bound = Data ? Data->Bound : 0;
	Fled = Data ? Data->Fled : 0;
	Lost = Data ? Data->Lost : 0;
}

void UGbPlayerSubsystem::ResetProgress()
{
	MakeFresh();
	Save();
}

void UGbPlayerSubsystem::StartEncounterGrace()
{
	GraceMetresLeft = GetDefault<UGbSettings>()->GraceMetres;
}

bool UGbPlayerSubsystem::ConsumeGraceMetre()
{
	if (GraceMetresLeft > 0)
	{
		--GraceMetresLeft;
		return false;
	}
	return true;
}

void UGbPlayerSubsystem::HandleBattleTurn(const FGbBattleTurn& Turn)
{
	if (!Turn.bEnded || !Data) return;

	const bool bWon = Turn.Outcome == EGbOutcome::Won || Turn.Outcome == EGbOutcome::Bound;
	switch (Turn.Outcome)
	{
	case EGbOutcome::Won: ++Data->Won; break;
	case EGbOutcome::Bound: ++Data->Bound; break;
	case EGbOutcome::Fled: ++Data->Fled; break;
	case EGbOutcome::Lost: ++Data->Lost; break;
	default: break;
	}

	if (bWon)
	{
		// The server already paid these points into the town's tally; this is the player's own copy.
		Data->Points += FMath::Max(0, Turn.Points);
		for (FGbTalisman& T : Data->Talismans)
		{
			if (T.Id == Data->PartnerId)
			{
				++T.Wins;
				break;
			}
		}
	}

	FGbTalisman NewTalisman;
	const bool bBound = Turn.Outcome == EGbOutcome::Bound;
	if (bBound)
	{
		NewTalisman.Id = FString::Printf(TEXT("tal-%d"), Data->NextTalisman++);
		NewTalisman.SpeciesId = Turn.View.Wild.SpeciesId;
		NewTalisman.Omen = Turn.View.Wild.Omen;
		NewTalisman.Seed = Turn.Seed;
		NewTalisman.BattleId = Turn.BattleId;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UGbApiSubsystem* Api = GI->GetSubsystem<UGbApiSubsystem>())
			{
				NewTalisman.BoundDay = Api->GetWorldState().Day;
			}
		}
		Data->Talismans.Add(NewTalisman);
		if (Turn.bHasChain && Turn.Chain.Status == TEXT("pending")) Settling.Add(Turn.BattleId, FPlatformTime::Seconds());
	}

	Save();
	if (bBound) OnTalismanBound.Broadcast(NewTalisman);
}

void UGbPlayerSubsystem::HandleShop(const FGbShop& Shop)
{
	if (!Data || Shop.Ember < 0 || Shop.Ember == Data->Ember) return;
	Data->Ember = Shop.Ember;
	Save();
}

void UGbPlayerSubsystem::HandleTrade(const FGbTrade& Trade)
{
	PendingTrades.Add(Trade.JobId, Trade);
	Settling.Add(Trade.JobId, FPlatformTime::Seconds());
}

void UGbPlayerSubsystem::HandleTradeReceipt(const FString& JobId, const FGbChainReceipt& Receipt)
{
	FGbTrade Trade;
	Settling.Remove(JobId);
	if (!PendingTrades.RemoveAndCopyValue(JobId, Trade) || !Data || Receipt.Status != TEXT("done")) return;
	if (Trade.bBuy)
	{
		FGbTalisman T;
		T.Id = FString::Printf(TEXT("tal-%d"), Data->NextTalisman++);
		T.SpeciesId = Trade.SpeciesId;
		T.Omen = Trade.Omen;
		T.ChainTokenId = Receipt.TokenId;
		T.ChainUrl = Receipt.ExplorerUrl;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UGbApiSubsystem* Api = GI->GetSubsystem<UGbApiSubsystem>()) T.BoundDay = Api->GetWorldState().Day;
		}
		Data->Talismans.Add(T);
		Data->Ember = FMath::Max(0, Data->Ember - Trade.Price);
	}
	else
	{
		const int32 At = Data->Talismans.IndexOfByPredicate([&Trade](const FGbTalisman& T) { return T.ChainTokenId == Trade.TokenId; });
		if (At != INDEX_NONE)
		{
			const bool bWasPartner = Data->Talismans[At].Id == Data->PartnerId;
			Data->Talismans.RemoveAt(At);
			// Never leave the player without a partner: fall back to the first talisman left.
			if (bWasPartner) Data->PartnerId = Data->Talismans.Num() > 0 ? Data->Talismans[0].Id : FString();
		}
		Data->Ember += Trade.Price;
	}
	Save();
	RefreshFromChain();
}

void UGbPlayerSubsystem::HandleChainReceipt(const FString& BattleId, const FGbChainReceipt& Receipt)
{
	// Done or failed, the mint has settled: a failed one stays a local-only talisman (hidden once on chain).
	const bool bWasSettling = Settling.Remove(BattleId) > 0;
	if (!Data) return;
	if (Receipt.Status != TEXT("done") || Receipt.TokenId.IsEmpty())
	{
		if (bWasSettling)
		{
			Save(); // the case redraws without it
			RefreshFromChain();
		}
		return;
	}
	for (FGbTalisman& T : Data->Talismans)
	{
		if (T.BattleId == BattleId)
		{
			T.ChainTokenId = Receipt.TokenId;
			T.ChainUrl = Receipt.ExplorerUrl;
			Save();
			RefreshFromChain(); // The Rite's points changed too.
			return;
		}
	}
}
