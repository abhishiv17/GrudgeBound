#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GbExorcistLook.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * The player's body: the young exorcist from client/game/playerArt.ts (dark hair tied up, long red scarf, cream
 * jacket, gold belt, indigo trousers, a paper talisman in hand), built as a blocky paper doll from the engine's
 * basic shapes so it sits with the low-poly Ward. It walks by itself from the owner's velocity, so it works on
 * any pawn: the template character in Lantern Ward (whose mannequin it hides) and the co-op character.
 */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbExorcistLook : public USceneComponent
{
	GENERATED_BODY()

public:
	UGbExorcistLook();

	/** Who is wearing the doll. The default is the player; townsfolk change colours and drop the exorcist's things. */
	struct FOutfit
	{
		FLinearColor Hair, HairHi, Top, TopSh, Pants, Boot, Scarf, Hat;
		/** Belt, cuffs and boot tops (gold on the player), skin, and the apron and skirt cloth. */
		FLinearColor Belt, Trim, Skin, Apron, Skirt;
		bool bScarfTail = true, bTalisman = true, bTopKnot = true, bPonytail = true, bHat = false;
		/** Only the player: split coat tails and the satchel on its strap. */
		bool bCoatTails = true, bSatchel = true;
		/** Townsfolk variety: an apron, a long skirt or robe, a beard, a bun at the nape. */
		bool bApron = false, bSkirt = false, bBeard = false, bBun = false;
		/** Only the big parts cast shadows (a street of walkers stays cheap). */
		bool bLightShadows = false;
		float Height = 1.f;
		FOutfit();
	};
	/** Before the doll is built (before the component registers on a running actor). */
	void SetOutfit(const FOutfit& In) { Outfit = In; }
    void SetPresentation(const FString& Look);
    static FOutfit PresentationOutfit(const FString& Look);
	/** Walk at this speed (cm/s) instead of reading the owner's velocity; negative = read the velocity. */
	void SetWalkSpeed(float Speed) { ForcedSpeed = Speed; }

	/** Gives the pawn this body if it has none yet (and hides its skeletal mesh). */
	static UGbExorcistLook* Ensure(APawn* Pawn);

	/** The scarf and hair tie; co-op binders differ by this colour. */
	void SetScarfColor(const FLinearColor& Color);
	/** Down in the co-op mission: the doll kneels and stops walking. */
	void SetDowned(bool bDown) { bDowned = bDown; }
	/** Screenshot tool: walk on the spot. */
	void SetPreviewWalk(bool bWalk) { bPreviewWalk = bWalk; }

	/** Joint angles (degrees) and offsets of one moment of the walk. Pure, so it can be checked without a world. */
	struct FPose { float LegL = 0, LegR = 0, ArmL = 0, ArmR = 0, Bob = 0, Lean = 0, Scarf = 0, Tail = 0; };
	static FPose Pose(float Phase, float Move, float Time);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void BuildDoll();
    FString Presentation;
	USceneComponent* Pivot(USceneComponent* Parent, const FVector& At);
	UStaticMeshComponent* Box(USceneComponent* Parent, const FVector& Centre, const FVector& Size, const FLinearColor& Color, UStaticMesh* Shape = nullptr);
	UMaterialInstanceDynamic* Paint(const FLinearColor& Color);

	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY() TMap<FColor, TObjectPtr<UMaterialInstanceDynamic>> Paints;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ScarfPaint;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ScarfDarkPaint;

	UPROPERTY() TObjectPtr<USceneComponent> Doll;
	UPROPERTY() TObjectPtr<USceneComponent> Upper;
	UPROPERTY() TObjectPtr<USceneComponent> Legs[2];
	UPROPERTY() TObjectPtr<USceneComponent> Arms[2];
	UPROPERTY() TObjectPtr<USceneComponent> ScarfTail;
	UPROPERTY() TObjectPtr<USceneComponent> ScarfTip;
	UPROPERTY() TObjectPtr<USceneComponent> Ponytail;

	float Phase = 0.f;
	float Move = 0.f;
	float Clock = 0.f;
	float Kneel = 0.f;
	bool bDowned = false;
	bool bPreviewWalk = false;
	float ForcedSpeed = -1.f;
	FOutfit Outfit;
};
