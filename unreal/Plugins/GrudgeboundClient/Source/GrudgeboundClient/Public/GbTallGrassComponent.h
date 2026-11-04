// Tall grass where curses lurk. While the player's pawn walks inside this box, every metre walked rolls
// EncounterChancePerMetre (Project Settings > Grudgebound); a hit asks the server for a battle (POST /battle/start).
// Same rule as the web client (client/game/scenes/WardScene.ts): 12% per tile, 3 safe tiles after a battle.
//
// Use: add to any actor (or use AGbTallGrass), size the box over the grass.
#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GbTallGrassComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGbEncounterRolledEvent);

UCLASS(ClassGroup = (Grudgebound), meta = (BlueprintSpawnableComponent))
class GRUDGEBOUNDCLIENT_API UGbTallGrassComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UGbTallGrassComponent();

	/** Ward whose Grudge picks the curse. Empty = Project Settings > Grudgebound > Ward Id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grudgebound")
	FString WardId;

	/** Fired when a curse appears here, just before the server is asked for the battle. */
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound")
	FGbEncounterRolledEvent OnEncounterRolled;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** The local player's pawn if it stands in this grass, else null. */
	APawn* PawnInside() const;

	float MetresWalked = 0.f;
	FVector LastPawnLocation = FVector::ZeroVector;
	bool bHadPawn = false;
};

/** A ready-made tall-grass patch: the trigger box plus simple blade meshes for the greybox. */
UCLASS(ClassGroup = (Grudgebound))
class GRUDGEBOUNDCLIENT_API AGbTallGrass : public AActor
{
	GENERATED_BODY()

public:
	AGbTallGrass();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grudgebound")
	TObjectPtr<UGbTallGrassComponent> Grass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grudgebound")
	TObjectPtr<class UInstancedStaticMeshComponent> Blades;

	/** Patch size in metres (1 m = one web-client tile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grudgebound", meta = (ClampMin = "1", ClampMax = "50"))
	FIntPoint SizeMetres = FIntPoint(6, 4);

	/** Blades per square metre (visual only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grudgebound", meta = (ClampMin = "0", ClampMax = "12"))
	int32 BladesPerMetre = 5;
};
