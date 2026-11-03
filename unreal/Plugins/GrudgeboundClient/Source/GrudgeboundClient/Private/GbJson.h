// Reads the server's JSON into the structs in GbTypes.h, and writes request bodies.
// Missing or null fields give the struct's default, so a newer server with extra fields still parses.
#pragma once

#include "CoreMinimal.h"
#include "GbTypes.h"

class FJsonObject;
class FJsonValue;

namespace GbJson
{
	bool ParseObject(const FString& Text, TSharedPtr<FJsonObject>& Out);
	bool ParseArray(const FString& Text, TArray<TSharedPtr<FJsonValue>>& Out);

	/** The "error" field of an error answer, or the raw text when it isn't JSON. */
	FString ErrorMessage(const FString& Text);

	FGbWorldState WorldState(const TSharedPtr<FJsonObject>& O);
	FGbTownEvent TownEvent(const TSharedPtr<FJsonObject>& O);
	FGbCurseInfo Curse(const TSharedPtr<FJsonObject>& O);
	FGbBattleView BattleView(const TSharedPtr<FJsonObject>& O);
	FGbBattleStart BattleStart(const TSharedPtr<FJsonObject>& O);
	FGbBattleTurn BattleTurn(const TSharedPtr<FJsonObject>& O);
	FGbChainReceipt ChainReceipt(const TSharedPtr<FJsonObject>& O);
	FGbRulesOverview RulesOverview(const TSharedPtr<FJsonObject>& O);
	FGbRuleVerdict RuleVerdict(const TSharedPtr<FJsonObject>& O);
	/** {"account":..,"templateId":..,"params":{..}} */
	FString ProposeBody(const FString& Account, const FString& TemplateId, const TMap<FString, FString>& Params);

	EGbOutcome Outcome(const FString& Text);

	/** {"ward":..,"playerCurse":..,"by":..,"account":..} (empty fields left out) */
	FString StartBody(const FString& Ward, const FString& PlayerCurse, const FString& By, const FString& Account = FString());
	/** {"battleId":..,"action":{"kind":..}} */
	FString TurnBody(const FString& BattleId, EGbAction Action);
}
