#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GbUiDirector.h"
#include "GbTideWard.h"
#include "GbTownsfolk.h"
#include "GbUserSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbFerryRoundTripTest, "Grudgebound.Ward.FerryRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbFerryRoundTripTest::RunTest(const FString&)
{
	FEditorScriptExecutionGuard AllowActorEvents; // actors in an editor world ignore delegate UFUNCTIONs otherwise
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	APlayerController* PC = World->SpawnActor<APlayerController>();
	World->AddController(PC); // editor fixture does not run gameplay PostInitializeComponents
	PC->SetAsLocalPlayerController();
	ACharacter* Pawn = World->SpawnActor<ACharacter>();
	PC->Possess(Pawn);
	AGbUiDirector* Director = World->SpawnActor<AGbUiDirector>();
	AGbBuildingDoor* Near = World->SpawnActor<AGbBuildingDoor>();
	AGbBuildingDoor* Far = World->SpawnActor<AGbBuildingDoor>();
	Near->SetActorLocation(FVector(-1240,-50,0)); Far->SetActorLocation(FVector(-14700,-50,0));
	Near->Use = Far->Use = EGbBuildingUse::Travel;
	Near->TravelWard = TEXT("tide"); Far->TravelWard = TEXT("lantern");
	Near->TargetFeet = FVector(-14960,-50,0); Far->TargetFeet = FVector(-1040,-50,0);
	Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GbBuildings::Teleport(Pawn, FVector::ZeroVector);
	Director->UseBuilding(Near);
	TestFalse(TEXT("Cannot board from across town"), Director->IsCrossing());
	GbBuildings::Teleport(Pawn, Near->GetActorLocation());
	UBoxComponent* Range = Near->FindComponentByClass<UBoxComponent>();
	Range->OnComponentBeginOverlap.Broadcast(Range, Pawn, Pawn->GetCapsuleComponent(), 0, false, FHitResult());
	// If the fixture gave the pier no input component, it is used through its click path instead.
	if (Near->InputComponent)
	{
		bool bBound = false;
		for (const FInputKeyBinding& Binding : Near->InputComponent->KeyBindings)
		{
			if (Binding.Chord.Key == UGbUserSettings::Get()->KeyFor(TEXT("Interact")))
			{ bBound = true; Binding.KeyDelegate.Execute(Binding.Chord.Key); break; }
		}
		TestTrue(TEXT("Pier binds the configured interact key"), bBound);
	}
	else Near->FindComponentByClass<UInstancedStaticMeshComponent>()->OnClicked.Broadcast(Near->FindComponentByClass<UInstancedStaticMeshComponent>(), EKeys::LeftMouseButton);
	if (!TestTrue(TEXT("Interact starts the crossing"), Director->IsCrossing()))
	{ World->DestroyWorld(false); return false; }
	TestTrue(TEXT("Walking locked aboard"), PC->IsMoveInputIgnored());
	TestFalse(TEXT("Pawn cannot fall through the sea"), Pawn->GetActorEnableCollision());
	TestEqual(TEXT("Character physics suspended"), Pawn->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	Director->TickCrossing(5.f, PC, Pawn);
	TestTrue(TEXT("Halfway across open water"), FMath::IsNearlyEqual(Pawn->GetActorLocation().X, -7970.0, 1.0));
	TestTrue(TEXT("Boat carries the player"), Director->FerryBoat && FVector::Dist2D(Director->FerryBoat->GetActorLocation(), Pawn->GetActorLocation()) < 1.f);
	Director->UseBuilding(Near);
	TestEqual(TEXT("Repeated interaction cannot restart travel"), Director->CrossTime, 5.f);
	PC->SetIgnoreMoveInput(true); // settings menu opens during the trip
	Director->TickCrossing(5.f, PC, Pawn);
	TestFalse(TEXT("Crossing finishes in ten seconds"), Director->IsCrossing());
	TestTrue(TEXT("Menu's movement lock survives landing"), PC->IsMoveInputIgnored());
	PC->SetIgnoreMoveInput(false);
	TestFalse(TEXT("Closing menu restores movement"), PC->IsMoveInputIgnored());
	TestEqual(TEXT("Lands at Tide pier"), Pawn->GetActorLocation(), Near->TargetFeet + FVector(0,0,Pawn->GetSimpleCollisionHalfHeight()+5.f));
	TestTrue(TEXT("Collision restored"), Pawn->GetActorEnableCollision());
	TestEqual(TEXT("Walking restored"), Pawn->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
	GbBuildings::Teleport(Pawn, Far->GetActorLocation());
	Range = Far->FindComponentByClass<UBoxComponent>();
	Range->OnComponentBeginOverlap.Broadcast(Range, Pawn, Pawn->GetCapsuleComponent(), 0, false, FHitResult());
	Far->FindComponentByClass<UInstancedStaticMeshComponent>()->OnClicked.Broadcast(Far->FindComponentByClass<UInstancedStaticMeshComponent>(), EKeys::LeftMouseButton);
	TestTrue(TEXT("Click boards return ferry"), Director->IsCrossing());
	Director->TickCrossing(10.f, PC, Pawn);
	TestEqual(TEXT("Return lands on Lantern pier"), Pawn->GetActorLocation(), Far->TargetFeet + FVector(0,0,Pawn->GetSimpleCollisionHalfHeight()+5.f));
	GbBuildings::Teleport(Pawn, Near->GetActorLocation());
	Director->UseBuilding(Near); Director->TickCrossing(3.f, PC, Pawn);
	Director->FinishCrossing(false);
	TestTrue(TEXT("Interrupted trip returns to departure pier"), FVector::Dist2D(Pawn->GetActorLocation(), Near->GetActorLocation()) < 1.f);
	TestTrue(TEXT("Interrupted trip restores collision"), Pawn->GetActorEnableCollision());
	TestFalse(TEXT("Interrupted trip releases movement"), PC->IsMoveInputIgnored());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbTownsfolkRoutesTest, "Grudgebound.Ward.TownsfolkRoutes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbTownsfolkRoutesTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	AGbWardGreybox* Lantern = World->SpawnActor<AGbWardGreybox>();
	AGbTideWard* Tide = World->SpawnActor<AGbTideWard>(FVector(-16000,0,0), FRotator::ZeroRotator);
	int32 Count = 0;
	for (const AGbWardGreybox* Ward : { Lantern, static_cast<AGbWardGreybox*>(Tide) })
	{
		const TArray<TArray<FVector>> Paths = Ward->GetTownsfolkPaths();
		Count += Paths.Num();
		TArray<UInstancedStaticMeshComponent*> Parts; Ward->GetComponents(Parts);
		for (int32 Route = 0; Route < Paths.Num(); ++Route)
		{
			bool bBlocked = false;
			for (float D = 0; D <= AGbTownsfolk::Length(Paths[Route]) && !bBlocked; D += 10.f)
			{
				FVector Direction;
				const FVector Feet = AGbTownsfolk::Along(Paths[Route], D, Direction);
				for (UInstancedStaticMeshComponent* Part : Parts)
				{
					if (Part->GetFName() != TEXT("Blockers") && Part->GetFName() != TEXT("Board")) continue;
					for (int32 I = 0; I < Part->GetInstanceCount(); ++I)
					{
						FTransform T; Part->GetInstanceTransform(I, T, true);
						const FVector P = T.InverseTransformPosition(Feet + FVector(0,0,40));
						const FVector Margin = FVector(25,25,0) / T.GetScale3D();
						if (FMath::Abs(P.X) < 50+Margin.X && FMath::Abs(P.Y) < 50+Margin.Y && FMath::Abs(P.Z) < 50)
						{
							AddError(FString::Printf(TEXT("%s route %d intersects %s[%d] at %s"), *Ward->WardId, Route, *Part->GetName(), I, *Feet.ToString()));
							bBlocked = true; break;
						}
					}
					if (bBlocked) break;
				}
			}
		}
	}
	TestEqual(TEXT("Eight Lantern walkers and five dockers"), Count, 13);
	World->DestroyWorld(false);
	return true;
}
#endif
