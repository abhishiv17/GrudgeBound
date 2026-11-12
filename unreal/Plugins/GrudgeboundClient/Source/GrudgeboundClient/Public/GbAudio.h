// Every sound in the game: footsteps, panel and battle effects and the night-market loop.
// The sounds are synthesised by unreal/tools/make-audio.py and imported to /Game/Grudgebound/Audio.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "GbAudio.generated.h"

class UAudioComponent;
class USoundBase;
class AGbUiDirector;

/** What the player is looking at; it decides how loud the market is and which effects play on a change. */
enum class EGbAudioScene : uint8 { None, Title, Street, Indoors, Panel, Battle };

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbAudio : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UGbAudio* Get(const UObject* Context);
	/** Plays a sound by its file name ("hit_1"). Safe anywhere: silent with no game instance or no such sound. */
	static void Play(const UObject* Context, FName Sound, float Volume = 1.f, float Pitch = 1.f);
	/** The imported sound, or null. */
	USoundBase* Find(FName Sound);
	/** Every sound the game plays, for the import check. */
	static const TArray<FName>& Names();
	/** Which footstep a walker makes. Pure, so it can be checked without a world. */
	static FName StepSound(EGbAudioScene Scene, bool bInGrass, int32 Step);
	/** How loud the market loop is in a scene, before the player's own volume. */
	static float AmbienceLevel(EGbAudioScene Scene);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGbAudio, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

private:
	void PlayNow(FName Sound, float Volume, float Pitch);
	UPROPERTY() TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
	TWeakObjectPtr<AGbUiDirector> Director;
	EGbAudioScene Scene = EGbAudioScene::None;
	float AmbienceNow = 0.f;
	float Strode = 0.f;
	int32 Steps = 0;
};
