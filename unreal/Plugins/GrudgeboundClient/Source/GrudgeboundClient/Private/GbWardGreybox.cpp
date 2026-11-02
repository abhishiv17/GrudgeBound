#include "GbWardGreybox.h"

#include "GbApiSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbTallGrassComponent.h"
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

namespace
{
	constexpr float Tile = 100.f; // 1 web tile = 1 metre

	/** Tall-grass patches from buildLanternWard: column, row, width, height in tiles. */
	const FIntRect GrassPatches[] = {
		FIntRect(3, 3, 3 + 9, 3 + 6),
		FIntRect(31, 3, 31 + 9, 3 + 6),
		FIntRect(4, 18, 4 + 10, 18 + 4),
		FIntRect(30, 18, 30 + 10, 18 + 4),
	};

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
}

AGbWardGreybox::AGbWardGreybox()
{
	PrimaryActorTick.bCanEverTick = false;

	using GbGreybox::EShape;
	Ground = MakePart(this, nullptr, TEXT("Ground"), EShape::Cube, true);
	RootComponent = Ground;

	Streets = MakePart(this, Ground, TEXT("Streets"), EShape::Cube, false);
	Plaza = MakePart(this, Ground, TEXT("Plaza"), EShape::Cube, false);
	Mosaic = MakePart(this, Ground, TEXT("Mosaic"), EShape::Cube, false);
	Walls = MakePart(this, Ground, TEXT("Walls"), EShape::Cube, true);
	Banners = MakePart(this, Ground, TEXT("Banners"), EShape::Cube, false);
	Water = MakePart(this, Ground, TEXT("Water"), EShape::Cube, true);
	Quay = MakePart(this, Ground, TEXT("Quay"), EShape::Cube, true);
	Wood = MakePart(this, Ground, TEXT("Wood"), EShape::Cube, true);
	Awnings = MakePart(this, Ground, TEXT("Awnings"), EShape::Cube, false);
	Posts = MakePart(this, Ground, TEXT("Posts"), EShape::Cylinder, true);
	Lights = MakePart(this, Ground, TEXT("Lights"), EShape::Sphere, false);
	Plants = MakePart(this, Ground, TEXT("Plants"), EShape::Sphere, true);
	Fountain = MakePart(this, Ground, TEXT("Fountain"), EShape::Cylinder, true);
	Board = MakePart(this, Ground, TEXT("Board"), EShape::Cube, true);
	Houses = MakePart(this, Ground, TEXT("Houses"), EShape::Cube, true);
	Roofs = MakePart(this, Ground, TEXT("Roofs"), EShape::Cube, false);
	GrassBlades = MakePart(this, Ground, TEXT("GrassBlades"), EShape::Cone, false);
	GrassBlades->SetCastShadow(false);
	GrassFloor = MakePart(this, Ground, TEXT("GrassFloor"), EShape::Cube, false);

	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(GrassPatches)); ++i)
	{
		UGbTallGrassComponent* Patch = CreateDefaultSubobject<UGbTallGrassComponent>(*FString::Printf(TEXT("TallGrass%d"), i));
		Patch->SetupAttachment(Ground);
		TallGrass.Add(Patch);
	}
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

	for (UInstancedStaticMeshComponent* Mesh : { Ground.Get(), Streets.Get(), Plaza.Get(), Mosaic.Get(), Walls.Get(), Banners.Get(),
		Water.Get(), Quay.Get(), Wood.Get(), Awnings.Get(), Posts.Get(), Lights.Get(), Plants.Get(), Fountain.Get(), Board.Get(),
		Houses.Get(), Roofs.Get(), GrassBlades.Get(), GrassFloor.Get() })
	{
		Mesh->ClearInstances();
	}

	// --- ground: the town (rows 0-23) and the pier; the harbour (rows 24-29) is a pit with water at the bottom
	Block(Ground, 0, 0, Cols, 24, -20.f, 20.f);
	Block(Ground, 20, 24, 3, 5, -20.f, 20.f);
	Block(Water, 0, 24, Cols, 6, -90.f, 20.f);
	// quay wall along the water, open where the pier starts; low rails along the pier
	Block(Quay, 0, 24, 20, 1, -70.f, 120.f);
	Block(Quay, 23, 24, Cols - 23, 1, -70.f, 120.f);
	Block(Quay, 19.8f, 25, 0.2f, 4, 0.f, 50.f);
	Block(Quay, 23.0f, 25, 0.2f, 4, 0.f, 50.f);
	Block(Quay, 20, 28.8f, 3, 0.2f, 0.f, 50.f);

	// --- streets: main street (east-west) and cross street (north-south), flat, walk-through
	Block(Streets, 1, 12, Cols - 2, 3, 0.f, 1.f);
	Block(Streets, 20, 1, 3, 23, 0.f, 1.f);

	// --- plaza with the Rite board, rug, fountain, plants, banner posts, a crate
	Block(Plaza, 16, 9, 11, 9, 0.f, 2.f);
	Block(Mosaic, 19, 12, 5, 3, 0.f, 3.f);
	Block(Awnings, 20, 11, 3, 1, 0.f, 3.f); // rug, same crimson as the awnings
	Block(Board, 21.1f, 10.4f, 0.8f, 0.2f, 0.f, 200.f);
	Block(Fountain, 17, 15, 2, 2, 0.f, 70.f);
	for (const FIntPoint P : { FIntPoint(16, 9), FIntPoint(26, 9), FIntPoint(16, 17), FIntPoint(26, 17) })
	{
		Block(Plants, P.X + 0.1f, P.Y + 0.1f, 0.8f, 0.8f, 0.f, 90.f);
	}
	for (const FIntPoint P : { FIntPoint(18, 9), FIntPoint(24, 9) })
	{
		Block(Posts, P.X + 0.4f, P.Y + 0.4f, 0.2f, 0.2f, 0.f, 300.f);
		Block(Banners, P.X + 0.5f, P.Y + 0.25f, 0.6f, 0.05f, 140.f, 140.f);
	}
	Block(Wood, 25.1f, 16.1f, 0.8f, 0.8f, 0.f, 80.f);

	// --- market stalls (two tiles each) along both sides of the main street, a lantern after each
	for (int32 X = 3; X < Cols - 3; X += 4)
	{
		if (X >= 14 && X <= 27) continue;
		for (const int32 Y : { 11, 15 })
		{
			Block(Wood, X, Y + 0.15f, 2, 0.7f, 0.f, 110.f);
			Block(Awnings, X - 0.1f, Y - 0.05f, 2.2f, 1.1f, 200.f, 8.f);
			Block(Posts, X + 0.05f, Y + 0.45f, 0.1f, 0.1f, 0.f, 200.f);
			Block(Posts, X + 1.85f, Y + 0.45f, 0.1f, 0.1f, 0.f, 200.f);
			Block(Posts, X + 2.45f, Y + 0.45f, 0.1f, 0.1f, 0.f, 240.f);
			Block(Lights, X + 2.3f, Y + 0.3f, 0.4f, 0.4f, 230.f, 40.f);
		}
	}

	// --- houses (greybox only; the web map has none): paper walls, crimson roofs
	const FIntRect HouseRects[] = {
		FIntRect(13, 2, 13 + 5, 2 + 5), FIntRect(25, 2, 25 + 5, 2 + 5),
		FIntRect(15, 19, 15 + 4, 19 + 4), FIntRect(24, 19, 24 + 5, 19 + 4),
	};
	for (const FIntRect& R : HouseRects)
	{
		Block(Houses, R.Min.X, R.Min.Y, R.Width(), R.Height(), 0.f, 300.f);
		Block(Roofs, R.Min.X - 0.2f, R.Min.Y - 0.2f, R.Width() + 0.4f, R.Height() + 0.4f, 300.f, 30.f);
	}

	// --- town wall with crimson banners (north), and the west and east walls down to the quay
	Block(Walls, 0, 0, Cols, 1, 0.f, 300.f);
	Block(Walls, 0, 0, 1, 24, 0.f, 300.f);
	Block(Walls, Cols - 1, 0, 1, 24, 0.f, 300.f);
	for (int32 X = 3; X < Cols; X += 6)
	{
		Block(Banners, X + 0.2f, 1.0f, 0.6f, 0.05f, 80.f, 180.f);
	}

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
		// A darker floor under the blades so the patch reads from the top-down camera.
		Block(GrassFloor, R.Min.X, R.Min.Y, R.Width(), R.Height(), 0.f, 1.5f);
	}

	// --- colours (paper-and-ink palette)
	using namespace GbGreybox;
	Paint(Ground, Color::Grass, this);
	Paint(Streets, Color::Stone, this);
	Paint(Plaza, Color::StoneDark, this);
	Paint(Mosaic, Color::Gold, this);
	Paint(GrassFloor, Color::TallGrass * 0.8f, this);
	Paint(Walls, Color::Mortar, this);
	Paint(Banners, Color::Crimson, this);
	Paint(Water, Color::Water, this);
	Paint(Quay, Color::StoneDark, this);
	Paint(Wood, Color::Wood, this);
	Paint(Awnings, Color::Crimson, this);
	Paint(Posts, Color::WoodDark, this);
	Paint(Lights, Color::Lantern, this);
	Paint(Plants, Color::Leaf, this);
	Paint(Fountain, Color::Teal, this);
	Paint(Board, Color::Paper, this);
	Paint(Houses, Color::Paper, this);
	Paint(Roofs, Color::Ink, this);
	Paint(GrassBlades, Color::TallGrass, this);
}

