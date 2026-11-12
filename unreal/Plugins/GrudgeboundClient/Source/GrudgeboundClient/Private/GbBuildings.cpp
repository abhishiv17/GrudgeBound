#include "GbBuildings.h"
#include "GbApiSubsystem.h"
#include "GbUiDirector.h"
#include "GbGreyboxMaterials.h"
#include "GbUserSettings.h"
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
#include "Camera/PlayerCameraManager.h"
#include "GbExorcistLook.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

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
 Super::OnConstruction(T); Marker->ClearInstances(); Marker->AddInstance(FTransform(FRotator::ZeroRotator,FVector(0,0,4),FVector(1.1f,1.1f,.06f)));
 GbGreybox::Paint(Marker,GbGreybox::Color::Gold,this); Prompt->SetText(FText::FromString(Label));
 Prompt->SetTextRenderColor(GbGreybox::Color::Paper.ToFColor(true));
}
void AGbBuildingDoor::BeginPlay()
{
 Super::BeginPlay();
 GetWorldTimerManager().SetTimer(FaceTimer,this,&AGbBuildingDoor::FaceCamera,.05f,true);
}
void AGbBuildingDoor::FaceCamera()
{
 // The camera turns now, so the sign turns with it.
 const APlayerController* PC=GetWorld()->GetFirstPlayerController(); if (!PC || !PC->PlayerCameraManager) return;
 const FRotator Camera=PC->PlayerCameraManager->GetCameraRotation();
 Prompt->SetWorldRotation(FRotator(-Camera.Pitch,Camera.Yaw+180,0));
}
void AGbBuildingDoor::EnterRange(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&)
{
 APawn* Pawn=Cast<APawn>(Other); if (!Pawn || !Pawn->IsLocallyControlled()) return;
 Nearby=Pawn; APlayerController* PC=Cast<APlayerController>(Pawn->GetController()); if (!PC) return;
 EnableInput(PC); PC->bEnableClickEvents=true;
 if (InputComponent) { InputComponent->ClearActionBindings(); InputComponent->KeyBindings.Reset(); InputComponent->BindKey(UGbUserSettings::Get()->KeyFor(TEXT("Interact")),IE_Pressed,this,&AGbBuildingDoor::Interact); }
 Prompt->SetText(FText::FromString(Label + TEXT("  [") + UGbUserSettings::Get()->KeyLabel(TEXT("Interact")) + TEXT("]")));
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
 VisitorName->SetText(FText::GetEmpty());
 // Hard references, so the furniture is cooked into a packaged game.
 for (const TCHAR* Path:{TEXT("/Game/Grudgebound/Env/Pirate/chest.chest"),
  TEXT("/Game/Grudgebound/Env/Pirate/barrel.barrel"),
  TEXT("/Game/Grudgebound/Env/Pirate/crate.crate"),
  TEXT("/Game/Grudgebound/Env/Pirate/crate_bottles.crate_bottles"),
  TEXT("/Game/Grudgebound/Env/Pirate/bottle.bottle"),
  TEXT("/Game/Grudgebound/Env/Pirate/bottle_large.bottle_large"),
  TEXT("/Game/Grudgebound/Env/FantasyTown/stall_bench.stall_bench")})
 {
  ConstructorHelpers::FObjectFinder<UStaticMesh> Found(Path);
  if (Found.Succeeded()) PropMeshes.Add(FName(FPaths::GetBaseFilename(Path)),Found.Object);
 }
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
 Dress(); // before Super, so the keepers' dolls begin play with the room
 VisitorName->SetText(FText::GetEmpty());
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

// ---------------------------------------------------------------- interior dressing (picture only)

void AGbBuildingRoom::Prop(FName Mesh,const FVector& At,float Yaw,float Fit,bool bHeight)
{
 UStaticMesh* Asset=PropMeshes.FindRef(Mesh); if (!Asset) return;
 TObjectPtr<UInstancedStaticMeshComponent>& Part=PropParts.FindOrAdd(Mesh);
 if (!Part)
 {
  Part=NewObject<UInstancedStaticMeshComponent>(this); Part->SetStaticMesh(Asset); Part->SetupAttachment(Structure);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetCanEverAffectNavigation(false);
  AddInstanceComponent(Part); Part->RegisterComponent();
 }
 const FBox Box=Asset->GetBoundingBox(); const FVector Size=Box.GetSize();
 const float Scale=Fit/FMath::Max(1.,bHeight?Size.Z:FMath::Max(Size.X,Size.Y));
 const FRotator Turn(0,Yaw,0);
 // Stand the mesh's footprint centre on At.
 const FVector Centre=Turn.RotateVector(FVector(Box.GetCenter().X,Box.GetCenter().Y,Box.Min.Z)*Scale);
 Part->AddInstance(FTransform(Turn,At-Centre,FVector(Scale)));
}

void AGbBuildingRoom::Slab(const FVector& Centre,const FVector& Size,const FLinearColor& Color)
{
 TObjectPtr<UInstancedStaticMeshComponent>& Part=SlabParts.FindOrAdd(Color.ToFColor(true));
 if (!Part)
 {
  Part=NewObject<UInstancedStaticMeshComponent>(this); GbGreybox::SetupMesh(Part,GbGreybox::EShape::Cube); Part->SetupAttachment(Structure);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetCanEverAffectNavigation(false);
  AddInstanceComponent(Part); Part->RegisterComponent(); GbGreybox::Paint(Part,Color,this);
 }
 Part->AddInstance(FTransform(FRotator::ZeroRotator,Centre,Size/100.f));
}

void AGbBuildingRoom::Keeper(const FVector& At,const FLinearColor& Top,const FLinearColor& Hair,const FLinearColor& Scarf,bool bHat,bool bRobe,bool bApron)
{
 UGbExorcistLook* Doll=NewObject<UGbExorcistLook>(this);
 UGbExorcistLook::FOutfit Outfit; Outfit.Top=Top; Outfit.TopSh=Top*.82f; Outfit.Hair=Hair; Outfit.HairHi=Hair*1.3f; Outfit.Scarf=Scarf;
 Outfit.Pants=GbGreybox::Color::Hex(TEXT("3E3A4A")); Outfit.bTalisman=false; Outfit.bScarfTail=false; Outfit.bCoatTails=false; Outfit.bSatchel=false; Outfit.Trim=Scarf;
 Outfit.bSkirt=bRobe; Outfit.Skirt=Top*.9f; Outfit.bApron=bApron; Outfit.Apron=Scarf; Outfit.bHat=bHat; Outfit.bTopKnot=!bHat; Outfit.bPonytail=false;
 Doll->SetOutfit(Outfit); Doll->SetWalkSpeed(0.f);
 Doll->SetupAttachment(Structure); Doll->SetRelativeLocation(At); Doll->SetRelativeRotation(FRotator(0,180,0)); // facing the door
 AddInstanceComponent(Doll); Doll->RegisterComponent();
}

void AGbBuildingRoom::Dress()
{
 using namespace GbGreybox::Color;
 if (!Structure || !PropParts.IsEmpty() || !SlabParts.IsEmpty()) return;
 Furniture->SetVisibility(false); // collision and the saved nav stay as they were
 const FLinearColor Floor=Hex(TEXT("C9965F")), Seam=Hex(TEXT("A87A4A")), Linen=Hex(TEXT("FFF6E4"));
 const auto Candle=[this](const FVector& On) { Slab(On+FVector(0,0,9),FVector(8,8,18),GbGreybox::Color::Paper); Slab(On+FVector(0,0,21),FVector(4,4,6),GbGreybox::Color::Lantern); };
 // Floorboards, wainscot and trim on the walls, a rug by the door, a warm lamp.
 Slab(FVector(0,0,.6f),FVector(780,680,1.2f),Floor);
 for (float X=-330;X<=330;X+=60) Slab(FVector(X,0,1.3f),FVector(3,680,.4f),Seam);
 Slab(FVector(387,0,45),FVector(6,680,90),Wood); Slab(FVector(386,0,93),FVector(8,680,6),WoodDark);
 for (const float Side:{-1.f,1.f}) { Slab(FVector(0,Side*337,45),FVector(780,6,90),Wood); Slab(FVector(0,Side*336,93),FVector(780,8,6),WoodDark); }
 Slab(FVector(-90,0,1.5f),FVector(270,200,1),Gold); Slab(FVector(-90,0,1.8f),FVector(250,180,1),Crimson);
 UPointLightComponent* Lamp=NewObject<UPointLightComponent>(this); Lamp->SetupAttachment(Structure); Lamp->SetRelativeLocation(FVector(40,0,240));
 Lamp->SetLightColor(Lantern); Lamp->SetIntensity(3000); Lamp->SetAttenuationRadius(900); Lamp->SetCastShadows(false);
 AddInstanceComponent(Lamp); Lamp->RegisterComponent();

 switch (Building)
 {
 case EGbBuilding::Room:
 {
  // The bed fills the grey bed box (180,0); the bookshelf stands where the headboard box is (240,0).
  Slab(FVector(180,40,22),FVector(104,220,44),Wood); Slab(FVector(180,40,50),FVector(96,212,14),Linen);
  Slab(FVector(176,0,59),FVector(100,140,6),Crimson); Slab(FVector(176,-68,61),FVector(100,6,2),Gold);
  Slab(FVector(180,118,62),FVector(66,38,14),Paper);
  Slab(FVector(242,0,80),FVector(16,300,160),WoodDark);
  for (const float Z:{108.f,152.f}) Slab(FVector(232,0,Z),FVector(20,296,4),Wood);
  const FLinearColor Books[]={Crimson,Teal,Gold,Leaf,Dread,Paper}; int32 I=0;
  for (float Y=-130;Y<=120;Y+=17) { if ((I%5)!=4) Slab(FVector(230,Y,128+(I%3)*2),FVector(16,12,36-(I%3)*4),Books[I%6]); ++I; }
  Slab(FVector(180,-100,30),FVector(56,46,60),WoodDark); Candle(FVector(180,-100,60));
  Prop(TEXT("chest"),FVector(180,-230,0),90,95);
  Prop(TEXT("barrel"),FVector(330,290,0),0,62); Prop(TEXT("crate"),FVector(330,215,0),15,58);
  // A writing table by the door with the debt notice on it.
  Slab(FVector(-150,-285,72),FVector(80,100,6),Wood);
  for (const FVector& Leg:{FVector(-180,-325,36),FVector(-120,-325,36),FVector(-180,-245,36),FVector(-120,-245,36)}) Slab(Leg,FVector(6,6,72),WoodDark);
  Slab(FVector(-150,-285,75.6f),FVector(34,44,1),Paper); Slab(FVector(-150,-285,76.4f),FVector(4,26,.4f),Crimson);
  DebtNotice->SetRelativeLocation(FVector(-150,-285,110));
  break;
 }
 case EGbBuilding::RiteHall:
  // Kessa behind the Rite's altar (the grey altar box and her pillar); the Vow Shrine to her right.
  Slab(FVector(180,0,44),FVector(100,300,88),WoodDark); Slab(FVector(180,0,90),FVector(106,306,4),Crimson);
  Slab(FVector(127,0,62),FVector(3,130,52),Crimson); Slab(FVector(126,0,62),FVector(3,130,4),Gold);
  Slab(FVector(185,40,94),FVector(42,54,6),Paper);
  Candle(FVector(180,-120,92)); Candle(FVector(180,-100,92)); Candle(FVector(180,120,92)); Candle(FVector(180,100,92));
  Keeper(FVector(285,0,0),Hex(TEXT("5A2E4F")),Hex(TEXT("C9C3D6")),Gold,false,true);
  Slab(FVector(200,-262,45),FVector(56,56,90),Stone); Slab(FVector(200,-262,92),FVector(64,64,4),StoneDark); Slab(FVector(200,-262,95),FVector(50,50,2),Crimson);
  Candle(FVector(188,-274,96)); Candle(FVector(212,-250,96));
  Prop(TEXT("stall_bench"),FVector(-120,-200,0),90,150); Prop(TEXT("stall_bench"),FVector(-120,200,0),90,150);
  break;
 case EGbBuilding::CountingHouse:
  // The Broker behind the counter, stock along the back wall.
  Slab(FVector(180,0,44),FVector(100,300,88),Wood); Slab(FVector(180,0,91),FVector(108,308,6),WoodDark);
  Prop(TEXT("crate_bottles"),FVector(180,-95,94),0,52); Prop(TEXT("bottle_large"),FVector(175,20,94),0,30,true);
  Slab(FVector(185,90,96),FVector(40,50,4),Paper); for (const float Y:{70.f,82.f,94.f}) Slab(FVector(160,Y,96),FVector(10,10,4),Gold);
  Keeper(FVector(285,0,0),Hex(TEXT("2F6B5E")),Hex(TEXT("5A3A26")),Gold,true);
  Prop(TEXT("crate"),FVector(345,-260,0),0,70); Prop(TEXT("crate"),FVector(345,-260,70),20,56); Prop(TEXT("crate"),FVector(345,-180,0),-10,60);
  Prop(TEXT("barrel"),FVector(345,180,0),0,62); Prop(TEXT("barrel"),FVector(345,255,0),0,62);
  break;
 case EGbBuilding::Mender:
  // The Mender at a table of salves; a cot by the right wall.
  Slab(FVector(180,0,44),FVector(100,300,88),Wood); Slab(FVector(180,0,90),FVector(106,306,4),WoodDark); Slab(FVector(180,0,92.5f),FVector(60,290,1),Linen);
  Prop(TEXT("bottle"),FVector(175,-110,92),0,24,true); Prop(TEXT("bottle_large"),FVector(185,-70,92),0,30,true); Prop(TEXT("bottle"),FVector(170,90,92),0,22,true);
  Slab(FVector(185,10,96),FVector(30,60,8),Leaf);
  Keeper(FVector(285,0,0),Linen,Hex(TEXT("8A8590")),Leaf,false,true,true);
  Slab(FVector(40,265,22),FVector(190,84,44),Wood); Slab(FVector(40,265,50),FVector(182,76,12),Linen); Slab(FVector(15,265,58),FVector(120,80,5),Leaf);
  Prop(TEXT("crate_bottles"),FVector(345,-250,0),0,60); Prop(TEXT("barrel"),FVector(345,250,0),0,62);
  break;
 default: break;
 }
}
