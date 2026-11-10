#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GbTypes.h"
#include "GbBuildings.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UTextRenderComponent;

UENUM()
enum class EGbBuilding : uint8 { Outside, CountingHouse, Mender, RiteHall, Room };
UENUM()
enum class EGbBuildingUse : uint8 { Enter, Exit, Broker, Mender, Rules, Vows, Room, Case };

namespace GbBuildings
{
 GRUDGEBOUNDCLIENT_API FVector Origin(EGbBuilding Kind);
 GRUDGEBOUNDCLIENT_API FVector Spawn(EGbBuilding Kind);
 GRUDGEBOUNDCLIENT_API bool Allows(EGbBuilding Kind, EGbBuildingUse Use);
 GRUDGEBOUNDCLIENT_API bool Teleport(APawn* Pawn, const FVector& Feet);
}

/** Local interactions only; no server movement or new economy endpoints. */
UCLASS()
class GRUDGEBOUNDCLIENT_API AGbBuildingDoor : public AActor
{
 GENERATED_BODY()
public:
 AGbBuildingDoor();
 UPROPERTY(EditAnywhere, Category="Building") EGbBuilding Building = EGbBuilding::CountingHouse;
 UPROPERTY(EditAnywhere, Category="Building") EGbBuildingUse Use = EGbBuildingUse::Enter;
 UPROPERTY(EditAnywhere, Category="Building") FVector TargetFeet = FVector::ZeroVector;
 UPROPERTY(EditAnywhere, Category="Building") FString Label = TEXT("Counting House");
 virtual void OnConstruction(const FTransform&) override;
protected:
 virtual void BeginPlay() override;
private:
 UFUNCTION() void EnterRange(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit);
 UFUNCTION() void LeaveRange(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex);
 UFUNCTION() void Clicked(UPrimitiveComponent* Component, FKey Key);
 void Interact();
 TWeakObjectPtr<APawn> Nearby;
 UPROPERTY() TObjectPtr<UBoxComponent> Trigger;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Marker;
 UPROPERTY() TObjectPtr<UTextRenderComponent> Prompt;
};

/** Small offsite room; clamps its camera focus to the room, never streams a level. */
UCLASS()
class GRUDGEBOUNDCLIENT_API AGbBuildingRoom : public AActor
{
 GENERATED_BODY()
public:
 AGbBuildingRoom();
 UPROPERTY(EditAnywhere, Category="Building") EGbBuilding Building = EGbBuilding::CountingHouse;
 virtual void OnConstruction(const FTransform&) override;
 virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& Out) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void TownEvents(const TArray<FGbTownEvent>& Events);
 void ClearVisitor();
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Structure;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Furniture;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Visitor;
 UPROPERTY() TObjectPtr<UTextRenderComponent> VisitorName;
 UPROPERTY() TObjectPtr<UTextRenderComponent> DebtNotice;
 FTimerHandle VisitorTimer;
};
