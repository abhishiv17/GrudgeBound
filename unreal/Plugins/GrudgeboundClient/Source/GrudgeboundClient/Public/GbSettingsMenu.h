// The in-game menu (Esc, or F1): Game, Controls (rebindable keys), Camera, Display and Sound.
// One host actor per game world, spawned by UGbGameFlowSubsystem in Lantern Ward and in the co-op mission.
// The menu is plain Slate on top of every other screen; it never pauses the server-driven game.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "GbSettingsMenu.generated.h"

class SWidget;
class SGbSettingsMenu;
class IInputProcessor;

UCLASS(NotPlaceable, Transient)
class GRUDGEBOUNDCLIENT_API AGbSettingsHost : public AActor
{
	GENERATED_BODY()

public:
	AGbSettingsHost();
	static AGbSettingsHost* Find(const UWorld* World);

	bool IsOpen() const { return bOpen; }
	/** Tab: 0 Game, 1 Controls, 2 Camera, 3 Display, 4 Sound. */
	void Open(int32 Tab = 0);
	void Close();

	/** Set while another screen owns the menu key (the opening uses Esc to skip). F1 still opens the menu. */
	bool bKeyBusy = false;

	/** From the input pre-processor: true when the key was used by the menu. */
	bool HandleKey(const FKey& Key);
	bool HandleMouseKey(const FKey& Key);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool IsViewportActive() const;

	bool bOpen = false;
	bool bAddedIgnore = false;
	TSharedPtr<SGbSettingsMenu> Menu;
	TSharedPtr<SWidget> MenuRoot;
	TSharedPtr<SWidget> FpsRoot;
	TSharedPtr<IInputProcessor> Input;
};
