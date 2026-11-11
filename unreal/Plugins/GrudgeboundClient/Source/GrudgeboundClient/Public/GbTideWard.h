// Tide Ward (docs/BUILD_PLAN.md P4-U11): docks, warehouses, smugglers. A timber wharf standing in the sea south of
// Lantern Ward, reached by the ferry from Lantern's pier. Built the same way as Lantern Ward (basic-shape layout,
// Kenney kits on top) on the same 44 x 30 tile grid, so it reuses that class's builders:
//
//   - north pier (the ferry landing) and south pier (a moored ship), a paved quay street between them
//   - two long warehouses north of the street, a shack and fish stalls south of it, a watch tower in the corner
//   - three reed beds where the Tide Ward's curses lurk (the server rolls them from the Tide Ward's Grudge)
//
// The sun, the night grade and the player start stay with Lantern Ward: this actor only builds the place.
#pragma once

#include "CoreMinimal.h"
#include "GbWardGreybox.h"
#include "GbTideWard.generated.h"

UCLASS(ClassGroup = (Grudgebound))
class GRUDGEBOUNDCLIENT_API AGbTideWard : public AGbWardGreybox
{
	GENERATED_BODY()

public:
	AGbTideWard();
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Where the ferry sets the player down (feet), and where the ferry back is boarded: both on the north pier. */
	UFUNCTION(BlueprintPure, Category = "Grudgebound")
	FVector GetLandingLocation() const;
	UFUNCTION(BlueprintPure, Category = "Grudgebound")
	FVector GetFerryLocation() const;

protected:
	virtual void TownsfolkWalks(TArray<TArray<FVector2D>>& Out) const override;

private:
	void BuildDocks(FRandomStream& Rand);
};
