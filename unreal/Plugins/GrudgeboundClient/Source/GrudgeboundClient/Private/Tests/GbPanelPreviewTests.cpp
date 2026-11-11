// Renders the building and pocket panels filled with sample data (no server), over the market backdrop, at 1080p
// and 720p: Saved/PanelPreview/<height>/<panel>.png. Also checks the shared shell's behaviour.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "GbWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"

namespace GbPanelPreview
{
	FGbTalisman Talisman(const TCHAR* Id, const TCHAR* Species, const TCHAR* Omen, int32 Day, int32 Wins, const TCHAR* Token = TEXT(""), int32 Purse = 0)
	{
		FGbTalisman T; T.Id = Id; T.SpeciesId = Species; T.Omen = Omen; T.BoundDay = Day; T.Wins = Wins; T.ChainTokenId = Token; T.Purse = Purse;
		return T;
	}
	FGbCurseInfo Curse(const TCHAR* Name, const TCHAR* Emotion) { FGbCurseInfo C; C.Name = Name; C.Emotion = Emotion; return C; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbPanelPreviewTest, "Grudgebound.Ui.PanelPreviews", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbPanelPreviewTest::RunTest(const FString&)
{
	using namespace GbPanelPreview;
	FWidgetRenderer Renderer(false);
	UTexture2D* Backdrop = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Grudgebound/UI/Battle/backdrop.backdrop"));
	FSlateBrush BackdropBrush; BackdropBrush.SetResourceObject(Backdrop); BackdropBrush.ImageSize = FVector2D(1920, 1080);
	FAssetCompilingManager::Get().FinishAllCompilation();

	const auto Capture = [&](UGbWidgetBase* W, const TCHAR* Name)
	{
		for (float T = 0; T < 0.4f; T += 0.02f) W->NativeTick(FGeometry(), 0.02f);
		FAssetCompilingManager::Get().FinishAllCompilation();
		TArray<UWidget*> All; W->WidgetTree->GetAllWidgets(All);
		for (UWidget* Widget : All) if (UImage* Image = Cast<UImage>(Widget)) if (UTexture2D* Texture = Cast<UTexture2D>(Image->GetBrush().GetResourceObject())) Texture->WaitForPendingInitOrStreaming();
		if (Backdrop) Backdrop->WaitForPendingInitOrStreaming();
		FlushRenderingCommands();
		// The Ward stand-in behind the panel: the battle's market backdrop.
		const TSharedRef<SWidget> Panel = W->TakeWidget();
		for (const FIntPoint Size : { FIntPoint(1920, 1080), FIntPoint(1280, 720) })
		{
			// The engine's default DPI curve (shortest side 1080 -> 1.0, 720 -> 0.667), as the game applies it.
			const TSharedRef<SWidget> Scene = SNew(SOverlay) + SOverlay::Slot()[SNew(SImage).Image(&BackdropBrush)]
				+ SOverlay::Slot()[SNew(SDPIScaler).DPIScale(Size.Y / 1080.f)[Panel]];
			Renderer.DrawWidget(Scene, FVector2D(Size)); Scene->Invalidate(EInvalidateWidgetReason::Layout); Scene->SlatePrepass(1.f);
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(Scene, FVector2D(Size));
			if (!TestNotNull(TEXT("Panel renders"), Target)) continue;
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			TestTrue(TEXT("Panel pixels read back"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags));
			for (FColor& P : Pixels) P.A = 255;
			TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PanelPreview"), FString::FromInt(Size.Y)); IFileManager::Get().MakeDirectory(*Dir, true);
			TestTrue(TEXT("Preview saved"), FFileHelper::SaveArrayToFile(Png, *FPaths::Combine(Dir, FString(Name) + TEXT(".png"))));
		}
	};
	const auto Make = [](UClass* Class)
	{
		UGbWidgetBase* W = NewObject<UGbWidgetBase>(GetTransientPackage(), Class); W->AddToRoot(); W->Initialize(); W->NativeOnInitialized();
		W->TakeWidget(); W->SetVisibility(ESlateVisibility::Visible);
		return W;
	};

	// ---- Talisman Case: six talismans, the starter as partner.
	{
		UGbTalismanCaseWidget* W = Cast<UGbTalismanCaseWidget>(Make(UGbTalismanCaseWidget::StaticClass()));
		const TArray<FGbTalisman> Ts = {
			Talisman(TEXT("tal-starter"), TEXT("cinder-grin"), TEXT("whisper"), -1, 7),
			Talisman(TEXT("tal-1"), TEXT("hushmoth"), TEXT("wail"), 2, 3, TEXT("tal-12"), 5),
			Talisman(TEXT("tal-2"), TEXT("weepwick"), TEXT("whisper"), 3, 1, TEXT("tal-15")),
			Talisman(TEXT("tal-3"), TEXT("riot-bell"), TEXT("calamity"), 4, 0),
			Talisman(TEXT("tal-4"), TEXT("tallow-widow"), TEXT("wail"), 6, 2, TEXT("tal-21")),
			Talisman(TEXT("tal-5"), TEXT("lantern-drowned"), TEXT("whisper"), 7, 0) };
		const TMap<FString, FGbCurseInfo> Cs = {
			{ TEXT("cinder-grin"), Curse(TEXT("Cinder Grin"), TEXT("rage")) }, { TEXT("hushmoth"), Curse(TEXT("Hushmoth"), TEXT("dread")) },
			{ TEXT("weepwick"), Curse(TEXT("Weepwick"), TEXT("sorrow")) }, { TEXT("riot-bell"), Curse(TEXT("Riot Bell"), TEXT("rage")) },
			{ TEXT("tallow-widow"), Curse(TEXT("Tallow Widow"), TEXT("sorrow")) }, { TEXT("lantern-drowned"), Curse(TEXT("Lantern Drowned"), TEXT("dread")) } };
		W->Show(Ts, Cs, TEXT("tal-starter"), 4, 5, 1, 0, 42, 60, true);
		TestEqual(TEXT("Case lists every talisman"), W->List->GetChildrenCount(), 6);
		Capture(W, TEXT("TalismanCase"));
		W->RemoveFromRoot();
	}

	// ---- Mender: one talisman drained, the price known.
	{
		UGbMenderWidget* W = Cast<UGbMenderWidget>(Make(UGbMenderWidget::StaticClass()));
		FGbSatchel S; S.bValid = true; S.Carried = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15") }; S.Case = { TEXT("tal-21") }; S.Ember = 60;
		const TCHAR* Ids[] = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15"), TEXT("tal-21") };
		const int32 Hp[] = { 44, 12, 0, 40 };
		const TCHAR* Sp[] = { TEXT("cinder-grin"), TEXT("hushmoth"), TEXT("weepwick"), TEXT("tallow-widow") };
		const TCHAR* Nm[] = { TEXT("Cinder Grin"), TEXT("Hushmoth"), TEXT("Weepwick"), TEXT("Tallow Widow") };
		for (int32 I = 0; I < 4; ++I) { FGbInk Ink; Ink.Hp = Hp[I]; Ink.MaxHp = 44; S.Ink.Add(Ids[I], Ink); W->Names.Add(Ids[I], Nm[I]); W->Species.Add(Ids[I], Sp[I]); }
		W->Data = S; W->Price = 12; W->bBusy = false; W->Redraw();
		TestEqual(TEXT("Mender starts on the first drained talisman"), W->Selected, FString(TEXT("tal-12")));
		TestTrue(TEXT("Restore offered for a drained talisman the player can pay for"), W->RestoreButton->GetIsEnabled());
		W->Status->SetText(FText::FromString(TEXT("Hushmoth's ink is restored.")));
		Capture(W, TEXT("Mender"));
		W->Selected = TEXT("starter"); W->Redraw();
		TestFalse(TEXT("No restore for a full talisman"), W->RestoreButton->GetIsEnabled());
		W->RemoveFromRoot();
	}

	// ---- Your room.
	{
		UGbRoomWidget* W = Cast<UGbRoomWidget>(Make(UGbRoomWidget::StaticClass()));
		TestTrue(TEXT("Room's ink note hidden while empty"), W->ShellNote && !W->ShellNote->IsVisible());
		Capture(W, TEXT("Room"));
		W->RemoveFromRoot();
	}
	return true;
}
#endif
