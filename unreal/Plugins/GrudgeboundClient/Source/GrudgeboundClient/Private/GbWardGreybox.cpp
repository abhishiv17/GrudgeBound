#include "GbWardGreybox.h"

#include "GbApiSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbTallGrassComponent.h"
#include "GbTownsfolk.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Math/RandomStream.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace
{
	constexpr float Tile = 100.f; // 1 web tile = 1 metre
	// OWNER TEXT: owner may replace these three locked-gate lines.
	const TCHAR* GateLines[] = { TEXT("Not that gate. Not tonight."), TEXT("Ash Ward is closed to debtors."), TEXT("The docks gate opens for the Tide Ward's business, not yours.") };

	/** Tall-grass patches from buildLanternWard: column, row, width, height in tiles. */
	const FIntRect GrassPatches[] = {
		FIntRect(3, 3, 3 + 9, 3 + 6),
		FIntRect(31, 3, 31 + 9, 3 + 6),
		FIntRect(4, 18, 4 + 10, 18 + 4),
		FIntRect(30, 18, 30 + 10, 18 + 4),
	};

	/** Market stalls: first column of each two-tile stall, on both sides of the main street. */
	const int32 StallCols[] = { 3, 7, 11, 31, 35, 39 };

	UInstancedStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, GbGreybox::EShape Shape, bool bSolid)
	{
		UInstancedStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		if (Parent) Mesh->SetupAttachment(Parent);
		GbGreybox::SetupMesh(Mesh, Shape);
		Mesh->SetMobility(EComponentMobility::Static);
		if (bSolid)
		{
			Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		else
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCanEverAffectNavigation(false);
		}
		return Mesh;
	}

	FBox2D TileRect(float Col, float Row, float W, float H)
	{
		return FBox2D(FVector2D(Col, Row), FVector2D(Col + W, Row + H));
	}
}

AGbWardGreybox::AGbWardGreybox()
{
	PrimaryActorTick.bCanEverTick = false;

	using GbGreybox::EShape;
	Ground = MakePart(this, nullptr, TEXT("Ground"), EShape::Cube, true);
	RootComponent = Ground;

	Yard = MakePart(this, Ground, TEXT("Yard"), EShape::Cube, false);
	FlagA = MakePart(this, Ground, TEXT("FlagA"), EShape::Cube, false);
	FlagB = MakePart(this, Ground, TEXT("FlagB"), EShape::Cube, false);
	StreetA = MakePart(this, Ground, TEXT("StreetA"), EShape::Cube, false);
	StreetB = MakePart(this, Ground, TEXT("StreetB"), EShape::Cube, false);
	Mosaic = MakePart(this, Ground, TEXT("Mosaic"), EShape::Cube, false);
	Rug = MakePart(this, Ground, TEXT("Rug"), EShape::Cube, false);
	Water = MakePart(this, Ground, TEXT("Water"), EShape::Cube, false);
	Quay = MakePart(this, Ground, TEXT("Quay"), EShape::Cube, true);
	Board = MakePart(this, Ground, TEXT("Board"), EShape::Cube, true);
	Blockers = MakePart(this, Ground, TEXT("Blockers"), EShape::Cube, true);
	GrassBlades = MakePart(this, Ground, TEXT("GrassBlades"), EShape::Cone, false);
	GrassFloor = MakePart(this, Ground, TEXT("GrassFloor"), EShape::Cube, false);
	for (UInstancedStaticMeshComponent* Flat : { Yard.Get(), FlagA.Get(), FlagB.Get(), StreetA.Get(), StreetB.Get(), Mosaic.Get(), Rug.Get(), GrassBlades.Get(), GrassFloor.Get(), Water.Get() })
	{
		Flat->SetCastShadow(false);
	}

	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(GrassPatches)); ++i)
	{
		UGbTallGrassComponent* Patch = CreateDefaultSubobject<UGbTallGrassComponent>(*FString::Printf(TEXT("TallGrass%d"), i));
		Patch->SetupAttachment(Ground);
		TallGrass.Add(Patch);
	}
	const TCHAR* Names[] = { TEXT("Bone"), TEXT("Ash"), TEXT("Tide") };
	for (const TCHAR* Name : Names)
	{
		UBoxComponent* Gate = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("%sGate"), Name));
		Gate->SetupAttachment(Ground); Gate->SetCollisionProfileName(TEXT("Trigger"));
		Gate->SetCanEverAffectNavigation(false); Gate->OnComponentBeginOverlap.AddDynamic(this, &AGbWardGreybox::GateOverlap);
		Gates.Add(Gate);
	}
	DocksBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("DocksBarrier"));
	DocksBarrier->SetupAttachment(Ground); DocksBarrier->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	DocksBarrier->SetBoxExtent(FVector(20,150,110)); DocksBarrier->SetRelativeLocation(TileToLocal(21.5f,28.7f,110));
}

FVector AGbWardGreybox::TileToLocal(float Col, float Row, float Z)
{
	// Row 0 is the top of the web map (north, the town wall) -> +X; column 0 is the left edge -> -Y.
	return FVector((Rows * 0.5f - Row) * Tile, (Col - Cols * 0.5f) * Tile, Z);
}

void AGbWardGreybox::Block(UInstancedStaticMeshComponent* Mesh, float Col, float Row, float W, float H, float BottomCm, float HeightCm)
{
	const FVector Centre = TileToLocal(Col + W * 0.5f, Row + H * 0.5f, BottomCm + HeightCm * 0.5f);
	// Basic shapes are 100 cm across with the pivot in the middle: scale = size in metres.
	const FVector Scale(H * Tile / 100.f, W * Tile / 100.f, HeightCm / 100.f);
	Mesh->AddInstance(FTransform(FRotator::ZeroRotator, Centre, Scale));
}

FVector AGbWardGreybox::GetPlayerStartLocation() const
{
	return GetActorTransform().TransformPosition(TileToLocal(21.5f, 13.5f, 0.f));
}

