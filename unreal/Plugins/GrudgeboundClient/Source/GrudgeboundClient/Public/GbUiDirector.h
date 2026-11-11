// Runs the Phase 1 loop on screen: walk -> tall grass -> encounter (Fight / Slip away) -> battle -> back to the Ward.
// Spawned automatically on Play by UGbGameFlowSubsystem when Project Settings > Grudgebound > Create Default Ui is on.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "GbTypes.h"
#include "GbBuildings.h"
#include "GbUiDirector.generated.h"

class UGbHudWidget;
class UGbEncounterWidget;
class UGbBattleWidget;
class UGbTalismanCaseWidget;
class UGbRulesWidget;
class UGbShopWidget;
class UGbSignInWidget;
class UGbVowWidget;
class UGbIntroWidget;
class UGbSatchelWidget;
class UGbMenderWidget;
class UGbRoomWidget;
class UGbObservatoryWidget;

UCLASS(NotPlaceable, Transient)
class GRUDGEBOUNDCLIENT_API AGbUiDirector : public AActor
{
	GENERATED_BODY()

public:
	AGbUiDirector();
	void UseBuilding(AGbBuildingDoor* Door);
	EGbBuilding GetCurrentBuilding() const { return CurrentBuilding; }
	/** Puts mouse/keyboard focus back where the current screen wants it (after the settings menu closes). */
	void RestoreInput();
	/** Esc: closes the open panel, if there is one. */
	bool CloseTopPanel();

protected:
	virtual void BeginPlay() override;
	/** Walking keys and the camera, from the player's settings. Lives here, not in the pawn or controller, so it
	    works with whatever pawn the level's game mode spawns (the template Blueprints have their own classes). */
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	enum class EScreen : uint8 { Ward, Encounter, Battle, TalismanCase, Rules, Shop, SignIn, Vows, Intro, Satchel, Mender, Room, Observatory };
	void ToggleObservatory();
	void CloseBuildingPanel();
	void RoomCase();
	EGbBuilding CurrentBuilding = EGbBuilding::Outside;
	bool bBuildingTransition = false;
	FTimerHandle BuildingTimer;
	void ShowEntry();
	/** Title screen > Enter: sign in first if needed (browser), then the opening or the Ward. */
	void EnterWard();
	bool bEntered = false;
	/** The title looks over the Ward from a fixed camera; entering glides down to the player. */
	void UpdateTitleCamera(bool bTitle);
	UPROPERTY() TObjectPtr<class ACameraActor> TitleCamera;
	bool bTitleCamera = false;
	float CameraYaw = 0.f;
	float SmoothedYaw = 0.f;
	float SmoothedPitch = 0.f;
	bool bCameraDirty = false;
	bool bKeyWalking = false;
	void FinishIntro();
	void ReplayIntro();
	UFUNCTION() void HandleSessionChanged();
	void SignOut();

	void ShowScreen(EScreen Screen);
	/** (Re)binds the panel keys from UGbUserSettings. */
	void BindKeys();
	void ToggleTalismanCase();
	/** R: the Rite (rules panel). */
	void ToggleRules();
	/** B: the Satchel; its Broker button opens the temporary shop bridge. */
	void ToggleSatchel();
	void ToggleShop();
	void ToggleVows();
	UFUNCTION() void HandleTradeReceipt(const FString& JobId, const FGbChainReceipt& Receipt);
	UFUNCTION() void HandleRuleReceipt(const FString& JobId, const FGbChainReceipt& Receipt);
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
	UPROPERTY() TObjectPtr<UGbRulesWidget> Rules;
	UPROPERTY() TObjectPtr<UGbShopWidget> Shop;
	UPROPERTY() TObjectPtr<UGbSignInWidget> SignIn;
	UPROPERTY() TObjectPtr<UGbVowWidget> VowShrine;
	UPROPERTY() TObjectPtr<UGbIntroWidget> Intro;
	UPROPERTY() TObjectPtr<UGbSatchelWidget> Satchel;
	UPROPERTY() TObjectPtr<UGbMenderWidget> Mender;
	UPROPERTY() TObjectPtr<UGbRoomWidget> Room;
	UPROPERTY() TObjectPtr<UGbObservatoryWidget> Observatory;

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
