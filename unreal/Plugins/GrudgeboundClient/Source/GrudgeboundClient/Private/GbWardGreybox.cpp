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
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

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
	MeshTable = {
		{TEXT("stall"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/FantasyTown/stall_red.stall_red"))},
		{TEXT("house"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/FantasyTown/wall_wood_window_shutters.wall_wood_window_shutters"))},
		{TEXT("roof"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/FantasyTown/roof_high.roof_high"))},
		{TEXT("wall"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/FantasyTown/wall.wall"))},
		{TEXT("lantern"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/FantasyTown/lantern.lantern"))},
		{TEXT("crate"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/Pirate/crate.crate"))},
		{TEXT("barrel"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/Pirate/barrel.barrel"))},
		{TEXT("fountain"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/FantasyTown/fountain_round.fountain_round"))},
		{TEXT("plant"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/Nature/plant_bush.plant_bush"))},
		{TEXT("fence"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/FantasyTown/fence.fence"))},
		{TEXT("pier"), FSoftObjectPath(TEXT("/Game/Grudgebound/Env/Pirate/platform_planks.platform_planks"))},
	};
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
	Dress();
}

void AGbWardGreybox::Slot(FName Name, float Col, float Row, float W, float H, float Bottom, float Height, bool bSolid)
{
	UInstancedStaticMeshComponent* Mesh = SlotMeshes.FindRef(Name);
	if (!Mesh)
	{
		Mesh = NewObject<UInstancedStaticMeshComponent>(this); Mesh->SetupAttachment(RootComponent);
		UStaticMesh* Asset = nullptr;
		if (const FSoftObjectPath* Path = MeshTable.Find(Name); Path && FPackageName::DoesPackageExist(Path->GetLongPackageName())) Asset = Cast<UStaticMesh>(Path->TryLoad());
		if (Asset) Mesh->SetStaticMesh(Asset); else GbGreybox::SetupMesh(Mesh, GbGreybox::EShape::Cube);
		Mesh->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block); Mesh->SetCanEverAffectNavigation(bSolid);
		AddInstanceComponent(Mesh); Mesh->RegisterComponent(); Dressing.Add(Mesh); SlotMeshes.Add(Name, Mesh);
		if (!Asset) GbGreybox::Paint(Mesh, Name == TEXT("plant") ? GbGreybox::Color::Leaf : GbGreybox::Color::Wood, this);
	}
	const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
	const FVector Size = Box.GetSize().ComponentMax(FVector(1));
	const FVector Scale(H * Tile / Size.X, W * Tile / Size.Y, Height / Size.Z);
	const FVector Centre = TileToLocal(Col + W / 2, Row + H / 2, Bottom + Height / 2);
	Mesh->AddInstance(FTransform(FRotator::ZeroRotator, Centre - Box.GetCenter() * Scale, Scale));
}

void AGbWardGreybox::Dress()
{
	// Older saved revisions did not serialize Dressing; recover only generated instance components.
	const TArray<UActorComponent*> SavedInstances = GetInstanceComponents();
	for (UActorComponent* C : SavedInstances)
	{
		if (C && (C->IsA<UInstancedStaticMeshComponent>() || C->IsA<UPointLightComponent>() || C->IsA<UTextRenderComponent>()))
		{ Dressing.Remove(C); C->DestroyComponent(); }
	}
	for (UActorComponent* C : Dressing) if (C) C->DestroyComponent();
	Dressing.Reset(); SlotMeshes.Reset();
	// Keep the original collision hulls: the decorative kit never changes a street's footprint.
	for (int X = 3; X < Cols - 3; X += 4)
	{
		if (X >= 14 && X <= 27) continue;
		for (int Y : {11, 15})
		{
			Slot(TEXT("stall"), X, Y, 2, 1, 0, 210);
			Slot(TEXT("lantern"), X + 2.3f, Y + 0.3f, .4f, .4f, 230, 40);
			UPointLightComponent* Light = NewObject<UPointLightComponent>(this); Light->SetupAttachment(RootComponent);
			Light->SetRelativeLocation(TileToLocal(X + 2.5f, Y + .5f, 240));
			Light->SetMobility(EComponentMobility::Movable); Light->SetLightColor(GbGreybox::Color::Lantern);
			Light->SetIntensity(1800); Light->SetAttenuationRadius(260); Light->SetCastShadows(false);
			AddInstanceComponent(Light); Light->RegisterComponent(); Dressing.Add(Light);
		}
	}
	// Imported roofs and window facades, fitted to the existing four house footprints.
	const FIntRect HousesAt[] = { FIntRect(13,2,18,7), FIntRect(25,2,30,7), FIntRect(15,19,19,23), FIntRect(24,19,29,23) };
	for (const FIntRect& R : HousesAt)
	{
		Slot(TEXT("house"), R.Min.X, R.Max.Y - .15f, R.Width(), .15f, 0, 300);
		Slot(TEXT("roof"), R.Min.X - .2f, R.Min.Y - .2f, R.Width() + .4f, R.Height() + .4f, 300, 120);
	}
	for (int X = 1; X < Cols - 1; X += 2) Slot(TEXT("wall"), X, .8f, 2, .2f, 0, 300);
	Slot(TEXT("fountain"), 17,15,2,2,0,85);
	for (const FIntPoint P : {FIntPoint(16,9),FIntPoint(26,9),FIntPoint(16,17),FIntPoint(26,17)}) Slot(TEXT("plant"),P.X,P.Y,1,1,0,100);
	for (const FIntRect& R : GrassPatches)
	{
		// Low visual fences at the back; all existing grass entrances/triggers stay accessible.
		for (int X = R.Min.X; X < R.Max.X; X += 2) Slot(TEXT("fence"), X,R.Min.Y,2,.12f,0,65);
	}
	for (int Y = 24; Y < 29; ++Y) Slot(TEXT("pier"),20,Y,3,1,0,8);
	Slot(TEXT("fence"),20,28.65f,3,.2f,0,180);
	for (int X : {4,10,29,36}) { Slot(TEXT("crate"),X,23, .7f,.7f,0,70); Slot(TEXT("barrel"),X+1,23,.6f,.6f,0,80); }
	// North-east den row: red lanterns, no additional dialogue.
	for (int X : {25,27,29})
	{
		Slot(TEXT("lantern"),X,7,.35f,.35f,220,45);
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this); Light->SetupAttachment(RootComponent);
		Light->SetRelativeLocation(TileToLocal(X+.175f,7.175f,240)); Light->SetMobility(EComponentMobility::Movable);
		Light->SetLightColor(GbGreybox::Color::Crimson); Light->SetIntensity(1200); Light->SetAttenuationRadius(220); Light->SetCastShadows(false);
		AddInstanceComponent(Light); Light->RegisterComponent(); Dressing.Add(Light);
	}
	const FVector GatePositions[] = { TileToLocal(21.5f,1.5f,100), TileToLocal(42.5f,13.5f,100), TileToLocal(21.5f,28.3f,100) };
	for (int I = 0; I < Gates.Num(); ++I) { Gates[I]->SetRelativeLocation(GatePositions[I]); Gates[I]->SetBoxExtent(FVector(90,90,120)); }
	const TCHAR* Labels[] = { TEXT("market street"), TEXT("gambling-den row"), TEXT("canal edge"), TEXT("tall-grass lots"), TEXT("Bone gate"), TEXT("Ash gate"), TEXT("Tide docks gate") };
	const FVector LabelAt[] = {TileToLocal(7,13,170),TileToLocal(27,8,180),TileToLocal(10,23,150),TileToLocal(8,4,140),GatePositions[0],GatePositions[1],GatePositions[2]};
	for (int I = 0; I < 7; ++I)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this); Label->SetupAttachment(RootComponent);
		Label->SetText(FText::FromString(Labels[I])); Label->SetWorldSize(32); Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetRelativeLocation(LabelAt[I] + FVector(0,0,150)); Label->SetRelativeRotation(FRotator(55,180,0));
		Label->SetTextRenderColor(GbGreybox::Color::Paper.ToFColor(true)); Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AddInstanceComponent(Label); Label->RegisterComponent(); Dressing.Add(Label);
	}
	const auto Imported = [this](FName Name) { const UInstancedStaticMeshComponent* M = SlotMeshes.FindRef(Name); return M && M->GetStaticMesh() && !M->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")); };
	// Keep original collision, but let the CC0 slot provide the visible surface.
	Wood->SetVisibility(!Imported(TEXT("stall"))); Awnings->SetVisibility(!Imported(TEXT("stall")));
	Roofs->SetVisibility(!Imported(TEXT("roof"))); Fountain->SetVisibility(!Imported(TEXT("fountain"))); Plants->SetVisibility(!Imported(TEXT("plant")));
}

void AGbWardGreybox::GateOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsLocallyControlled() || !GetGameInstance()) return;
	for (int I = 0; I < Gates.Num(); ++I) if (Gates[I] == Component)
		if (UGbApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGbApiSubsystem>()) Api->SayAsKessa(GateLines[I]);
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
	Ward->MeshTable.Reset(); Ward->RebuildWard();
	for (int I=0; I<Grass.Num(); ++I) TestEqual(TEXT("Fallback does not move encounter triggers"), Grass[I]->GetRelativeLocation(), Centres[I]);
	TArray<UPointLightComponent*> Lamps; Ward->GetComponents(Lamps);
	TestTrue(TEXT("Lanterns stay inside dynamic-light budget"), Lamps.Num() > 0 && Lamps.Num() <= 24);
	const int32 Count = Lamps.Num(); Ward->RebuildWard(); Lamps.Reset(); Ward->GetComponents(Lamps);
	TestEqual(TEXT("Reconstruction cannot duplicate lanterns"), Lamps.Num(), Count);
	World->DestroyWorld(false); return true;
}
#endif