void AGbWardGreybox::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	for (UInstancedStaticMeshComponent* Mesh : { Ground.Get(), Yard.Get(), FlagA.Get(), FlagB.Get(), StreetA.Get(), StreetB.Get(), Mosaic.Get(),
		Rug.Get(), Water.Get(), Quay.Get(), Board.Get(), Blockers.Get(), GrassBlades.Get(), GrassFloor.Get() })
	{
		Mesh->ClearInstances();
	}
	Occupied.Reset();

	// --- land and sea: the town sits on a shore that runs far past the wall, so the camera never sees an edge
	Block(Ground, -90, -90, 224, 114, -40.f, 40.f);
	Block(Ground, 20, 24, 3, 5, -40.f, 40.f); // the pier's core
	Block(Water, -90, 24, 224, 110, -90.f, 20.f);
	Block(Yard, 0.5f, 0.5f, 43, 23.5f, 0.f, 1.f);
	// quay wall along the water, open where the pier starts
	Block(Quay, -1.8f, 24, 21.8f, 1, -70.f, 120.f);
	Block(Quay, 23, 24, 22.8f, 1, -70.f, 120.f);

	// --- flagstones: lighter on the two streets and the plaza, sandier in the yards; the tall-grass lots stay earth
	FRandomStream Tone(77);
	for (int32 Row = 1; Row < 24; ++Row)
	{
		for (int32 Col = 1; Col < Cols - 1; ++Col)
		{
			bool bLot = false;
			for (const FIntRect& R : GrassPatches) bLot |= Col >= R.Min.X && Col < R.Max.X && Row >= R.Min.Y && Row < R.Max.Y;
			if (bLot) continue;
			const bool bStreet = (Row >= 12 && Row <= 14) || (Col >= 20 && Col <= 22) || (Col >= 16 && Col < 27 && Row >= 9 && Row < 18);
			const bool bPick = Tone.FRand() < 0.5f;
			const float Inset = Tone.FRandRange(0.025f, 0.055f);
			Block(bStreet ? (bPick ? StreetA : StreetB) : (bPick ? FlagA : FlagB), Col + Inset, Row + Inset, 1.f - 2.f * Inset, 1.f - 2.f * Inset, 0.f, bStreet ? 3.5f : 2.5f);
		}
	}
	// roads that leave by the two gates
	for (int32 Step = 1; Step < 46; ++Step)
	{
		for (int32 Lane = 0; Lane < 3; ++Lane)
		{
			if (Tone.FRand() < 0.8f) Block(Tone.FRand() < 0.5f ? FlagA : FlagB, 20.04f + Lane, -1.f - Step, 0.92f, 0.92f, 0.f, 2.f);
			if (Tone.FRand() < 0.8f) Block(Tone.FRand() < 0.5f ? FlagA : FlagB, 45.f + Step, 12.04f + Lane, 0.92f, 0.92f, 0.f, 2.f);
		}
	}
	// plaza inlay under the player start, and the rug before the Rite board
	for (int32 Col = 19; Col < 24; ++Col)
	{
		for (int32 Row = 12; Row < 15; ++Row) if ((Col + Row) % 2 == 1) Block(Mosaic, Col + 0.22f, Row + 0.22f, 0.56f, 0.56f, 0.f, 4.5f);
	}
	Block(Rug, 20.1f, 10.75f, 2.8f, 1.1f, 0.f, 4.5f);
	Block(Board, 20.6f, 10.36f, 1.8f, 0.12f, 75.f, 135.f);

	// --- what you cannot walk through (drawn only when the kits are missing)
	Solid(0, 0, Cols, 1, 300.f);
	Solid(0, 0, 1, 24, 300.f);
	Solid(Cols - 1, 0, 1, 24, 300.f);
	Solid(14.8f, 16.2f, 2, 2, 70.f); // fountain, beside the room's approach
	Solid(20.6f, 10.3f, 1.8f, 0.25f, 210.f); // Rite board
	Solid(19.8f, 25, 0.2f, 4, 60.f); Solid(23.0f, 25, 0.2f, 4, 60.f); Solid(20, 28.8f, 3, 0.2f, 60.f); // pier rails
	for (const int32 X : StallCols)
	{
		Solid(X + 0.1f, 10.3f, 1.8f, 1.5f, 200.f);
		Solid(X + 0.1f, 15.2f, 1.8f, 1.5f, 200.f);
	}
	Occupied.Add(TileRect(1, 10, Cols - 2, 7)); // main street and both stall rows
	Occupied.Add(TileRect(19.5f, 0, 4, 30)); // cross street and the pier
	Occupied.Add(TileRect(16, 9, 11, 9)); // plaza
	for (const FIntRect& R : GrassPatches) Occupied.Add(TileRect(R.Min.X, R.Min.Y, R.Width(), R.Height()));

	// --- tall grass: trigger boxes + blades
	FRandomStream Rand(1234);
	for (int32 i = 0; i < TallGrass.Num() && i < static_cast<int32>(UE_ARRAY_COUNT(GrassPatches)); ++i)
	{
		const FIntRect& R = GrassPatches[i];
		UGbTallGrassComponent* Patch = TallGrass[i];
		Patch->WardId = WardId;
		Patch->SetRelativeLocation(TileToLocal(R.Min.X + R.Width() * 0.5f, R.Min.Y + R.Height() * 0.5f, 100.f));
		Patch->SetBoxExtent(FVector(R.Height() * Tile * 0.5f, R.Width() * Tile * 0.5f, 100.f));

		const int32 Count = R.Width() * R.Height() * GrassBladesPerMetre;
		for (int32 b = 0; b < Count; ++b)
		{
			const float Col = Rand.FRandRange(R.Min.X, R.Max.X);
			const float Row = Rand.FRandRange(R.Min.Y, R.Max.Y);
			const float Height = Rand.FRandRange(0.35f, 0.6f);
			const float Width = Rand.FRandRange(0.06f, 0.1f);
			const FRotator Lean(Rand.FRandRange(-12.f, 12.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(-12.f, 12.f));
			GrassBlades->AddInstance(FTransform(Lean, TileToLocal(Col, Row, Height * 50.f), FVector(Width, Width, Height)));
		}
		// A darker floor under the blades so the patch reads from the camera.
		Block(GrassFloor, R.Min.X, R.Min.Y, R.Width(), R.Height(), 0.f, 2.f);
	}

	Dress();

	// --- colours (paper-and-ink palette)
	using namespace GbGreybox;
	Paint(Ground, Color::Grass, this);
	Paint(Yard, Color::Mortar * 0.85f, this);
	Paint(FlagA, Color::StoneDark, this);
	Paint(FlagB, Color::StoneDark * 0.9f, this);
	Paint(StreetA, Color::Stone * 0.8f, this);
	Paint(StreetB, Color::Stone * 0.72f, this);
	Paint(Mosaic, Color::Gold, this);
	Paint(Rug, Color::Crimson, this);
	Paint(Water, Color::Water, this);
	Paint(Quay, Color::StoneDark * 0.8f, this);
	Paint(Board, Color::Paper, this);
	Paint(Blockers, Color::Paper, this);
	Paint(GrassFloor, Color::TallGrass * 0.8f, this);
	Paint(GrassBlades, Color::TallGrass, this);
	Blockers->SetVisibility(!bKitFound);
}

// ================================================================ dressing helpers

UInstancedStaticMeshComponent* AGbWardGreybox::Kit(const TCHAR* Mesh, bool bShadow)
{
	const FName Key(Mesh);
	if (const TObjectPtr<UInstancedStaticMeshComponent>* Found = KitMeshes.Find(Key)) return Found->Get();

	UInstancedStaticMeshComponent* Component = nullptr;
	const FString Package = FString::Printf(TEXT("/Game/Grudgebound/Env/%s"), Mesh);
	UStaticMesh* Asset = FPackageName::DoesPackageExist(Package)
		? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, *FPaths::GetBaseFilename(Package))) : nullptr;
	if (Asset)
	{
		Component = NewObject<UInstancedStaticMeshComponent>(this);
		Component->SetupAttachment(RootComponent);
		Component->SetStaticMesh(Asset);
		Component->SetMobility(EComponentMobility::Static);
		// The kit meshes are picture only: Solid() decides where you can walk.
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(bShadow);
		AddInstanceComponent(Component);
		Component->RegisterComponent();
		Dressing.Add(Component);
	}
	KitMeshes.Add(Key, Component);
	return Component;
}

