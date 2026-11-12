// Guidance: where the tracked Journal task's next step is, a gold beacon over it, and the HUD line under the objective.
// The server names the step ("find:<id>", "npc:<role>", "rival", "ward:<id>"); the client finds it in the level and,
// when it lies across the water or behind a door, points at the ferry or the door first.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GbTypes.h"
#include "GbGuide.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;

namespace GbGuide
{
	enum class ERegion : uint8 { Lantern, Tide, Room };
	/** Lantern Ward sits at the origin, Tide Ward far to the west (-X), building interiors far to the east (+X). */
	GRUDGEBOUNDCLIENT_API ERegion RegionOf(const FVector& At);
	/** The tracked, unfinished Journal task with somewhere to go, or null. */
	GRUDGEBOUNDCLIENT_API const FGbJournalEntry* Tracked(const FGbSatchel& Satchel);
	/** Where to point for a server target, seen from From. Via is "", "ferry" or "exit" when the beacon shows the way there first. */
	GRUDGEBOUNDCLIENT_API bool Resolve(UWorld* World, const FString& Target, const FVector& From, FVector& Out, FString& Via);
	/** One line for the HUD under the tracked objective ("Beacon 42 m away", "Take the ferry to Tide Ward."), or "". */
	GRUDGEBOUNDCLIENT_API FString HudLine(UWorld* World, const FGbSatchel& Satchel, const FVector& From);
}

/** A gold arrow bobbing over the next step, with a ring on the ground and a soft light. */
UCLASS(NotPlaceable, Transient)
class GRUDGEBOUNDCLIENT_API AGbWaypoint : public AActor
{
	GENERATED_BODY()

public:
	AGbWaypoint();
	virtual void Tick(float DeltaSeconds) override;
	/** Shows the beacon over a point on the ground, or hides it. */
	void Point(bool bShow, const FVector& Ground = FVector::ZeroVector);
	bool IsShowing() const { return bShowing; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Arrow;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ring;
	UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
	float Clock = 0.f;
	bool bShowing = false;
};
