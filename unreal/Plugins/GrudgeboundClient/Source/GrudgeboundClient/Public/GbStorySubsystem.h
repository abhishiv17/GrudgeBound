#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GbStory.h"
#include "GbStorySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGbStoryChanged);

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbStorySaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) FGbStoryProgress Progress;
};

/** Narrative only. Follows authenticated gameplay; never changes the economy or world clock. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbStorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** Makes entry independent of multicast delegate callback order. */
	void RefreshSession() { HandleSessionChanged(); }
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Story") bool ShouldShowIntro() const;
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Story") FString GetChapterTitle() const;
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Story") FString GetObjective() const;
	/** 1 Arrival, 2 Debts Come Due, 3 The Calamity. */
	int32 GetChapter() const { return Data ? GbStory::Chapter(Data->Progress) : 1; }
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Story") void CompleteIntro();
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Story") void VisitBroker();
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Story") FGbStoryChanged OnStoryChanged;
private:
	friend class FGbStoryReceiptsTest;
	UFUNCTION() void HandleSessionChanged();
	UFUNCTION() void HandleEvents(const TArray<FGbTownEvent>& Events);
	UFUNCTION() void HandleWorld(const FGbWorldState& State);
	UFUNCTION() void HandlePlayer();
	UFUNCTION() void HandleShop(const FGbShop& Shop);
	UFUNCTION() void HandleBattle(const FGbBattleTurn& Turn);
	UFUNCTION() void HandleBattleReceipt(const FString& Id, const FGbChainReceipt& Receipt);
	UFUNCTION() void HandleVerdict(const FGbRuleVerdict& Verdict);
	UFUNCTION() void HandleRuleReceipt(const FString& Id, const FGbChainReceipt& Receipt);
	UFUNCTION() void HandleVowSworn(const FString& Id, const FGbChainReceipt& Receipt);
	void SaveAndNotify();
	UPROPERTY() TObjectPtr<UGbStorySaveGame> Data;
	FString ActiveAccount;
	FString ActiveSlot;
	TSet<FString> PendingBinds;
	TSet<FString> PendingResponses;
	TSet<FString> AcceptedRules;
};