void AGbWardGreybox::Put(const TCHAR* Mesh, float Col, float Row, float ZCm, float Yaw, float Scale, bool bShadow)
{
	if (!bKitFound) return;
	if (UInstancedStaticMeshComponent* Component = Kit(Mesh, bShadow))
	{
		Component->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), TileToLocal(Col, Row, ZCm), FVector(Scale)));
	}
}

void AGbWardGreybox::Edge(const TCHAR* Mesh, float Col, float Row, int32 Side, float Size, float ZCm)
{
	// A town-kit wall is a slab 40-50 cm along its own +X, with its outside face (shutters, door frame) towards -X.
	// So the piece is turned to look outwards and stands 0.4 of its length outside the edge: the face ends flush on it.
	const float Out = 0.4f * Size;
	switch (Side)
	{
	case 0: Put(Mesh, Col, Row - Out, ZCm, 180.f, Size); break;
	case 1: Put(Mesh, Col + Out, Row, ZCm, -90.f, Size); break;
	case 2: Put(Mesh, Col, Row + Out, ZCm, 0.f, Size); break;
	default: Put(Mesh, Col - Out, Row, ZCm, 90.f, Size); break;
	}
}

void AGbWardGreybox::Solid(float Col, float Row, float W, float H, float HeightCm)
{
	Block(Blockers, Col, Row, W, H, 0.f, HeightCm);
}

void AGbWardGreybox::Lamp(float Col, float Row, float ZCm, const FLinearColor& Color, float Intensity, float Radius)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(TileToLocal(Col, Row, ZCm));
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetLightColor(Color);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(false);
	AddInstanceComponent(Light);
	Light->RegisterComponent();
	Dressing.Add(Light);
}

bool AGbWardGreybox::IsFree(float Col, float Row, float Margin) const
{
	for (const FBox2D& Box : Occupied)
	{
		if (Col > Box.Min.X - Margin && Col < Box.Max.X + Margin && Row > Box.Min.Y - Margin && Row < Box.Max.Y + Margin) return false;
	}
	return true;
}

void AGbWardGreybox::BuildHouse(const FHouse& H)
{
	const float Width = H.NX * H.S, Depth = 2 * H.S;
	if (H.bSolid) Solid(H.Col, H.Row, Width, Depth, 300.f);
	Occupied.Add(TileRect(H.Col - 0.4f, H.Row - 0.4f, Width + 0.8f, Depth + 0.8f));

	const TCHAR* Stone[] = { TEXT("FantasyTown/wall_window_shutters"), TEXT("FantasyTown/wall"), TEXT("FantasyTown/wall_window_glass"), TEXT("FantasyTown/wall_detail_cross") };
	const TCHAR* Wood[] = { TEXT("FantasyTown/wall_wood_window_shutters"), TEXT("FantasyTown/wall_wood_detail_cross"), TEXT("FantasyTown/wall_wood_window_glass"), TEXT("FantasyTown/wall_wood_detail_diagonal") };
	const float Storey = H.S * 100.f;
	for (int32 Floor = 0; Floor < H.Floors; ++Floor)
	{
		const bool bTimber = Floor > 0 || H.Floors == 1;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const int32 Cells = Side % 2 == 0 ? H.NX : 2;
			for (int32 Cell = 0; Cell < Cells; ++Cell)
			{
				const float Along = (Cell + 0.5f) * H.S;
				const float Col = Side == 1 ? H.Col + Width : Side == 3 ? H.Col : H.Col + Along;
				const float Row = Side == 0 ? H.Row : Side == 2 ? H.Row + Depth : H.Row + Along;
				const TCHAR* Piece = (bTimber ? Wood : Stone)[(Cell + Side * 3 + Floor) % 4];
				if (Floor == 0 && Side == H.DoorSide && Cell == H.DoorCell) Piece = bTimber ? TEXT("FantasyTown/wall_wood_door") : TEXT("FantasyTown/wall_door");
				Edge(Piece, Col, Row, Side, H.S, Floor * Storey);
			}
		}
	}
	// Roof: two slopes meeting on an east-west ridge; the end pieces carry the barge boards.
	const FString Base = H.bHighRoof ? TEXT("FantasyTown/roof_high") : TEXT("FantasyTown/roof");
	for (int32 Cell = 0; Cell < H.NX; ++Cell)
	{
		for (int32 Half = 0; Half < 2; ++Half)
		{
			const bool bNorth = Half == 0;
			const bool bFirst = Cell == 0, bLast = Cell == H.NX - 1;
			const TCHAR* Suffix = (bNorth ? bLast : bFirst) ? TEXT("_left") : (bNorth ? bFirst : bLast) ? TEXT("_right") : Cell % 2 == 1 ? TEXT("_window") : TEXT("");
			Put(*(Base + Suffix), H.Col + (Cell + 0.5f) * H.S, H.Row + (Half + 0.5f) * H.S, H.Floors * Storey, bNorth ? 180.f : 0.f, H.S);
		}
	}
}

void AGbWardGreybox::BuildWalls()
{
	// Harbour-kit rampart, 2 m a piece and 4.4 m tall, with its inner face on the old wall line.
	const TCHAR* Wall = TEXT("Pirate/castle_wall");
	for (float Col = 0.5f; Col < 19.f; Col += 2.f) Put(Wall, Col, -0.4f, 0.f, 90.f, 1.f);
	for (float Col = 24.5f; Col < 44.f; Col += 2.f) Put(Wall, Col, -0.4f, 0.f, 90.f, 1.f);
	for (float Row = 1.6f; Row < 24.f; Row += 2.f) Put(Wall, -0.4f, Row, 0.f, 0.f, 1.f);
	for (float Row = 2.5f; Row < 11.f; Row += 2.f) Put(Wall, 44.4f, Row, 0.f, 0.f, 1.f);
	for (float Row = 16.5f; Row < 24.f; Row += 2.f) Put(Wall, 44.4f, Row, 0.f, 0.f, 1.f);
	Put(TEXT("Pirate/castle_gate"), 21.5f, -0.4f, 0.f, 90.f, 1.f); // Bone gate
	Put(TEXT("Pirate/castle_gate"), 44.4f, 13.5f, 0.f, 0.f, 1.f); // Ash gate
	Put(TEXT("Pirate/tower_complete_large"), -0.4f, -0.4f, 0.f, 0.f, 1.1f);
	Put(TEXT("Pirate/tower_complete_large"), 44.4f, -0.4f, 0.f, 0.f, 1.1f);
	Put(TEXT("Pirate/tower_complete_small"), -0.6f, 25.2f, -70.f, 90.f, 1.f);
	Put(TEXT("Pirate/tower_complete_small"), 44.6f, 25.2f, -70.f, 90.f, 1.f);

	// Both gates are shut: timber leaves fill the arches.
	for (const float Col : { 20.5f, 22.5f })
	{
		Edge(TEXT("FantasyTown/wall_wood_detail_cross"), Col, 0.7f, 2, 2.f);
		Edge(TEXT("FantasyTown/wall_wood"), Col, 0.7f, 2, 2.f, 200.f);
	}
	for (const float Row : { 12.5f, 14.5f })
	{
		Edge(TEXT("FantasyTown/wall_wood_detail_cross"), 43.3f, Row, 3, 2.f);
		Edge(TEXT("FantasyTown/wall_wood"), 43.3f, Row, 3, 2.f, 200.f);
	}
	// Crimson banners along the inside of the north wall.
	for (float Col = 3.5f; Col < 43.f; Col += 5.f)
	{
		if (Col > 18.f && Col < 25.f) continue;
		Edge(TEXT("FantasyTown/banner_red"), Col, 1.02f, 2, 2.6f, 130.f);
	}
}

