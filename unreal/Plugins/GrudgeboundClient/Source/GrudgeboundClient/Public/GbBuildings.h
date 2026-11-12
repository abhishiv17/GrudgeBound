#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GbTypes.h"
#include "GbBuildings.generated.h"

class UBoxComponent;
class UStaticMesh;
class UInstancedStaticMeshComponent;
class UTextRenderComponent;

UENUM()
enum class EGbBuilding : uint8 { Outside, CountingHouse, Mender, RiteHall, Room, Storehouse };
UENUM()
enum class EGbBuildingUse : uint8 { Enter, Exit, Broker, Mender, Rules, Vows, Room, Case, Travel, Discover };

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
 /** Use == Travel: the Ward at the other end ("tide", "lantern"). The door stands outside; TargetFeet is the landing. */
 UPROPERTY(EditAnywhere, Category="Building") FString TravelWard;
 UPROPERTY(EditAnywhere, Category="Building") FString DiscoveryId;
 bool IsClaimed() const;
 virtual void OnConstruction(const FTransform&) override;
protected:
 virtual void BeginPlay() override;
private:
 UFUNCTION() void EnterRange(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit);
 UFUNCTION() void LeaveRange(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex);
 UFUNCTION() void Clicked(UPrimitiveComponent* Component, FKey Key);
 void Interact();
 void FaceCamera();
 FTimerHandle FaceTimer;
 TWeakObjectPtr<APawn> Nearby;
 UPROPERTY() TObjectPtr<UBoxComponent> Trigger;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Marker;
 UPROPERTY() TObjectPtr<UTextRenderComponent> Prompt;
 UPROPERTY() TObjectPtr<class UStaticMeshComponent> DiscoveryProp;
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
 /** Picture only, on Play: kit furniture, floor and wall trim, a lamp and whoever keeps the building. The grey
  *  Furniture boxes keep their collision (and the saved nav) but are hidden. */
 void Dress();
 /** A kit mesh (Env/<Pack>/<name>) scaled so its larger side is Fit cm (or its height, bHeight), standing on At. */
 void Prop(FName Mesh, const FVector& At, float Yaw, float Fit, bool bHeight=false);
 /** A painted box, Size in cm, Centre in room space. */
 void Slab(const FVector& Centre, const FVector& Size, const FLinearColor& Color);
 void Keeper(const FVector& At, const FLinearColor& Top, const FLinearColor& Hair, const FLinearColor& Scarf, bool bHat, bool bRobe=false, bool bApron=false);
 UPROPERTY() TMap<FName, TObjectPtr<UStaticMesh>> PropMeshes;
 UPROPERTY(Transient) TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> PropParts;
 UPROPERTY(Transient) TMap<FColor, TObjectPtr<UInstancedStaticMeshComponent>> SlabParts;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Structure;
 UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Walls;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Furniture;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Visitor;
 UPROPERTY() TObjectPtr<UTextRenderComponent> VisitorName;
 UPROPERTY() TObjectPtr<UTextRenderComponent> DebtNotice;
 FTimerHandle VisitorTimer;
};
