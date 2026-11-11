#include "GbTownsfolk.h"

#include "GbExorcistLook.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FLinearColor FolkHex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	// Working clothes in the town's own colours; nobody but the player wears the red scarf and the talisman.
	const TCHAR* Tops[] = { TEXT("8C6A4A"), TEXT("5E7F8C"), TEXT("B58E5A"), TEXT("6E5A7E"), TEXT("7C8B5A"), TEXT("A65F4A"), TEXT("D8CBB0") };
	const TCHAR* Bottoms[] = { TEXT("3E3A4A"), TEXT("5A4632"), TEXT("3F5560"), TEXT("4A3B52") };
	const TCHAR* Hairs[] = { TEXT("2A2238"), TEXT("5A3A26"), TEXT("8A8590"), TEXT("3B2A22"), TEXT("B58A4A") };
	const TCHAR* Kerchiefs[] = { TEXT("D9A532"), TEXT("3A7CA5"), TEXT("4F8A3C"), TEXT("E6D6B2"), TEXT("6B46C1") };
}

AGbTownsfolk::AGbTownsfolk()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Look = CreateDefaultSubobject<UGbExorcistLook>(TEXT("Look"));
	Look->SetupAttachment(RootComponent);
}

void AGbTownsfolk::Setup(const TArray<FVector>& WorldPath, int32 Seed)
{
	Path = WorldPath;
	PathLength = Length(Path);
	Rand.Initialize(Seed);

	UGbExorcistLook::FOutfit Outfit;
	const auto Pick = [this](const TCHAR* const* List, int32 Count) { return FolkHex(List[Rand.RandRange(0, Count - 1)]); };
	Outfit.Top = Pick(Tops, UE_ARRAY_COUNT(Tops)); Outfit.TopSh = Outfit.Top * 0.82f;
	Outfit.Pants = Pick(Bottoms, UE_ARRAY_COUNT(Bottoms));
	Outfit.Hair = Pick(Hairs, UE_ARRAY_COUNT(Hairs)); Outfit.HairHi = Outfit.Hair * 1.4f;
	Outfit.Scarf = Pick(Kerchiefs, UE_ARRAY_COUNT(Kerchiefs));
	Outfit.bScarfTail = false; Outfit.bTalisman = false;
	Outfit.bHat = Rand.FRand() < 0.4f;
	Outfit.bTopKnot = !Outfit.bHat && Rand.FRand() < 0.35f;
	Outfit.bPonytail = Rand.FRand() < 0.4f;
	Outfit.bLightShadows = true;
	Outfit.Height = Rand.FRandRange(0.9f, 1.04f);
	Look->SetOutfit(Outfit);

	Speed = Rand.FRandRange(95.f, 150.f);
	Travelled = Rand.FRandRange(0.f, 2.f * PathLength); // not everyone starts at an end
	FVector Direction;
	if (Path.Num() > 0) { SetActorLocation(Along(Path, Travelled, Direction)); SetActorRotation(Direction.Rotation()); }
}

float AGbTownsfolk::Length(const TArray<FVector>& Path)
{
	float Total = 0.f;
	for (int32 I = 1; I < Path.Num(); ++I) Total += FVector::Dist(Path[I - 1], Path[I]);
	return Total;
}

FVector AGbTownsfolk::Along(const TArray<FVector>& Path, float Distance, FVector& OutDirection)
{
	OutDirection = FVector::ForwardVector;
	if (Path.Num() == 0) return FVector::ZeroVector;
	const float Total = Length(Path);
	if (Path.Num() == 1 || Total <= 0.f) return Path[0];
	// There and back: the second half of the lap runs the path in reverse.
	float D = FMath::Fmod(FMath::Max(0.f, Distance), 2.f * Total);
	const bool bBack = D >= Total;
	if (bBack) D = 2.f * Total - D;
	for (int32 I = 1; I < Path.Num(); ++I)
	{
		const float Segment = FVector::Dist(Path[I - 1], Path[I]);
		if (Segment <= KINDA_SMALL_NUMBER) continue;
		if (D <= Segment || I == Path.Num() - 1)
		{
			OutDirection = (Path[I] - Path[I - 1]).GetSafeNormal() * (bBack ? -1.f : 1.f);
			return FMath::Lerp(Path[I - 1], Path[I], Segment > 0.f ? FMath::Clamp(D / Segment, 0.f, 1.f) : 0.f);
		}
		D -= Segment;
	}
	return Path.Last();
}

void AGbTownsfolk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Path.Num() < 2 || PathLength <= 0.f) return;

	float Step = Speed;
	if (Pause > 0.f) { Pause -= DeltaSeconds; Step = 0.f; }
	FVector Direction;
	Along(Path, Travelled, Direction);
	// Wait rather than walk through the player.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (const APawn* Player = PC ? PC->GetPawn() : nullptr)
	{
		const FVector To = Player->GetActorLocation() - GetActorLocation();
		if (FMath::Abs(To.Z) < 220.f && To.SizeSquared2D() < FMath::Square(150.f) && FVector::DotProduct(To.GetSafeNormal2D(), Direction) > 0.2f) Step = 0.f;
	}

	const float End = Travelled < PathLength ? PathLength : 2.f * PathLength;
	Travelled = FMath::Min(Travelled + Step * DeltaSeconds, End);
	// Stop at the endpoint even on a slow frame, then face back along the route.
	if (Travelled >= End)
	{
		Pause = Rand.FRandRange(1.2f, 4.5f);
		Step = 0.f;
		if (Travelled >= 2.f * PathLength) Travelled = 0.f;
	}

	const FVector At = Along(Path, Travelled, Direction);
	SetActorLocation(At);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Direction.Rotation(), DeltaSeconds, 7.f));
	Look->SetWalkSpeed(Step);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbTownsfolkPathTest, "Grudgebound.Ward.TownsfolkPath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbTownsfolkPathTest::RunTest(const FString&)
{
	const TArray<FVector> Path = { FVector(0, 0, 0), FVector(300, 0, 0), FVector(300, 400, 0) };
	FVector Dir;
	TestEqual(TEXT("Path length"), AGbTownsfolk::Length(Path), 700.f);
	TestEqual(TEXT("Out: on the first leg"), AGbTownsfolk::Along(Path, 150.f, Dir), FVector(150, 0, 0));
	TestEqual(TEXT("Out: facing along it"), Dir, FVector(1, 0, 0));
	TestEqual(TEXT("Out: round the corner"), AGbTownsfolk::Along(Path, 500.f, Dir), FVector(300, 200, 0));
	TestEqual(TEXT("Back: same place, facing home"), AGbTownsfolk::Along(Path, 900.f, Dir), FVector(300, 200, 0));
	TestEqual(TEXT("Back: direction reversed"), Dir, FVector(0, -1, 0));
	TestEqual(TEXT("A full lap ends at the start"), AGbTownsfolk::Along(Path, 1400.f, Dir), FVector(0, 0, 0));
	return true;
}
#endif
