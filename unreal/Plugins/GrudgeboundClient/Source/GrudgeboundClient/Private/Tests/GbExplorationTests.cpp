#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GbBuildings.h"
#include "GbTownsfolk.h"
#include "GbUiDirector.h"
#include "GbUserSettings.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbExplorationCameraTest, "Grudgebound.Ward.ExplorationCamera", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbExplorationCameraTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	APlayerController* PC = World->SpawnActor<APlayerController>();
	World->AddController(PC);
	PC->SetAsLocalPlayerController();
	PC->PlayerInput = NewObject<UPlayerInput>(PC);
	ACharacter* Pawn = World->SpawnActor<ACharacter>();
	PC->Possess(Pawn);
	USpringArmComponent* Boom = NewObject<USpringArmComponent>(Pawn);
	Boom->SetupAttachment(Pawn->GetRootComponent()); Boom->RegisterComponent();
	UCameraComponent* Camera = NewObject<UCameraComponent>(Pawn);
	Camera->SetupAttachment(Boom); Camera->RegisterComponent();
	AGbUiDirector* Director = World->SpawnActor<AGbUiDirector>();
	AGbBuildingRoom* Room = World->SpawnActor<AGbBuildingRoom>();
	UGbUserSettings* S = UGbUserSettings::Get();
	const float SavedPitch = S->CameraPitch, SavedDistance = S->CameraDistance, SavedSensitivity = S->MouseRotateSensitivity;
	const bool SavedSmooth = S->bSmoothCamera, SavedInvert = S->bInvertMouseRotate;
	const TMap<FName, FName> SavedKeys = S->KeyOverrides;
	ON_SCOPE_EXIT
	{
		S->CameraPitch = SavedPitch; S->CameraDistance = SavedDistance; S->MouseRotateSensitivity = SavedSensitivity;
		S->bSmoothCamera = SavedSmooth; S->bInvertMouseRotate = SavedInvert; S->KeyOverrides = SavedKeys;
		Director->bCameraDirty = false; // never persist a fixture's camera settings
		World->DestroyWorld(false);
	};
	S->KeyOverrides.Reset(); S->bSmoothCamera = false; S->bInvertMouseRotate = false; S->MouseRotateSensitivity = 1.f;
	S->CameraPitch = S->DefaultPitch; S->CameraDistance = S->DefaultDistance;
	Director->CameraYaw = S->DefaultYaw;
	TestTrue(TEXT("Exploration accepts camera controls"), Director->CanControlCamera(PC, false));
	PC->SetIgnoreMoveInput(true); Director->bBuildingTransition = true;
	TestFalse(TEXT("Door fade blocks the camera"), Director->CanControlCamera(PC, false));
	Director->bCrossing = true;
	TestTrue(TEXT("Ferry walking lock leaves camera available"), Director->CanControlCamera(PC, false));
	TestFalse(TEXT("Menu blocks camera aboard ferry"), Director->CanControlCamera(PC, true));
	Director->Current = AGbUiDirector::EScreen::Battle;
	TestFalse(TEXT("Battle blocks camera aboard ferry"), Director->CanControlCamera(PC, false));
	Director->Current = AGbUiDirector::EScreen::Ward;
	PC->SetIgnoreLookInput(true);
	TestFalse(TEXT("Explicit look lock is respected"), Director->CanControlCamera(PC, false));
	PC->SetIgnoreLookInput(false); PC->SetIgnoreMoveInput(false);
	Director->bCrossing = Director->bBuildingTransition = false;

	const auto Drag = [&](float Y)
	{
		PC->PlayerInput->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, EKeys::MiddleMouseButton, IE_Pressed, 1.f, false, FPlatformTime::Cycles64()));
		PC->PlayerInput->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, EKeys::MouseY, Y, 1.f / 60.f, 1, FPlatformTime::Cycles64()));
		PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), 1.f / 60.f, false);
		Director->Tick(1.f / 60.f);
	};
	Drag(10.f);
	TestTrue(TEXT("Vertical mouse drag changes actual camera pitch"), S->CameraPitch > S->DefaultPitch);
	S->bInvertMouseRotate = true; S->CameraPitch = S->DefaultPitch;
	Drag(10.f);
	TestTrue(TEXT("Inversion reverses tilt"), S->CameraPitch < S->DefaultPitch);
	S->bInvertMouseRotate = false;
	Drag(10000.f);
	TestEqual(TEXT("Drag cannot tilt past maximum"), S->CameraPitch, S->MaxPitch);
	Drag(-10000.f);
	TestEqual(TEXT("Drag cannot tilt below minimum"), S->CameraPitch, S->MinPitch);
	const float BeforePanel = S->CameraPitch;
	Director->Current = AGbUiDirector::EScreen::Satchel;
	Drag(10.f);
	TestEqual(TEXT("Panel consumes camera drag"), S->CameraPitch, BeforePanel);

	Pawn->SetActorLocation(Room->GetActorLocation());
	Boom->SetWorldRotation(FRotator(-40, 40, 0)); Boom->TargetArmLength = 2100.f;
	FMinimalViewInfo DefaultView, TurnedView, NearView;
	Room->CalcCamera(0.f, DefaultView);
	Boom->SetWorldRotation(FRotator(-60, 130, 0));
	Room->CalcCamera(0.f, TurnedView);
	TestTrue(TEXT("Room responds to orbit"), FMath::IsNearlyEqual(TurnedView.Rotation.Yaw - DefaultView.Rotation.Yaw, 90.0));
	TestTrue(TEXT("Room responds to tilt"), TurnedView.Rotation.Pitch < DefaultView.Rotation.Pitch);
	Boom->TargetArmLength = 700.f; Boom->SetWorldRotation(FRotator(-28, 40, 0));
	Room->CalcCamera(0.f, NearView);
	TestTrue(TEXT("Room zoom moves closer"), FVector::Dist(NearView.Location, Room->GetActorLocation()) < FVector::Dist(DefaultView.Location, Room->GetActorLocation()));
	TestTrue(TEXT("Room keeps camera above walls"), NearView.Rotation.Pitch <= -48.f && NearView.Location.Z - Room->GetActorLocation().Z > 600.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbTownsfolkMovementTest, "Grudgebound.Ward.TownsfolkMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbTownsfolkMovementTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AGbTownsfolk* Walker = World->SpawnActor<AGbTownsfolk>();
	Walker->Setup({ FVector::ZeroVector, FVector(400,0,0) }, 17);
	Walker->Travelled = 0.f; Walker->SetActorLocation(FVector::ZeroVector);
	Walker->Tick(.1f);
	TestTrue(TEXT("Starts gently instead of jumping to cruising speed"), Walker->CurrentSpeed > 0.f && Walker->CurrentSpeed < Walker->Speed);
	int32 FarStops = 0, HomeStops = 0;
	for (int32 I = 0; I < 1800; ++I)
	{
		const float OldPause = Walker->Pause;
		Walker->Tick(1.f / 30.f);
		if (OldPause <= 0.f && Walker->Pause > 0.f)
		{
			if (Walker->GetActorLocation().X > 399.f) ++FarStops; else ++HomeStops;
		}
		if (Walker->GetActorLocation().X < 0.f || Walker->GetActorLocation().X > 400.f)
		{ AddError(TEXT("Walker left the street route")); break; }
	}
	TestTrue(TEXT("Repeated laps pause at both ends and resume"), FarStops >= 2 && HomeStops >= 2);
	Walker->Travelled = 0.f; Walker->Pause = Walker->CurrentSpeed = 0.f; Walker->SetActorLocation(FVector::ZeroVector);
	APlayerController* PC = World->SpawnActor<APlayerController>(); World->AddController(PC);
	ACharacter* Player = World->SpawnActor<ACharacter>(); PC->Possess(Player);
	Player->SetActorLocation(FVector(100,0,90));
	Walker->Tick(.2f);
	TestEqual(TEXT("Yields to a player ahead"), Walker->GetActorLocation(), FVector::ZeroVector);
	Player->SetActorLocation(FVector(100,500,90));
	Walker->Tick(.2f);
	TestTrue(TEXT("Resumes when the player clears the route"), Walker->GetActorLocation().X > 0.f);
	Walker->Tick(20.f);
	TestEqual(TEXT("Slow frame stops at endpoint without overshooting"), Walker->GetActorLocation(), FVector(400,0,0));
	TestTrue(TEXT("Slow frame still starts an endpoint pause"), Walker->Pause > 0.f);
	return true;
}
#endif
