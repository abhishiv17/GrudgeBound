// GrudgeboundApi: every call the Unreal client makes to the game server (docs/UNREAL_SETUP.md section 5).
// Lives as long as the game instance, so it survives level changes.
//
// Blueprint use: Get Game Instance Subsystem (GbApiSubsystem), bind to the On... events, call StartBattle / SendAction.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineTypes.h"
#include "GbTypes.h"
#include "GbApiSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGbServerStatusEvent, bool, bOnline, int32, Tick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbWorldStateEvent, const FGbWorldState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbTownEventsEvent, const TArray<FGbTownEvent>&, Events);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbKessaLineEvent, const FString&, Line);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbBattleStartedEvent, const FGbBattleStart&, Battle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbBattleTurnEvent, const FGbBattleTurn&, Turn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbShopEvent, const FGbShop&, Shop);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbTradeEvent, const FGbTrade&, Trade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbRulesEvent, const FGbRulesOverview&, Rules);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGbRuleVerdictEvent, const FGbRuleVerdict&, Verdict);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGbChainReceiptEvent, const FString&, BattleId, const FGbChainReceipt&, Receipt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGbApiErrorEvent, const FString&, What, int32, Status, const FString&, Message);

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------ town (GET /health, /state, /events, /curses)

	/** Fetch /state and /events every PollSeconds (Project Settings > Grudgebound). Also loads /curses once. */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Server")
	void StartPolling();

	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Server")
	void StopPolling();

	/** Fetch /state and /events now. */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Server")
	void RefreshNow();

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Server")
	bool IsServerOnline() const { return bOnline; }

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Server")
	FString GetServerUrl() const;

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Town")
	FGbWorldState GetWorldState() const { return WorldState; }

	/** The named Ward from the latest /state. False before the first answer. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Town")
	bool GetWard(const FString& WardId, FGbWardSummary& OutWard) const;

	/** Name and looks of a curse; falls back to the id until /curses has loaded. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Town")
	FGbCurseInfo GetCurse(const FString& SpeciesId) const;

	/** What Kessa last said: the newest town log line, or a line the game pushed with SayAsKessa. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Town")
	FString GetKessaLine() const { return KessaLine; }

	/** Show a line in Kessa's banner without the server (e.g. "You slipped away"). */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Town")
	void SayAsKessa(const FString& Line);

	// ------------------------------------------------------------ battles (POST /battle/start, /battle/turn)

	/**
	 * The player walked into tall grass and a curse appears. The server picks the curse from the Ward's Grudge.
	 * Answer: OnBattleStarted (or OnError). Ignored while a battle is open or a request is in flight.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Battle")
	void StartBattle(const FString& WardId, const FString& PlayerCurse);

	/** Send one move. Answer: OnBattleTurn. A 409 (move not available) keeps the turn and the battle. */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Battle")
	void SendAction(EGbAction Action);

	/** Walk away from the encounter ("Slip away"). No call needed: the server drops the battle after 15 minutes. */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Battle")
	void AbandonBattle();

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Battle")
	bool IsBattleOpen() const { return !BattleId.IsEmpty(); }

	/** True while a /battle request is waiting for its answer (grey out the buttons). */
	UFUNCTION(BlueprintPure, Category = "Grudgebound|Battle")
	bool IsBattleBusy() const { return bBattleRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "Grudgebound|Battle")
	FGbBattleView GetBattleView() const { return BattleView; }

	// ------------------------------------------------------------ rules (GET /rules, POST /rules/propose)

	/** Templates, rules in force, cost and your points. Answer: OnRules (or OnError "rules"). */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Rules")
	void FetchRules();

	/**
	 * Propose a rule as Project Settings > Grudgebound > Player Account. Answer: OnRuleVerdict (Kessa's verdict),
	 * then OnRuleReceipt once it is on NEAR. Refusals (too few points, ...) come as OnError "rules/propose".
	 */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Rules")
	void ProposeRule(const FString& TemplateId, const TMap<FString, FString>& Params);

	// ------------------------------------------------------------ shop (GET /shop, POST /shop/buy, /shop/sell)

	/** Prices, reasons, stock, buy-back offers and your chain Ember. Answer: OnShop (or OnError "shop"). */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Shop")
	void FetchShop(const FString& MerchantId);

	/** Buy a talisman (ItemId "talisman-sorrow", ...). Answer: OnTrade, then OnTradeReceipt. Refusals: OnError "shop/buy". */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Shop")
	void BuyItem(const FString& MerchantId, const FString& ItemId);

	/** Sell a talisman you own on chain. Answer: OnTrade, then OnTradeReceipt. Refusals: OnError "shop/sell". */
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|Shop")
	void SellTalisman(const FString& MerchantId, const FString& TokenId);

	// ------------------------------------------------------------ events

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Server")
	FGbServerStatusEvent OnServerStatus;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Town")
	FGbWorldStateEvent OnWorldState;

	/** Only the lines that are new since the last fetch. */
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Town")
	FGbTownEventsEvent OnTownEvents;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Town")
	FGbKessaLineEvent OnKessaLine;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Battle")
	FGbBattleStartedEvent OnBattleStarted;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Battle")
	FGbBattleTurnEvent OnBattleTurn;

	/**
	 * A finished battle's NEAR receipt is final ("done" or "failed"). The last turn answers with "pending"
	 * (testnet takes a few seconds), so this subsystem polls GET /chain?battle=<id> until then.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Battle")
	FGbChainReceiptEvent OnChainReceipt;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Shop")
	FGbShopEvent OnShop;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Shop")
	FGbTradeEvent OnTrade;

	/** A trade's NEAR receipt is final (BattleId holds the trade's job id). */
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Shop")
	FGbChainReceiptEvent OnTradeReceipt;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Rules")
	FGbRulesEvent OnRules;

	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Rules")
	FGbRuleVerdictEvent OnRuleVerdict;

	/** A proposal's NEAR receipt is final (BattleId holds the proposal's job id). */
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Rules")
	FGbChainReceiptEvent OnRuleReceipt;

	/** What = "state", "events", "curses", "battle/start" or "battle/turn"; Status 0 = no answer (server down). */
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|Server")
	FGbApiErrorEvent OnError;

private:
	using FDone = TFunction<void(int32 Status, const FString& Body)>;
	void Send(const FString& Verb, const FString& Path, const FString& Body, FDone Done);

	void Poll();
	void FetchState();
	void FetchEvents();
	void FetchCurses();
	void SetOnline(bool bNowOnline);
	/** Ask GET /chain?<Kind>=<id> again in a second, up to TriesLeft times (Kind: "battle", "rule" or "shop"). */
	void SendTrade(bool bBuy, const FString& MerchantId, const FString& Key, const FString& Value);
	void PollChain(const FString& Kind, const FString& Id, int32 TriesLeft);

	FTimerHandle PollTimer;
	bool bOnline = false;
	bool bStateInFlight = false;
	bool bEventsInFlight = false;
	bool bCursesLoaded = false;
	bool bCursesInFlight = false;

	FGbWorldState WorldState;
	TMap<FString, FGbCurseInfo> Curses;
	FString KessaLine;

	/** Newest event tick seen; /events?since= returns events with a later tick. */
	int32 LastEventTick = -1;
	/** Texts already shown for LastEventTick, so a tick fetched twice isn't announced twice. */
	TSet<FString> SeenAtLastTick;

	FString BattleId;
	FGbBattleView BattleView;
	bool bBattleRequestInFlight = false;
};
