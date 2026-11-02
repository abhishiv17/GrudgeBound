#include "GbTallGrassComponent.h"

#include "GbApiSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	constexpr float CmPerMetre = 100.f;
	/** Ignore jumps bigger than this in one frame (teleports, respawns). */
	constexpr float MaxStepCm = 300.f;
}

UGbTallGrassComponent::UGbTallGrassComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
	SetBoxExtent(FVector(300.f, 200.f, 100.f));
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	ShapeColor = FColor(80, 160, 60);
}

void UGbTallGrassComponent::BeginPlay()
{
	Super::BeginPlay();
	MetresWalked = 0.f;
	bHadPawn = false;
}

APawn* UGbTallGrassComponent::PawnInside() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn) return nullptr;

	// Feet position against the box, in the box's own space (works when the grass is rotated or scaled).
	const FVector Local = GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
	const FVector Extent = GetUnscaledBoxExtent();
	const bool bInside = FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z + 200.f;
	return bInside ? Pawn : nullptr;
}

void UGbTallGrassComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* Pawn = PawnInside();
	if (!Pawn)
	{
		bHadPawn = false;
		return;
	}

	const FVector Now = Pawn->GetActorLocation();
	if (!bHadPawn)
	{
		bHadPawn = true;
		LastPawnLocation = Now;
		return;
	}
	const float StepCm = FVector::Dist2D(Now, LastPawnLocation);
	LastPawnLocation = Now;
	if (StepCm <= KINDA_SMALL_NUMBER || StepCm > MaxStepCm) return;

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UGbApiSubsystem* Api = GI ? GI->GetSubsystem<UGbApiSubsystem>() : nullptr;
	UGbPlayerSubsystem* Player = GI ? GI->GetSubsystem<UGbPlayerSubsystem>() : nullptr;
	if (!Api || !Player) return;
	// No new curse while one is already on screen or being asked for.
	if (Api->IsBattleOpen() || Api->IsBattleBusy()) return;

	MetresWalked += StepCm / CmPerMetre;
	while (MetresWalked >= 1.f)
	{
		MetresWalked -= 1.f;
		if (!Player->ConsumeGraceMetre()) continue;

		const UGbSettings* Settings = GetDefault<UGbSettings>();
		if (FMath::FRand() < Settings->EncounterChancePerMetre)
		{
			MetresWalked = 0.f;
			OnEncounterRolled.Broadcast();
			Api->StartBattle(WardId.IsEmpty() ? Settings->WardId : WardId, Player->GetPartner().SpeciesId);
			return;
		}
	}
}

// ---------------------------------------------------------------- AGbTallGrass

AGbTallGrass::AGbTallGrass()
{
	PrimaryActorTick.bCanEverTick = false;

	Grass = CreateDefaultSubobject<UGbTallGrassComponent>(TEXT("Grass"));
	RootComponent = Grass;

	Blades = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Blades"));
	Blades->SetupAttachment(Grass);
	Blades->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blades->SetCanEverAffectNavigation(false);
	Blades->SetCastShadow(false);
	GbGreybox::SetupMesh(Blades, GbGreybox::EShape::Cone);
}

void AGbTallGrass::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Grass->SetBoxExtent(FVector(SizeMetres.X * 50.f, SizeMetres.Y * 50.f, 100.f));
	GbGreybox::FillGrassBlades(Blades, SizeMetres, BladesPerMetre, static_cast<int32>(GetUniqueID()));
	GbGreybox::Paint(Blades, GbGreybox::Color::TallGrass, this);
}
