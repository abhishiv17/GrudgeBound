// Sound and street talk: every sound is imported, footsteps follow the ground, every walker has something to say
// in every mood and chapter, and a walker asked a question stands still. Also renders the speech panel to
// Saved/PanelPreview/<height>/talk.png.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "GbAudio.h"
#include "GbTalk.h"
#include "GbTownsfolk.h"
#include "GbUserSettings.h"
#include "AssetCompilingManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Sound/SoundWave.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbAudioTest, "Grudgebound.Audio.Sounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbAudioTest::RunTest(const FString&)
{
	for (const FName Name : UGbAudio::Names())
	{
		const FString Text = Name.ToString();
		const USoundWave* Wave = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/Grudgebound/Audio/%s.%s"), *Text, *Text));
		if (!TestNotNull(*FString::Printf(TEXT("%s is imported"), *Text), Wave)) continue;
		TestEqual(*FString::Printf(TEXT("%s loops only if it is a loop"), *Text), Wave->bLooping != 0, Text.EndsWith(TEXT("_loop")));
		TestTrue(*FString::Printf(TEXT("%s has a length"), *Text), Wave->Duration > 0.02f);
	}
	TestEqual(TEXT("Stone outdoors"), UGbAudio::StepSound(EGbAudioScene::Street, false, 0), FName(TEXT("step_stone_1")));
	TestEqual(TEXT("Boards indoors"), UGbAudio::StepSound(EGbAudioScene::Indoors, false, 2), FName(TEXT("step_wood_3")));
	TestEqual(TEXT("Grass wins over the street"), UGbAudio::StepSound(EGbAudioScene::Street, true, 1), FName(TEXT("step_grass_2")));
	for (int32 Step = 0; Step < 12; ++Step)
		TestNotEqual(TEXT("No footstep twice in a row"), UGbAudio::StepSound(EGbAudioScene::Street, false, Step), UGbAudio::StepSound(EGbAudioScene::Street, false, Step + 1));
	TestTrue(TEXT("The market is loudest in the street"), UGbAudio::AmbienceLevel(EGbAudioScene::Street) > UGbAudio::AmbienceLevel(EGbAudioScene::Panel)
		&& UGbAudio::AmbienceLevel(EGbAudioScene::Panel) > UGbAudio::AmbienceLevel(EGbAudioScene::Indoors)
		&& UGbAudio::AmbienceLevel(EGbAudioScene::Indoors) > UGbAudio::AmbienceLevel(EGbAudioScene::Battle));
	TestEqual(TEXT("Silent outside the Wards"), UGbAudio::AmbienceLevel(EGbAudioScene::None), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbTalkTest, "Grudgebound.Ward.Talk", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbTalkTest::RunTest(const FString&)
{
	// Everyone has a trade and a line for every mood, chapter and turn, short enough for the panel.
	const TPair<const TCHAR*, int32> Wards[] = { { TEXT("lantern"), 8 }, { TEXT("tide"), 5 } };
	for (const auto& Ward : Wards)
	{
		TSet<FString> Roles;
		for (int32 Index = 0; Index < Ward.Value; ++Index)
		{
			const FString Role = GbTalk::Role(Ward.Key, Index);
			TestFalse(TEXT("A walker has a trade"), Role.IsEmpty());
			Roles.Add(Role);
			for (int32 Mood = 0; Mood < 4; ++Mood) for (int32 Chapter = 1; Chapter <= 3; ++Chapter) for (int32 Turn = 0; Turn < 6; ++Turn)
			{
				const FString Line = GbTalk::Line(Ward.Key, Index, static_cast<GbTalk::EMood>(Mood), Chapter, Turn);
				if (Line.IsEmpty() || Line.Len() > 110) AddError(FString::Printf(TEXT("%s %d: bad line '%s'"), Ward.Key, Index, *Line));
			}
			TestNotEqual(TEXT("Asking again gets something new"), GbTalk::Line(Ward.Key, Index, GbTalk::EMood::Calm, 1, 0), GbTalk::Line(Ward.Key, Index, GbTalk::EMood::Calm, 1, 1));
		}
		TestEqual(TEXT("No two walkers of a Ward share a trade"), Roles.Num(), Ward.Value);
	}
	TestNotEqual(TEXT("The mood changes what is said"), GbTalk::Line(TEXT("lantern"), 0, GbTalk::EMood::Rage, 1, 1), GbTalk::Line(TEXT("lantern"), 0, GbTalk::EMood::Sorrow, 1, 1));
	TestNotEqual(TEXT("The chapter changes what is said"), GbTalk::Line(TEXT("lantern"), 0, GbTalk::EMood::Calm, 1, 2), GbTalk::Line(TEXT("lantern"), 0, GbTalk::EMood::Calm, 3, 2));

	FGbWardSummary Ward; Ward.Total = 12.f; Ward.Grudge.Rage = 8.f;
	TestTrue(TEXT("A low Grudge is a calm street"), GbTalk::Mood(Ward) == GbTalk::EMood::Calm);
	Ward.Total = 60.f; Ward.Grudge.Rage = 10.f; Ward.Grudge.Sorrow = 35.f; Ward.Grudge.Dread = 15.f;
	TestTrue(TEXT("The loudest feeling sets the mood"), GbTalk::Mood(Ward) == GbTalk::EMood::Sorrow);
	Ward.Grudge.Dread = 40.f;
	TestTrue(TEXT("Dread can lead"), GbTalk::Mood(Ward) == GbTalk::EMood::Dread);

	// A walker who is asked something stops, turns to the player and walks on when released.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AGbTownsfolk* Walker = World->SpawnActor<AGbTownsfolk>();
	Walker->Setup({ FVector::ZeroVector, FVector(800, 0, 0) }, 5);
	Walker->SetVoice(TEXT("tide"), 2);
	TestEqual(TEXT("The voice is kept"), GbTalk::Role(Walker->GetWardId(), Walker->GetVoice()), FString(TEXT("Deckhand")));
	Walker->Travelled = 0.f; Walker->Pause = 0.f; Walker->SetActorLocation(FVector::ZeroVector);
	for (int32 I = 0; I < 30; ++I) Walker->Tick(1.f / 30.f);
	const FVector Before = Walker->GetActorLocation();
	TestTrue(TEXT("Walks when left alone"), Before.X > 10.f);
	ACharacter* Player = World->SpawnActor<ACharacter>();
	Player->SetActorLocation(Before + FVector(0, 150, 0));
	Walker->Hold(2.f, Player);
	for (int32 I = 0; I < 30; ++I) Walker->Tick(1.f / 30.f);
	TestEqual(TEXT("Stands still while talking"), Walker->GetActorLocation(), Before);
	TestTrue(TEXT("Turns to the player"), FMath::Abs(FRotator::NormalizeAxis(Walker->GetActorRotation().Yaw - 90.f)) < 10.f);
	TestEqual(TEXT("Turns are counted"), Walker->NextTurn() + Walker->NextTurn(), 1);
	Walker->Release();
	Player->SetActorLocation(Before + FVector(-300, 600, 0));
	for (int32 I = 0; I < 60; ++I) Walker->Tick(1.f / 30.f);
	TestTrue(TEXT("Walks on afterwards"), Walker->GetActorLocation().X > Before.X + 10.f);
	Walker->ShowPrompt(true);
	TestTrue(TEXT("The prompt names the key"), Walker->Prompt->Text.ToString().Contains(UGbUserSettings::Get()->KeyLabel(TEXT("Interact"))));

	// The speech panel, over the market backdrop.
	UGbTalkWidget* Panel = NewObject<UGbTalkWidget>(); Panel->AddToRoot(); Panel->Initialize();
	ON_SCOPE_EXIT { Panel->RemoveFromRoot(); };
	if (!TestNotNull(TEXT("The panel builds"), Panel->WidgetTree->RootWidget.Get())) return false;
	const FString Longest = GbTalk::Line(TEXT("lantern"), 6, GbTalk::EMood::Calm, 1, 3);
	Panel->Say(GbTalk::Role(TEXT("lantern"), 6), Longest);
	TestEqual(TEXT("The panel shows the line"), Panel->Line->GetText().ToString(), Longest);
	TestTrue(TEXT("A line stays long enough to read"), GbTalk::ReadingTime(Longest) >= 4.f && GbTalk::ReadingTime(Longest) <= 9.f);
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	for (float T = 0; T < 0.4f; T += 0.02f) Panel->NativeTick(FGeometry(), 0.02f);
	UTexture2D* Backdrop = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Grudgebound/UI/Battle/backdrop.backdrop"));
	FSlateBrush BackdropBrush; BackdropBrush.SetResourceObject(Backdrop); BackdropBrush.ImageSize = FVector2D(1920, 1080);
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (Backdrop) Backdrop->WaitForPendingInitOrStreaming();
	FlushRenderingCommands();
	FWidgetRenderer Renderer(false);
	for (const FIntPoint Size : { FIntPoint(1920, 1080), FIntPoint(1280, 720) })
	{
		const TSharedRef<SWidget> Scene = SNew(SOverlay) + SOverlay::Slot()[SNew(SImage).Image(&BackdropBrush)]
			+ SOverlay::Slot()[SNew(SDPIScaler).DPIScale(Size.Y / 1080.f)[Slate]];
		Renderer.DrawWidget(Scene, FVector2D(Size)); Scene->Invalidate(EInvalidateWidgetReason::Layout); Scene->SlatePrepass(1.f);
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(Scene, FVector2D(Size));
		if (!TestNotNull(TEXT("The panel renders"), Target)) continue;
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		TestTrue(TEXT("Pixels read back"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags));
		for (FColor& P : Pixels) P.A = 255;
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PanelPreview"), FString::FromInt(Size.Y)); IFileManager::Get().MakeDirectory(*Dir, true);
		TestTrue(TEXT("Preview saved"), FFileHelper::SaveArrayToFile(Png, *FPaths::Combine(Dir, TEXT("talk.png"))));
	}
	return true;
}
#endif
