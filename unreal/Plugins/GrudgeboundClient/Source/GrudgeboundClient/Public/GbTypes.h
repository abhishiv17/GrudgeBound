// Data the game server sends, as Blueprint-readable structs.
// Field meanings: docs/UNREAL_SETUP.md section 5, and server/src/battles.ts.
#pragma once

#include "CoreMinimal.h"
#include "GbTypes.generated.h"

/** A move sent to POST /battle/turn. */
UENUM(BlueprintType)
enum class EGbAction : uint8
{
	Attack,
	Ward,
	Seal,
	GildedSeal,
	Salve,
	Flee,
};

/** How a battle ended. None while it is still going. */
UENUM(BlueprintType)
enum class EGbOutcome : uint8
{
	None,
	/** Exorcised: points, no talisman. */
	Won,
	/** Sealed into a talisman: points and a talisman. */
	Bound,
	Fled,
	Lost,
};

USTRUCT(BlueprintType)
struct FGbGrudge
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") float Rage = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") float Sorrow = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") float Dread = 0.f;
};

USTRUCT(BlueprintType)
struct FGbWardSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbGrudge Grudge;
	/** Rage + Sorrow + Dread, out of 100. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") float Total = 0.f;
	/** Curses roaming the Ward right now. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Wild = 0;
};

USTRUCT(BlueprintType)
struct FGbMerchantSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Ward;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Ember = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bClosed = false;
	/** Item id -> sell price in Ember. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TMap<FString, int32> Prices;
};

USTRUCT(BlueprintType)
struct FGbLeaderEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Name;
	/** "player" or "agent". */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Kind;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Points = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Ember = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Talismans = 0;
};

USTRUCT(BlueprintType)
struct FGbRule
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Id = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Author;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Cost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString TemplateId;
};

/** GET /state: the whole town. */
USTRUCT(BlueprintType)
struct FGbWorldState
{
	GENERATED_BODY()

	/** False until the first answer from the server arrives. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Tick = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Day = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Hour = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bNight = true;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbWardSummary> Wards;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbMerchantSummary> Merchants;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbLeaderEntry> Leaderboard;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbRule> Rules;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 BurnedEmber = 0;
};

/** One line of the town log (GET /events). */
USTRUCT(BlueprintType)
struct FGbTownEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Tick = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Day = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Hour = 0;
	/** e.g. "bankruptcy", "calamity", "rule", "capture". */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Type;
	/** Empty when the event is town-wide. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Ward;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Actor;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Text;
};

/** GET /curses: a curse's name and looks (stats stay on the server). */
USTRUCT(BlueprintType)
struct FGbCurseInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Name;
	/** "rage", "sorrow" or "dread". */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Emotion;
	/** "whisper", "wail" or "calamity". */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Omen;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Home;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString BornFrom;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString MoveName;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString MoveEffect;
};

/** One side of a battle. */
USTRUCT(BlueprintType)
struct FGbSide
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString SpeciesId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Emotion;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Omen;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Hp = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 MaxHp = 1;
};

/** Everything the battle screen draws; sent after start and after every turn. */
USTRUCT(BlueprintType)
struct FGbBattleView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Turn = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") EGbOutcome Outcome = EGbOutcome::None;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbSide Player;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbSide Wild;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Salves = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 GildedSeals = 0;
	/** 0-1, shown on the buttons. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") float SealChance = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") float GildedSealChance = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") float FleeChance = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bCanAttack = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bCanWard = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bCanSeal = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bCanGildedSeal = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bCanSalve = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bCanFlee = false;
};

/** POST /battle/start answer: the curse met in the tall grass. */
USTRUCT(BlueprintType)
struct FGbBattleStart
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString BattleId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Ward;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbCurseInfo Wild;
	/** "advantage", "disadvantage" or "even": how your partner fares against this curse. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Matchup;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbBattleView View;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FString> Lines;
};

/** The "chain" part of a finished battle, and GET /chain?battle=<id>: the binding or exorcism on NEAR (P2-C4). */
USTRUCT(BlueprintType)
struct FGbChainReceipt
{
	GENERATED_BODY()

