#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GbTownsfolk.generated.h"

class UGbExorcistLook;
class UTextRenderComponent;

/**
 * Someone walking a street: no trade, no collision; they stroll back and forth along a path, stop a moment at each
 * end and wait when the player stands in their way. Walk up and press Interact and they stop to say a line
 * (UGbTalkSubsystem, GbTalk.h). The Wards spawn them on Play (AGbWardGreybox::BeginPlay), so they are never saved
 * into a level.
 */
UCLASS()
class GRUDGEBOUNDCLIENT_API AGbTownsfolk : public AActor
{
	GENERATED_BODY()

public:
	AGbTownsfolk();
	/** Call between SpawnActorDeferred and FinishSpawning: the path (world, on the ground) and who this is. */
	void Setup(const TArray<FVector>& WorldPath, int32 Seed);
	virtual void Tick(float DeltaSeconds) override;

	/** Where a walker Distance along a there-and-back path of these points stands, and which way it faces. */
	static FVector Along(const TArray<FVector>& Path, float Distance, FVector& OutDirection);
	static float Length(const TArray<FVector>& Path);

	/** Which Ward they live in and which of its voices they are (GbTalk::Role / Line). */
	void SetVoice(const FString& InWardId, int32 InVoice) { WardId = InWardId; Voice = InVoice; }
	const FString& GetWardId() const { return WardId; }
	int32 GetVoice() const { return Voice; }
	/** Counts the lines asked for so far. */
	int32 NextTurn() { return Turn++; }
	/** "Talk [E]" over their head while the player is next to them. */
	void ShowPrompt(bool bShow);
	/** Stand still facing Toward for this long (talking); Release lets them walk on. */
	void Hold(float Seconds, const AActor* Toward);
	void Release() { HoldTime = 0.f; }
	bool IsHeld() const { return HoldTime > 0.f; }

private:
	friend class FGbTownsfolkMovementTest;
	friend class FGbTalkTest;
	UPROPERTY() TObjectPtr<UGbExorcistLook> Look;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Prompt;
	TWeakObjectPtr<const AActor> HoldToward;
	float HoldTime = 0.f;
	FString WardId = TEXT("lantern");
	int32 Voice = 0;
	int32 Turn = 0;
	TArray<FVector> Path;
	float Travelled = 0.f;
	float Speed = 120.f;
	float CurrentSpeed = 0.f;
	float Pause = 0.f;
	float PathLength = 0.f;
	FRandomStream Rand;
};
