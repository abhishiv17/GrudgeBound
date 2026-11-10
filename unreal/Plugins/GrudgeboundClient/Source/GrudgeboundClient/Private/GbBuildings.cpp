#include "GbBuildings.h"
#include "GbApiSubsystem.h"
#include "GbUiDirector.h"
#include "GbGreyboxMaterials.h"
#include "Camera/CameraTypes.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

FVector GbBuildings::Origin(EGbBuilding Kind) { return FVector(20000 + static_cast<int32>(Kind)*1600, 0, 0); }
FVector GbBuildings::Spawn(EGbBuilding Kind) { return Origin(Kind) + FVector(-50,0,0); }
bool GbBuildings::Teleport(APawn* Pawn,const FVector& Feet)
{
 return Pawn && Pawn->SetActorLocation(Feet+FVector(0,0,Pawn->GetSimpleCollisionHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
}
bool GbBuildings::Allows(EGbBuilding Kind, EGbBuildingUse Use)
{
 if (Use == EGbBuildingUse::Broker) return Kind == EGbBuilding::CountingHouse;
 if (Use == EGbBuildingUse::Mender) return Kind == EGbBuilding::Mender;
 if (Use == EGbBuildingUse::Rules || Use == EGbBuildingUse::Vows) return Kind == EGbBuilding::RiteHall;
 if (Use == EGbBuildingUse::Room || Use == EGbBuildingUse::Case) return Kind == EGbBuilding::Room;
 return false;
}

AGbBuildingDoor::AGbBuildingDoor()
{
 PrimaryActorTick.bCanEverTick=false;
 Trigger=CreateDefaultSubobject<UBoxComponent>(TEXT("Range")); RootComponent=Trigger;
 Trigger->SetBoxExtent(FVector(75,75,130)); Trigger->SetCollisionProfileName(TEXT("Trigger")); Trigger->SetCanEverAffectNavigation(false);
 Trigger->OnComponentBeginOverlap.AddDynamic(this,&AGbBuildingDoor::EnterRange);
 Trigger->OnComponentEndOverlap.AddDynamic(this,&AGbBuildingDoor::LeaveRange);
 Marker=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Marker")); Marker->SetupAttachment(Trigger);
 GbGreybox::SetupMesh(Marker,GbGreybox::EShape::Cube); Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Marker->SetCollisionResponseToAllChannels(ECR_Ignore); Marker->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Marker->SetCanEverAffectNavigation(false);
 Marker->OnClicked.AddDynamic(this,&AGbBuildingDoor::Clicked);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Prompt")); Prompt->SetupAttachment(Trigger);
 Prompt->SetRelativeLocation(FVector(0,0,220)); Prompt->SetRelativeRotation(FRotator(55,180,0));
 Prompt->SetWorldSize(27); Prompt->SetHorizontalAlignment(EHTA_Center);
}
void AGbBuildingDoor::OnConstruction(const FTransform& T)
{
 Super::OnConstruction(T); Marker->ClearInstances(); Marker->AddInstance(FTransform(FRotator::ZeroRotator,FVector(0,0,30),FVector(.8f,.8f,.6f)));
 GbGreybox::Paint(Marker,GbGreybox::Color::Gold,this); Prompt->SetText(FText::FromString(Label));
 Prompt->SetTextRenderColor(GbGreybox::Color::Paper.ToFColor(true));
}
void AGbBuildingDoor::BeginPlay() { Super::BeginPlay(); }
void AGbBuildingDoor::EnterRange(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&)
{
 APawn* Pawn=Cast<APawn>(Other); if (!Pawn || !Pawn->IsLocallyControlled()) return;
 Nearby=Pawn; APlayerController* PC=Cast<APlayerController>(Pawn->GetController()); if (!PC) return;
 EnableInput(PC); PC->bEnableClickEvents=true;
 if (InputComponent) { InputComponent->ClearActionBindings(); InputComponent->KeyBindings.Reset(); InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AGbBuildingDoor::Interact); }
 Prompt->SetText(FText::FromString(Label + TEXT("  [E]")));
}
void AGbBuildingDoor::LeaveRange(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32)
{
 if (Nearby.Get()!=Other) return;
 if (const APawn* Pawn=Nearby.Get()) DisableInput(Cast<APlayerController>(Pawn->GetController()));
 Nearby.Reset(); Prompt->SetText(FText::FromString(Label));
}
void AGbBuildingDoor::Clicked(UPrimitiveComponent*,FKey) { Interact(); }
void AGbBuildingDoor::Interact()
{
 const APawn* Pawn=Nearby.Get(); if (!Pawn || FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation())>FMath::Square(230.f)) return;
 for (TActorIterator<AGbUiDirector> It(GetWorld());It;++It) { It->UseBuilding(this); break; }
}

