#include "GbCoopMission.h"
#include "GbExorcistLook.h"
#include "GbGreyboxMaterials.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

namespace GbBell
{
	bool Collect(FGbBellMission& M, EGbBellObject Object)
	{
		if (M.Phase != EGbBellPhase::Search) return false;
		const int32 Index = static_cast<int32>(Object) - static_cast<int32>(EGbBellObject::ClueNotice);
		if (Index < 0 || Index > 2 || (M.Clues & (1 << Index))) return false;
		M.Clues |= 1 << Index;
		if (M.Clues == 7) M.Phase = EGbBellPhase::Ritual;
		return true;
	}
	int32 RequiredAnchors(int32 PartySize) { return FMath::Clamp(PartySize, 1, 2); }
	EGbBellEnding ResolveVotes(const TArray<EGbBellEnding>& Votes)
	{
		int32 Names = 0, Bell = 0;
		for (const auto V : Votes) { Names += V == EGbBellEnding::ReturnNames; Bell += V == EGbBellEnding::BreakBell; }
		if (Names + Bell == 0) return EGbBellEnding::None;
		return Names >= Bell ? EGbBellEnding::ReturnNames : EGbBellEnding::BreakBell;
	}
	FVector Position(EGbBellObject O)
	{
		switch (O)
		{
		case EGbBellObject::Mira: return FVector(-2200, 0, 0);
		case EGbBellObject::Nara: return FVector(400, 2050, 0);
		case EGbBellObject::Silk: return FVector(-1300, -2050, 0);
		case EGbBellObject::Eiden: return FVector(650, 0, 0);
		case EGbBellObject::ClueNotice: return FVector(-1000, -1850, 0);
		case EGbBellObject::ClueContract: return FVector(400, 1700, 0);
		case EGbBellObject::ClueSeal: return FVector(1900, -1800, 0);
		case EGbBellObject::AnchorA: return FVector(1200, -600, 0);
		case EGbBellObject::AnchorB: return FVector(1200, 600, 0);
		default: return FVector(1600, 0, 0);
		}
	}
	FString Label(EGbBellObject O)
	{
		switch (O)
		{
		case EGbBellObject::Mira: return TEXT("Mira / courier");
		case EGbBellObject::Nara: return TEXT("Nara / archivist");
		case EGbBellObject::Silk: return TEXT("Silk / merchant");
		case EGbBellObject::Eiden: return TEXT("Eiden / bellkeeper");
		case EGbBellObject::ClueNotice: return TEXT("The wrong notice / E");
		case EGbBellObject::ClueContract: return TEXT("An unfinished contract / E");
		case EGbBellObject::ClueSeal: return TEXT("The ninth seal / E");
		case EGbBellObject::AnchorA: return TEXT("West witness seal / hold E");
		case EGbBellObject::AnchorB: return TEXT("East witness seal / hold E");
		default: return TEXT("THE UNWRITTEN");
		}
	}
	FString Objective(const FGbBellMission& M)
	{
		switch (M.Phase)
		{
		case EGbBellPhase::Arrival: return TEXT("The wrong recipient / Find Mira by the landing. E to speak.");
		case EGbBellPhase::Search: return TEXT("Three unfinished lines / Find the notice, contract and ninth seal. Explore the outer banks.");
		case EGbBellPhase::Ritual: return TEXT("Two voices / Hold E at different witness seals together for 3 seconds.");
		case EGbBellPhase::Revelation: return TEXT("The bellkeeper's truth / Confront Eiden at the court. E to speak.");
		case EGbBellPhase::Boss: return TEXT("Ninth toll / Channel both seals to expose the Collector. Cast during the opening; flee the toll warning.");
		case EGbBellPhase::Decision: return TEXT("Who gets remembered? / Each player votes: 1 return the names, 2 break the bell.");
		case EGbBellPhase::Defeat: return TEXT("The toll took the party / R to retry from the ritual. Your clues remain.");
		default: return M.Ending == EGbBellEnding::ReturnNames ? TEXT("Names returned / Mira remembers. Kessa gave up her name.") : TEXT("Bell broken / Kessa stays. Mira's name is still missing.");
		}
	}
}

AGbCoopState::AGbCoopState() { SetNetUpdateFrequency(10); }
void AGbCoopState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AGbCoopState, Mission); DOREPLIFETIME(AGbCoopState, Line);
}

namespace
{
	UInstancedStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, GbGreybox::EShape Shape, bool bSolid = false)
	{
		auto* Mesh = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		if (Parent) Mesh->SetupAttachment(Parent);
		GbGreybox::SetupMesh(Mesh, Shape);
		Mesh->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bSolid) Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->SetCanEverAffectNavigation(false);
		return Mesh;
	}
	void Block(UInstancedStaticMeshComponent* Mesh, FVector P, FVector Scale)
	{
		Mesh->AddInstance(FTransform(FRotator::ZeroRotator, P, Scale));
	}
}

