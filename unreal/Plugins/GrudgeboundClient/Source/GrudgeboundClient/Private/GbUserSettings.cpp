#include "GbUserSettings.h"

#include "AudioDevice.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"

UGbUserSettings* UGbUserSettings::Get()
{
	return GetMutableDefault<UGbUserSettings>();
}

const TArray<FGbActionDef>& UGbUserSettings::Actions()
{
	static const TArray<FGbActionDef> List = {
		{ TEXT("MoveUp"), TEXT("Walk up"), TEXT("Movement"), EKeys::W },
		{ TEXT("MoveDown"), TEXT("Walk down"), TEXT("Movement"), EKeys::S },
		{ TEXT("MoveLeft"), TEXT("Walk left"), TEXT("Movement"), EKeys::A },
		{ TEXT("MoveRight"), TEXT("Walk right"), TEXT("Movement"), EKeys::D },
		{ TEXT("Interact"), TEXT("Interact / enter"), TEXT("Movement"), EKeys::E },

		{ TEXT("RotateLeft"), TEXT("Rotate camera left"), TEXT("Camera"), EKeys::Q },
		{ TEXT("RotateRight"), TEXT("Rotate camera right"), TEXT("Camera"), EKeys::C },
		{ TEXT("RotateHold"), TEXT("Hold and drag to orbit / tilt"), TEXT("Camera"), EKeys::MiddleMouseButton },
		{ TEXT("ZoomIn"), TEXT("Zoom in"), TEXT("Camera"), EKeys::MouseScrollUp },
		{ TEXT("ZoomOut"), TEXT("Zoom out"), TEXT("Camera"), EKeys::MouseScrollDown },
		{ TEXT("ResetCamera"), TEXT("Reset camera"), TEXT("Camera"), EKeys::Home },

		{ TEXT("Satchel"), TEXT("Satchel"), TEXT("Panels"), EKeys::B },
		{ TEXT("Case"), TEXT("Talisman Case"), TEXT("Panels"), EKeys::T },
		{ TEXT("Rite"), TEXT("The Rite"), TEXT("Panels"), EKeys::R },
		{ TEXT("Vows"), TEXT("Vow Shrine"), TEXT("Panels"), EKeys::V },
		{ TEXT("Observatory"), TEXT("Observatory"), TEXT("Panels"), EKeys::O },
		{ TEXT("Story"), TEXT("Replay the opening"), TEXT("Panels"), EKeys::I },
		{ TEXT("Proof"), TEXT("Open last ledger proof"), TEXT("Panels"), EKeys::L },

		{ TEXT("Settings"), TEXT("Menu / settings"), TEXT("System"), EKeys::Escape },
		{ TEXT("SignOut"), TEXT("Sign out"), TEXT("System"), EKeys::F10 },
	};
	return List;
}

FKey UGbUserSettings::KeyFor(FName Action) const
{
	if (const FName* Override = KeyOverrides.Find(Action))
	{
		const FKey Key(*Override);
		if (Key.IsValid() || *Override == NAME_None) return Key;
	}
	for (const FGbActionDef& Def : Actions()) if (Def.Id == Action) return Def.Default;
	return EKeys::Invalid;
}

FString UGbUserSettings::KeyName(const FKey& Key)
{
	if (!Key.IsValid()) return TEXT("Unbound");
	if (Key == EKeys::Escape) return TEXT("Esc");
	if (Key == EKeys::LeftMouseButton) return TEXT("Left Mouse");
	if (Key == EKeys::RightMouseButton) return TEXT("Right Mouse");
	if (Key == EKeys::MiddleMouseButton) return TEXT("Middle Mouse");
	if (Key == EKeys::MouseScrollUp) return TEXT("Wheel Up");
	if (Key == EKeys::MouseScrollDown) return TEXT("Wheel Down");
	if (Key == EKeys::SpaceBar) return TEXT("Space");
	return Key.GetDisplayName(false).ToString();
}

FString UGbUserSettings::KeyLabel(FName Action) const
{
	return KeyName(KeyFor(Action));
}

bool UGbUserSettings::IsDown(const APlayerController* PC, FName Action) const
{
	const FKey Key = KeyFor(Action);
	return PC && Key.IsValid() && PC->IsInputKeyDown(Key);
}

bool UGbUserSettings::WasPressed(const APlayerController* PC, FName Action) const
{
	const FKey Key = KeyFor(Action);
	return PC && Key.IsValid() && PC->WasInputKeyJustPressed(Key);
}

FName UGbUserSettings::SetKey(FName Action, const FKey& Key)
{
	const FKey Old = KeyFor(Action);
	if (Old == Key) return NAME_None;
	FName Swapped = NAME_None;
	for (const FGbActionDef& Def : Actions())
	{
		if (Def.Id != Action && KeyFor(Def.Id) == Key) { Swapped = Def.Id; break; }
	}
	const auto Store = [this](FName Id, const FKey& Value)
	{
		for (const FGbActionDef& Def : Actions())
		{
			if (Def.Id != Id) continue;
			if (Def.Default == Value) KeyOverrides.Remove(Id); else KeyOverrides.Add(Id, Value.GetFName());
		}
	};
	Store(Action, Key);
	if (Swapped != NAME_None) Store(Swapped, Old);
	Save();
	return Swapped;
}

void UGbUserSettings::ResetControls()
{
	KeyOverrides.Reset();
	Save();
}

void UGbUserSettings::ResetCamera()
{
	CameraDistance = DefaultDistance; CameraPitch = DefaultPitch; CameraRotateSpeed = DefaultRotateSpeed;
	MouseRotateSensitivity = 1.f; bInvertMouseRotate = false; bSmoothCamera = true;
	Save();
}

void UGbUserSettings::ResetSound()
{
	MasterVolume = 1.f; bMuteInBackground = true;
	Save();
}

void UGbUserSettings::Save()
{
	SaveConfig();
	OnChanged.Broadcast();
}

void UGbUserSettings::ApplySound(UWorld* World) const
{
	FApp::SetUnfocusedVolumeMultiplier(bMuteInBackground ? 0.f : 1.f);
	if (!World) return;
	if (FAudioDeviceHandle Device = World->GetAudioDevice()) Device->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.f, 1.f));
}
