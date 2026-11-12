#include "GbExorcistLook.h"

#include "GbUserSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The palette of client/game/playerArt.ts, so both clients show the same person.
	FLinearColor DollHex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	const TCHAR* HairHex = TEXT("2A2238"); const TCHAR* HairHiHex = TEXT("4A3F63");
	const TCHAR* SkinHex = TEXT("F2C9A0"); const TCHAR* BlushHex = TEXT("E8A08A"); const TCHAR* EyeHex = TEXT("1A1726");
	const TCHAR* ScarfHex = TEXT("C8302A");
	const TCHAR* TopHex = TEXT("EFE6D2"); const TCHAR* TopShHex = TEXT("C9BFA8"); const TCHAR* BeltHex = TEXT("D9A532");
	const TCHAR* PantsHex = TEXT("2B3A66"); const TCHAR* BootHex = TEXT("4A2E1E");
	const TCHAR* PaperCol = TEXT("F3E9D2"); const TCHAR* MarkCol = TEXT("B0153A");

	/** Doll centimetres to world: the doll is drawn about 146 tall and stands about 1.7 m in the Ward. */
	constexpr float DollScale = 1.18f;
	constexpr float HipZ = 46.f, ShoulderZ = 84.f;
}

UGbExorcistLook::FOutfit::FOutfit()
	: Hair(DollHex(HairHex)), HairHi(DollHex(HairHiHex)), Top(DollHex(TopHex)), TopSh(DollHex(TopShHex)), Pants(DollHex(PantsHex)), Boot(DollHex(BootHex)), Scarf(DollHex(ScarfHex)), Hat(DollHex(TEXT("D9B36A"))),
	  Belt(DollHex(BeltHex)), Trim(DollHex(BeltHex)), Skin(DollHex(SkinHex)), Apron(DollHex(TopHex)), Skirt(DollHex(PantsHex))
{
}

UGbExorcistLook::UGbExorcistLook()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Hard references on the class default object, so the shapes are cooked into a packaged game.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Cube = CubeMesh.Object; Sphere = SphereMesh.Object; Cylinder = CylinderMesh.Object; BaseMaterial = Material.Object;
}

UGbExorcistLook* UGbExorcistLook::Ensure(APawn* Pawn)
{
	if (!Pawn) return nullptr;
	if (UGbExorcistLook* Have = Pawn->FindComponentByClass<UGbExorcistLook>()) return Have;
	UGbExorcistLook* Look = NewObject<UGbExorcistLook>(Pawn, TEXT("ExorcistLook"));
	Look->SetupAttachment(Pawn->GetRootComponent());
	Look->RegisterComponent();
	return Look;
}

UMaterialInstanceDynamic* UGbExorcistLook::Paint(const FLinearColor& Color)
{
	const FColor Key = Color.ToFColor(true);
	if (const TObjectPtr<UMaterialInstanceDynamic>* Have = Paints.Find(Key)) return *Have;
	UMaterialInstanceDynamic* Made = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (Made) Made->SetVectorParameterValue(TEXT("Color"), Color);
	Paints.Add(Key, Made);
	return Made;
}

USceneComponent* UGbExorcistLook::Pivot(USceneComponent* Parent, const FVector& At)
{
	USceneComponent* P = NewObject<USceneComponent>(GetOwner());
	P->SetupAttachment(Parent); P->SetRelativeLocation(At); P->RegisterComponent();
	return P;
}

UStaticMeshComponent* UGbExorcistLook::Box(USceneComponent* Parent, const FVector& Centre, const FVector& Size, const FLinearColor& Color, UStaticMesh* Shape)
{
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(GetOwner());
	M->SetupAttachment(Parent);
	M->SetStaticMesh(Shape ? Shape : Cube.Get());
	M->SetRelativeLocation(Centre);
	M->SetRelativeScale3D(Size / 100.f); // the basic shapes are 1 m across
	M->SetMaterial(0, Paint(Color));
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M->SetCanEverAffectNavigation(false);
	M->SetGenerateOverlapEvents(false);
	if (Outfit.bLightShadows && Size.X * Size.Y * Size.Z < 4000.f) M->SetCastShadow(false);
	M->RegisterComponent();
	return M;
}

