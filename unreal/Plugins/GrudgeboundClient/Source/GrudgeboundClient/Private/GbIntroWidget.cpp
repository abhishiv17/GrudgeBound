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

namespace
{
	const FLinearColor& Paper=GbGreybox::Color::Paper; const FLinearColor& Ink=GbGreybox::Color::Ink; const FLinearColor& Gold=GbGreybox::Color::Gold;
	const FLinearColor& Crimson=GbGreybox::Color::Crimson; const FLinearColor& Sorrow=GbGreybox::Color::Sorrow; const FLinearColor& Dread=GbGreybox::Color::Dread;
}

class SGbStoryArt : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGbStoryArt) : _Panel(0) {}
		SLATE_ARGUMENT(int32, Panel)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args) { PanelIndex = Args._Panel; }
	void SetPanel(int32 Index) { PanelIndex = Index; Invalidate(EInvalidateWidgetReason::Paint); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(480, 330); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2f Scale(Size.X / 480.f, Size.Y / 330.f);
		int32 Layer = LayerId;
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		auto Box = [&](float X, float Y, float W, float H, FLinearColor Color)
		{
			FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2f(W * Scale.X, H * Scale.Y),
				FSlateLayoutTransform(FVector2f(X * Scale.X, Y * Scale.Y))), Brush, ESlateDrawEffect::None, Color * Style.GetColorAndOpacityTint());
		};
		auto Line = [&](std::initializer_list<FVector2f> Points, FLinearColor Color, float Width = 2.f)
		{
			TArray<FVector2f> Scaled;
			for (const auto& P : Points) Scaled.Add(P * Scale);
			FSlateDrawElement::MakeLines(Elements, ++Layer, Geometry.ToPaintGeometry(), MoveTemp(Scaled), ESlateDrawEffect::None,
				Color * Style.GetColorAndOpacityTint(), true, Width * FMath::Min(Scale.X, Scale.Y));
		};
		auto Receipt = [&](float X, float Y, float W, float H)
		{
			Box(X + 4, Y + 5, W, H, Ink.CopyWithNewOpacity(0.3f)); Box(X, Y, W, H, Paper);
			Line({ {X, Y}, {X + W, Y}, {X + W, Y + H}, {X, Y + H}, {X, Y} }, Ink);
			for (int32 I = 0; I < 4; ++I) Box(X + 7, Y + 12 + I * 9, W - 16 - (I % 2) * 8, 2, Ink.CopyWithNewOpacity(0.4f));
		};
		Box(0, 0, 480, 330, Paper * FLinearColor(0.88f, 0.9f, 0.95f));
		// Paper grain is deterministic; it never flashes or changes the story.
		for (int32 I = 0; I < 105; ++I) Box(8 + (I * 47) % 463, 8 + (I * 83) % 313, 2, 1, Ink.CopyWithNewOpacity(0.08f));
		if (PanelIndex == 0)
		{
			Box(14, 14, 452, 187, Ink); Box(366, 35, 40, 40, Gold);
			for (int32 I = 0; I < 7; ++I)
			{
				const float X = 30 + I * 62, Y = 108 - (I % 3) * 15;
				Box(X, Y, 47, 96, Sorrow); Line({ {X - 4, Y}, {X + 24, Y - 25}, {X + 52, Y} }, Paper, 4);
				Box(X + 19, Y + 25, 10, 18, Gold);
			}
			Box(14, 201, 452, 114, Sorrow);
			for (int32 I = 0; I < 7; ++I) Line({ {25.f + I * 9, 220.f + I * 12}, {300.f + I * 15, 224.f + I * 12} }, Paper.CopyWithNewOpacity(0.3f));
			Line({ {48, 252}, {195, 252}, {174, 283}, {68, 283}, {48, 252} }, Ink, 7);
			Line({ {110, 250}, {110, 181}, {156, 236}, {110, 236} }, Paper, 4);
			Receipt(320, 219, 96, 77); Box(379, 267, 22, 16, Crimson);
		}
		else if (PanelIndex == 1)
		{
			for (int32 I = 0; I < 5; ++I) { Box(25 + I * 87, 38, 69, 170, Ink); Box(29 + I * 87, 43, 61, 162, Dread); }
			Box(22, 205, 435, 19, Ink); Receipt(35, 222, 90, 75); Receipt(325, 230, 100, 66);
			Box(174, 146, 130, 116, Crimson); Box(184, 158, 110, 90, Ink);
			Line({ {175, 145}, {198, 128}, {239, 141}, {281, 127}, {305, 145} }, Gold, 8);
			Box(201, 181, 17, 10, Paper); Box(261, 181, 17, 10, Paper);
			Line({ {198, 211}, {212, 229}, {226, 211}, {240, 229}, {254, 211}, {270, 229}, {282, 211} }, Paper, 3);
			Line({ {178, 232}, {148, 269}, {131, 257} }, Ink, 6); Line({ {300, 232}, {325, 268}, {342, 253} }, Ink, 6);
			Box(145, 78, 15, 15, Crimson); Box(240, 65, 14, 14, Sorrow); Box(330, 97, 13, 13, Dread);
		}
		else if (PanelIndex == 2)
		{
			Box(16, 16, 448, 298, Ink);
			Box(172, 60, 134, 203, Gold.CopyWithNewOpacity(0.2f));
			Box(185, 72, 108, 176, Gold); Box(195, 81, 88, 158, Paper);
			for (int32 I = 0; I < 7; ++I) Line({ {194, 98.f + I * 20}, {285, 98.f + I * 20} }, Gold, 2);
			Box(181, 66, 116, 12, Crimson); Box(182, 243, 115, 12, Crimson);
			Line({ {237, 28}, {237, 63} }, Paper); Line({ {211, 128}, {217, 129} }, Ink, 4); Line({ {262, 128}, {268, 129} }, Ink, 4);
			Line({ {227, 152}, {238, 159}, {249, 152} }, Ink, 2);
			Receipt(108, 202, 46, 83); Receipt(323, 187, 46, 90);
			Line({ {187, 204}, {151, 230} }, Paper, 3); Line({ {293, 205}, {325, 217} }, Paper, 3);
			Line({ {229, 255}, {218, 280}, {233, 296}, {242, 280}, {252, 297}, {261, 280}, {247, 255} }, Crimson, 3);
		}
		else
		{
			Box(50, 59, 380, 234, Ink); Box(60, 49, 176, 232, Paper); Box(242, 49, 176, 232, Paper);
			Line({ {238, 49}, {238, 280} }, Gold, 5);
			for (int32 I = 0; I < 8; ++I) { Box(78, 80 + I * 20, 138 - (I % 3) * 17, 2, Ink.CopyWithNewOpacity(0.4f)); Box(263, 80 + I * 20, 132 - (I % 2) * 24, 2, Ink.CopyWithNewOpacity(0.4f)); }
			Box(300, 178, 54, 54, Crimson); Box(311, 189, 32, 32, Gold); Box(321, 199, 12, 12, Crimson);
			Receipt(23, 216, 43, 86); Receipt(405, 204, 43, 92);
			Line({ {70, 34}, {141, 24}, {240, 33}, {341, 23}, {410, 34} }, Ink, 2);
			for (int32 I = 0; I < 6; ++I) { Box(80 + I * 62, 22, 14, 22, Gold); Box(83 + I * 62, 25, 8, 14, Crimson); }
		}
		Line({ {6, 6}, {474, 6}, {474, 324}, {6, 324}, {6, 6} }, Ink, 2);
		return Layer;
	}
private:
	int32 PanelIndex = 0;
};

TSharedRef<SWidget> UGbStoryArt::RebuildWidget() { return SAssignNew(Art, SGbStoryArt).Panel(PanelIndex); }
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
