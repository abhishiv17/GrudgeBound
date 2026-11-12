#include "GbGuide.h"

#include "GbBuildings.h"
#include "GbGreyboxMaterials.h"
#include "GbTownsfolk.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	GbGuide::ERegion RegionOfWard(const FString& Ward) { return Ward == TEXT("tide") ? GbGuide::ERegion::Tide : GbGuide::ERegion::Lantern; }

	/** The quest NPCs, as townsfolk voices (the same pairs GbTalk sends to the server). */
	bool NpcVoice(const FString& Role, FString& Ward, int32& Voice)
	{
		if (Role == TEXT("lamplighter")) { Ward = TEXT("lantern"); Voice = 0; return true; }
		if (Role == TEXT("receipt-clerk")) { Ward = TEXT("lantern"); Voice = 3; return true; }
		if (Role == TEXT("net-mender")) { Ward = TEXT("tide"); Voice = 1; return true; }
		return false;
	}

	AGbBuildingDoor* FindDoor(UWorld* World, TFunctionRef<bool(const AGbBuildingDoor&)> Match, const FVector& From)
	{
		AGbBuildingDoor* Best = nullptr; float BestDist = TNumericLimits<float>::Max();
		for (TActorIterator<AGbBuildingDoor> It(World); It; ++It)
			if (Match(**It)) { const float D = FVector::DistSquared(It->GetActorLocation(), From); if (D < BestDist) { BestDist = D; Best = *It; } }
		return Best;
	}
}

GbGuide::ERegion GbGuide::RegionOf(const FVector& At)
{
	if (At.X < -8000.f) return ERegion::Tide;
	if (At.X > 20000.f) return ERegion::Room;
	return ERegion::Lantern;
}

const FGbJournalEntry* GbGuide::Tracked(const FGbSatchel& S)
{
	if (!S.bValid || !S.bExplorationAvailable || !(S.Profile.bComplete || S.Profile.bLegacyStarter)) return nullptr;
	for (const FGbJournalEntry& J : S.Journal)
		if (J.Id == S.Tracked) return J.Status != TEXT("complete") && !J.Target.IsEmpty() ? &J : nullptr;
	return nullptr;
}

bool GbGuide::Resolve(UWorld* World, const FString& Target, const FVector& From, FVector& Out, FString& Via)
{
	Via.Reset();
	if (!World || Target.IsEmpty()) return false;
	bool bFound = false; FVector Goal = FVector::ZeroVector; ERegion GoalRegion = ERegion::Lantern;
	FString Ward; int32 Voice = 0;
	if (Target == TEXT("rival"))
	{
		if (AGbBuildingDoor* D = FindDoor(World, [](const AGbBuildingDoor& Door) { return Door.Use == EGbBuildingUse::Rival && !Door.IsHidden(); }, From))
		{ Goal = D->GetActorLocation(); GoalRegion = RegionOf(Goal); bFound = true; }
	}
	else if (Target.StartsWith(TEXT("find:")))
	{
		const FString Id = Target.Mid(5);
		if (AGbBuildingDoor* D = FindDoor(World, [&Id](const AGbBuildingDoor& Door) { return Door.DiscoveryId == Id; }, From))
		{ Goal = D->GetActorLocation(); GoalRegion = RegionOf(Goal); bFound = true; }
	}
	else if (Target.StartsWith(TEXT("npc:")) && NpcVoice(Target.Mid(4), Ward, Voice))
	{
		for (TActorIterator<AGbTownsfolk> It(World); It; ++It)
			if (It->GetWardId() == Ward && It->GetVoice() == Voice) { Goal = It->GetActorLocation(); bFound = true; break; }
		GoalRegion = RegionOfWard(Ward);
		if (!bFound && RegionOf(From) == GoalRegion) return false; // that walker is not in this level
	}
	else if (Target.StartsWith(TEXT("ward:")))
	{
		GoalRegion = RegionOfWard(Target.Mid(5));
		if (RegionOf(From) == GoalRegion) return false; // anywhere here will do: no beacon
	}
	else return false;

	const ERegion Here = RegionOf(From);
	if (Here == GoalRegion) { if (!bFound) return false; Out = Goal; return true; }
	if (Here == ERegion::Room)
	{
		AGbBuildingDoor* Exit = FindDoor(World, [](const AGbBuildingDoor& Door) { return Door.Use == EGbBuildingUse::Exit; }, From);
		if (!Exit) return false;
		Out = Exit->GetActorLocation(); Via = TEXT("exit"); return true;
	}
	if (GoalRegion == ERegion::Room)
	{
		// Only the storehouse has a room with a target in it; its door stands in Tide.
		AGbBuildingDoor* Door = FindDoor(World, [](const AGbBuildingDoor& D) { return D.DiscoveryId == TEXT("storehouse-door"); }, From);
		if (!Door) return false;
		if (Here == RegionOf(Door->GetActorLocation())) { Out = Door->GetActorLocation(); Via = TEXT("door"); return true; }
		GoalRegion = RegionOf(Door->GetActorLocation());
	}
	const FString To = GoalRegion == ERegion::Tide ? TEXT("tide") : TEXT("lantern");
	AGbBuildingDoor* Ferry = FindDoor(World, [&To](const AGbBuildingDoor& Door) { return Door.Use == EGbBuildingUse::Travel && Door.TravelWard == To; }, From);
	if (!Ferry) return false;
	Out = Ferry->GetActorLocation(); Via = TEXT("ferry"); return true;
}

