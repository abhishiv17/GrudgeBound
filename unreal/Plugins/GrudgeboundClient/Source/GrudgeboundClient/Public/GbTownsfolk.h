#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GbTownsfolk.generated.h"

class UGbExorcistLook;

/**
 * Someone walking a street: picture only. No dialogue, no trade, no collision; they stroll back and forth along a
 * path, stop a moment at each end and wait when the player stands in their way. The Wards spawn them on Play
 * (AGbWardGreybox::BeginPlay), so they are never saved into a level.
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

private:
	UPROPERTY() TObjectPtr<UGbExorcistLook> Look;
	TArray<FVector> Path;
	float Travelled = 0.f;
	float Speed = 120.f;
	float Pause = 0.f;
	float PathLength = 0.f;
	FRandomStream Rand;
};