void AGbWardGreybox::BuildHarbour(FRandomStream& Rand)
{
	// pier deck, rails and the Tide docks gate at its end
	for (const float Row : { 24.75f, 26.25f, 27.75f, 28.3f })
	{
		for (const float Col : { 20.75f, 22.25f }) Put(TEXT("FantasyTown/planks"), Col, Row, 1.f, 0.f, 1.5f);
	}
	for (const float Row : { 25.6f, 26.85f, 28.1f })
	{
		Edge(TEXT("FantasyTown/fence"), 20.f, Row, 3, 1.25f);
		Edge(TEXT("FantasyTown/fence"), 23.f, Row, 1, 1.25f);
	}
	for (const float Col : { 20.75f, 22.25f }) Edge(TEXT("FantasyTown/wall_wood_detail_cross"), Col, 28.95f, 2, 1.5f);
	for (const float Col : { 19.85f, 23.15f })
	{
		Put(TEXT("FantasyTown/pillar_wood"), Col, 29.f, -80.f, 0.f, 2.6f);
		Put(TEXT("FantasyTown/pillar_wood"), Col, 24.6f, -80.f, 0.f, 2.2f);
	}
	Put(TEXT("FantasyTown/lantern"), 19.4f, 23.5f, 0.f, 0.f, 1.9f);
	Put(TEXT("FantasyTown/lantern"), 23.6f, 23.5f, 0.f, 0.f, 1.9f);
	Lamp(19.4f, 23.5f, 270.f, GbGreybox::Color::Lantern, 2600.f, 520.f);
	Lamp(23.6f, 23.5f, 270.f, GbGreybox::Color::Lantern, 2600.f, 520.f);

	// boats at the pier, ships in the roads, a ghost of a ship further out
	Put(TEXT("Pirate/boat_row_large"), 18.2f, 26.6f, -78.f, 8.f, 0.9f);
	Put(TEXT("Pirate/boat_row_small"), 5.5f, 25.9f, -78.f, 80.f, 0.9f);
	Put(TEXT("Pirate/ship_large"), 33.5f, 28.6f, -95.f, 0.f, 1.f);
	Put(TEXT("Pirate/ship_small"), 8.5f, 30.5f, -95.f, 14.f, 1.f);
	Put(TEXT("Pirate/ship_medium"), -14.f, 44.f, -95.f, 62.f, 1.f);
	Put(TEXT("Pirate/ship_large"), 55.f, 47.f, -95.f, -28.f, 1.f);
	Put(TEXT("Pirate/ship_ghost"), 6.f, 62.f, -95.f, 84.f, 1.1f); // west of the ferry's course
	Put(TEXT("Pirate/rocks_a"), -5.f, 27.5f, -130.f, 40.f, 1.2f);
	Put(TEXT("Pirate/rocks_b"), 48.5f, 28.f, -150.f, 200.f, 1.2f);
	Put(TEXT("Pirate/rocks_c"), 13.f, 34.f, -110.f, 120.f, 1.f);
	Put(TEXT("Pirate/rocks_a"), 40.f, 38.f, -140.f, 300.f, 1.4f);

	// cargo along the quay, and a net shed in the south-east corner
	const FVector2D Stacks[] = { FVector2D(2.2f, 23.1f), FVector2D(11.f, 23.2f), FVector2D(32.f, 23.2f), FVector2D(37.5f, 23.1f), FVector2D(1.9f, 16.6f), FVector2D(42.f, 9.6f) };
	for (const FVector2D& At : Stacks)
	{
		Solid(At.X - 0.6f, At.Y - 0.45f, 2.1f, 0.9f, 120.f);
		Occupied.Add(TileRect(At.X - 0.8f, At.Y - 0.7f, 2.5f, 1.4f));
		Put(TEXT("Pirate/crate"), At.X, At.Y, 0.f, Rand.FRandRange(-12.f, 12.f), 0.75f);
		Put(TEXT("Pirate/crate_bottles"), At.X + 0.85f, At.Y + 0.05f, 0.f, 90.f + Rand.FRandRange(-10.f, 10.f), 0.62f);
		Put(TEXT("Pirate/crate"), At.X + 0.1f, At.Y - 0.05f, 58.f, Rand.FRandRange(20.f, 50.f), 0.55f);
		Put(TEXT("Pirate/barrel"), At.X - 0.5f, At.Y + 0.1f, 0.f, Rand.FRandRange(0.f, 360.f), 0.55f);
		Put(TEXT("Pirate/barrel"), At.X + 1.3f, At.Y - 0.15f, 0.f, Rand.FRandRange(0.f, 360.f), 0.5f);
	}
	Put(TEXT("Pirate/structure_roof"), 41.5f, 21.8f, 0.f, 0.f, 0.85f);
	Put(TEXT("Pirate/chest"), 41.5f, 21.6f, 0.f, 200.f, 0.55f);
	Put(TEXT("Pirate/tool_paddle"), 40.6f, 20.9f, 0.f, 30.f, 0.8f);
	Solid(40.5f, 20.8f, 2.f, 2.f, 220.f);
	Occupied.Add(TileRect(40.2f, 20.4f, 2.7f, 2.8f));
}

