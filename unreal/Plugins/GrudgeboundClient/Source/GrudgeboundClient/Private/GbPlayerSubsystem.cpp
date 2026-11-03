#include "GbPlayerSubsystem.h"

#include "GbApiSubsystem.h"
#include "GbSettings.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

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
	}
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
	return Data ? Data->Talismans : TArray<FGbTalisman>();
}

FGbTalisman UGbPlayerSubsystem::GetPartner() const
{
	if (Data)
	{
		for (const FGbTalisman& T : Data->Talismans)
		{
			if (T.Id == Data->PartnerId) return T;
		}
		if (Data->Talismans.Num() > 0) return Data->Talismans[0];
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
	if (!Data->Talismans.ContainsByPredicate([&](const FGbTalisman& T) { return T.Id == TalismanId; })) return;
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
	}

	Save();
	if (bBound) OnTalismanBound.Broadcast(NewTalisman);
}

void UGbPlayerSubsystem::HandleChainReceipt(const FString& BattleId, const FGbChainReceipt& Receipt)
{
	if (!Data || Receipt.Status != TEXT("done") || Receipt.TokenId.IsEmpty()) return;
	for (FGbTalisman& T : Data->Talismans)
	{
		if (T.BattleId == BattleId)
		{
			T.ChainTokenId = Receipt.TokenId;
			T.ChainUrl = Receipt.ExplorerUrl;
			Save();
			return;
		}
	}
}