void AGbWardGreybox::BeginPlay()
{
	Super::BeginPlay();
	if (bMovePlayerToStart)
	{
		// Next tick: the player's pawn may not be possessed yet when the level begins play.
		GetWorldTimerManager().SetTimerForNextTick(this, &AGbWardGreybox::MovePlayerToStart);
	}

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
	const FGbWorldState State = Api->GetWorldState();
	if (!State.bValid) return;

	constexpr float Step = 0.1f;
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
		const FLinearColor DayColor = FMath::Lerp(FLinearColor(1.f, 0.62f, 0.38f), FLinearColor(1.f, 0.97f, 0.92f), High);
		L->SetLightColor(FMath::Lerp(DayColor, FLinearColor(0.55f, 0.66f, 1.f), NightBlend));
		L->SetIntensity(SunBaseIntensity * FMath::Lerp(1.f, 0.25f, NightBlend));
	}
	if (SkyLight && SkyLight->GetLightComponent())
	{
		SkyLight->GetLightComponent()->SetIntensity(SkyBaseIntensity * FMath::Lerp(1.f, 0.45f, NightBlend));
	}
	if (NightGrade)
	{
		NightGrade->Settings.ColorGain = FMath::Lerp(FVector4(1.f, 1.f, 1.f, 1.f), FVector4(0.5f, 0.6f, 0.95f, 0.8f), NightBlend);
		NightGrade->Settings.AutoExposureBias = FMath::Lerp(0.f, -0.8f, NightBlend);
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