void AGbWardGreybox::BuildPlazaAndStreets(FRandomStream& Rand)
{
	using namespace GbGreybox;
	// --- market stalls with their goods; awnings face the street
	int32 Index = 0;
	for (const int32 X : StallCols)
	{
		for (const bool bNorth : { true, false })
		{
			const float Row = bNorth ? 11.f : 16.f;
			Put(Index % 2 == 0 ? TEXT("FantasyTown/stall_red") : TEXT("FantasyTown/stall_green"), X + 1.f, Row, 0.f, bNorth ? 0.f : 180.f, 2.f);
			const float Back = bNorth ? Row - 0.75f : Row + 0.75f;
			Put(Index % 3 == 0 ? TEXT("Pirate/crate_bottles") : TEXT("Pirate/crate"), X + 2.35f, Back, 0.f, Rand.FRandRange(-20.f, 20.f), 0.55f);
			if (Index % 2 == 1) Put(TEXT("Pirate/barrel"), X - 0.3f, Back, 0.f, 0.f, 0.5f);
			Put(TEXT("FantasyTown/stall_stool"), X + 0.4f + Rand.FRandRange(0.f, 1.f), bNorth ? Row - 1.05f : Row + 1.05f, 0.f, 0.f, 1.7f);
			++Index;
		}
	}
	// --- street lanterns: every one stands there, about every other one is a real light (19 in the Ward, budget 24)
	for (const int32 X : { 6, 10, 14, 30, 34, 38 })
	{
		const bool bLitNorth = X == 6 || X == 14 || X == 30 || X == 38;
		Put(TEXT("FantasyTown/lantern"), X + 0.1f, 11.55f, 0.f, 0.f, 1.9f);
		Put(TEXT("FantasyTown/lantern"), X + 0.1f, 15.45f, 0.f, 0.f, 1.9f);
		if (bLitNorth) Lamp(X + 0.1f, 11.7f, 270.f, Color::Lantern, 2600.f, 520.f);
		else Lamp(X + 0.1f, 15.3f, 270.f, Color::Lantern, 2600.f, 520.f);
	}
	const FVector2D CrossLamps[] = { FVector2D(19.55f, 4.f), FVector2D(23.45f, 7.f), FVector2D(19.55f, 20.5f), FVector2D(23.45f, 21.8f) };
	for (const FVector2D& At : CrossLamps)
	{
		Put(TEXT("FantasyTown/lantern"), At.X, At.Y, 0.f, 0.f, 1.9f);
		Lamp(At.X, At.Y, 270.f, Color::Lantern, 2600.f, 520.f);
	}

	// --- plaza: fountain with benches, the Rite board between its posts, banners, trees and planters in the corners
	Put(TEXT("FantasyTown/fountain_round_detail"), 15.8f, 17.2f, 0.f, 0.f, 1.05f);
	Put(TEXT("FantasyTown/stall_bench"), 14.4f, 17.2f, 0.f, 0.f, 1.8f);
	Put(TEXT("FantasyTown/stall_bench"), 15.8f, 18.65f, 0.f, 90.f, 1.8f);
	for (const float Col : { 20.5f, 22.5f }) Put(TEXT("FantasyTown/pillar_wood"), Col, 10.42f, 0.f, 0.f, 2.3f);
	for (const float Col : { 14.4f, 30.7f })
	{
		Put(TEXT("FantasyTown/pillar_wood"), Col - 0.75f, 9.5f, 0.f, 0.f, 3.4f);
		Put(TEXT("FantasyTown/pillar_wood"), Col + 0.75f, 9.5f, 0.f, 0.f, 3.4f);
		Edge(TEXT("FantasyTown/banner_red"), Col, 9.5f, 2, 2.8f, 60.f);
		Solid(Col - 0.9f, 9.4f, 1.8f, 0.2f, 300.f);
	}
	const FVector2D Corners[] = { FVector2D(12.5f, 9.f), FVector2D(32.5f, 9.f), FVector2D(13.2f, 18.f), FVector2D(32.5f, 18.f) };
	for (int32 I = 0; I < 4; ++I)
	{
		const FVector2D& At = Corners[I];
		Block(GrassFloor, At.X - 0.5f, At.Y - 0.5f, 1.f, 1.f, 0.f, 12.f);
		Put(I % 2 == 0 ? TEXT("FantasyTown/tree") : TEXT("FantasyTown/tree_crooked"), At.X, At.Y, 8.f, Rand.FRandRange(0.f, 360.f), 1.7f);
		Solid(At.X - 0.4f, At.Y - 0.4f, 0.8f, 0.8f, 200.f);
		const float Inward = At.X < 21.f ? 1.f : -1.f;
		Put(TEXT("FantasyTown/lantern"), At.X + Inward * 1.1f, At.Y, 0.f, 0.f, 1.9f);
		Lamp(At.X + Inward * 1.1f, At.Y, 270.f, Color::Lantern, 2800.f, 560.f);
	}
	const FVector2D Pots[] = { FVector2D(12.6f, 9.8f), FVector2D(32.f, 9.8f), FVector2D(14.f, 18.6f), FVector2D(32.f, 18.8f), FVector2D(24.6f, 17.5f) };
	for (const FVector2D& At : Pots)
	{
		Put(TEXT("Nature/pot_large"), At.X, At.Y, 14.f, Rand.FRandRange(0.f, 360.f), 20.f);
		Put(Rand.FRand() < 0.5f ? TEXT("Nature/plant_flatTall") : TEXT("Nature/plant_bushDetailed"), At.X, At.Y, 40.f, Rand.FRandRange(0.f, 360.f), 24.f);
		Solid(At.X - 0.35f, At.Y - 0.35f, 0.7f, 0.7f, 100.f);
	}

	// --- den row: red lanterns in front of the north-east hall (no new text)
	for (const float Col : { 25.6f, 28.9f, 30.6f })
	{
		Put(TEXT("FantasyTown/lantern"), Col, 7.55f, 0.f, 0.f, 1.5f);
		Lamp(Col, 7.6f, 215.f, Color::Crimson, 2200.f, 380.f);
	}
	// --- carts by the Ash gate and the south-west lot
	Put(TEXT("FantasyTown/cart_high"), 41.3f, 16.9f, 0.f, 100.f, 1.5f); Solid(40.4f, 16.2f, 1.9f, 1.4f, 130.f);
	Put(TEXT("FantasyTown/cart"), 11.8f, 18.5f, 0.f, 20.f, 1.5f); Solid(11.f, 17.7f, 1.5f, 1.8f, 100.f);
}

