// Runs the Phase 1 loop on screen: walk -> tall grass -> encounter (Fight / Slip away) -> battle -> back to the Ward.
// Spawned automatically on Play by UGbGameFlowSubsystem when Project Settings > Grudgebound > Create Default Ui is on.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "GbTypes.h"
#include "GbUiDirector.generated.h"

class UGbHudWidget;
class UGbEncounterWidget;
class UGbBattleWidget;
class UGbTalismanCaseWidget;

UCLASS(NotPlaceable, Transient)
class GRUDGEBOUNDCLIENT_API AGbUiDirector : public AActor
{
	GENERATED_BODY()

public:
	AGbUiDirector();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	enum class EScreen : uint8 { Ward, Encounter, Battle, TalismanCase };

	void ShowScreen(EScreen Screen);
	void ToggleTalismanCase();
	void Fight();
	void SlipAway();
	void BackToWard();

	UFUNCTION() void HandleBattleStarted(const FGbBattleStart& Start);
	UFUNCTION() void HandleBattleTurn(const FGbBattleTurn& Turn);
	UFUNCTION() void HandleApiError(const FString& What, int32 Status, const FString& Message);
	UFUNCTION() void HandleChainReceipt(const FString& BattleId, const FGbChainReceipt& Receipt);
	/** L: open the last NEAR transaction in the browser. */
	void OpenLastChainLink();

	UPROPERTY() TObjectPtr<UGbHudWidget> Hud;
	UPROPERTY() TObjectPtr<UGbEncounterWidget> Encounter;
	UPROPERTY() TObjectPtr<UGbBattleWidget> Battle;
	UPROPERTY() TObjectPtr<UGbTalismanCaseWidget> TalismanCase;

	EScreen Current = EScreen::Ward;
	/** False until the first dialog opens. */
	bool bInputTouched = false;
	FGbBattleStart PendingBattle;
	/** Wild curse of each battle that just ended, for Kessa's line when its receipt arrives. */
	TMap<FString, FString> CurseOfBattle;
	FString LastChainUrl;
};

/** Starts polling the server and spawns the UI director when a game world begins play. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbGameFlowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
