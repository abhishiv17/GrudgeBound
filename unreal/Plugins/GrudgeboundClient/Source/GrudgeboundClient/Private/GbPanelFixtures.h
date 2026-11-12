#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
class UGbWidgetBase;

/** Sample data for the building and pocket panels (no server). Shared by the panel preview test and -GbShotPanels. */
struct FGbPanelFixtures
{
	/** Every picture: TalismanCase, Mender, Satchel-<pocket>, Broker, Rite, Vows, Observatory-<tab>, Room. */
	static const TArray<FString>& Shots();
	/** Fills W (the panel the shot is of) with that shot's sample data. */
	static void Fill(UGbWidgetBase* W, const FString& Shot);
};
#endif