void UGbExorcistLook::BeginPlay()
{
	Super::BeginPlay();
    BuildDoll();
}
UGbExorcistLook::FOutfit UGbExorcistLook::PresentationOutfit(const FString& Look)
{
 FOutfit Out;
 const bool Feminine=Look==TEXT("feminine");
 Out.bPonytail=Feminine; Out.bTopKnot=!Feminine; Out.bBun=Feminine;
 Out.bSkirt=Feminine; Out.Skirt=Out.Pants;
 return Out;
}
void UGbExorcistLook::SetPresentation(const FString& Look)
{
 const FString Id=Look==TEXT("feminine")?TEXT("feminine"):TEXT("masculine");
 if (Presentation==Id) return; Presentation=Id; Outfit=PresentationOutfit(Id);
 if (Doll)
 {
  TArray<USceneComponent*> Children; GetChildrenComponents(true,Children);
  for (int32 I=Children.Num()-1;I>=0;--I) if (Children[I]) Children[I]->DestroyComponent();
  Doll=nullptr; Upper=nullptr; Legs[0]=Legs[1]=nullptr; Arms[0]=Arms[1]=nullptr;
  ScarfTail=nullptr; ScarfTip=nullptr; Ponytail=nullptr;
 }
 if (HasBegunPlay()) BuildDoll();
}
void UGbExorcistLook::BuildDoll()
{
	AActor* Owner = GetOwner();
	if (!Owner || Doll || !Cube || !BaseMaterial) return;

	// The template character's mannequin steps aside; its capsule and movement stay.
	float Feet = 0.f; // a plain actor stands on its own origin
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		Feet = -Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		if (USkeletalMeshComponent* Mannequin = Character->GetMesh()) { Mannequin->SetHiddenInGame(true, false); Mannequin->SetCastShadow(false); }
	}

	// Everything below is in doll centimetres: X forward, Y right, Z up from the soles.
	Doll = Pivot(this, FVector(0, 0, Feet));
	Doll->SetRelativeScale3D(FVector(DollScale * Outfit.Height));
	ScarfPaint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	ScarfDarkPaint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	SetScarfColor(Outfit.Scarf);

	// Legs swing from the hip; boots poke forward.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Y = Side == 0 ? -8.f : 8.f;
		Legs[Side] = Pivot(Doll, FVector(0, Y, HipZ));
		Box(Legs[Side], FVector(0, 0, -17), FVector(14, 13, 36), Outfit.Pants);
		Box(Legs[Side], FVector(2.5f, 0, -40), FVector(20, 14, 12), Outfit.Boot);
		Box(Legs[Side], FVector(0.5f, 0, -32.5f), FVector(15.5f, 14.5f, 3), Outfit.Trim); // boot top
	}

	// Hips, belt, jacket, scarf, head and arms lean together.
	Upper = Pivot(Doll, FVector(0, 0, HipZ));
	const auto Up = [this](const FVector& Centre, const FVector& Size, const FLinearColor& Color, UStaticMesh* Shape = nullptr)
	{
		return Box(Upper, Centre - FVector(0, 0, HipZ), Size, Color, Shape);
	};
	const FLinearColor Pants = Outfit.Pants, Top = Outfit.Top, TopSh = Outfit.TopSh, Hair = Outfit.Hair, HairHi = Outfit.HairHi;
	const FLinearColor Belt = Outfit.Belt, Skin = Outfit.Skin, Eye = DollHex(EyeHex), Blush = DollHex(BlushHex);
	Up(FVector(0, 0, 49), FVector(17, 30, 10), Pants);
	Up(FVector(0, 0, 55.5f), FVector(19, 32, 4), Belt);
	Up(FVector(0, 0, 72), FVector(18, 31, 30), Top);
	Up(FVector(-0.5f, 0, 60.5f), FVector(20, 33, 7), TopSh);          // jacket hem flares over the belt
	Up(FVector(9.2f, 0, 74), FVector(1, 3, 24), TopSh);               // front seam
	if (Outfit.bCoatTails)
	{
		// The coat splits into two tails over the hips.
		Up(FVector(-9.5f, -8.5f, 51), FVector(3, 13, 16), Top);
		Up(FVector(-9.5f, 8.5f, 51), FVector(3, 13, 16), TopSh);
	}
	if (Outfit.bSkirt)
	{
		// A long skirt or robe: the legs walk inside it, only the boots show.
		Up(FVector(0, 0, 36), FVector(23, 34, 34), Outfit.Skirt);
		Up(FVector(0, 0, 20.5f), FVector(25, 36, 3), Outfit.Skirt * 0.82f);
	}
	if (Outfit.bApron)
	{
		Up(FVector(Outfit.bSkirt ? 12.2f : 9.8f, 0, 58), FVector(1.4f, 22, 40), Outfit.Apron);
		Up(FVector(9.8f, 0, 80), FVector(1.4f, 14, 8), Outfit.Apron);   // bib
	}
	if (Outfit.bSatchel)
	{
		// The Satchel: a strap from the right shoulder to a leather bag on the left hip, behind the arm.
		const FLinearColor Leather = DollHex(TEXT("8A5A34")), LeatherDark = DollHex(TEXT("5E3A20"));
		for (const float X : { 9.8f, -9.8f }) Up(FVector(X, 0, 72), FVector(1.2f, 4, 42), LeatherDark)->SetRelativeRotation(FRotator(0, 0, X > 0 ? 38.f : -38.f));
		Up(FVector(-13, -11, 55), FVector(9, 17, 14), Leather);
		Up(FVector(-13.4f, -11, 60), FVector(10, 18, 5), LeatherDark);   // flap
		Up(FVector(-18.2f, -11, 56), FVector(1, 3, 3), Outfit.Trim);     // clasp
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Y = Side == 0 ? -20.f : 20.f;
		Arms[Side] = Pivot(Upper, FVector(0, Y, ShoulderZ - HipZ));
		Box(Arms[Side], FVector(0, 0, -13), FVector(10, 9, 28), Side == 0 ? Top : TopSh);
		Box(Arms[Side], FVector(0, 0, -30), FVector(9, 8, 7), Skin);
		Box(Arms[Side], FVector(0, 0, -25.5f), FVector(10.8f, 9.8f, 3), Outfit.Trim); // cuff
		if (Side == 1 && Outfit.bTalisman)
		{
			// The paper talisman, held up in the right hand, crimson mark on both faces.
			Box(Arms[Side], FVector(5, 0, -34), FVector(1.2f, 11, 22), DollHex(PaperCol));
			Box(Arms[Side], FVector(5, 0, -33), FVector(1.6f, 3, 9), DollHex(MarkCol));
		}
	}

	// Scarf: a thick wrap, a knot at the right shoulder and a long tail that streams behind when walking.
	const auto ScarfBox = [this](USceneComponent* Parent, const FVector& Centre, const FVector& Size, bool bDark)
	{
		UStaticMeshComponent* M = Box(Parent, Centre, Size, FLinearColor::White);
		M->SetMaterial(0, bDark ? ScarfDarkPaint : ScarfPaint);
		return M;
	};
	ScarfBox(Upper, FVector(0, 0, 91 - HipZ), FVector(25, 35, 9), false);
	ScarfBox(Upper, FVector(0, 0, 87.5f - HipZ), FVector(25.5f, 35.5f, 2), true);
	ScarfTail = Pivot(Upper, FVector(-12, 13, 90 - HipZ));
	ScarfTip = Pivot(ScarfTail, FVector(0, 0, -24));
	if (Outfit.bScarfTail)
	{
		ScarfBox(Upper, FVector(-9, 13, 90 - HipZ), FVector(8, 9, 10), false);
		ScarfBox(ScarfTail, FVector(0, 0, -12), FVector(3, 10, 24), false);
		ScarfBox(ScarfTip, FVector(0, 0, -9), FVector(3, 10, 18), false);
		ScarfBox(ScarfTip, FVector(0, 0, -19.5f), FVector(3.2f, 10.2f, 3), true);
	}

	// Head: a big paper-doll block. Face forward, hair over the top, back and sides, a fringe, a top knot with
	// its red tie and a ponytail down the back.
	Up(FVector(1, 0, 111), FVector(30, 34, 28), Skin);
	Up(FVector(-7, 0, 113), FVector(20, 38, 30), Hair);               // back and sides
	Up(FVector(0, 0, 127), FVector(34, 38, 8), Hair);                 // crown
	Up(FVector(15, 0, 122.5f), FVector(4, 36, 5), Hair);              // fringe
	Up(FVector(15, -13, 118.5f), FVector(4, 8, 4), Hair);
	Up(FVector(15, 13, 118.5f), FVector(4, 8, 4), Hair);
	Up(FVector(4, -6, 131.2f), FVector(12, 10, 1), HairHi);           // sheen
	Up(FVector(16.2f, -7, 110), FVector(1, 5, 7), Eye);
	Up(FVector(16.2f, 7, 110), FVector(1, 5, 7), Eye);
	Up(FVector(16.2f, -12.5f, 104), FVector(1, 5, 3), Blush);
	Up(FVector(16.2f, 12.5f, 104), FVector(1, 5, 3), Blush);
	Up(FVector(16.6f, -8.3f, 112), FVector(0.6f, 1.8f, 2.2f), FLinearColor::White); // a glint in each eye
	Up(FVector(16.6f, 5.7f, 112), FVector(0.6f, 1.8f, 2.2f), FLinearColor::White);
	Up(FVector(16.2f, 0, 101.5f), FVector(1, 4, 1.2f), Blush * 0.7f);              // mouth
	if (Outfit.bBeard)
	{
		Up(FVector(14.5f, 0, 99.5f), FVector(5, 24, 8), Hair);
		Up(FVector(14.5f, -14.5f, 105), FVector(5, 5, 8), Hair);
		Up(FVector(14.5f, 14.5f, 105), FVector(5, 5, 8), Hair);
	}
	if (Outfit.bBun) Up(FVector(-19, 0, 119), FVector(11, 13, 12), Hair);
	if (Outfit.bTopKnot && !Outfit.bHat)
	{
		UStaticMeshComponent* Tie = Up(FVector(-4, 0, 132.5f), FVector(10, 10, 3), Outfit.Scarf, Cylinder);
		Tie->SetMaterial(0, ScarfPaint);
		Up(FVector(-4, 0, 139.5f), FVector(13, 13, 12), Hair);        // top knot
	}
	if (Outfit.bHat)
	{
		Up(FVector(0, 0, 132.f), FVector(50, 50, 3), Outfit.Hat, Cylinder); // wide brim
		Up(FVector(0, 0, 136.f), FVector(26, 26, 7), Outfit.Hat, Cylinder);
	}
	Ponytail = Pivot(Upper, FVector(-17, 0, 108 - HipZ));
	if (Outfit.bPonytail)
	{
		Box(Ponytail, FVector(0, 0, -10), FVector(6, 9, 22), Hair);
		Box(Ponytail, FVector(0, 0, -22), FVector(4, 6, 5), Hair);
	}
}

