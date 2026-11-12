#include "GbIntroWidget.h"
#include "GbStory.h"
#include "GbGreyboxMaterials.h"
#include "GbUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
	const FLinearColor& Paper=GbGreybox::Color::Paper; const FLinearColor& Ink=GbGreybox::Color::Ink; const FLinearColor& Gold=GbGreybox::Color::Gold;
	const FLinearColor& Crimson=GbGreybox::Color::Crimson; const FLinearColor& Sorrow=GbGreybox::Color::Sorrow; const FLinearColor& Dread=GbGreybox::Color::Dread;
}

// The four story pictures, drawn on a 150 x 103 grid of art pixels (4 screen pixels each at 1080p) so they sit
// with the game's sprites, which are placed on the same grid (K = art pixels per sprite pixel).
using FGbSpriteMap = TMap<FName, FSlateBrush>;

class SGbStoryArt : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGbStoryArt) : _Panel(0) {}
		SLATE_ARGUMENT(int32, Panel)
		SLATE_ARGUMENT(FGbSpriteMap, Sprites)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args) { PanelIndex = Args._Panel; Sprites = Args._Sprites; }
	void SetPanel(int32 Index) { PanelIndex = Index; Invalidate(EInvalidateWidgetReason::Paint); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(600, 412); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2f G(Size.X / 150.f, Size.Y / 103.f);
		int32 Layer = LayerId;
		const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FLinearColor Tint = Style.GetColorAndOpacityTint();
		const auto Cell = [&](float X, float Y, float W, float H, const FLinearColor& Color)
		{
			FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2f(W * G.X, H * G.Y),
				FSlateLayoutTransform(FVector2f(X * G.X, Y * G.Y))), White, ESlateDrawEffect::None, Color * Tint);
		};
		const auto Sprite = [&](const TCHAR* Name, float X, float Y, float K)
		{
			const FSlateBrush* Brush = Sprites.Find(FName(Name));
			if (!Brush) return;
			FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2f(Brush->ImageSize.X * K * G.X, Brush->ImageSize.Y * K * G.Y),
				FSlateLayoutTransform(FVector2f(X * G.X, Y * G.Y))), Brush, ESlateDrawEffect::None, Tint);
		};
		const auto H = [](const TCHAR* Hex) { return GbUi::Hex(Hex); };
		const auto A = [](const FLinearColor& Color, float Alpha) { return Color.CopyWithNewOpacity(Alpha); };
		const FLinearColor Slip = H(TEXT("E9DBBB")), Fold = H(TEXT("D6C29A")), Black = H(TEXT("05040F")), Lamp = GbGreybox::Color::Lantern;
		// The debt notice: a folded slip with a crimson heading and a few ruled lines.
		const auto Notice = [&](float X, float Y, float W, float Ht)
		{
			Cell(X + 2, Y + 2, W, Ht, A(Black, 0.4f)); Cell(X - 1, Y - 1, W + 2, Ht + 2, Black); Cell(X, Y, W, Ht, Slip);
			Cell(X + 3, Y + 3, FMath::RoundToFloat(W * 0.55f), 2, Crimson);
			for (int32 I = 0; I < 3 && 8 + I * 3 < Ht - 2; ++I) Cell(X + 3, Y + 8 + I * 3, W - 6 - (I % 2) * 5, 1, A(Ink, 0.55f));
		};
		// A string of lit lanterns sagging between two ends.
		const auto Lanterns = [&](float Y, float Sag)
		{
			for (int32 X = 0; X < 150; ++X) { const float T = (X - 75) / 75.f; Cell(X, FMath::RoundToFloat(Y + Sag * (1.f - T * T)), 1, 1, A(Fold, 0.5f)); }
			for (int32 I = 0; I < 12; ++I)
			{
				const float X = 6 + I * 12, T = (X + 1 - 75) / 75.f, Top = FMath::RoundToFloat(Y + Sag * (1.f - T * T)) + 1;
				Cell(X - 1, Top, 5, 6, A(Lamp, 0.14f)); Cell(X, Top, 3, 4, I % 2 ? Crimson : Gold); Cell(X + 1, Top + 1, 1, 2, Lamp);
			}
		};

		if (PanelIndex == 0)
		{
			// Arrival: Hollowmere's harbour by night, a rowing boat, and the notice that brought you.
			Cell(0, 0, 150, 20, H(TEXT("0E0B26"))); Cell(0, 20, 150, 14, H(TEXT("1A1238"))); Cell(0, 34, 150, 12, H(TEXT("2C1B45"))); Cell(0, 46, 150, 14, H(TEXT("46223F")));
			for (int32 I = 0; I < 26; ++I) Cell((I * I * 13 + I * 41 + 9) % 150, (I * I * 7 + I * 17 + 3) % 32, 1, 1, A(Paper, I % 3 ? 0.45f : 0.9f));
			Cell(126, 15, 8, 12, Slip); Cell(124, 17, 12, 8, Slip); Cell(128, 18, 2, 2, Fold); Cell(131, 22, 2, 1, Fold);
			Lanterns(8, 6);
			static const int32 Houses[][3] = { {0,14,22}, {15,12,30}, {28,16,20}, {45,13,34}, {59,15,24}, {75,12,28}, {88,17,19}, {106,13,32}, {120,14,23}, {135,15,27} };
			int32 HouseNo = 0;
			for (const auto& Hs : Houses)
			{
				const int32 X = Hs[0], W = Hs[1], Top = 60 - Hs[2];
				Cell(X, Top, W, Hs[2], H(HouseNo++ % 2 ? TEXT("2C2654") : TEXT("231E46")));
				for (int32 S = 0; S < 5 && W + 2 - 2 * S > 0; ++S) Cell(X - 1 + S, Top - 1 - S, W + 2 - 2 * S, 1, H(TEXT("42366E")));
				for (int32 WY = Top + 4; WY < 55; WY += 7) for (int32 WX = X + 2; WX < X + W - 3; WX += 5)
					Cell(WX, WY, 2, 3, (WX * 3 + WY) % 4 ? Lamp : H(TEXT("15122B")));
			}
			Cell(0, 60, 150, 12, H(TEXT("1B2F66"))); Cell(0, 72, 150, 16, H(TEXT("162654"))); Cell(0, 88, 150, 15, H(TEXT("111D44"))); Cell(0, 60, 150, 1, Black);
			for (int32 I = 0; I < 44; ++I) Cell((I * 29 + 7) % 146, 62 + (I * 13) % 39, 2 + I % 4, 1, I % 3 == 0 ? A(Lamp, 0.5f) : A(H(TEXT("4A78C8")), 0.5f));
			for (int32 J = 0; J < 8; ++J) Cell(125 + J % 2, 63 + J * 4, 10 - J, 1, A(Slip, 0.3f));
			// The boat: lantern on the prow, the exorcist at the oar.
			Cell(46, 66, 11, 12, A(Lamp, 0.1f)); Cell(48, 68, 7, 8, A(Lamp, 0.16f));
			Cell(51, 73, 1, 11, H(TEXT("6B4A32"))); Cell(50, 70, 3, 4, Lamp); Cell(50, 69, 3, 1, Black);
			Cell(35, 71, 6, 1, Black); Cell(36, 70, 4, 1, Black); Cell(36, 72, 4, 3, H(TEXT("E8C9A0"))); Cell(35, 75, 6, 2, Crimson); Cell(35, 77, 6, 7, H(TEXT("3A3566")));
			for (int32 K = 0; K < 6; ++K) Cell(32 - K * 2, 80 + K * 2, 3, 1, H(TEXT("8A6442")));
			Cell(20, 84, 38, 1, H(TEXT("8A6442"))); Cell(21, 85, 36, 2, H(TEXT("4A3426"))); Cell(23, 87, 32, 2, H(TEXT("2E2019"))); Cell(26, 89, 26, 1, Black);
			Cell(47, 92, 8, 1, A(Lamp, 0.5f)); Cell(49, 95, 5, 1, A(Lamp, 0.3f));
			Notice(108, 54, 34, 44); Cell(108, 74, 34, 1, Fold); Sprite(TEXT("wax_seal"), 119, 75, 1);
		}
		else if (PanelIndex == 1)
		{
			// What the market leaves behind: shuttered fronts, dropped receipts, and the things that got up.
			Cell(0, 0, 150, 74, H(TEXT("171230")));
			for (int32 Bay = 0; Bay < 3; ++Bay)
			{
				const float X = 4 + Bay * 50;
				for (int32 S = 0; S < 7; ++S) Cell(X + S * 6, 8, 6, 9, H(S % 2 ? TEXT("5A1026") : TEXT("8C8372")));
				Cell(X, 17, 42, 2, Black);
				if (Bay == 1) { Cell(X + 3, 19, 36, 55, Black); continue; }
				Cell(X + 3, 19, 36, 55, H(TEXT("262043")));
				for (int32 R = 0; R < 9; ++R) Cell(X + 3, 23 + R * 6, 36, 1, H(TEXT("0E0B26")));
				Cell(X + 19, 66, 4, 4, H(TEXT("B08A2E")));
				Notice(X + 14, 27, 14, 13);
			}
			Cell(0, 74, 150, 29, H(TEXT("2A2238"))); Cell(0, 74, 150, 1, Black);
			for (int32 I = 0; I < 40; ++I) Cell((I * 31) % 146, 77 + (I * 17) % 25, 4, 1, H(TEXT("3A304E")));
			for (int32 I = 0; I < 7; ++I)
			{
				const float X = 6 + I * 21 + (I % 2) * 5, Y = 86 + (I * 7) % 13;
				Cell(X, Y, 7, 5, Slip); Cell(X + 1, Y + 1, 4, 1, A(Ink, 0.45f)); Cell(X + 1, Y + 3, 3, 1, A(Ink, 0.45f));
			}
			Cell(28, 26, 94, 56, A(Dread, 0.08f)); Cell(38, 20, 74, 66, A(Dread, 0.1f)); Cell(50, 16, 50, 72, A(Dread, 0.1f));
			Cell(12, 92, 36, 3, A(Black, 0.45f)); Cell(104, 90, 36, 3, A(Black, 0.45f)); Cell(49, 92, 52, 3, A(Black, 0.5f));
			Sprite(TEXT("curse_weepwick"), 6, 46, 1); Sprite(TEXT("curse_cinder_grin"), 98, 44, 1); Sprite(TEXT("curse_the_shuttered"), 43, 30, 1);
			Sprite(TEXT("emo_sorrow"), 24, 30, 1); Sprite(TEXT("emo_dread"), 69, 15, 1); Sprite(TEXT("emo_rage"), 116, 28, 1);
			for (int32 K = 0; K < 3; ++K) { Cell(29, 43 + K * 3, 1, 1, A(Sorrow, 0.7f)); Cell(74, 27 + K * 2, 1, 1, A(Dread, 0.8f)); Cell(121, 41 + K * 3, 1, 1, A(H(TEXT("C8402A")), 0.7f)); }
		}
		else if (PanelIndex == 2)
		{
			// Kessa's offer: the paper lantern in the Rite Hall, holding out a talisman.
			Cell(0, 0, 150, 58, H(TEXT("2A1A2E"))); Cell(0, 58, 150, 12, H(TEXT("1E1424"))); Cell(0, 58, 150, 1, H(TEXT("3A263C")));
			Cell(0, 70, 150, 33, H(TEXT("3A2A22")));
			for (int32 I = 0; I < 6; ++I) Cell(0, 70 + I * 6, 150, 1, H(TEXT("2A1D18")));
			for (int32 I = 0; I < 12; ++I) Cell((I * 27 + (I % 2) * 13) % 150, 71 + (I % 5) * 6, 1, 5, H(TEXT("2A1D18")));
			for (const float X : { 3.f, 133.f })
			{
				Cell(X, 0, 14, 40, H(TEXT("7A1028"))); Cell(X, 35, 14, 2, Gold); Cell(X + 5, 38, 4, 2, H(TEXT("2A1A2E"))); Cell(X + 5, 14, 4, 4, Gold); Cell(X + 6, 15, 2, 2, H(TEXT("7A1028")));
			}
			// Lamplight around Kessa and the talisman, stepped so it reads as a glow rather than a box.
			for (int32 R = 0; R < 5; ++R) { Cell(20 + R * 5, 6 + R * 7, 48 - R * 10, 82 - R * 14, A(Lamp, 0.035f)); Cell(8 + R * 6, 22 + R * 4, 72 - R * 12, 50 - R * 8, A(Lamp, 0.035f)); }
			for (int32 R = 0; R < 5; ++R) { Cell(100 + R * 2, 4 + R * 2, 24 - R * 4, 84 - R * 4, A(Gold, 0.04f)); Cell(92 + R * 2, 18 + R * 4, 40 - R * 4, 56 - R * 8, A(Gold, 0.04f)); }
			Cell(24, 90, 40, 3, A(Black, 0.4f));
			Sprite(TEXT("kessa"), 18, 14, 2);
			Sprite(TEXT("talisman"), 98, 14, 2);
			for (const FVector2f& S : { FVector2f(92, 40), FVector2f(89, 28), FVector2f(129, 44), FVector2f(130, 60), FVector2f(111, 8), FVector2f(93, 14), FVector2f(128, 18), FVector2f(111, 82), FVector2f(92, 70), FVector2f(128, 76) })
				Cell(S.X, S.Y, 2, 2, Gold);
			for (int32 K = 0; K < 5; ++K) Cell(72 + K * 5, 48 - (K == 2 ? 2 : K == 1 || K == 3 ? 1 : 0), 2, 2, A(Lamp, 0.85f));
			Notice(64, 84, 24, 13);
		}
		else
		{
			// A place in the ledger: the Rite's book open on a desk, sealed.
			Cell(0, 0, 150, 103, H(TEXT("2E211C")));
			for (int32 I = 0; I < 9; ++I) Cell(0, 6 + I * 12, 150, 1, H(TEXT("241915")));
			Lanterns(2, 5);
			Cell(13, 21, 128, 77, A(Black, 0.4f)); Cell(10, 18, 130, 77, Black); Cell(11, 19, 128, 75, H(TEXT("6A1228")));
			Cell(15, 22, 59, 68, H(TEXT("EFE3C6"))); Cell(76, 22, 59, 68, H(TEXT("EFE3C6"))); Cell(74, 22, 2, 68, Fold); Cell(15, 88, 120, 2, Fold);
			Cell(72, 22, 2, 66, A(Fold, 0.5f)); Cell(76, 22, 2, 66, A(Fold, 0.5f));
			Cell(100, 90, 4, 11, Gold); Cell(100, 99, 2, 2, H(TEXT("2E211C")));
			Cell(20, 26, 30, 2, Crimson);
			for (int32 I = 0; I < 6; ++I)
			{
				Sprite(I < 4 ? TEXT("pip_lit") : TEXT("pip_dim"), 20, 31 + I * 9, 1);
				Cell(31, 34 + I * 9, 36 - (I % 3) * 6, 1, A(Ink, 0.6f)); Cell(31, 37 + I * 9, 20, 1, A(Ink, 0.3f));
			}
			Cell(81, 26, 24, 2, Crimson);
			for (int32 I = 0; I < 2; ++I) Cell(81, 33 + I * 5, 48 - I * 10, 1, A(Ink, 0.6f));
			Sprite(TEXT("emo_rage"), 82, 44, 1); Sprite(TEXT("emo_sorrow"), 99, 44, 1); Sprite(TEXT("emo_dread"), 116, 44, 1);
			for (const float X : { 82.f, 99.f, 116.f }) Cell(X, 57, 11, 1, A(Ink, 0.5f));
			Sprite(TEXT("stamp_tool"), 82, 60, 1); Sprite(TEXT("wax_seal"), 108, 64, 1);
		}
		Cell(0, 0, 150, 1, Black); Cell(0, 102, 150, 1, Black); Cell(0, 0, 1, 103, Black); Cell(149, 0, 1, 103, Black);
		return Layer;
	}