FString GbGuide::HudLine(UWorld* World, const FGbSatchel& S, const FVector& From)
{
	const FGbJournalEntry* J = Tracked(S);
	if (!J) return FString();
	FVector At; FString Via;
	if (!Resolve(World, J->Target, From, At, Via))
		return J->Target.StartsWith(TEXT("ward:")) && RegionOf(From) != ERegion::Room ? FString(TEXT("Anywhere in this Ward's tall grass.")) : FString();
	if (Via == TEXT("ferry")) return RegionOf(At) == ERegion::Tide ? TEXT("Take the ferry back to Lantern Ward first.") : TEXT("Take the ferry to Tide Ward first.");
	if (Via == TEXT("exit")) return TEXT("Leave the building first.");
	const int32 Metres = FMath::RoundToInt(FVector::Dist2D(At, From) / 100.f);
	return Metres <= 2 ? FString(TEXT("Right here.")) : FString::Printf(TEXT("Beacon %d m away."), Metres);
}

// ================================================================ the beacon

AGbWaypoint::AGbWaypoint()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Arrow = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Arrow")); Arrow->SetupAttachment(RootComponent);
	GbGreybox::SetupMesh(Arrow, GbGreybox::EShape::Cone);
	Ring = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ring")); Ring->SetupAttachment(RootComponent);
	GbGreybox::SetupMesh(Ring, GbGreybox::EShape::Cylinder);
	for (UInstancedStaticMeshComponent* Mesh : { Arrow.Get(), Ring.Get() })
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCanEverAffectNavigation(false); Mesh->SetCastShadow(false);
	}
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow")); Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 250.f)); Glow->SetIntensity(2500.f); Glow->SetAttenuationRadius(420.f);
	Glow->SetLightColor(GbGreybox::Color::Lantern); Glow->SetCastShadows(false);
	SetActorHiddenInGame(true);
}

void AGbWaypoint::BeginPlay()
{
	Super::BeginPlay();
	// A cone turned point-down above head height, and a thin disc on the ground.
	Arrow->AddInstance(FTransform(FRotator(180.f, 0.f, 0.f), FVector::ZeroVector, FVector(.6f, .6f, .8f)));
	Ring->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 3.f), FVector(1.5f, 1.5f, .02f)));
	GbGreybox::Paint(Arrow, GbGreybox::Color::Gold, this);
	GbGreybox::Paint(Ring, GbGreybox::Color::Lantern, this);
}

void AGbWaypoint::Point(bool bShow, const FVector& Ground)
{
	bShowing = bShow;
	SetActorHiddenInGame(!bShow);
	if (bShow) SetActorLocation(Ground);
}

void AGbWaypoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bShowing) return;
	Clock += DeltaSeconds;
	Arrow->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 290.f + FMath::Sin(Clock * 2.4f) * 18.f), FRotator(0.f, Clock * 70.f, 0.f));
	const float Pulse = 1.f + FMath::Sin(Clock * 2.4f) * .08f;
	Ring->SetRelativeScale3D(FVector(Pulse, Pulse, 1.f));
}