void UGbExorcistLook::SetScarfColor(const FLinearColor& Color)
{
	if (ScarfPaint) ScarfPaint->SetVectorParameterValue(TEXT("Color"), Color);
	if (ScarfDarkPaint) ScarfDarkPaint->SetVectorParameterValue(TEXT("Color"), Color * FLinearColor(0.68f, 0.62f, 0.72f));
}

UGbExorcistLook::FPose UGbExorcistLook::Pose(float Phase, float Move, float Time)
{
	FPose P;
	const float Swing = FMath::Sin(Phase);
	P.LegL = 36.f * Move * Swing;                 // forward when positive
	P.LegR = -P.LegL;
	// Arms answer the opposite leg; standing still they only breathe. The talisman arm stays a little raised.
	P.ArmL = -30.f * Move * Swing + 2.f * FMath::Sin(Time * 1.8f);
	P.ArmR = 14.f + 22.f * Move * Swing - 2.f * FMath::Sin(Time * 1.8f);
	P.Bob = Move * 2.6f * FMath::Abs(FMath::Cos(Phase)) + (1.f - Move) * 0.8f * FMath::Sin(Time * 1.8f);
	P.Lean = 7.f * Move;
	// Hanging cloth: straight down at rest, streaming back with a flutter at a run.
	P.Scarf = 6.f + 3.f * FMath::Sin(Time * 1.3f) + Move * (58.f + 7.f * FMath::Sin(Time * 11.f));
	P.Tail = 4.f + 2.f * FMath::Sin(Time * 1.6f + 1.f) + Move * (30.f + 5.f * FMath::Sin(Time * 9.f + 0.6f));
	return P;
}