private:
	int32 PanelIndex = 0;
	FGbSpriteMap Sprites;
};

TSharedRef<SWidget> UGbStoryArt::RebuildWidget()
{
	// The game's own pixel art (UI/Battle). A picture that is not imported is simply left out.
	FGbSpriteMap Brushes;
	for (const TCHAR* Name : { TEXT("kessa"), TEXT("talisman"), TEXT("wax_seal"), TEXT("stamp_tool"), TEXT("pip_lit"), TEXT("pip_dim"), TEXT("emo_rage"), TEXT("emo_sorrow"),
		TEXT("emo_dread"), TEXT("curse_the_shuttered"), TEXT("curse_weepwick"), TEXT("curse_cinder_grin") })
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Grudgebound/UI/Battle/%s.%s"), Name, Name));
		if (!Texture) continue;
#if WITH_EDITOR
		// In the editor a texture reports a placeholder's size until it has compiled; wait for the real one.
		FTextureCompilingManager::Get().FinishCompilation({ Texture });
		Texture->WaitForPendingInitOrStreaming();
#endif
		Textures.Add(FName(Name), Texture);
		FSlateBrush Brush; Brush.SetResourceObject(Texture); Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY()); Brush.DrawAs = ESlateBrushDrawType::Image;
		Brushes.Add(FName(Name), Brush);
	}
	return SAssignNew(Art, SGbStoryArt).Panel(PanelIndex).Sprites(Brushes);
}
void UGbStoryArt::SetPanel(int32 Index) { PanelIndex = Index; if (Art.IsValid()) Art->SetPanel(Index); }
void UGbStoryArt::ReleaseSlateResources(bool bReleaseChildren) { Super::ReleaseSlateResources(bReleaseChildren); Art.Reset(); }

