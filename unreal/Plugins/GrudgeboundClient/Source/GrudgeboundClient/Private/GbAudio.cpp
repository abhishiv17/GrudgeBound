#include "GbAudio.h"

#include "GbTallGrassComponent.h"
#include "GbUiDirector.h"
#include "GbUserSettings.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	// One stride of the paper doll.
	constexpr float StrideCm = 95.f;
}

UGbAudio* UGbAudio::Get(const UObject* Context)
{
	const UWorld* World = Context ? Context->GetWorld() : nullptr;
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	return Game ? Game->GetSubsystem<UGbAudio>() : nullptr;
}

void UGbAudio::Play(const UObject* Context, FName Sound, float Volume, float Pitch)
{
	if (UGbAudio* Audio = Get(Context)) Audio->PlayNow(Sound, Volume, Pitch);
}

const TArray<FName>& UGbAudio::Names()
{
	static const TArray<FName> All = {
		TEXT("step_stone_1"), TEXT("step_stone_2"), TEXT("step_stone_3"),
		TEXT("step_wood_1"), TEXT("step_wood_2"), TEXT("step_wood_3"),
		TEXT("step_grass_1"), TEXT("step_grass_2"),
		TEXT("ui_click"), TEXT("ui_open"), TEXT("ui_close"), TEXT("door"), TEXT("talk"),
		TEXT("whoosh"), TEXT("hit_1"), TEXT("hit_2"), TEXT("ward"), TEXT("heal"),
		TEXT("seal_stamp"), TEXT("seal_break"), TEXT("bound"), TEXT("exorcised"), TEXT("lost"), TEXT("encounter"),
		TEXT("market_loop"),
	};
	return All;
}

USoundBase* UGbAudio::Find(FName Sound)
{
	if (const TObjectPtr<USoundBase>* Known = Sounds.Find(Sound)) return Known->Get();
	const FString Name = Sound.ToString();
	USoundBase* Loaded = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Grudgebound/Audio/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn);
	Sounds.Add(Sound, Loaded);
	return Loaded;
}

void UGbAudio::PlayNow(FName Sound, float Volume, float Pitch)
{
	UWorld* World = GetWorld();
	USoundBase* Wave = Find(Sound);
	if (!World || !Wave) return;
	const float Effects = FMath::Clamp(UGbUserSettings::Get()->EffectsVolume, 0.f, 1.f);
	if (Volume * Effects > 0.001f) UGameplayStatics::PlaySound2D(World, Wave, Volume * Effects, Pitch);
}

FName UGbAudio::StepSound(EGbAudioScene Scene, bool bInGrass, int32 Step)
{
	if (bInGrass) return Step % 2 ? TEXT("step_grass_2") : TEXT("step_grass_1");
	static const FName Stone[] = { TEXT("step_stone_1"), TEXT("step_stone_2"), TEXT("step_stone_3") };
	static const FName Wood[] = { TEXT("step_wood_1"), TEXT("step_wood_2"), TEXT("step_wood_3") };
	// 0 1 2 1: never the same step twice in a row.
	const int32 Pick = Step % 4 == 3 ? 1 : Step % 4;
	return Scene == EGbAudioScene::Indoors ? Wood[Pick] : Stone[Pick];
}

float UGbAudio::AmbienceLevel(EGbAudioScene Scene)
{
	switch (Scene)
	{
	case EGbAudioScene::Title: return 0.9f;
	case EGbAudioScene::Street: return 0.8f;
	case EGbAudioScene::Panel: return 0.5f;
	case EGbAudioScene::Indoors: return 0.35f; // through the wall
	case EGbAudioScene::Battle: return 0.25f;
	default: return 0.f;
	}
}

void UGbAudio::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !World->HasBegunPlay()) return;

	// Only the Wards have a director; the co-op level and menus stay quiet.
	if (!Director.IsValid() || Director->GetWorld() != World)
	{
		Director.Reset();
		for (TActorIterator<AGbUiDirector> It(World); It; ++It) { Director = *It; break; }
	}
	const EGbAudioScene Now = Director.IsValid() ? Director->GetAudioScene() : EGbAudioScene::None;
	if (Now != Scene)
	{
		const bool bWasWalking = Scene == EGbAudioScene::Street || Scene == EGbAudioScene::Indoors;
		const bool bWalking = Now == EGbAudioScene::Street || Now == EGbAudioScene::Indoors;
		if (bWasWalking && bWalking) PlayNow(TEXT("door"), 0.8f, 1.f);
		else if (Now == EGbAudioScene::Battle && bWasWalking) PlayNow(TEXT("encounter"), 0.9f, 1.f);
		else if (Now == EGbAudioScene::Panel && bWasWalking) PlayNow(TEXT("ui_open"), 0.8f, 1.f);
		else if (Scene == EGbAudioScene::Panel && bWalking) PlayNow(TEXT("ui_close"), 0.8f, 1.f);
		Scene = Now;
		Strode = 0.f;
	}

	// The market: one loop, turned up and down by where the player is.
	const float Wanted = AmbienceLevel(Scene) * FMath::Clamp(UGbUserSettings::Get()->AmbienceVolume, 0.f, 1.f);
	if (!IsValid(Ambience) || Ambience->GetWorld() != World)
	{
		Ambience = nullptr;
		AmbienceNow = 0.f;
		if (Wanted > 0.f)
			if (USoundBase* Loop = Find(TEXT("market_loop")))
			{
				Ambience = UGameplayStatics::CreateSound2D(World, Loop, 1.f, 1.f, 0.f, nullptr, false, false);
				if (Ambience) { Ambience->bIsUISound = true; Ambience->SetVolumeMultiplier(0.001f); Ambience->Play(); }
			}
	}
	if (Ambience)
	{
		AmbienceNow = FMath::FInterpConstantTo(AmbienceNow, Wanted, DeltaTime, 0.9f);
		Ambience->SetVolumeMultiplier(FMath::Max(AmbienceNow, 0.001f));
	}

	// Footsteps, one per stride, from how far the pawn really moved.
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || (Scene != EGbAudioScene::Street && Scene != EGbAudioScene::Indoors) || Director->IsCrossing()) return;
	const UPawnMovementComponent* Movement = Pawn->GetMovementComponent();
	const float Speed = Pawn->GetVelocity().Size2D();
	if (Speed < 40.f || (Movement && Movement->IsFalling())) { Strode = FMath::Min(Strode, StrideCm * 0.5f); return; }
	Strode += Speed * DeltaTime;
	if (Strode < StrideCm) return;
	Strode = FMath::Fmod(Strode, StrideCm);
	bool bInGrass = false;
	TArray<UPrimitiveComponent*> Touching;
	Pawn->GetOverlappingComponents(Touching);
	for (const UPrimitiveComponent* Component : Touching) if (Component && Component->IsA<UGbTallGrassComponent>()) { bInGrass = true; break; }
	PlayNow(StepSound(Scene, bInGrass, Steps++), bInGrass ? 0.5f : 0.38f, FMath::FRandRange(0.93f, 1.07f));
}
