// Renders the building and pocket panels filled with sample data (no server), over the market backdrop, at 1080p
// and 720p: Saved/PanelPreview/<height>/<panel>.png. Also checks the shared shell's behaviour.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "GbWidgets.h"
#include "GbSatchelWidget.h"
#include "GbVowWidget.h"
#include "GbObservatoryWidget.h"
#include "GbPanelFixtures.h"
#include "Components/WidgetSwitcher.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbPanelPreviewTest, "Grudgebound.Ui.PanelPreviews", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbPanelPreviewTest::RunTest(const FString&)
{
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
		FGbPanelFixtures::Fill(W, TEXT("TalismanCase"));
		TestEqual(TEXT("Case lists every talisman"), W->List->GetChildrenCount(), 6);
		Capture(W, TEXT("TalismanCase"));
		W->RemoveFromRoot();
	}

	// ---- Mender: one talisman drained, the price known.
	{
		UGbMenderWidget* W = Cast<UGbMenderWidget>(Make(UGbMenderWidget::StaticClass()));
		FGbPanelFixtures::Fill(W, TEXT("Mender"));
		TestEqual(TEXT("Mender starts on the first drained talisman"), W->Selected, FString(TEXT("tal-12")));
		TestTrue(TEXT("Restore offered for a drained talisman the player can pay for"), W->RestoreButton->GetIsEnabled());
		Capture(W, TEXT("Mender"));
		W->Selected = TEXT("starter"); W->Redraw();
		TestFalse(TEXT("No restore for a full talisman"), W->RestoreButton->GetIsEnabled());
		W->RemoveFromRoot();
	}

	// ---- Satchel: four carried, two in the Case, one drained; every pocket.
	{
		UGbSatchelWidget* W = Cast<UGbSatchelWidget>(Make(UGbSatchelWidget::StaticClass()));
		for (const TCHAR* Name : { TEXT("Satchel-Talismans"), TEXT("Satchel-Supplies"), TEXT("Satchel-Keepsakes"), TEXT("Satchel-Ledger") })
		{
			FGbPanelFixtures::Fill(W, Name); Capture(W, Name);
		}
		TestEqual(TEXT("Satchel lists carried and Case talismans"), W->Rows->GetChildrenCount(), 6);
		TestTrue(TEXT("A drained carried talisman can take a salve"), W->SalveButton->GetIsEnabled());
		W->RemoveFromRoot();
	}

	// ---- The Broker.
	{
		UGbShopWidget* W = Cast<UGbShopWidget>(Make(UGbShopWidget::StaticClass()));
		FGbPanelFixtures::Fill(W, TEXT("Broker"));
		TestEqual(TEXT("Broker lists every stock line"), W->Stock->GetChildrenCount(), 6);
		Capture(W, TEXT("Broker"));
		W->RemoveFromRoot();
	}

	// ---- The Rite.
	{
		UGbRulesWidget* W = Cast<UGbRulesWidget>(Make(UGbRulesWidget::StaticClass()));
		FGbPanelFixtures::Fill(W, TEXT("Rite"));
		TestEqual(TEXT("Rite shows a slip per rule in force"), W->ActiveSlips.Num(), 2);
		TestTrue(TEXT("Rite can propose with the ledger up"), W->ProposeButton->GetIsEnabled());
		TestTrue(TEXT("Repeal row shown for ledger rules"), W->RepealRow->GetVisibility() != ESlateVisibility::Collapsed);
		Capture(W, TEXT("Rite"));
		W->RemoveFromRoot();
	}

	// ---- The Vow Shrine (V3 being sworn).
	{
		UGbVowWidget* W = Cast<UGbVowWidget>(Make(UGbVowWidget::StaticClass()));
		FGbPanelFixtures::Fill(W, TEXT("Vows"));
		TestEqual(TEXT("Shrine shows a slip per vow"), W->Slips.Num(), 3);
		TestEqual(TEXT("Only the holding vow can be released"), W->ActiveIds.Num(), 1);
		TestTrue(TEXT("V3 offers the player's talismans"), W->ParamValues.Num() == 2 && W->ParamRow->GetVisibility() != ESlateVisibility::Collapsed);
		TestTrue(TEXT("Swear enabled with enough points"), W->Swear->GetIsEnabled());
		Capture(W, TEXT("Vows"));
		W->HandleNextTemplate();
		TestTrue(TEXT("V4 stakes 25 of 42 points"), W->Swear->GetIsEnabled());
		W->RemoveFromRoot();
	}

	// ---- The Observatory: every tab.
	{
		UGbObservatoryWidget* W = Cast<UGbObservatoryWidget>(Make(UGbObservatoryWidget::StaticClass()));
		for (const TCHAR* Name : { TEXT("Observatory-Grudge"), TEXT("Observatory-Prices"), TEXT("Observatory-Rules") })
		{
			FGbPanelFixtures::Fill(W, Name); Capture(W, Name);
		}
		TestEqual(TEXT("Observatory offers a button per Ward"), W->WardButtons.Num(), 4);
		W->Stop();
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
