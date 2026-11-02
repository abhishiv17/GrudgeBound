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

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	int32 GetPoints() const;

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	int32 GetEmber() const;

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	TArray<FGbTalisman> GetTalismans() const;

	/** The talisman whose curse fights for you. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Player")
	FGbTalisman GetPartner() const;

	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Player")
	void SetPartner(const FString& TalismanId);

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
	UFUNCTION()
	void HandleBattleTurn(const FGbBattleTurn& Turn);

	void Load();
	void Save();
	void MakeFresh();

	UPROPERTY()
	TObjectPtr<UGbSaveGame> Data;

	int32 GraceMetresLeft = 0;
};
