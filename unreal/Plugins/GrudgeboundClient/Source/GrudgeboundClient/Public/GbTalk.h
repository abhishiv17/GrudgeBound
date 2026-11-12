// Townsfolk you can talk to: walk up to someone on the street, press Interact, and they stop and say a line.
// What they say follows their trade, the mood of their Ward's Grudge and the story chapter. Picture and words only:
// no trade, no quest, nothing sent to the server.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GbTypes.h"
#include "GbWidgets.h"
#include "GbTalk.generated.h"

class AGbTownsfolk;
class AGbUiDirector;

namespace GbTalk
{
	enum class EMood : uint8 { Calm, Rage, Sorrow, Dread };
	/** The Ward's loudest feeling, or Calm while its Grudge is low. */
	GRUDGEBOUNDCLIENT_API EMood Mood(const FGbWardSummary& Ward);
	/** Who the Index-th walker of a Ward is: "Lamplighter". */
	GRUDGEBOUNDCLIENT_API FString Role(const FString& WardId, int32 Index);
	/** What they say the Turn-th time you ask: their trade, then the mood, then the chapter, and round again. */
	/** With gossip about the player, every fourth line is one of those (different townsfolk start on different deeds). */
	GRUDGEBOUNDCLIENT_API FString Line(const FString& WardId, int32 Index, EMood Mood, int32 Chapter, int32 Turn, const TArray<FString>& Gossip = TArray<FString>());
	/** How long a line of this length stays up, in seconds. */
	GRUDGEBOUNDCLIENT_API float ReadingTime(const FString& Line);
}

/** The speech panel: who is speaking on a crimson tag, the line on paper, above Kessa's banner. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbTalkWidget : public UGbWidgetBase
{
	GENERATED_BODY()
public:
	void Say(const FString& Speaker, const FString& Line);
protected:
	virtual void BuildTree() override;
private:
	friend class FGbTalkTest;
	UPROPERTY() TObjectPtr<UTextBlock> Speaker;
	UPROPERTY() TObjectPtr<UTextBlock> Line;
	UPROPERTY() TObjectPtr<UTextBlock> Hint;
};

/** Picks the walker the player is next to, shows its prompt and opens the speech panel on Interact. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbTalkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGbTalkSubsystem, STATGROUP_Tickables); }
	/** Talk range, cm. */
	static constexpr float Reach = 210.f;
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
private:
 UFUNCTION() void QuestReply(const FString& Message);
 UFUNCTION() void QuestError(const FString& What, int32 Code, const FString& Message);
 bool bAwaitingQuest = false;
	void Close();
	UPROPERTY() TObjectPtr<UGbTalkWidget> Panel;
	TWeakObjectPtr<AGbUiDirector> Director;
	TWeakObjectPtr<AGbTownsfolk> Focus;
	TWeakObjectPtr<AGbTownsfolk> Speaking;
	float Remaining = 0.f;
};
