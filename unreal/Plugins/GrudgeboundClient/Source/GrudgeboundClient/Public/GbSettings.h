// Project Settings > Game > Grudgebound. Saved to Config/DefaultGame.ini so the whole team shares them.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GbSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Grudgebound"))
class GRUDGEBOUNDCLIENT_API UGbSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGbSettings();

	/** Game server base URL. A packaged build can override it: Grudgebound.exe -GbServer=https://host:port */
	UPROPERTY(config, EditAnywhere, Category = "Server")
	FString ServerUrl = TEXT("http://localhost:8787");

	/** How often /state and /events are fetched, in seconds. */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (ClampMin = "0.5", ClampMax = "60"))
	float PollSeconds = 2.f;

	/** Seconds before a request to the server counts as failed. */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (ClampMin = "1", ClampMax = "60"))
	float RequestTimeoutSeconds = 6.f;

	/** Ward the player is in (Phase 1 has only Lantern Ward). */
	UPROPERTY(config, EditAnywhere, Category = "Player")
	FString WardId = TEXT("lantern");

	/** Chance of meeting a curse per metre walked in tall grass (web client: 12% per tile). */
	UPROPERTY(config, EditAnywhere, Category = "Encounters", meta = (ClampMin = "0", ClampMax = "1"))
	float EncounterChancePerMetre = 0.12f;

	/** Metres of tall grass you can cross after a battle before the next curse may appear. */
	UPROPERTY(config, EditAnywhere, Category = "Encounters", meta = (ClampMin = "0", ClampMax = "20"))
	int32 GraceMetres = 3;

	/** Create the HUD, encounter dialog, battle screen and Talisman Case on Play. Turn off when Blueprint widgets replace them. */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	bool bCreateDefaultUi = true;

	/** The server URL with any -GbServer= override applied, without a trailing slash. */
	FString GetServerUrl() const;
};