void UGbIntroWidget::BuildTree()
{
	SetIsFocusable(true);
	// The same poster as every panel: the page's title on the crimson ribbon, its picture and words on the paper card.
	UTextBlock* TitleText = nullptr; UTextBlock* Sub = nullptr; UHorizontalBox* Footer = nullptr;
	UVerticalBox* Shell = MakeShell(TEXT(""), TEXT("wax_seal"), 1240.f, TitleText, Sub, Footer);
	Title = TitleText; Sub->SetVisibility(ESlateVisibility::Collapsed);
	// A story page gets no server replies, so it has no ink note; and the Ward is blacked out, not just dimmed.
	if (ShellNote && ShellNote->GetParent()) ShellNote->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
	ShellNote = nullptr; ShellStatus = nullptr;
	if (UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		if (UBorder* Shade = Cast<UBorder>(Root->GetChildAt(0))) Shade->SetBrushColor(FLinearColor(Ink.R, Ink.G, Ink.B, 0.94f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); Shell->AddChildToVerticalBox(Row);
	UBorder* Frame = MakePanel(GbUi::PaperHi(), FMargin(10.f));
	Illustration = WidgetTree->ConstructWidget<UGbStoryArt>(); Frame->SetContent(Sized(Illustration, 600.f, 412.f));
	UHorizontalBoxSlot* ArtSlot = Row->AddChildToHorizontalBox(Frame);
	ArtSlot->SetVerticalAlignment(VAlign_Center); ArtSlot->SetPadding(FMargin(0.f, 0.f, 30.f, 0.f));

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Col); TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// Page marker: where you are in the town's story, and a pip for each page.
	UHorizontalBox* Marker = WidgetTree->ConstructWidget<UHorizontalBox>();
	Page = MakeText(TEXT(""), 18, Crimson, true); Page->SetAutoWrapText(false);
	UHorizontalBoxSlot* PageSlot = Marker->AddChildToHorizontalBox(Page); PageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); PageSlot->SetVerticalAlignment(VAlign_Center);
	Pips.Reset();
	for (int32 I = 0; I < GbStory::IntroPanels().Num(); ++I)
	{
		UBorder* Pip = WidgetTree->ConstructWidget<UBorder>(); Pips.Add(Pip);
		UHorizontalBoxSlot* PipSlot = Marker->AddChildToHorizontalBox(Sized(Pip, 16.f, 16.f)); PipSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f)); PipSlot->SetVerticalAlignment(VAlign_Center);
	}
	Col->AddChildToVerticalBox(Marker)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(); Rule->SetBrushColor(FLinearColor(Ink.R, Ink.G, Ink.B, 0.35f));
	Col->AddChildToVerticalBox(Sized(Rule, 0.f, 3.f))->SetPadding(FMargin(0.f, 10.f, 0.f, 16.f));
	Body = MakeText(TEXT(""), 21, Ink);
	// An explicit wrap width avoids AutoWrap's first-frame measurement delay.
	Body->SetAutoWrapText(false); Body->SetWrapTextAt(520.f);
	UVerticalBoxSlot* BodySlot = Col->AddChildToVerticalBox(Body); BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// The replay key as it is bound now, on a keycap.
	UHorizontalBox* HintRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	UBorder* Cap = MakePanel(GbUi::PaperHi(), FMargin(8.f, 2.f));
	UTextBlock* CapText = MakeText(KeyName(TEXT("Story")), 16, Ink, true); CapText->SetAutoWrapText(false); Cap->SetContent(CapText);
	UHorizontalBoxSlot* CapSlot = HintRow->AddChildToHorizontalBox(Cap); CapSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f)); CapSlot->SetVerticalAlignment(VAlign_Center);
	UTextBlock* Hint = MakeText(TEXT("replays this story from the Ward."), 15, GbUi::InkSoft()); Hint->SetAutoWrapText(false);
	HintRow->AddChildToHorizontalBox(Hint)->SetVerticalAlignment(VAlign_Center);
	Col->AddChildToVerticalBox(HintRow)->SetPadding(FMargin(0.f, 12.f, 0.f, 2.f));

	const auto Foot = [Footer](UWidget* W, float Right)
	{
		UHorizontalBoxSlot* S = Footer->AddChildToHorizontalBox(W); S->SetPadding(FMargin(0.f, 0.f, Right, 0.f)); S->SetVerticalAlignment(VAlign_Center);
	};
	UTextBlock* Label = nullptr;
	UButton* Skip = MakeKeyButton(TEXT("Skip intro"), TEXT("Esc"), Label, GbUi::Paper2());
	Skip->OnClicked.AddDynamic(this, &UGbIntroWidget::HandleSkip); Foot(Sized(Skip, 250.f, 56.f), 14.f);
	UButton* Next = MakeKeyButton(TEXT("Turn the page"), TEXT("Enter"), NextLabel, Crimson);
	Next->OnClicked.AddDynamic(this, &UGbIntroWidget::HandleNext); Foot(Sized(Next, 470.f, 56.f), 0.f);
}

