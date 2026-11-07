#pragma once

#include "CoreMinimal.h"
#include "GbTypes.h"
#include "GbStory.generated.h"

/** Local narrative history only; it never grants currency, tokens or gameplay rewards. */
USTRUCT(BlueprintType)
struct FGbStoryProgress
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") bool bIntroSeen = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") bool bFirstBind = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") bool bVisitedBroker = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") bool bDebtAcknowledged = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") bool bBankruptcy = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") bool bCalamity = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") bool bResponded = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") FString DebtWard;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Grudgebound|Story") FString CalamityWard;
};

namespace GbStory
{
	struct FPanel { FString Title; FString Text; };
	const TArray<FPanel>& IntroPanels();
	int32 Chapter(const FGbStoryProgress& Progress);
	FString ChapterTitle(const FGbStoryProgress& Progress);
	FString Objective(const FGbStoryProgress& Progress);
	/** Returns whether a new real-world fact was learned. */
	bool Observe(FGbStoryProgress& Progress, const FGbTownEvent& Event);
	bool Observe(FGbStoryProgress& Progress, const FGbWorldState& World);
	/** A response must be the player's own successful action, after the chapter is available. */
	bool Respond(FGbStoryProgress& Progress);
	FString SaveSlot(const FString& Account, const FString& ServerUrl);
}