void UGbExorcistLook::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Doll) return;
	const AActor* Owner = GetOwner();
	float Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;
	if (ForcedSpeed >= 0.f) Speed = ForcedSpeed;
	if (bPreviewWalk) Speed = 420.f;
	if (bDowned) Speed = 0.f;

	Clock += DeltaTime;
	Move = FMath::FInterpTo(Move, FMath::Clamp(Speed / 320.f, 0.f, 1.f), DeltaTime, 10.f);
	// One full stride (two steps) every 1.5 m of ground, whatever the speed: the feet do not slide.
	Phase = FMath::Fmod(Phase + Speed * DeltaTime * (2.f * PI / (150.f * DollScale)), 2.f * PI);
	if (Move < 0.02f) Phase = FMath::FInterpTo(Phase, 0.f, DeltaTime, 8.f); // feet come back together
	Kneel = FMath::FInterpTo(Kneel, bDowned ? 1.f : 0.f, DeltaTime, 6.f);

	const FPose P = Pose(Phase, Move, Clock);
	Legs[0]->SetRelativeRotation(FRotator(FMath::Lerp(P.LegL, 70.f, Kneel), 0, 0));
	Legs[1]->SetRelativeRotation(FRotator(FMath::Lerp(P.LegR, -75.f, Kneel), 0, 0));
	Arms[0]->SetRelativeRotation(FRotator(P.ArmL, 0, 0));
	Arms[1]->SetRelativeRotation(FRotator(P.ArmR, 0, 0));
	Upper->SetRelativeLocation(FVector(0, 0, HipZ + P.Bob - 22.f * Kneel));
	Upper->SetRelativeRotation(FRotator(-P.Lean - 18.f * Kneel, 0, 0));
	Legs[0]->SetRelativeLocation(FVector(0, -8, HipZ + P.Bob - 22.f * Kneel));
	Legs[1]->SetRelativeLocation(FVector(0, 8, HipZ + P.Bob - 22.f * Kneel));
	ScarfTail->SetRelativeRotation(FRotator(P.Lean - P.Scarf, 0, 4.f)); // negative pitch swings a hanging part backwards
	ScarfTip->SetRelativeRotation(FRotator(-P.Scarf * 0.35f, 0, 0));
	Ponytail->SetRelativeRotation(FRotator(P.Lean - P.Tail, 0, 0));
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbExorcistPoseTest, "Grudgebound.Character.ExorcistPose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbExorcistPoseTest::RunTest(const FString&)
{
	const UGbExorcistLook::FPose Still = UGbExorcistLook::Pose(1.f, 0.f, 0.f);
	TestEqual(TEXT("Standing: legs together"), Still.LegL, 0.f);
	TestTrue(TEXT("Standing: talisman arm stays raised"), Still.ArmR > 8.f);
	TestTrue(TEXT("Standing: scarf hangs"), Still.Scarf < 12.f);
	const UGbExorcistLook::FPose Stride = UGbExorcistLook::Pose(PI / 2.f, 1.f, 0.f);
	TestTrue(TEXT("Walking: legs opposite"), Stride.LegL > 30.f && FMath::IsNearlyEqual(Stride.LegR, -Stride.LegL));
	TestTrue(TEXT("Walking: left arm answers the left leg"), Stride.ArmL < -20.f);
	TestTrue(TEXT("Walking: scarf streams behind"), Stride.Scarf > 45.f);
	TestTrue(TEXT("Walking: leans into the step"), Stride.Lean > 0.f);
	return true;
}
#endif