AGbCoopObject::AGbCoopObject()
{
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = 0.15f;
	bReplicates = true; bAlwaysRelevant = true;
	Body = Part(this, nullptr, TEXT("Body"), GbGreybox::EShape::Cube); RootComponent = Body;
	Trim = Part(this, Body, TEXT("Trim"), GbGreybox::EShape::Sphere);
	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Name")); Nameplate->SetupAttachment(Body);
	Nameplate->SetHorizontalAlignment(EHTA_Center); Nameplate->SetWorldSize(32); Nameplate->SetTextRenderColor(FColor(245, 225, 170));
}
void AGbCoopObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform); Body->ClearInstances(); Trim->ClearInstances();
	const bool bNpc = static_cast<int32>(Kind) <= static_cast<int32>(EGbBellObject::Eiden);
	if (bNpc) { Block(Body, FVector(0,0,70), FVector(.65f,.6f,1.3f)); Block(Trim, FVector(0,0,160), FVector(.6f)); }
	else if (Kind == EGbBellObject::Collector)
	{
		Block(Body, FVector(0,0,210), FVector(2.2f,3.5f,3.6f));
		for (int32 I=0; I<4; ++I) Block(Trim, FVector(0,(I-1.5f)*85,410), FVector(.5f));
		Block(Trim,FVector(0,0,12),FVector::ZeroVector); // Server-clock toll telegraph, shown locally below.
	}
	else { Block(Body, FVector(0,0,20), FVector(1.5f,1.5f,.3f)); Block(Trim, FVector(0,0,60), FVector(.45f)); }
	GbGreybox::Paint(Body, bNpc ? GbGreybox::Color::Teal : GbGreybox::Color::Crimson, this);
	GbGreybox::Paint(Trim, GbGreybox::Color::Paper, this);
	Nameplate->SetRelativeLocation(FVector(0,0,Kind == EGbBellObject::Collector ? 500 : bNpc ? 210 : 110));
	Nameplate->SetText(FText::FromString(GbBell::Label(Kind)));
}
void AGbCoopObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* S = GetWorld()->GetGameState<AGbCoopState>();
	if (S && Kind == EGbBellObject::Collector)
	{
		SetActorHiddenInGame(S->Mission.Phase != EGbBellPhase::Boss);
		const bool bOpen = S->Mission.OpenUntil > S->GetServerWorldTimeSeconds();
		GbGreybox::Paint(Body, bOpen ? GbGreybox::Color::Gold : GbGreybox::Color::Dread, this);
		const float UntilToll=S->Mission.PulseAt-S->GetServerWorldTimeSeconds();
		const bool bWarning=UntilToll>0 && UntilToll<=2;
		Trim->UpdateInstanceTransform(4,FTransform(FRotator::ZeroRotator,FVector(0,0,12),bWarning?FVector(16,16,.02f):FVector::ZeroVector),false,true);
		GbGreybox::Paint(Trim,GbGreybox::Color::Rage,this);
	}
	if (S && Kind>=EGbBellObject::ClueNotice && Kind<=EGbBellObject::ClueSeal)
	{
		const bool bFound=(S->Mission.Clues & (1 << (static_cast<int32>(Kind)-static_cast<int32>(EGbBellObject::ClueNotice))))!=0;
		Nameplate->SetText(FText::FromString(bFound?TEXT("Witnessed by the party"):GbBell::Label(Kind)));
		GbGreybox::Paint(Trim,bFound?GbGreybox::Color::Leaf:GbGreybox::Color::Paper,this);
	}
	if (auto* PC = GetWorld()->GetFirstPlayerController())
	{
		FRotator Facing = PC->GetControlRotation(); Facing.Pitch = 0; Facing.Roll = 0; Facing.Yaw += 180;
		Nameplate->SetWorldRotation(Facing);
	}
}

