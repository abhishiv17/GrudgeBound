// The player's own settings: key bindings, camera and sound. Saved per machine in GameUserSettings.ini
// (Saved/Config/...), next to the engine's display settings. Edited in game through the settings menu (Esc).
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "GbUserSettings.generated.h"

class APlayerController;
class UWorld;

/** One action the player can rebind. */
struct FGbActionDef
{
	FName Id;
	FString Label;
	FString Group;
	FKey Default;
};

UCLASS(Config = GameUserSettings)
class GRUDGEBOUNDCLIENT_API UGbUserSettings : public UObject
{
	GENERATED_BODY()

public:
	static UGbUserSettings* Get();
	/** Every rebindable action, in the order the Controls tab lists them. */
	static const TArray<FGbActionDef>& Actions();

	FKey KeyFor(FName Action) const;
	/** Short name of the action's key for prompts: "E", "Esc", "Mouse Wheel Up". */
	FString KeyLabel(FName Action) const;
	static FString KeyName(const FKey& Key);
	bool IsDown(const APlayerController* PC, FName Action) const;
	bool WasPressed(const APlayerController* PC, FName Action) const;

	/** Gives the action this key. If another action had it, the two swap; returns that other action (or None). */
	FName SetKey(FName Action, const FKey& Key);
	void ResetControls();
	void ResetCamera();
	void ResetSound();
	/** Writes the ini and tells listeners (key hints, input bindings). */
	void Save();
	/** Master volume and background muting. */
	void ApplySound(UWorld* World) const;

	FSimpleMulticastDelegate OnChanged;

	/** Action id -> key name, only for actions moved off their default. */
	UPROPERTY(Config) TMap<FName, FName> KeyOverrides;

	static constexpr float DefaultDistance = 2100.f;
	static constexpr float DefaultPitch = 40.f;
	static constexpr float MinPitch = 28.f;
	static constexpr float MaxPitch = 78.f;
	static constexpr float DefaultRotateSpeed = 110.f;
	/** Heading the camera starts at and returns to: a quarter view, so houses show two walls. */
	static constexpr float DefaultYaw = 40.f;
	static constexpr float MaxDistance = 3600.f;
	/** Camera boom length in cm (mouse wheel). */
	UPROPERTY(Config) float CameraDistance = DefaultDistance;
	/** Degrees the camera looks down. */
	UPROPERTY(Config) float CameraPitch = DefaultPitch;
	/** Degrees per second for the rotate keys. */
	UPROPERTY(Config) float CameraRotateSpeed = DefaultRotateSpeed;
	/** Multiplier for rotating with the mouse held. */
	UPROPERTY(Config) float MouseRotateSensitivity = 1.f;
	UPROPERTY(Config) bool bInvertMouseRotate = false;
	UPROPERTY(Config) bool bSmoothCamera = true;

	UPROPERTY(Config) float MasterVolume = 1.f;
	/** The night-market loop, and footsteps / panel / battle effects. Both sit under the master volume. */
	UPROPERTY(Config) float AmbienceVolume = 0.7f;
	UPROPERTY(Config) float EffectsVolume = 1.f;
	UPROPERTY(Config) bool bMuteInBackground = true;
	UPROPERTY(Config) bool bShowFps = false;
	/** Stops battle bobbing, lunges, shakes and moving numbers; keeps static outcome feedback. */
	UPROPERTY(Config) bool bReducedBattleMotion = false;
};
