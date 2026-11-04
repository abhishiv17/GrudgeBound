// Lantern Ward greybox (BUILD_PLAN P1-U2): the web client's map (client/game/tiles.ts, buildLanternWard)
// rebuilt from blocks, 1 tile = 1 metre. Drop one into a level at the origin and press Play.
//
//   - town wall (north, west, east), harbour with a quay and a pier (south)
//   - main street and cross street, plaza with the Rite board, fountain, plants, banner posts
//   - market stalls with lanterns, four houses (greybox only), four tall-grass patches where curses lurk
//
// Screen "up" is +X (the Top Down template camera looks along +X), screen "right" is +Y.
// Everything is instanced static meshes, so the nav mesh is built around it like any level geometry.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GbWardGreybox.generated.h"

class UInstancedStaticMeshComponent;
class UGbTallGrassComponent;
class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;

UCLASS(ClassGroup = (Grudgebound))
class GRUDGEBOUNDCLIENT_API AGbWardGreybox : public AActor
{
	GENERATED_BODY()

public:
	AGbWardGreybox();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Ward whose Grudge picks the curses in this map's tall grass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grudgebound")
	FString WardId = TEXT("lantern");

	/** On Play, put the player on the cross street in front of the Rite board (where the web client starts). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grudgebound")
	bool bMovePlayerToStart = true;

	/** On Play, move the level's sun with the server's clock (hour of day, night) and grade the picture blue at night. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grudgebound")
	bool bFollowServerClock = true;

	/** Grass blades per square metre (visual only; lower it if the editor is slow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grudgebound", meta = (ClampMin = "0", ClampMax = "12"))
	int32 GrassBladesPerMetre = 4;

	/** World position of the player start (feet). */
	UFUNCTION(BlueprintPure, Category = "Grudgebound")
	FVector GetPlayerStartLocation() const;

	static constexpr int32 Cols = 44;
	static constexpr int32 Rows = 30;

protected:
	virtual void BeginPlay() override;

private:
	/** Adds a block covering tiles [Col, Col+W) x [Row, Row+H), from BottomCm up HeightCm. */
	void Block(UInstancedStaticMeshComponent* Mesh, float Col, float Row, float W, float H, float BottomCm, float HeightCm);
	/** Local position of a tile-space point (column, row) at height Z. */
	static FVector TileToLocal(float Col, float Row, float Z = 0.f);
	void MovePlayerToStart();
	/** Day/night: eases the sun, sky light and night grade toward the server's hour (runs on a 0.1 s timer). */
	void UpdateLighting();

	UPROPERTY(Transient) TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY(Transient) TObjectPtr<ASkyLight> SkyLight;
	UPROPERTY(Transient) TObjectPtr<APostProcessVolume> NightGrade;
	FTimerHandle LightingTimer;
	FRotator SunBaseRotation = FRotator::ZeroRotator;
	float SunBaseIntensity = 0.f;
	float SkyBaseIntensity = 0.f;
	float NightBlend = 0.f;
	float SunPitch = -45.f;

	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Ground;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Streets;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Plaza;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Mosaic;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Walls;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Banners;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Water;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Quay;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Wood;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Awnings;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Lights;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Plants;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Fountain;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Board;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Houses;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Roofs;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> GrassBlades;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> GrassFloor;

	/** The four tall-grass patches (same places as the web map). */
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TArray<TObjectPtr<UGbTallGrassComponent>> TallGrass;
};