void UGbIntroWidget::Open() { PanelIndex = 0; Redraw(); }
void UGbIntroWidget::Redraw()
{
	const auto& Panels = GbStory::IntroPanels(); const auto& Panel = Panels[PanelIndex];
	Page->SetText(FText::FromString(FString::Printf(TEXT("HOLLOWMERE  /  %d OF %d"), PanelIndex + 1, Panels.Num())));
	Title->SetText(FText::FromString(Panel.Title)); Body->SetText(FText::FromString(Panel.Text)); Illustration->SetPanel(PanelIndex);
	NextLabel->SetText(FText::FromString(PanelIndex + 1 == Panels.Num() ? TEXT("Enter Lantern Ward") : TEXT("Turn the page")));
	for (int32 I = 0; I < Pips.Num(); ++I) if (Pips[I]) Pips[I]->SetBrushColor(I <= PanelIndex ? Crimson : GbUi::Paper3());
}
void UGbIntroWidget::HandleNext() { if (++PanelIndex >= GbStory::IntroPanels().Num()) OnFinished.Broadcast(); else Redraw(); }
void UGbIntroWidget::HandleSkip() { OnFinished.Broadcast(); }
FReply UGbIntroWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.IsRepeat()) return FReply::Handled();
	if (Event.GetKey() == EKeys::Escape) { HandleSkip(); return FReply::Handled(); }
	if (Event.GetKey() == EKeys::Enter || Event.GetKey() == EKeys::SpaceBar) { HandleNext(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}
