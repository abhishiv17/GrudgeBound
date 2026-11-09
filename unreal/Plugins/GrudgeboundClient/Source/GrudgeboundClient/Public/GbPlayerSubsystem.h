// The player's own progress: points, Ember, talismans, partner, battle record.
// Phase 1: saved on this PC (SaveGame slot "Grudgebound"), mirroring client/lib/playerStore.ts.
// Phase 2: filled from the chain instead (talismans = NEP-245 tokens, points = the Rite contract).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GbTypes.h"
#include "GbPlayerSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FGbTalisman
{
	GENERATED_BODY()

	/** Local id now ("tal-starter", "tal-1", ...); becomes the NEP-245 token id once minted. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") FString Id;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") FString SpeciesId;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") FString Omen;
	/** In-game day it was bound; -1 for the starter Kessa gives you. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") int32 BoundDay = -1;
	/** Seed of the battle that bound it (replayable); -1 for the starter. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") int64 Seed = -1;
	/** Battles this talisman has won or bound a curse in. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") int32 Wins = 0;
	/** Server battle that bound it, to match the NEAR receipt that arrives a few seconds later. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") FString BattleId;
	/** Token id on the items contract once minted on NEAR (e.g. "tal-7"); empty while off-chain. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") FString ChainTokenId;
	/** Explorer link of the mint transaction. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") FString ChainUrl;
	/** Ember this talisman has earned on chain (bounties). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound") int32 Purse = 0;
};

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) int32 Points = 0;
	UPROPERTY(SaveGame) int32 Ember = 60;
	UPROPERTY(SaveGame) TArray<FGbTalisman> Talismans;
	UPROPERTY(SaveGame) FString PartnerId;
	UPROPERTY(SaveGame) int32 NextTalisman = 1;
	UPROPERTY(SaveGame) int32 Won = 0;
	UPROPERTY(SaveGame) int32 Bound = 0;
	UPROPERTY(SaveGame) int32 Fled = 0;
	UPROPERTY(SaveGame) int32 Lost = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGbPlayerChangedEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbTalismanBoundEvent, const FGbTalisman&, Talisman);

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbPlayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Same purse every simulated participant starts with in the engine. */
	static constexpr int32 StartingEmber = 60;
	static const TCHAR* StarterSpecies;
	static const TCHAR* StarterOmen;
	static const TCHAR* StarterId;
	static const TCHAR* SlotName;
	/** Server's reserved id for the built-in partner; distinct from the old local SaveGame id. */
	static FString ServerTokenId(const FGbTalisman& Talisman);
	FGbSatchel GetSatchel() const { return Satchel; }
	FString GetPartnerTokenId() const;
	bool CanStartBattle(FString& Hint) const;
	bool CanChoosePartner(const FGbTalisman& Talisman) const;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** Identity whose local save is currently loaded (used to avoid cross-profile narrative callbacks). */
	FString GetActiveAccount() const { return ActiveAccount; }

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	int32 GetPoints() const;

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	int32 GetEmber() const;

	/**
	 * The talismans in your case. Once the chain has answered, only the starter, talismans on NEAR and ones still
	 * being minted are shown: Phase-1 binds and binds whose NEAR write failed stay in the save but out of sight.
	 */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	TArray<FGbTalisman> GetTalismans() const;

	/** The talisman whose curse fights for you. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	FGbTalisman GetPartner() const;

	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Player")
	void SetPartner(const FString& TalismanId);

	/** Ask the server for your talismans, points and Ember on NEAR; the case updates when they arrive. */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Player")
	void RefreshFromChain();

	/** True once the chain has answered for your Player Account: points, Ember and NEAR talismans are the chain's. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	bool IsOnChain() const { return bOnChain; }

	/** Your account on the block explorer (empty off-chain). */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	FString GetAccountUrl() const { return AccountUrl; }

	/** Won / bound / fled / lost counts. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	void GetRecord(int32& Won, int32& Bound, int32& Fled, int32& Lost) const;

	/** Start over with only the starter talisman. */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Player")
	void ResetProgress();

	/** After a battle: metres of tall grass that are safe before the next curse (Project Settings > Grudgebound). */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Encounters")
	void StartEncounterGrace();

	/** Called by tall grass once per metre walked: true when a curse may be rolled for, false during grace. */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Encounters")
	bool ConsumeGraceMetre();

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Player")
	FGbPlayerChangedEvent OnPlayerChanged;

	/** A curse was sealed into a new talisman. */
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Player")
	FGbTalismanBoundEvent OnTalismanBound;

private:
	friend class FGbSatchelPartnerTest;
	friend class FGbSatchelSupplyTradeTest;
	UFUNCTION() void HandleSatchel(const FGbSatchel& View);
	UFUNCTION() void HandleSatchelError(const FString& What, int32 Status, const FString& Message);
	FGbSatchel Satchel;
	UFUNCTION() void HandleSessionChanged();
	FString ActiveAccount;
	FString SaveSlot = TEXT("Grudgebound");
	UFUNCTION()
	void HandleBattleTurn(const FGbBattleTurn& Turn);

	UFUNCTION()
	void HandleChainReceipt(const FString& BattleId, const FGbChainReceipt& Receipt);

	/** The shop reports your chain Ember: it replaces the local count. */
	UFUNCTION()
	void HandleShop(const FGbShop& Shop);

	UFUNCTION()
	void HandleTrade(const FGbTrade& Trade);

	UFUNCTION()
	void HandleTradeReceipt(const FString& JobId, const FGbChainReceipt& Receipt);

	/** Trades waiting for their NEAR receipt. */
	TMap<FString, FGbTrade> PendingTrades;

	/** Battle ids and trade job ids still settling on NEAR, with when they started (FPlatformTime::Seconds). */
	TMap<FString, double> Settling;

	/** Longest a mint or trade may hold back the talisman list (its receipt may never come: server restart, poll gave up). */
	static constexpr double SettleTimeoutSeconds = 90.0;

	/** True while a mint or trade is settling; forgets ones older than SettleTimeoutSeconds. */
	bool IsSettling();

	/** See GetTalismans. */
	bool IsShown(const FGbTalisman& T) const;

	UFUNCTION()
	void HandlePlayerView(const FGbPlayerView& View);

	bool bOnChain = false;
	FString AccountUrl;

	void Load();
	void Save();
	void MakeFresh();

	UPROPERTY()
	TObjectPtr<UGbSaveGame> Data;

	int32 GraceMetresLeft = 0;
};