AGbBuildingRoom::AGbBuildingRoom()
{
 PrimaryActorTick.bCanEverTick=false;
 Structure=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Structure")); RootComponent=Structure;
 GbGreybox::SetupMesh(Structure,GbGreybox::EShape::Cube); Structure->SetCollisionProfileName(TEXT("BlockAll"));
 Furniture=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Furniture")); Furniture->SetupAttachment(Structure);
 GbGreybox::SetupMesh(Furniture,GbGreybox::EShape::Cube); Furniture->SetCollisionProfileName(TEXT("BlockAll"));
 Visitor=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Visitor")); Visitor->SetupAttachment(Structure);
 GbGreybox::SetupMesh(Visitor,GbGreybox::EShape::Cylinder); Visitor->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visitor->SetCanEverAffectNavigation(false);
 VisitorName=CreateDefaultSubobject<UTextRenderComponent>(TEXT("VisitorName")); VisitorName->SetupAttachment(Structure);
 VisitorName->SetWorldSize(27); VisitorName->SetHorizontalAlignment(EHTA_Center); VisitorName->SetRelativeRotation(FRotator(55,180,0));
 VisitorName->SetRelativeLocation(FVector(100,100,190));
 DebtNotice=CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebtNotice")); DebtNotice->SetupAttachment(Structure);
 DebtNotice->SetRelativeLocation(FVector(300,-200,160)); DebtNotice->SetRelativeRotation(FRotator(55,180,0)); DebtNotice->SetWorldSize(17);
}
void AGbBuildingRoom::OnConstruction(const FTransform& T)
{
 Super::OnConstruction(T); Structure->ClearInstances(); Furniture->ClearInstances();
 const auto Box=[this](FVector At,FVector Scale){Structure->AddInstance(FTransform(FRotator::ZeroRotator,At,Scale));};
 Box(FVector(0,0,-10),FVector(8,7,.2f));
 Box(FVector(400,0,150),FVector(.2f,7,3)); Box(FVector(-400,0,150),FVector(.2f,7,3));
 Box(FVector(0,350,150),FVector(8,.2f,3)); Box(FVector(0,-350,150),FVector(8,.2f,3));
 Furniture->AddInstance(FTransform(FRotator::ZeroRotator,FVector(180,0,45),FVector(1,3,.9f)));
 if (Building==EGbBuilding::RiteHall) Furniture->AddInstance(FTransform(FRotator::ZeroRotator,FVector(240,0,145),FVector(.4f,.4f,1.1f)));
 if (Building==EGbBuilding::Room)
 {
  Furniture->AddInstance(FTransform(FRotator::ZeroRotator,FVector(240,0,80),FVector(.2f,3,1.6f)));
  Furniture->AddInstance(FTransform(FRotator::ZeroRotator,FVector(180,-230,40),FVector(.8f,1,.8f)));
 }
 DebtNotice->SetVisibility(Building==EGbBuilding::Room);
 DebtNotice->SetText(FText::FromString(TEXT("Debt notice\nYou arrived with a debt notice\nfolded inside your coat.")));
 DebtNotice->SetTextRenderColor(GbGreybox::Color::Ink.ToFColor(true));
 GbGreybox::Paint(Structure,GbGreybox::Color::Paper,this); GbGreybox::Paint(Furniture,GbGreybox::Color::Wood,this); GbGreybox::Paint(Visitor,GbGreybox::Color::Crimson,this);
}
void AGbBuildingRoom::CalcCamera(float,FMinimalViewInfo& Out)
{
 const APawn* Pawn=GetWorld()->GetFirstPlayerController()?GetWorld()->GetFirstPlayerController()->GetPawn():nullptr;
 FVector Focus=Pawn?Pawn->GetActorLocation()-GetActorLocation():FVector::ZeroVector;
 Focus.X=FMath::Clamp(Focus.X,-30.,30.); Focus.Y=FMath::Clamp(Focus.Y,-30.,30.); Focus.Z=0;
 Out.Location=GetActorLocation()+Focus+FVector(-700,0,1100); Out.Rotation=FRotator(-58,0,0); Out.FOV=55; Out.AspectRatio=16.f/9.f;
}
void AGbBuildingRoom::BeginPlay()
{
 Super::BeginPlay(); if (Building==EGbBuilding::CountingHouse && GetGameInstance())
 if (UGbApiSubsystem* Api=GetGameInstance()->GetSubsystem<UGbApiSubsystem>()) Api->OnTownEvents.AddUniqueDynamic(this,&AGbBuildingRoom::TownEvents);
}
void AGbBuildingRoom::EndPlay(EEndPlayReason::Type Reason)
{
 if (GetGameInstance()) if (UGbApiSubsystem* Api=GetGameInstance()->GetSubsystem<UGbApiSubsystem>()) Api->OnTownEvents.RemoveDynamic(this,&AGbBuildingRoom::TownEvents);
 GetWorldTimerManager().ClearTimer(VisitorTimer); Super::EndPlay(Reason);
}
void AGbBuildingRoom::TownEvents(const TArray<FGbTownEvent>& Events)
{
 const APlayerController* PC=GetWorld()->GetFirstPlayerController(); if (!PC || !PC->GetPawn() || FVector::DistSquared(PC->GetPawn()->GetActorLocation(),GetActorLocation())>FMath::Square(600.f)) return;
 const UGbApiSubsystem* Api=GetGameInstance()->GetSubsystem<UGbApiSubsystem>(); if (!Api) return;
 for (const FGbTownEvent& E:Events)
 {
  // Agents' Broker purchases are logged by server/src/agentChain.ts as type "agent" ("... paid from its own
  // account."); its other "agent" events are Rite proposals, which happen in the Rite Hall, not here.
  if (E.Type!=TEXT("agent") || !E.Text.Contains(TEXT("paid from its own account"))) continue;
  for (const FGbLeaderEntry& Agent:Api->GetWorldState().Leaderboard)
  {
   if (Agent.Kind!=TEXT("agent") || (E.Actor!=Agent.Id && !E.Actor.StartsWith(TEXT("agent-")+Agent.Id+TEXT(".")))) continue;
   Visitor->ClearInstances(); Visitor->AddInstance(FTransform(FRotator::ZeroRotator,FVector(100,100,70),FVector(.4f,.4f,1.4f)));
   VisitorName->SetText(FText::FromString(Agent.Name)); GetWorldTimerManager().SetTimer(VisitorTimer,this,&AGbBuildingRoom::ClearVisitor,5,false);
  }
 }
}
void AGbBuildingRoom::ClearVisitor() { Visitor->ClearInstances(); VisitorName->SetText(FText::GetEmpty()); }