	/** "pending" (still writing; poll again), "done" or "failed". */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Status;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Network;
	/** NEAR account that got the talisman and points. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Account;
	/** Token id on the items contract, e.g. "tal-7"; empty for an exorcism. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString TokenId;
	/** Rule id on the rite contract, for a rule proposal; -1 otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 RuleId = -1;
	/** Explorer link of the mint (bind), the verdict (rule), or else the points transaction. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString ExplorerUrl;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Error;
};

/** POST /battle/turn answer. */
USTRUCT(BlueprintType)
struct FGbBattleTurn
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString BattleId;
	/** The move as the server words it, e.g. "attack". */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Action;
	/** Ready-made log lines for this turn. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FString> Lines;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbBattleView View;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") EGbOutcome Outcome = EGbOutcome::None;
	/** True on the last turn; Points and Seed are only set then. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bEnded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Points = 0;
	/** Seed of the battle, revealed at the end so anyone can replay it. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int64 Seed = -1;
	/** True when the server is writing this battle to NEAR (it runs with chain keys); see Chain. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bHasChain = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbChainReceipt Chain;
};

/** A talisman as the items contract reports it (GET /player). */
USTRUCT(BlueprintType)
struct FGbChainTalisman
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString TokenId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString SpeciesId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Emotion;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Omen;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Level = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Wins = 0;
	/** Ember the talisman has earned; moves with it when traded. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Purse = 0;
};

/** GET /player?account=: you as the chain sees you. */
USTRUCT(BlueprintType)
struct FGbPlayerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Account;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Name;
	/** False when the server runs off-chain: keep the local save. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bChain = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Points = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Ember = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbChainTalisman> Talismans;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString ExplorerUrl;
};

/** One line of a merchant's stock from GET /shop. */
USTRUCT(BlueprintType)
struct FGbShopItem
{
	GENERATED_BODY()

	/** e.g. "talisman-sorrow", "binding-seal". */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Stock = 0;
	/** What the merchant charges you. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Price = 0;
	/** The merchant's one-line reason (written by the AI when the server has a NEAR AI key). */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Text;
	/** What the merchant pays for one of yours (talismans only); 0 when it doesn't buy them. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 BuyBack = 0;
};

/** GET /shop?merchant=&account=. */
USTRUCT(BlueprintType)
struct FGbShop
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString MerchantId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString MerchantName;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bClosed = false;
	/** Your Ember on chain; -1 when unknown (off-chain server, no account). */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Ember = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbShopItem> Items;
};

/** POST /shop/buy or /shop/sell answer; Chain finishes later (OnTradeReceipt). */
USTRUCT(BlueprintType)
struct FGbTrade
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString JobId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bBuy = true;
	/** Ember paid (buy) or received (sell). */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Price = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString SpeciesId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Omen;
	/** Sold: the token handed over. Bought: set on the receipt once minted. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString TokenId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbChainReceipt Chain;
};

/** One value a rule parameter may take, with the words the rule shows for it. */
USTRUCT(BlueprintType)
struct FGbRuleOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Value;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Label;
};

USTRUCT(BlueprintType)
struct FGbRuleParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Key;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbRuleOption> Options;
};

/** A rule template from GET /rules, e.g. "No PvP in {ward}". */
USTRUCT(BlueprintType)
struct FGbRuleTemplate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbRuleParam> Params;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bCruel = false;
};

USTRUCT(BlueprintType)
struct FGbActiveRule
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Id = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Author;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Cost = 0;
};

/** GET /rules?account=: everything the Rules panel draws. */
USTRUCT(BlueprintType)
struct FGbRulesOverview
{
	GENERATED_BODY()

	/** False when the server runs off-chain (proposals are refused). */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bChain = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Cost = 0;
	/** Your points on the rite contract; -1 when unknown. */
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Points = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbRuleTemplate> Templates;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") TArray<FGbActiveRule> Active;
};

/** POST /rules/propose answer: Kessa's verdict now; Chain finishes later (OnRuleReceipt). */
USTRUCT(BlueprintType)
struct FGbRuleVerdict
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString JobId;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") bool bAccepted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Reason;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") int32 Cost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Grudgebound") FGbChainReceipt Chain;
};