AGbCoopCharacter::AGbCoopCharacter()
{
	bReplicates = true; SetReplicateMovement(true);
	GetCapsuleComponent()->InitCapsuleSize(35, 90);
	GetCharacterMovement()->MaxWalkSpeed = 600; GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationYaw = false;
	Robe = Part(this, RootComponent, TEXT("Robe"), GbGreybox::EShape::Cube);
	Face = Part(this, RootComponent, TEXT("Face"), GbGreybox::EShape::Sphere);
	Block(Robe, FVector(0,0,-25), FVector(.65f,.6f,1.2f)); Block(Face, FVector(0,0,55), FVector(.5f));
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom")); Boom->SetupAttachment(RootComponent);
	Boom->SetUsingAbsoluteRotation(true); Boom->SetRelativeRotation(FRotator(-60,0,0)); Boom->TargetArmLength = 1500; Boom->bDoCollisionTest = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera")); Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
}
void AGbCoopCharacter::BeginPlay()
{
	Super::BeginPlay();
	// The exorcist's body replaces the greybox robe; binders differ by scarf colour.
	if (UGbExorcistLook::Ensure(this)) { Robe->SetVisibility(false); Face->SetVisibility(false); }
	OnRep_Color(); OnRep_Health();
}
void AGbCoopCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (const auto* PS=GetPlayerState()) BinderIndex=PS->GetPlayerId();
	OnRep_Color();
}
void AGbCoopCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AGbCoopCharacter, Health); DOREPLIFETIME(AGbCoopCharacter, BinderIndex);
}
void AGbCoopCharacter::OnRep_Color()
{
	const FLinearColor Colors[] = { GbGreybox::Color::Gold, GbGreybox::Color::Teal, GbGreybox::Color::Crimson, GbGreybox::Color::Leaf };
	GbGreybox::Paint(Robe, Colors[FMath::Abs(BinderIndex)%4], this); GbGreybox::Paint(Face, GbGreybox::Color::Paper, this);
	if (UGbExorcistLook* Look = FindComponentByClass<UGbExorcistLook>()) Look->SetScarfColor(Colors[FMath::Abs(BinderIndex)%4]);
}
void AGbCoopCharacter::OnRep_Health()
{
	if (IsDown()) GetCharacterMovement()->DisableMovement(); else GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (UGbExorcistLook* Look = FindComponentByClass<UGbExorcistLook>()) Look->SetDowned(IsDown());
}
void AGbCoopCharacter::Hurt(float Damage) { if (HasAuthority() && !IsDown()) { Health = FMath::Max(0.f, Health-Damage); OnRep_Health(); } }
void AGbCoopCharacter::Restore(float Amount) { if (HasAuthority()) { Health = FMath::Clamp(Amount,1.f,100.f); OnRep_Health(); } }

AGbCoopDistrict::AGbCoopDistrict()
{
	Stone = Part(this,nullptr,TEXT("Stone"),GbGreybox::EShape::Cube,true); RootComponent = Stone;
	Ink = Part(this,Stone,TEXT("Ink"),GbGreybox::EShape::Cube);
	Paper = Part(this,Stone,TEXT("Paper"),GbGreybox::EShape::Cube);
	Lanterns = Part(this,Stone,TEXT("Lanterns"),GbGreybox::EShape::Sphere);
	Wood = Part(this,Stone,TEXT("Wood"),GbGreybox::EShape::Cube);
}
void AGbCoopDistrict::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	for (auto* M : {Stone.Get(),Ink.Get(),Paper.Get(),Lanterns.Get(),Wood.Get()}) M->ClearInstances();
	Block(Stone,FVector(0,0,-20),FVector(64,56,.4f));
	for (int32 Sign : {-1,1})
	{
		// The canal has stopped moving: its ink surface is walkable in this sealed district.
		Block(Ink,FVector(0,Sign*1300,2),FVector(60,4,.04f));
		for (int32 X : {-1400,0,1400}) Block(Wood,FVector(X,Sign*1300,8),FVector(3.5f,5,.1f));
		Block(Stone,FVector(0,Sign*2800,150),FVector(64,.5f,3));
		for (int32 X=-2600; X<=2600; X+=650)
		{
			Block(Paper,FVector(X,Sign*2520,130),FVector(4,3,2.6f));
			Block(Wood,FVector(X,Sign*2520,280),FVector(4.5f,3.5f,.25f));
			Block(Lanterns,FVector(X,Sign*2220,230),FVector(.5f,.5f,.8f));
		}
	}
	Block(Stone,FVector(-3200,0,150),FVector(.5f,56,3)); Block(Stone,FVector(3200,0,150),FVector(.5f,56,3));
	Block(Wood,FVector(-2400,0,8),FVector(8,8,.1f));
	Block(Paper,FVector(1600,0,3),FVector(18,18,.03f));
	Block(Ink,FVector(1600,0,5),FVector(16,16,.03f));
	GbGreybox::Paint(Stone,GbGreybox::Color::StoneDark,this); GbGreybox::Paint(Ink,GbGreybox::Color::Ink,this);
	GbGreybox::Paint(Paper,GbGreybox::Color::Paper,this); GbGreybox::Paint(Wood,GbGreybox::Color::Wood,this);
	GbGreybox::Paint(Lanterns,GbGreybox::Color::Lantern,this);
}