void AGbWardGreybox::BuildGreenery(FRandomStream& Rand)
{
	const TCHAR* Flowers[] = { TEXT("Nature/flower_redA"), TEXT("Nature/flower_redB"), TEXT("Nature/flower_yellowA"), TEXT("Nature/flower_yellowB"), TEXT("Nature/flower_purpleA"), TEXT("Nature/flower_purpleB") };
	const TCHAR* Tufts[] = { TEXT("Nature/grass_large"), TEXT("Nature/grass_leafs"), TEXT("Nature/grass_leafsLarge") };
	const TCHAR* Bushes[] = { TEXT("Nature/plant_bush"), TEXT("Nature/plant_bushLarge"), TEXT("Nature/plant_bushDetailed"), TEXT("Nature/plant_bushSmall") };
	const TCHAR* Stones[] = { TEXT("Nature/rock_smallA"), TEXT("Nature/rock_smallD"), TEXT("Nature/rock_smallE"), TEXT("Nature/stone_smallB"), TEXT("Nature/rock_smallFlatA") };
	const TCHAR* TownTrees[] = { TEXT("FantasyTown/tree"), TEXT("FantasyTown/tree_high"), TEXT("FantasyTown/tree_crooked"), TEXT("FantasyTown/tree_high_round"), TEXT("FantasyTown/tree_high_crooked") };
	const TCHAR* LeafTrees[] = { TEXT("Nature/tree_default"), TEXT("Nature/tree_oak"), TEXT("Nature/tree_detailed"), TEXT("Nature/tree_fat"), TEXT("Nature/tree_default_fall"), TEXT("Nature/tree_oak_fall"), TEXT("Nature/tree_thin") };
	const auto Any = [&Rand](const TCHAR* const* List, int32 Count) { return List[Rand.RandRange(0, Count - 1)]; };

	// --- tall-grass lots: fenced on three sides and open towards the street, wild inside
	for (const FIntRect& R : GrassPatches)
	{
		const bool bNorthLot = R.Min.Y < 12;
		const int32 Across = FMath::RoundToInt(R.Width() / 2.f), Down = FMath::RoundToInt(R.Height() / 2.f);
		const float SA = static_cast<float>(R.Width()) / Across, SD = static_cast<float>(R.Height()) / Down;
		for (int32 K = 0; K < Across; ++K)
		{
			Edge(K % 4 == 2 ? TEXT("FantasyTown/fence_broken") : TEXT("FantasyTown/fence"), R.Min.X + (K + 0.5f) * SA, bNorthLot ? R.Min.Y : R.Max.Y, bNorthLot ? 0 : 2, SA);
		}
		for (int32 K = 0; K < Down; ++K)
		{
			Edge(TEXT("FantasyTown/fence"), R.Min.X, R.Min.Y + (K + 0.5f) * SD, 3, SD);
			Edge(TEXT("FantasyTown/fence"), R.Max.X, R.Min.Y + (K + 0.5f) * SD, 1, SD);
		}
		const int32 Area = R.Width() * R.Height();
		for (int32 I = 0; I < Area; ++I)
		{
			Put(Any(Tufts, 3), Rand.FRandRange(R.Min.X + 0.3f, R.Max.X - 0.3f), Rand.FRandRange(R.Min.Y + 0.3f, R.Max.Y - 0.3f), 12.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(20.f, 30.f), false);
		}
		for (int32 I = 0; I < Area / 4; ++I)
		{
			Put(Any(Flowers, 6), Rand.FRandRange(R.Min.X + 0.4f, R.Max.X - 0.4f), Rand.FRandRange(R.Min.Y + 0.4f, R.Max.Y - 0.4f), 12.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(20.f, 26.f), false);
		}
		const float BackRow = bNorthLot ? R.Min.Y + 0.7f : R.Max.Y - 0.7f;
		Put(TEXT("Nature/rock_largeB"), R.Min.X + 1.2f, BackRow, 10.f, Rand.FRandRange(0.f, 360.f), 22.f);
		Put(TEXT("Nature/stump_old"), R.Max.X - 2.4f, BackRow + (bNorthLot ? 0.9f : -0.9f), 10.f, Rand.FRandRange(0.f, 360.f), 26.f);
		Put(TEXT("Nature/mushroom_redGroup"), R.Max.X - 2.9f, BackRow + (bNorthLot ? 1.4f : -1.4f), 10.f, 0.f, 24.f, false);
		Put(Any(LeafTrees, 7), R.Max.X - 0.9f, BackRow, 12.f, Rand.FRandRange(0.f, 360.f), 30.f);
		Solid(R.Max.X - 1.2f, BackRow - 0.3f, 0.6f, 0.6f, 200.f);
	}

	// --- yards inside the wall: trees in little planters, bushes against the houses, weeds between the flags
	int32 Trees = 0;
	for (int32 Try = 0; Try < 400 && Trees < 16; ++Try)
	{
		const float Col = Rand.FRandRange(1.9f, 42.1f), Row = Rand.FRandRange(1.9f, 22.6f);
		if (!IsFree(Col, Row, 0.9f)) continue;
		const bool bPine = Rand.FRand() < 0.45f;
		Block(GrassFloor, Col - 0.75f, Row - 0.75f, 1.5f, 1.5f, 0.f, 10.f);
		if (bPine) Put(Any(TownTrees, 5), Col, Row, 8.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(1.5f, 2.1f));
		else Put(Any(LeafTrees, 7), Col, Row, 14.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(27.f, 36.f));
		Solid(Col - 0.3f, Row - 0.3f, 0.6f, 0.6f, 200.f);
		Occupied.Add(TileRect(Col - 1.6f, Row - 1.6f, 3.2f, 3.2f));
		++Trees;
	}
	for (int32 I = 0; I < 260; ++I)
	{
		const float Col = Rand.FRandRange(1.3f, 42.7f), Row = Rand.FRandRange(1.3f, 23.6f);
		if (!IsFree(Col, Row, 0.2f)) continue;
		const float Roll = Rand.FRand();
		if (Roll < 0.16f) Put(Any(Bushes, 4), Col, Row, 12.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(22.f, 34.f));
		else if (Roll < 0.26f) Put(Any(Stones, 5), Col, Row, 10.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(16.f, 28.f));
		else if (Roll < 0.48f) Put(Any(Flowers, 6), Col, Row, 12.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(18.f, 24.f), false);
		else Put(Any(Tufts, 3), Col, Row, 12.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(14.f, 24.f), false);
	}

	// --- outside the wall: a few farmsteads, then woods and crags up to the fog
	const FHouse Farms[] = {
		{ 6.f, -13.f, 3, 2.5f, 2, 2, 1, true, false }, { 29.f, -11.f, 2, 2.5f, 1, 2, 0, false, false }, { 36.f, -18.f, 3, 2.5f, 2, 2, 1, true, false },
		{ 52.f, 3.f, 2, 2.5f, 2, 2, 0, true, false }, { 50.f, 17.f, 3, 2.5f, 1, 2, 1, false, false }, { -13.f, 5.f, 2, 2.5f, 1, 2, 0, false, false }, { -11.f, 15.f, 3, 2.5f, 2, 2, 1, true, false },
	};
	for (const FHouse& Farm : Farms) BuildHouse(Farm);
	Occupied.Add(TileRect(-4.f, -3.5f, 52.f, 29.f)); // the walled town itself
	Occupied.Add(TileRect(19.f, -60.f, 5.f, 60.f)); // north road
	Occupied.Add(TileRect(44.f, 11.f, 60.f, 5.f)); // east road
	for (int32 I = 0; I < 520; ++I)
	{
		const float Col = Rand.FRandRange(-55.f, 99.f), Row = Rand.FRandRange(-55.f, 23.f);
		if (!IsFree(Col, Row, 1.2f)) continue;
		const float Roll = Rand.FRand();
		if (Roll < 0.5f) Put(Any(TownTrees, 5), Col, Row, 0.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(2.2f, 3.4f));
		else if (Roll < 0.8f) Put(Any(LeafTrees, 7), Col, Row, 14.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(38.f, 56.f));
		else if (Roll < 0.9f) Put(Rand.FRand() < 0.5f ? TEXT("FantasyTown/rock_large") : TEXT("FantasyTown/rock_wide"), Col, Row, -10.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(1.6f, 3.6f));
		else Put(Any(Bushes, 4), Col, Row, 12.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(40.f, 70.f));
	}
}

void AGbWardGreybox::ResetDressing()
{
	// Older saved revisions did not serialize Dressing; recover only generated instance components.
	const TArray<UActorComponent*> SavedInstances = GetInstanceComponents();
	for (UActorComponent* C : SavedInstances)
	{
		if (C && (C->IsA<UInstancedStaticMeshComponent>() || C->IsA<UPointLightComponent>() || C->IsA<UTextRenderComponent>()))
		{ Dressing.Remove(C); C->DestroyComponent(); }
	}
	for (UActorComponent* C : Dressing) if (C) C->DestroyComponent();
	Dressing.Reset(); KitMeshes.Reset();
	bKitFound = bUseKit && FPackageName::DoesPackageExist(TEXT("/Game/Grudgebound/Env/FantasyTown/roof_high_left"));
}

