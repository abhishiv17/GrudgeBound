// Renders the building and pocket panels filled with sample data (no server), over the market backdrop, at 1080p
// and 720p: Saved/PanelPreview/<height>/<panel>.png. Also checks the shared shell's behaviour.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "GbWidgets.h"
#include "GbSatchelWidget.h"
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

	// ---- Satchel: four carried, two in the Case, one drained; every pocket.
	{
		UGbSatchelWidget* W = Cast<UGbSatchelWidget>(Make(UGbSatchelWidget::StaticClass()));
		FGbSatchel S; S.bValid = true; S.Carried = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15"), TEXT("tal-21") }; S.Case = { TEXT("tal-30"), TEXT("tal-31") };
		S.Blank = 3; S.Gilded = 1; S.Salves = 2; S.Ember = 60; S.Points = 42; S.Keepsakes = { TEXT("Keepsake one"), TEXT("Keepsake two") };
		const TCHAR* Ids[] = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15"), TEXT("tal-21"), TEXT("tal-30"), TEXT("tal-31") };
		const TCHAR* Sp[] = { TEXT("cinder-grin"), TEXT("hushmoth"), TEXT("weepwick"), TEXT("tallow-widow"), TEXT("riot-bell"), TEXT("lantern-drowned") };
		const TCHAR* Nm[] = { TEXT("Cinder Grin"), TEXT("Hushmoth"), TEXT("Weepwick"), TEXT("Tallow Widow"), TEXT("Riot Bell"), TEXT("Lantern Drowned") };
		const int32 Hp[] = { 44, 30, 0, 44, 44, 18 };
		for (int32 I = 0; I < 6; ++I)
		{
			FGbInk Ink; Ink.Hp = Hp[I]; Ink.MaxHp = 44; S.Ink.Add(Ids[I], Ink); W->Names.Add(Ids[I], Nm[I]);
			FGbTalisman T = Talisman(I == 0 ? TEXT("tal-starter") : Ids[I], Sp[I], TEXT("whisper"), I, 0, I == 0 ? TEXT("") : Ids[I]);
			W->Talismans.Add(T);
		}
		W->Data = S; W->PartnerToken = TEXT("starter"); W->SelectedToken = TEXT("tal-15"); W->bBusy = false;
		const TCHAR* Names[] = { TEXT("Satchel-Talismans"), TEXT("Satchel-Supplies"), TEXT("Satchel-Keepsakes"), TEXT("Satchel-Ledger") };
		for (int32 I = 0; I < 4; ++I) { W->Pockets->SetActiveWidgetIndex(I); W->Redraw(); Capture(W, Names[I]); }
		TestEqual(TEXT("Satchel lists carried and Case talismans"), W->Rows->GetChildrenCount(), 6);
		TestTrue(TEXT("A drained carried talisman can take a salve"), W->SalveButton->GetIsEnabled());
		W->RemoveFromRoot();
	}

	// ---- The Broker: the Lantern Ward stock (shared/src/engine.ts), 60 Ember, a trade waiting on the ledger.
	{
		UGbShopWidget* W = Cast<UGbShopWidget>(Make(UGbShopWidget::StaticClass()));
		FGbShop Shop; Shop.MerchantId = UGbShopWidget::MerchantId; Shop.MerchantName = TEXT("the Talisman Broker"); Shop.Ember = 60;
		const auto Item = [&Shop](const TCHAR* Id, const TCHAR* Name, int32 Price, int32 Stock, const TCHAR* Text)
		{
			FGbShopItem It; It.Id = Id; It.Name = Name; It.Price = Price; It.Stock = Stock; It.Text = Text; It.BuyBack = Price / 2; Shop.Items.Add(It);
		};
		Item(TEXT("talisman-rage"), TEXT("Rage talismans"), 34, 6, TEXT("Riot season; rage sells."));
		Item(TEXT("talisman-sorrow"), TEXT("Sorrow talismans"), 28, 5, TEXT("Quiet week by the canal."));
		Item(TEXT("talisman-dread"), TEXT("Dread talismans"), 30, 6, TEXT("Steady."));
		Item(TEXT("blank-talisman"), TEXT("Blank talismans"), 15, 10, TEXT("Paper and ink."));
		Item(TEXT("gilded-talisman"), TEXT("Gilded talismans"), 30, 0, TEXT("Sold out until the gilder returns."));
		Item(TEXT("ink-salve"), TEXT("Ink Salves"), 10, 12, TEXT("Not for players yet."));
		W->Data = Shop;
		W->Redraw();
		W->Status->SetText(FText::FromString(TEXT("You bought a blank talisman for 15 Ember. Sealing it in the ledger...")));
		TestEqual(TEXT("Broker lists every stock line"), W->Stock->GetChildrenCount(), 6);
		Capture(W, TEXT("Broker"));
		W->RemoveFromRoot();
	}

	// ---- The Rite: two rules in force on the ledger, R1 being built (shared/src/rules.ts), Kessa answering.
	{
		UGbRulesWidget* W = Cast<UGbRulesWidget>(Make(UGbRulesWidget::StaticClass()));
		FGbRulesOverview Rules; Rules.bChain = true; Rules.Cost = 20; Rules.Points = 42;
		const auto Param = [](const TCHAR* Key, std::initializer_list<const TCHAR*> Labels)
		{
			FGbRuleParam P; P.Key = Key;
			for (const TCHAR* L : Labels) { FGbRuleOption O; O.Value = L; O.Label = L; P.Options.Add(O); }
			return P;
		};
		FGbRuleTemplate R1; R1.Id = TEXT("R1"); R1.Text = TEXT("{action} in {ward} gives x{multiplier} points");
		R1.Params = { Param(TEXT("action"), { TEXT("binding"), TEXT("exorcising") }), Param(TEXT("ward"), { TEXT("Lantern Ward"), TEXT("Tide Ward") }),
			Param(TEXT("multiplier"), { TEXT("1.5"), TEXT("2") }) };
		FGbRuleTemplate R8; R8.Id = TEXT("R8"); R8.Text = TEXT("No PvP in {ward}"); R8.Params = { Param(TEXT("ward"), { TEXT("Lantern Ward") }) };
		Rules.Templates = { R1, R8 };
		const auto Active = [&Rules](int32 Id, const TCHAR* Text, const TCHAR* Author, int32 Cost)
		{
			FGbActiveRule R; R.Id = Id; R.ChainId = Id - 1; R.Text = Text; R.Author = Author; R.Cost = Cost; R.RepealCost = Cost * 2; Rules.Active.Add(R);
		};
		Active(1, TEXT("No PvP in Lantern Ward"), TEXT("fen.grudgebound.testnet"), 20);
		Active(2, TEXT("Fleeing a battle costs 5 points"), TEXT("player.testnet"), 20);
		W->HandleRules(Rules);
		W->Status->SetText(FText::FromString(TEXT("Kessa is reading your rule...")));
		TestEqual(TEXT("Rite shows a slip per rule in force"), W->ActiveSlips.Num(), 2);
		TestTrue(TEXT("Rite can propose with the ledger up"), W->ProposeButton->GetIsEnabled());
		TestTrue(TEXT("Repeal row shown for ledger rules"), W->RepealRow->GetVisibility() != ESlateVisibility::Collapsed);
		Capture(W, TEXT("Rite"));
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
