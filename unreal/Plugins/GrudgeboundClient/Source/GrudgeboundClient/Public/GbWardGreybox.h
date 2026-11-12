// Lantern Ward: the web client's map (client/game/tiles.ts, buildLanternWard) as a walled harbour district,
// 1 tile = 1 metre. Drop one into a level at the origin and press Play.
//
//   - town wall with towers (north, west, east), the Bone and Ash gates, harbour with a quay and a pier (south)
//   - paved main street and cross street, plaza with the Rite board, fountain, banners and planters
//   - market stalls, four houses you can enter (doors are placed by unreal/tools/make-lantern-ward.py),
//     four fenced tall-grass lots where curses lurk, woods outside the wall and ships at anchor
//
// Screen "up" is +X at the default camera heading, screen "right" is +Y.
// The layout logic (ground, blockers, triggers) is basic shapes; everything you see on top of it is the three
// Kenney CC0 kits in /Game/Grudgebound/Env (docs/ASSETS.md), placed at one uniform scale per piece.
// Without the kits the blockers are drawn instead, so the project still opens and plays.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GbWardGreybox.generated.h"

class UInstancedStaticMeshComponent;
class UGbTallGrassComponent;
class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;
class UBoxComponent;
struct FRandomStream;

UCLASS(ClassGroup = (Grudgebound))
class GRUDGEBOUNDCLIENT_API AGbWardGreybox : public AActor
{
	GENERATED_BODY()

public:
	AGbWardGreybox();

	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Grudgebound|Environment")
	void RebuildWard() { OnConstruction(GetActorTransform()); }

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

	/** Dress the Ward with the CC0 kits. Off (or kits not imported): plain blocks only. */
	UPROPERTY(EditAnywhere, Category = "Grudgebound|Environment")
	bool bUseKit = true;

	/** World position of the player start (feet). */
	UFUNCTION(BlueprintPure, Category = "Grudgebound")
	FVector GetPlayerStartLocation() const;
	/** The same world-space routes used to spawn the cosmetic walkers. */
	TArray<TArray<FVector>> GetTownsfolkPaths() const;

	static constexpr int32 Cols = 44;
	static constexpr int32 Rows = 30;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(Transient) TArray<TObjectPtr<class AGbTownsfolk>> Townsfolk;

	// Shared with the other Wards built the same way (GbTideWard).
	/** A house built from wall and roof pieces. Two cells deep (north-south), NX cells wide; a cell is S metres. */
	struct FHouse
	{
		float Col, Row;
		int32 NX;
		float S;
		int32 Floors;
		/** 0 = door on the north face, 2 = on the south face. */
		int32 DoorSide;
		int32 DoorCell;
		bool bHighRoof;
		/** False for the houses outside the wall (backdrop only). */
		bool bSolid;
	};

	/** Everything visible on top of the layout: kit pieces, lamps, scatter. */
	void Dress();
	/** Walks for the townsfolk, as tile-space points (column, row); one walker per walk. */
	virtual void TownsfolkWalks(TArray<TArray<FVector2D>>& Out) const;
	/** Throws away the last dressing and looks for the kits again. */
	void ResetDressing();
	void BuildHouse(const FHouse& House);
	void BuildWalls();
	void BuildHarbour(FRandomStream& Rand);
	void BuildPlazaAndStreets(FRandomStream& Rand);
	void BuildGreenery(FRandomStream& Rand);

	/** The instanced component of one kit mesh ("FantasyTown/wall"); null when that asset is not imported. */
	UInstancedStaticMeshComponent* Kit(const TCHAR* Mesh, bool bShadow);
	/** Places a kit mesh at a tile-space point, turned Yaw degrees, at one uniform scale. */
	void Put(const TCHAR* Mesh, float Col, float Row, float ZCm, float Yaw, float Scale, bool bShadow = true);
	/** Places a wall-type piece (it sits on the +X edge of its own tile) on one side of a tile-space edge.
	    Side: 0 north, 1 east, 2 south, 3 west; (Col, Row) is the middle of the edge; Size is the piece length in tiles. */
	void Edge(const TCHAR* Mesh, float Col, float Row, int32 Side, float Size, float ZCm = 0.f);
	/** Invisible wall: blocks walking and cuts the nav mesh. */
	void Solid(float Col, float Row, float W, float H, float HeightCm = 200.f);
	void Lamp(float Col, float Row, float ZCm, const FLinearColor& Color, float Intensity, float Radius);
	/** True when nothing built so far covers this point (streets, plaza, houses, lots). */
	bool IsFree(float Col, float Row, float Margin = 0.f) const;

	UFUNCTION() void GateOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
	UPROPERTY() TArray<TObjectPtr<UActorComponent>> Dressing;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> KitMeshes;
	UPROPERTY() TArray<TObjectPtr<UBoxComponent>> Gates;
	UPROPERTY() TObjectPtr<UBoxComponent> DocksBarrier;
	TArray<FBox2D> Occupied;
	bool bKitFound = false;

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
	/** 1 around sunset and sunrise: the warm grade, and the hour the lanterns come on. */
	float DuskBlend = 0.f;
	/** How strongly this Ward's lanterns burn now (1 by day, more after dark). */
	float LampGlow = 1.f;
	/** Each lantern's own strength (X) and reach (Y), read once at BeginPlay. */
	TMap<TWeakObjectPtr<class UPointLightComponent>, FVector2D> LampBase;

	/** The land: walkable, and what a click-to-walk lands on. */
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Ground;
	/** Mortar under the flagstones inside the wall. */
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Yard;
	/** Flagstones in two tones each: the yards, and the lighter streets and plaza. */
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> FlagA;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> FlagB;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> StreetA;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> StreetB;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Mosaic;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Rug;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Water;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Quay;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Board;
	/** Hidden when the kits are in: houses, walls, stalls, trunks. */
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> Blockers;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> GrassBlades;
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TObjectPtr<UInstancedStaticMeshComponent> GrassFloor;

	/** The four tall-grass patches (same places as the web map). */
	UPROPERTY(VisibleAnywhere, Category = "Grudgebound|Parts") TArray<TObjectPtr<UGbTallGrassComponent>> TallGrass;
};