void AGbWardGreybox::Dress()
{
	ResetDressing();

	// The four houses with doors (make-lantern-ward.py puts a door actor in front of each).
	const FHouse Houses[] = {
		{ 13.5f, 2.5f, 2, 2.5f, 2, 2, 1, true, true },  // Mender's House
		{ 23.5f, 2.f, 3, 2.5f, 2, 2, 1, true, true },   // Rite Hall
		{ 15.f, 19.f, 2, 2.f, 1, 0, 1, false, true },   // your room
		{ 24.f, 19.f, 3, 2.f, 1, 0, 1, false, true },   // Counting House
	};
	for (const FHouse& House : Houses) BuildHouse(House);
 Occupied.Add(TileRect(14.4f,7.2f,5.7f,5.8f)); // Mender: keep both sightline and approach open
 Occupied.Add(TileRect(24.4f,6.7f,5.7f,6.3f)); // Rite Hall
 Occupied.Add(TileRect(15.2f,15.f,5.6f,4.2f)); // room
 Occupied.Add(TileRect(24.2f,15.f,5.6f,4.2f)); // Counting House
	Edge(TEXT("FantasyTown/banner_red"), 23.6f, 6.5f, 2, 2.5f, 260.f);
	Edge(TEXT("FantasyTown/banner_red"), 31.f, 6.5f, 2, 2.5f, 260.f);
	Edge(TEXT("FantasyTown/overhang"), 27.25f, 7.f, 2, 2.5f);
	Edge(TEXT("FantasyTown/overhang"), 17.25f, 7.5f, 2, 2.5f);

	FRandomStream Rand(4242);
	BuildWalls();
	BuildHarbour(Rand);
	BuildPlazaAndStreets(Rand);
	BuildGreenery(Rand);

	const FVector GatePositions[] = { TileToLocal(21.5f,1.5f,100), TileToLocal(42.5f,13.5f,100), TileToLocal(21.5f,28.3f,100) };
	for (int I = 0; I < Gates.Num(); ++I) { Gates[I]->SetRelativeLocation(GatePositions[I]); Gates[I]->SetBoxExtent(FVector(90,90,120)); }
}

void AGbWardGreybox::GateOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsLocallyControlled() || !GetGameInstance()) return;
	// The docks gate (2) says nothing now: the ferry to the Tide Ward leaves from the pier in front of it.
	for (int I = 0; I < Gates.Num() && I < 2; ++I) if (Gates[I] == Component)
		if (UGbApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGbApiSubsystem>()) Api->SayAsKessa(GateLines[I]);
}

void AGbWardGreybox::TownsfolkWalks(TArray<TArray<FVector2D>>& Out) const
{
	// Main street both ways, the cross street past the Rite board, round the plaza, down to the pier, along the stalls.
	Out.Add({ FVector2D(3.f, 13.2f), FVector2D(18.f, 13.2f), FVector2D(26.f, 13.6f), FVector2D(41.f, 13.6f) });
	Out.Add({ FVector2D(40.f, 12.4f), FVector2D(24.f, 12.4f), FVector2D(21.f, 12.8f), FVector2D(4.f, 12.5f) });
	Out.Add({ FVector2D(22.75f, 2.5f), FVector2D(22.75f, 9.5f), FVector2D(23.4f, 11.9f), FVector2D(22.3f, 13.f), FVector2D(22.3f, 22.5f) });
	Out.Add({ FVector2D(17.5f, 12.f), FVector2D(24.5f, 12.f), FVector2D(24.5f, 15.f), FVector2D(20.f, 15.f) });
	Out.Add({ FVector2D(20.4f, 15.f), FVector2D(20.4f, 22.8f), FVector2D(21.5f, 24.5f), FVector2D(21.5f, 26.3f) });
	Out.Add({ FVector2D(5.f, 14.4f), FVector2D(13.f, 14.4f) });
	Out.Add({ FVector2D(30.f, 12.2f), FVector2D(40.f, 12.2f) });
	Out.Add({ FVector2D(20.5f, 3.f), FVector2D(20.5f, 9.3f), FVector2D(19.6f, 11.9f), FVector2D(8.f, 13.8f) });
}

TArray<TArray<FVector>> AGbWardGreybox::GetTownsfolkPaths() const
{
	TArray<TArray<FVector2D>> Walks; TownsfolkWalks(Walks);
	TArray<TArray<FVector>> Paths;
	for (const TArray<FVector2D>& Walk : Walks)
	{
		TArray<FVector>& Path = Paths.AddDefaulted_GetRef();
		for (const FVector2D& P : Walk) Path.Add(GetActorTransform().TransformPosition(TileToLocal(P.X, P.Y, 4.f)));
	}
	return Paths;
}

void AGbWardGreybox::EndPlay(const EEndPlayReason::Type Reason)
{
	for (AGbTownsfolk* Walker : Townsfolk) if (IsValid(Walker)) Walker->Destroy();
	Townsfolk.Reset();
	GetWorldTimerManager().ClearTimer(LightingTimer);
	Super::EndPlay(Reason);
}

void AGbWardGreybox::BeginPlay()
{
	Super::BeginPlay();
	// People on the streets: picture only, made new on every Play.
	const TArray<TArray<FVector>> Paths = GetTownsfolkPaths();
	for (int32 I = 0; I < Paths.Num(); ++I)
	{
		const TArray<FVector>& Path = Paths[I];
		if (Path.Num() < 2) continue;
		if (AGbTownsfolk* Walker = GetWorld()->SpawnActorDeferred<AGbTownsfolk>(AGbTownsfolk::StaticClass(), FTransform(Path[0]), this))
		{
			Walker->SetFlags(RF_Transient);
			Walker->Setup(Path, GetTypeHash(WardId) + I * 7919);
			Walker->SetVoice(WardId, I);
			Walker->FinishSpawning(Walker->GetActorTransform());
			Townsfolk.Add(Walker);
		}
	}
	if (bMovePlayerToStart)
	{
		// Next tick: the player's pawn may not be possessed yet when the level begins play.
		GetWorldTimerManager().SetTimerForNextTick(this, &AGbWardGreybox::MovePlayerToStart);
	}

	// Every Ward brightens its own lanterns after dark; only one Ward turns the level's sun.
	TArray<UPointLightComponent*> Lamps; GetComponents(Lamps);
	for (UPointLightComponent* L : Lamps) if (L) LampBase.Add(L, FVector2D(L->Intensity, L->AttenuationRadius));
	if (!bFollowServerClock) GetWorldTimerManager().SetTimer(LightingTimer, this, &AGbWardGreybox::UpdateLighting, 0.1f, true);
	if (bFollowServerClock)
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It) { Sun = *It; break; }
		for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It) { SkyLight = *It; break; }
		if (Sun && Sun->GetLightComponent())
		{
			// The Basic level's sun is Stationary; it has to be Movable to turn during play.
			Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
			SunBaseRotation = Sun->GetActorRotation();
			SunPitch = SunBaseRotation.Pitch;
			SunBaseIntensity = Sun->GetLightComponent()->Intensity;
		}
		if (SkyLight && SkyLight->GetLightComponent())
		{
			SkyBaseIntensity = SkyLight->GetLightComponent()->Intensity;
		}
		// Night grade: applied after auto exposure, so the night stays dark even when exposure adapts.
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		NightGrade = GetWorld()->SpawnActor<APostProcessVolume>(Params);
		if (NightGrade)
		{
			NightGrade->bUnbound = true;
			NightGrade->Priority = 10.f;
			NightGrade->Settings.bOverride_ColorGain = true;
			NightGrade->Settings.bOverride_AutoExposureBias = true;
		}
		GetWorldTimerManager().SetTimer(LightingTimer, this, &AGbWardGreybox::UpdateLighting, 0.1f, true);
	}
}

