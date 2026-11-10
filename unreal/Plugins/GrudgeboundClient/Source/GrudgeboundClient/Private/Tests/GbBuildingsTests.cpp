#if WITH_DEV_AUTOMATION_TESTS
#include "GbBuildings.h"
#include "GbSatchelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbBuildingAccessTest,"Grudgebound.Buildings.RoomAccess",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbBuildingAccessTest::RunTest(const FString&)
{
 for (EGbBuildingUse Use:{EGbBuildingUse::Broker,EGbBuildingUse::Mender,EGbBuildingUse::Rules,EGbBuildingUse::Vows,EGbBuildingUse::Room}) TestFalse(TEXT("No remote building panel outside"),GbBuildings::Allows(EGbBuilding::Outside,Use));
 TestTrue(TEXT("Counter opens Broker only in Counting House"),GbBuildings::Allows(EGbBuilding::CountingHouse,EGbBuildingUse::Broker));
 TestFalse(TEXT("Mender cannot open Broker"),GbBuildings::Allows(EGbBuilding::Mender,EGbBuildingUse::Broker));
 TestTrue(TEXT("Rite Hall permits shrine"),GbBuildings::Allows(EGbBuilding::RiteHall,EGbBuildingUse::Vows));
 TestTrue(TEXT("Room permits bed and chest"),GbBuildings::Allows(EGbBuilding::Room,EGbBuildingUse::Room));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbBuildingRoundTripTest,"Grudgebound.Buildings.DoorRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbBuildingRoundTripTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,FName(TEXT("BuildingDoorFixture")));
 const FVector Outside(720,-650,0);
 AGbBuildingDoor* Enter=World->SpawnActor<AGbBuildingDoor>(); AGbBuildingDoor* Exit=World->SpawnActor<AGbBuildingDoor>();
 Enter->Use=EGbBuildingUse::Enter; Enter->TargetFeet=GbBuildings::Spawn(EGbBuilding::Mender);
 Exit->Use=EGbBuildingUse::Exit; Exit->TargetFeet=Outside;
 ACharacter* Pawn=World->SpawnActor<ACharacter>();
 TestTrue(TEXT("Enter teleports the native pawn"),GbBuildings::Teleport(Pawn,Enter->TargetFeet));
 TestTrue(TEXT("Interior is offsite"),FVector::Dist(Pawn->GetActorLocation(),Outside)>20000);
 TestTrue(TEXT("Exit teleports the native pawn"),GbBuildings::Teleport(Pawn,Exit->TargetFeet));
 TestEqual(TEXT("Exit returns to the outside doorway at standing height"),Pawn->GetActorLocation(),Outside+FVector(0,0,Pawn->GetSimpleCollisionHalfHeight()+5));
 World->DestroyWorld(false); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbBuildingSatchelTest,"Grudgebound.Buildings.SatchelHasNoBuildingShortcuts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbBuildingSatchelTest::RunTest(const FString&)
{
 UGbSatchelWidget* W=NewObject<UGbSatchelWidget>(); W->AddToRoot(); W->Initialize(); W->TakeWidget();
 TArray<UWidget*> Widgets; W->WidgetTree->GetAllWidgets(Widgets);
 for (UWidget* Widget:Widgets) if (const UTextBlock* T=Cast<UTextBlock>(Widget))
 {
  const FString Text=T->GetText().ToString();
  TestFalse(TEXT("Satchel offers no Broker, Mender or Rest shortcut"),Text==TEXT("Broker") || Text==TEXT("Mender") || Text==TEXT("Rest until morning (free)"));
 }
 W->RemoveFromRoot(); return true;
}
#endif
