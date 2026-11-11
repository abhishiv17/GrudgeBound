#include "GbTideWard.h"

#include "GbGreyboxMaterials.h"
#include "GbTallGrassComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Math/RandomStream.h"

namespace
{
	constexpr float TideTile = 100.f;

	// The wharf, in tiles: columns 4-40, rows 7-23, with a pier north (rows 1-7) and south (rows 23-28) on columns 20-23.
	constexpr float TideWest = 4.f, TideEast = 40.f, TideNorth = 7.f, TideSouth = 23.f;

	/** Reed beds: column, row, width, height. */
	const FIntRect TideReeds[] = {
		FIntRect(5, 17, 5 + 10, 17 + 5),
		FIntRect(29, 17, 29 + 10, 17 + 5),
		FIntRect(5, 8, 5 + 3, 8 + 4),
	};

	FBox2D TideRect(float Col, float Row, float W, float H) { return FBox2D(FVector2D(Col, Row), FVector2D(Col + W, Row + H)); }
	bool TideInReeds(float Col, float Row)
	{
		for (const FIntRect& R : TideReeds) if (Col >= R.Min.X && Col < R.Max.X && Row >= R.Min.Y && Row < R.Max.Y) return true;
		return false;
	}
}

AGbTideWard::AGbTideWard()
{
	WardId = TEXT("tide");
	bMovePlayerToStart = false; // the player starts in Lantern Ward and arrives here by ferry
	bFollowServerClock = false; // Lantern Ward already turns the level's one sun
	// No locked gates here.
	for (UBoxComponent* Gate : Gates) Gate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DocksBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector AGbTideWard::GetLandingLocation() const { return GetActorTransform().TransformPosition(TileToLocal(21.5f, 4.6f, 0.f)); }
FVector AGbTideWard::GetFerryLocation() const { return GetActorTransform().TransformPosition(TileToLocal(21.5f, 2.f, 0.f)); }

void AGbTideWard::OnConstruction(const FTransform& Transform)
{
	AActor::OnConstruction(Transform); // not Lantern Ward's layout

	for (UInstancedStaticMeshComponent* Mesh : { Ground.Get(), Yard.Get(), FlagA.Get(), FlagB.Get(), StreetA.Get(), StreetB.Get(), Mosaic.Get(),
		Rug.Get(), Water.Get(), Quay.Get(), Board.Get(), Blockers.Get(), GrassBlades.Get(), GrassFloor.Get() })
	{
		Mesh->ClearInstances();
	}
	Occupied.Reset();

	// --- sea all round (a hair under Lantern Ward's sea where the two meet), the wharf and its two piers
	Block(Water, -90, -34, 224, 160, -91.f, 20.f);
	Block(Ground, TideWest, TideNorth, TideEast - TideWest, TideSouth - TideNorth, -40.f, 40.f);
	Block(Ground, 20, 1, 3, 6, -40.f, 40.f);
	Block(Ground, 20, 23, 3, 5, -40.f, 40.f);
	// stone footing under the timber, showing as a rim at the waterline
	Block(Quay, TideWest - 0.35f, TideNorth - 0.35f, TideEast - TideWest + 0.7f, TideSouth - TideNorth + 0.7f, -90.f, 84.f);

	// --- decking: two boards a tile, running east-west; flagstones on the quay street and the lane between the piers
	FRandomStream Tone(311);
	const auto Boards = [&](int32 Col, int32 Row)
	{
		for (int32 Half = 0; Half < 2; ++Half)
		{
			const float Gap = Tone.FRandRange(0.02f, 0.04f);
			Block(Tone.FRand() < 0.5f ? FlagA : FlagB, Col + Gap * 0.5f, Row + Half * 0.5f + Gap, 1.f - Gap, 0.5f - 2.f * Gap, 0.f, Tone.FRandRange(2.f, 3.2f));
		}
	};
	for (int32 Row = static_cast<int32>(TideNorth); Row < static_cast<int32>(TideSouth); ++Row)
	{
		for (int32 Col = static_cast<int32>(TideWest); Col < static_cast<int32>(TideEast); ++Col)
		{
			if (TideInReeds(Col, Row)) continue;
			const bool bStreet = (Row >= 13 && Row <= 15) || (Col >= 20 && Col <= 22);
			if (!bStreet) { Boards(Col, Row); continue; }
			const float Inset = Tone.FRandRange(0.025f, 0.055f);
			Block(Tone.FRand() < 0.5f ? StreetA : StreetB, Col + Inset, Row + Inset, 1.f - 2.f * Inset, 1.f - 2.f * Inset, 0.f, 3.5f);
		}
	}
	for (int32 Col = 20; Col < 23; ++Col)
	{
		for (int32 Row = 1; Row < 7; ++Row) Boards(Col, Row);
		for (int32 Row = 23; Row < 28; ++Row) Boards(Col, Row);
	}

	// --- where you cannot walk: the wharf's edge (open at the piers), the pier rails
	Solid(TideWest, TideNorth - 0.2f, 16, 0.2f, 120.f); Solid(23, TideNorth - 0.2f, TideEast - 23, 0.2f, 120.f);
	Solid(TideWest, TideSouth, 16, 0.2f, 120.f); Solid(23, TideSouth, TideEast - 23, 0.2f, 120.f);
	Solid(TideWest - 0.2f, TideNorth, 0.2f, TideSouth - TideNorth, 120.f); Solid(TideEast, TideNorth, 0.2f, TideSouth - TideNorth, 120.f);
	Solid(19.8f, 1, 0.2f, 6, 120.f); Solid(23, 1, 0.2f, 6, 120.f); Solid(20, 0.8f, 3, 0.2f, 120.f);
	Solid(19.8f, 23, 0.2f, 5, 120.f); Solid(23, 23, 0.2f, 5, 120.f); Solid(20, 28, 3, 0.2f, 120.f);
	Occupied.Add(TideRect(TideWest, 12.5f, TideEast - TideWest, 3.f)); // quay street
	Occupied.Add(TideRect(19.5f, 0, 4, 30)); // the lane and both piers
	for (const FIntRect& R : TideReeds) Occupied.Add(TideRect(R.Min.X, R.Min.Y, R.Width(), R.Height()));

	// --- reed beds: trigger boxes, wet sand and reeds. The fourth patch of the base class is not used here.
	FRandomStream Rand(2718);
	for (int32 i = 0; i < TallGrass.Num(); ++i)
	{
		UGbTallGrassComponent* Patch = TallGrass[i];
		Patch->WardId = WardId;
		if (i >= static_cast<int32>(UE_ARRAY_COUNT(TideReeds)))
		{
			Patch->SetRelativeLocation(FVector(0, 0, -2000.f)); Patch->SetBoxExtent(FVector(1.f)); Patch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			continue;
		}
		const FIntRect& R = TideReeds[i];
		Patch->SetRelativeLocation(TileToLocal(R.Min.X + R.Width() * 0.5f, R.Min.Y + R.Height() * 0.5f, 100.f));
		Patch->SetBoxExtent(FVector(R.Height() * TideTile * 0.5f, R.Width() * TideTile * 0.5f, 100.f));
		const int32 Count = R.Width() * R.Height() * GrassBladesPerMetre;
		for (int32 b = 0; b < Count; ++b)
		{
			const float Height = Rand.FRandRange(0.55f, 1.05f), Width = Rand.FRandRange(0.05f, 0.08f);
			const FRotator Lean(Rand.FRandRange(-9.f, 9.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(-9.f, 9.f));
			GrassBlades->AddInstance(FTransform(Lean, TileToLocal(Rand.FRandRange(R.Min.X, R.Max.X), Rand.FRandRange(R.Min.Y, R.Max.Y), Height * 50.f), FVector(Width, Width, Height)));
		}
		Block(GrassFloor, R.Min.X, R.Min.Y, R.Width(), R.Height(), 0.f, 2.f);
	}

	ResetDressing();
	BuildDocks(Rand);

	using namespace GbGreybox;
	Paint(Ground, Color::WoodDark * 0.75f, this);
	Paint(FlagA, Color::Wood * 1.05f, this);
	Paint(FlagB, Color::Wood * 0.88f, this);
	Paint(StreetA, Color::Stone * 0.8f, this);
	Paint(StreetB, Color::Stone * 0.72f, this);
	Paint(Water, Color::Water, this);
	Paint(Quay, Color::StoneDark * 0.7f, this);
	Paint(Blockers, Color::Paper, this);
	Paint(GrassFloor, Color::Hex(TEXT("B9A476")), this);   // wet sand
	Paint(GrassBlades, Color::Hex(TEXT("5E8F6E")), this);  // sea reeds
	Blockers->SetVisibility(!bKitFound);
}

void AGbTideWard::TownsfolkWalks(TArray<TArray<FVector2D>>& Out) const
{
	// Dockers on the quay street both ways, up and down the lane between the piers, round by the shack.
	Out.Add({ FVector2D(7.5f, 14.5f), FVector2D(35.f, 14.5f) });
	Out.Add({ FVector2D(35.f, 13.6f), FVector2D(23.5f, 13.4f), FVector2D(8.f, 13.3f) });
	Out.Add({ FVector2D(21.5f, 7.5f), FVector2D(21.5f, 22.5f), FVector2D(21.5f, 27.f) });
	Out.Add({ FVector2D(20.5f, 5.6f), FVector2D(20.5f, 12.8f), FVector2D(15.f, 14.7f), FVector2D(8.f, 14.7f) });
	Out.Add({ FVector2D(22.6f, 15.5f), FVector2D(22.6f, 22.3f) });
}

void AGbTideWard::BuildDocks(FRandomStream& Rand)
{
	using namespace GbGreybox;

	// --- two long warehouses north of the street (doors on the street), a shack with the smugglers' flag south of it
	const FHouse Houses[] = {
		{ 9.f, 8.f, 4, 2.5f, 1, 2, 1, true, true },
		{ 24.f, 8.f, 4, 2.5f, 2, 2, 2, true, true },
		{ 15.5f, 18.f, 2, 2.f, 1, 0, 0, false, true },
	};
	for (const FHouse& House : Houses) BuildHouse(House);
	Edge(TEXT("FantasyTown/overhang"), 12.75f, 13.f, 2, 2.5f);
	Edge(TEXT("FantasyTown/overhang"), 30.25f, 13.f, 2, 2.5f);
	Edge(TEXT("FantasyTown/banner_green"), 26.f, 13.f, 2, 2.5f, 260.f);
	Edge(TEXT("FantasyTown/banner_green"), 32.5f, 13.f, 2, 2.5f, 260.f);
	Put(TEXT("Pirate/flag_pirate_high"), 19.9f, 18.4f, 0.f, 90.f, 1.f);

	// --- fish stalls east of the lane, facing it and the street
	for (const float Col : { 24.6f, 27.2f })
	{
		Put(Col < 26.f ? TEXT("FantasyTown/stall_green") : TEXT("FantasyTown/stall"), Col, 17.f, 0.f, 0.f, 2.f);
		Solid(Col - 0.9f, 16.3f, 1.8f, 1.5f, 200.f);
		Put(TEXT("Pirate/barrel"), Col - 1.15f, 16.6f, 0.f, 0.f, 0.5f);
		Put(TEXT("FantasyTown/stall_stool"), Col + 0.3f, 15.9f, 0.f, 0.f, 1.7f);
	}
	Occupied.Add(TideRect(23.2f, 15.6f, 5.6f, 2.6f));

	// --- watch tower over the north-east corner, a cannon beside it
	Put(TEXT("Pirate/tower_watch"), 37.6f, 9.6f, 0.f, 0.f, 1.f);
	Solid(36.2f, 8.2f, 2.8f, 2.8f, 280.f);
	Occupied.Add(TideRect(35.8f, 7.8f, 3.6f, 3.6f));
	Put(TEXT("Pirate/cannon"), 38.8f, 11.9f, 0.f, -90.f, 0.8f);
	Solid(38.2f, 11.3f, 1.3f, 1.2f, 100.f);

	// --- mooring posts round the wharf and along the piers; lanterns at the pier heads and on the street
	for (float Col = TideWest + 0.4f; Col < TideEast; Col += 2.4f)
	{
		if (Col > 19.4f && Col < 23.6f) continue;
		Put(TEXT("FantasyTown/pillar_wood"), Col, TideNorth - 0.05f, -60.f, 0.f, 1.9f);
		Put(TEXT("FantasyTown/pillar_wood"), Col, TideSouth + 0.05f, -60.f, 0.f, 1.9f);
	}
	for (float Row = TideNorth + 0.4f; Row < TideSouth; Row += 2.4f)
	{
		Put(TEXT("FantasyTown/pillar_wood"), TideWest - 0.05f, Row, -60.f, 0.f, 1.9f);
		Put(TEXT("FantasyTown/pillar_wood"), TideEast + 0.05f, Row, -60.f, 0.f, 1.9f);
	}
	for (const float Row : { 1.6f, 2.85f, 4.1f, 5.35f, 6.4f })
	{
		Edge(TEXT("FantasyTown/fence"), 20.f, Row, 3, 1.25f);
		if (Row > 3.5f) Edge(TEXT("FantasyTown/fence"), 23.f, Row, 1, 1.25f); // open where the ferry ties up
	}
	for (const float Row : { 23.6f, 24.85f, 26.1f, 27.35f })
	{
		Edge(TEXT("FantasyTown/fence"), 23.f, Row, 1, 1.25f);
		if (Row < 25.f) Edge(TEXT("FantasyTown/fence"), 20.f, Row, 3, 1.25f); // open where the ship's gangway lands
	}
	for (const float Col : { 19.85f, 23.15f })
	{
		Put(TEXT("FantasyTown/pillar_wood"), Col, 0.9f, -80.f, 0.f, 2.6f);
		Put(TEXT("FantasyTown/pillar_wood"), Col, 28.1f, -80.f, 0.f, 2.6f);
	}
	const FVector2D Lanterns[] = { FVector2D(19.45f, 7.5f), FVector2D(23.55f, 7.5f), FVector2D(19.45f, 22.5f), FVector2D(23.55f, 22.5f),
		FVector2D(8.f, 15.5f), FVector2D(15.5f, 12.6f), FVector2D(27.5f, 12.6f), FVector2D(35.f, 15.5f) };
	for (const FVector2D& At : Lanterns)
	{
		Put(TEXT("FantasyTown/lantern"), At.X, At.Y, 0.f, 0.f, 1.9f);
		Lamp(At.X, At.Y, 270.f, Color::Lantern, 2600.f, 520.f);
	}

	// --- cargo: stacks along the edges and outside the warehouse doors
	const FVector2D Stacks[] = { FVector2D(5.2f, 13.6f), FVector2D(17.2f, 7.8f), FVector2D(36.6f, 13.5f), FVector2D(24.4f, 21.6f),
		FVector2D(16.3f, 22.f), FVector2D(34.2f, 7.8f), FVector2D(9.6f, 15.9f), FVector2D(30.8f, 15.9f) };
	for (const FVector2D& At : Stacks)
	{
		Solid(At.X - 0.6f, At.Y - 0.45f, 2.1f, 0.9f, 120.f);
		Occupied.Add(TideRect(At.X - 0.8f, At.Y - 0.7f, 2.5f, 1.4f));
		Put(TEXT("Pirate/crate"), At.X, At.Y, 0.f, Rand.FRandRange(-12.f, 12.f), 0.75f);
		Put(TEXT("Pirate/crate_bottles"), At.X + 0.85f, At.Y + 0.05f, 0.f, 90.f + Rand.FRandRange(-10.f, 10.f), 0.62f);
		Put(TEXT("Pirate/crate"), At.X + 0.1f, At.Y - 0.05f, 58.f, Rand.FRandRange(20.f, 50.f), 0.55f);
		Put(TEXT("Pirate/barrel"), At.X - 0.5f, At.Y + 0.1f, 0.f, Rand.FRandRange(0.f, 360.f), 0.55f);
		Put(TEXT("Pirate/barrel"), At.X + 1.3f, At.Y - 0.15f, 0.f, Rand.FRandRange(0.f, 360.f), 0.5f);
	}
	Put(TEXT("FantasyTown/cart"), 12.4f, 15.7f, 0.f, 70.f, 1.5f);
	Solid(11.5f, 15.1f, 1.9f, 1.3f, 100.f);
	Put(TEXT("Pirate/chest"), 18.4f, 20.6f, 0.f, 160.f, 0.5f);
	Put(TEXT("Pirate/tool_paddle"), 15.f, 20.3f, 0.f, 20.f, 0.8f);

	// --- the reed beds: dune grass among the reeds, driftwood rocks at the back
	for (const FIntRect& R : TideReeds)
	{
		const int32 Area = R.Width() * R.Height();
		for (int32 I = 0; I < Area / 2; ++I)
		{
			Put(Rand.FRand() < 0.6f ? TEXT("Pirate/grass_plant") : TEXT("Pirate/grass"), Rand.FRandRange(R.Min.X + 0.4f, R.Max.X - 0.4f), Rand.FRandRange(R.Min.Y + 0.4f, R.Max.Y - 0.4f),
				2.f, Rand.FRandRange(0.f, 360.f), Rand.FRandRange(0.7f, 1.1f), false);
		}
		Put(TEXT("Nature/rock_largeB"), R.Min.X + 1.1f, R.Max.Y - 0.8f, 6.f, Rand.FRandRange(0.f, 360.f), 22.f);
		Put(TEXT("Nature/log"), R.Max.X - 1.6f, R.Max.Y - 0.9f, 6.f, Rand.FRandRange(0.f, 360.f), 26.f);
	}

	// --- on the water: the ferry at the north pier, a ship at the south pier, others at anchor, a wreck on the rocks
	Put(TEXT("Pirate/ship_pirate_medium"), 14.2f, 26.3f, -95.f, 0.f, 1.f);
	Put(TEXT("Pirate/boat_row_small"), 25.f, 25.6f, -78.f, 170.f, 0.9f);
	Put(TEXT("Pirate/boat_row_small"), 2.4f, 12.f, -78.f, 95.f, 0.9f);
	Put(TEXT("Pirate/boat_row_large"), 41.8f, 18.5f, -78.f, 88.f, 0.9f);
	Put(TEXT("Pirate/ship_medium"), 33.f, 2.6f, -95.f, 6.f, 1.f);
	Put(TEXT("Pirate/ship_small"), 8.f, 2.f, -95.f, -12.f, 1.f);
	Put(TEXT("Pirate/ship_large"), 52.f, 12.f, -95.f, 80.f, 1.f);
	Put(TEXT("Pirate/ship_wreck"), -9.f, 21.f, -150.f, 38.f, 1.f);
	Put(TEXT("Pirate/ship_ghost"), 30.f, 44.f, -95.f, 96.f, 1.1f);
	Put(TEXT("Pirate/rocks_a"), -6.f, 24.f, -130.f, 40.f, 1.3f);
	Put(TEXT("Pirate/rocks_b"), -11.f, 17.5f, -150.f, 200.f, 1.2f);
	Put(TEXT("Pirate/rocks_c"), 45.f, 26.f, -110.f, 120.f, 1.1f);
	Put(TEXT("Pirate/rocks_a"), 47.f, 4.f, -140.f, 300.f, 1.4f);
	Put(TEXT("Pirate/rocks_sand_a"), 10.f, 31.f, -170.f, 70.f, 1.f);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbTideWardTest, "Grudgebound.Ward.TideWard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbTideWardTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(TEXT("GbTideWardFixture")), nullptr, true);
	AGbTideWard* Ward = World->SpawnActor<AGbTideWard>();
	if (!TestNotNull(TEXT("Tide Ward spawns"), Ward)) { World->DestroyWorld(false); return false; }
	TArray<UGbTallGrassComponent*> Grass; Ward->GetComponents(Grass);
	int32 Live = 0;
	for (const UGbTallGrassComponent* G : Grass)
	{
		TestEqual(TEXT("Reed beds roll the Tide Ward's curses"), G->WardId, FString(TEXT("tide")));
		if (G->GetUnscaledBoxExtent().X > 50.f) ++Live; // the unused fourth patch is shrunk to nothing
	}
	TestEqual(TEXT("Three reed beds"), Live, 3);
	TArray<UBoxComponent*> Boxes; Ward->GetComponents(Boxes);
	for (const UBoxComponent* Box : Boxes) if (Box->GetFName() == TEXT("DocksBarrier")) TestTrue(TEXT("No Lantern barrier on the wharf"), Box->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	// Landing and ferry stand on the north pier (columns 20-23, rows 1-7), a short walk apart.
	const FVector Landing = Ward->GetLandingLocation(), Ferry = Ward->GetFerryLocation();
	TestTrue(TEXT("Landing on the pier"), FMath::Abs(Landing.Y) < 150.f && Landing.X > 800.f && Landing.X < 1400.f);
	TestTrue(TEXT("Landing is outside the ferry's reach, so arriving does not offer the trip back at once"), FVector::Dist(Landing, Ferry) > 240.f);
	TArray<UPointLightComponent*> Lamps; Ward->GetComponents(Lamps);
	TestTrue(TEXT("Lanterns inside the dynamic-light budget"), Lamps.Num() <= 12);
	const int32 Count = Lamps.Num(); Ward->RebuildWard(); Lamps.Reset(); Ward->GetComponents(Lamps);
	TestEqual(TEXT("Rebuilding does not duplicate lanterns"), Lamps.Num(), Count);
	World->DestroyWorld(false); return true;
}
#endif