void AGbWardGreybox::UpdateLighting()
{
	const UGameInstance* GI = GetGameInstance();
	const UGbApiSubsystem* Api = GI ? GI->GetSubsystem<UGbApiSubsystem>() : nullptr;
	if (!Api) return;
	FGbWorldState State = Api->GetWorldState();
	// Development aid: -GbHour=<0-23> fixes the clock (stills and trailer shots need no server).
	static const int32 FixedHour = []{ int32 H = -1; FParse::Value(FCommandLine::Get(), TEXT("GbHour="), H); return H; }();
	if (FixedHour >= 0) { State.bValid = true; State.Hour = FixedHour % 24; State.bNight = State.Hour >= 20 || State.Hour < 6; }
	if (!State.bValid) return;

	constexpr float Step = 0.1f;
	// Dusk and dawn: the two hours either side of the night (18:00-19:59 and 06:00-06:59), half strength at 17:00.
	const float DuskTarget = (State.Hour == 18 || State.Hour == 19 || State.Hour == 6) ? 1.f : (State.Hour == 17 || State.Hour == 5) ? 0.5f : 0.f;
	DuskBlend = FMath::FInterpConstantTo(DuskBlend, DuskTarget, Step, 0.5f);
	// Lanterns: as placed by day, stronger from dusk, strongest at night.
	LampGlow = FMath::FInterpConstantTo(LampGlow, State.bNight ? 7.f : 1.f + 2.5f * DuskTarget, Step, 2.5f);
	// After dark a lantern also reaches further, so the streets between them stay readable.
	const float Reach = FMath::GetMappedRangeValueClamped(FVector2D(1.f, 7.f), FVector2D(1.f, 1.7f), LampGlow);
	for (const TPair<TWeakObjectPtr<UPointLightComponent>, FVector2D>& Lamp : LampBase)
	{
		if (UPointLightComponent* L = Lamp.Key.Get()) { L->SetIntensity(Lamp.Value.X * LampGlow); L->SetAttenuationRadius(Lamp.Value.Y * Reach); }
	}
	if (!bFollowServerClock) return;
	// Sun height above the horizon: low at 06:00 and 18:00, highest at noon. At night a fixed "moon".
	const float DayT = FMath::Clamp((State.Hour - 6.f) / 12.f, 0.f, 1.f);
	const float Elevation = State.bNight ? 40.f : 12.f + 53.f * FMath::Sin(PI * DayT);
	NightBlend = FMath::FInterpConstantTo(NightBlend, State.bNight ? 1.f : 0.f, Step, 0.5f); // 2 s fade
	SunPitch = FMath::FInterpTo(SunPitch, -Elevation, Step, 1.5f);

	if (Sun && Sun->GetLightComponent())
	{
		ULightComponent* L = Sun->GetLightComponent();
		Sun->SetActorRotation(FRotator(SunPitch, SunBaseRotation.Yaw, SunBaseRotation.Roll));
		// Warm near the horizon, white at noon, pale blue moonlight at night.
		const float High = FMath::Clamp((Elevation - 12.f) / 40.f, 0.f, 1.f);
		const FLinearColor DayColor = FMath::Lerp(FMath::Lerp(FLinearColor(1.f, 0.62f, 0.38f), FLinearColor(1.f, 0.97f, 0.92f), High), FLinearColor(1.f, 0.66f, 0.42f), DuskBlend * (1.f - NightBlend));
		L->SetLightColor(FMath::Lerp(DayColor, FLinearColor(0.55f, 0.66f, 1.f), NightBlend));
		L->SetIntensity(SunBaseIntensity * FMath::Lerp(1.f, 0.25f, NightBlend));
	}
	if (SkyLight && SkyLight->GetLightComponent())
	{
		SkyLight->GetLightComponent()->SetIntensity(SkyBaseIntensity * FMath::Lerp(1.f, 0.6f, NightBlend));
	}
	if (NightGrade)
	{
		// Dusk warms and deepens the picture; night cools it. The lanterns carry the warmth after dark.
		const FVector4 DayGain = FMath::Lerp(FVector4(1.f, 1.f, 1.f, 1.f), FVector4(1.03f, 0.94f, 0.84f, 0.98f), DuskBlend);
		NightGrade->Settings.ColorGain = FMath::Lerp(DayGain, FVector4(0.58f, 0.66f, 0.98f, 0.85f), NightBlend);
		NightGrade->Settings.AutoExposureBias = FMath::Lerp(-0.1f * DuskBlend, -0.55f, NightBlend);
	}
}

void AGbWardGreybox::MovePlayerToStart()
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn) return;
	// Keep the pawn's half-height above the ground so it doesn't start inside the floor.
	const float HalfHeight = Pawn->GetSimpleCollisionHalfHeight();
	Pawn->SetActorLocation(GetPlayerStartLocation() + FVector(0.f, 0.f, HalfHeight + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbWardLayoutTest, "Grudgebound.Ward.LayoutAndFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbWardLayoutTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(TEXT("GbWardLayoutFixture")), nullptr, true);
	// CreateWorld already initializes its level and WorldSettings; never initialize it twice.
	AGbWardGreybox* Ward = World->SpawnActor<AGbWardGreybox>();
	if (!TestNotNull(TEXT("Native Ward spawns"), Ward)) { World->DestroyWorld(false); return false; }
	TestEqual(TEXT("Tile/player mapping preserved"), Ward->GetPlayerStartLocation(), FVector(150,-50,0));
	TArray<UBoxComponent*> Boxes; Ward->GetComponents(Boxes);
	const UBoxComponent* Barrier = nullptr;
	for (const UBoxComponent* Box : Boxes) if (Box->GetFName() == TEXT("DocksBarrier")) Barrier = Box;
	TestTrue(TEXT("Closed docks gate blocks independently of decorative mesh collision"), Barrier && Barrier->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	TArray<UGbTallGrassComponent*> Grass; Ward->GetComponents(Grass);
	TestEqual(TEXT("Four original trigger patches"), Grass.Num(), 4);
	TArray<FVector> Centres; for (const UGbTallGrassComponent* G : Grass) Centres.Add(G->GetRelativeLocation());
	if (Grass.Num() == 4) TestTrue(TEXT("North-west grass bounds preserved"), Grass[0]->GetUnscaledBoxExtent().Equals(FVector(300,450,100)));
	Ward->bUseKit=false; Ward->RebuildWard();
	for (int I=0; I<Grass.Num(); ++I) TestEqual(TEXT("Fallback does not move encounter triggers"), Grass[I]->GetRelativeLocation(), Centres[I]);
	TArray<UPointLightComponent*> Lamps; Ward->GetComponents(Lamps);
	TestTrue(TEXT("Lanterns stay inside dynamic-light budget"), Lamps.Num() > 0 && Lamps.Num() <= 24);
	const int32 Count = Lamps.Num(); Ward->RebuildWard(); Lamps.Reset(); Ward->GetComponents(Lamps);
	TestEqual(TEXT("Reconstruction cannot duplicate lanterns"), Lamps.Num(), Count);
	World->DestroyWorld(false); return true;
}
#endif
