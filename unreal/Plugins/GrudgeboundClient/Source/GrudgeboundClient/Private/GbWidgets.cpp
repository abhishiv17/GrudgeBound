#include "GbWidgets.h"

#include "GbApiSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

namespace
{
	namespace C = GbGreybox::Color;

	// Functions, not constants: the palette is only guaranteed to be initialised once the game runs.
	FLinearColor PaperPanel() { return C::Paper; }
	FLinearColor InkPanel() { return FLinearColor(C::Ink.R, C::Ink.G, C::Ink.B, 0.92f); }
	FLinearColor Dim() { return FLinearColor(C::Ink.R, C::Ink.G, C::Ink.B, 0.8f); }

	FLinearColor EmotionColor(const FString& Emotion)
	{
		if (Emotion == TEXT("rage")) return C::Rage;
		if (Emotion == TEXT("sorrow")) return C::Sorrow;
		if (Emotion == TEXT("dread")) return C::Dread;
		return C::Ink;
	}

	FString Capitalised(const FString& S)
	{
		return S.IsEmpty() ? S : S.Left(1).ToUpper() + S.Mid(1);
	}

	FString Percent(float Chance)
	{
		return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(Chance, 0.f, 1.f) * 100.f));
	}

	FString HpText(const FGbSide& S)
	{
		return FString::Printf(TEXT("%d / %d"), S.Hp, S.MaxHp);
	}

	float HpFraction(const FGbSide& S)
	{
		return S.MaxHp > 0 ? FMath::Clamp(static_cast<float>(S.Hp) / S.MaxHp, 0.f, 1.f) : 0.f;
	}

	FLinearColor HpColor(float Fraction)
	{
		return Fraction > 0.5f ? C::Leaf : Fraction > 0.2f ? C::Gold : C::Crimson;
	}

	UVerticalBoxSlot* AddRow(UVerticalBox* Box, UWidget* W, float Top = 2.f, float Bottom = 2.f)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
		S->SetPadding(FMargin(0.f, Top, 0.f, Bottom));
		S->SetHorizontalAlignment(HAlign_Fill);
		return S;
	}

	UHorizontalBoxSlot* AddCol(UHorizontalBox* Box, UWidget* W, bool bFill, float Right = 8.f)
	{
		UHorizontalBoxSlot* S = Box->AddChildToHorizontalBox(W);
		S->SetPadding(FMargin(0.f, 0.f, Right, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
		if (bFill) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return S;
	}

	/** Puts a widget in the middle of a canvas. */
	UCanvasPanelSlot* Centre(UCanvasPanel* Canvas, UWidget* W)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
		return S;
	}
}

// ================================================================ base

void UGbWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
}

UGbApiSubsystem* UGbWidgetBase::Api() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UGbApiSubsystem>() : nullptr;
}

UGbPlayerSubsystem* UGbWidgetBase::Player() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UGbPlayerSubsystem>() : nullptr;
}

UTextBlock* UGbWidgetBase::MakeText(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Text));
	T->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = T->GetFont();
	Font.Size = Size;
	Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	T->SetFont(Font);
	T->SetAutoWrapText(true);
	return T;
}

UBorder* UGbWidgetBase::MakePanel(const FLinearColor& Background, const FMargin& Inner)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrushColor(Background);
	B->SetPadding(Inner);
	return B;
}

UButton* UGbWidgetBase::MakeButton(const FString& Label, UTextBlock*& OutLabel, const FLinearColor& Tint)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetBackgroundColor(Tint);
	OutLabel = MakeText(Label, 15, C::Ink, true);
	OutLabel->SetJustification(ETextJustify::Center);
	OutLabel->SetAutoWrapText(false);
	B->AddChild(OutLabel);
	return B;
}

UProgressBar* UGbWidgetBase::MakeBar(const FLinearColor& Fill)
{
	UProgressBar* P = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	P->SetFillColorAndOpacity(Fill);
	P->SetPercent(0.f);
	return P;
}

UWidget* UGbWidgetBase::Sized(UWidget* Content, float Width, float Height)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (Width > 0.f) Box->SetWidthOverride(Width);
	if (Height > 0.f) Box->SetHeightOverride(Height);
	Box->SetContent(Content);
	return Box;
}

// ================================================================ HUD

void UGbHudWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// --- Ward card, top left
	UBorder* Card = MakePanel(PaperPanel(), FMargin(14.f, 10.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Sized(Col, 360.f, 0.f));

	WardTitle = MakeText(TEXT("Lantern Ward"), 18, C::Ink, true);
	AddRow(Col, WardTitle);
	GrudgeTotal = MakeText(TEXT("Grudge -- / 100"), 13, C::Ink);
	AddRow(Col, GrudgeTotal, 4.f);

	auto EmotionRow = [this, Col](const FString& Label, const FLinearColor& Color, TObjectPtr<UTextBlock>& OutText, TObjectPtr<UProgressBar>& OutBar)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		OutText = MakeText(Label, 13, Color, true);
		OutText->SetAutoWrapText(false);
		AddCol(Row, Sized(OutText, 90.f, 0.f), false);
		OutBar = MakeBar(Color);
		AddCol(Row, Sized(OutBar, 0.f, 12.f), true, 0.f);
		AddRow(Col, Row, 1.f, 1.f);
	};
	EmotionRow(TEXT("Rage 0"), C::Rage, RageText, RageBar);
	EmotionRow(TEXT("Sorrow 0"), C::Sorrow, SorrowText, SorrowBar);
	EmotionRow(TEXT("Dread 0"), C::Dread, DreadText, DreadBar);

	Stats = MakeText(TEXT(""), 14, C::Ink, true);
	Stats->SetAutoWrapText(false);
	AddRow(Col, Stats, 8.f);
	PartnerText = MakeText(TEXT(""), 13, C::Ink);
	AddRow(Col, PartnerText);
	ServerText = MakeText(TEXT("Server: connecting..."), 12, C::Ink);
	AddRow(Col, ServerText, 6.f);

	UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Card);
	CardSlot->SetAnchors(FAnchors(0.f, 0.f));
	CardSlot->SetPosition(FVector2D(24.f, 24.f));
	CardSlot->SetAutoSize(true);

	// --- key hint, top right
	UBorder* Hint = MakePanel(InkPanel(), FMargin(10.f, 6.f));
	Hint->SetContent(MakeText(TEXT("T  Talisman Case"), 13, C::Paper, true));
	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(Hint);
	HintSlot->SetAnchors(FAnchors(1.f, 0.f));
	HintSlot->SetAlignment(FVector2D(1.f, 0.f));
	HintSlot->SetPosition(FVector2D(-24.f, 24.f));
	HintSlot->SetAutoSize(true);

	// --- Kessa's banner, bottom centre
	UBorder* Banner = MakePanel(InkPanel(), FMargin(18.f, 10.f));
	KessaText = MakeText(TEXT(""), 16, C::Paper);
	KessaText->SetJustification(ETextJustify::Center);
	Banner->SetContent(Sized(KessaText, 760.f, 0.f));
	UCanvasPanelSlot* BannerSlot = Root->AddChildToCanvas(Banner);
	BannerSlot->SetAnchors(FAnchors(0.5f, 1.f));
	BannerSlot->SetAlignment(FVector2D(0.5f, 1.f));
	BannerSlot->SetPosition(FVector2D(0.f, -24.f));
	BannerSlot->SetAutoSize(true);
}

void UGbHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The HUD never takes clicks: clicks go through to the world (click-to-walk).
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UGbApiSubsystem* A = Api())
	{
		A->OnWorldState.AddUniqueDynamic(this, &UGbHudWidget::HandleWorldState);
		A->OnServerStatus.AddUniqueDynamic(this, &UGbHudWidget::HandleServerStatus);
		A->OnKessaLine.AddUniqueDynamic(this, &UGbHudWidget::HandleKessaLine);
	}
	if (UGbPlayerSubsystem* P = Player())
	{
		P->OnPlayerChanged.AddUniqueDynamic(this, &UGbHudWidget::HandlePlayerChanged);
	}
	Refresh();
}

void UGbHudWidget::NativeDestruct()
{
	if (UGbApiSubsystem* A = Api())
	{
		A->OnWorldState.RemoveDynamic(this, &UGbHudWidget::HandleWorldState);
		A->OnServerStatus.RemoveDynamic(this, &UGbHudWidget::HandleServerStatus);
		A->OnKessaLine.RemoveDynamic(this, &UGbHudWidget::HandleKessaLine);
	}
	if (UGbPlayerSubsystem* P = Player())
	{
		P->OnPlayerChanged.RemoveDynamic(this, &UGbHudWidget::HandlePlayerChanged);
	}
	Super::NativeDestruct();
}

void UGbHudWidget::Refresh()
{
	const UGbApiSubsystem* A = Api();
	const UGbPlayerSubsystem* P = Player();
	if (!A || !WardTitle) return;

	const FGbWorldState& S = A->GetWorldState();
	const FString WardId = GetDefault<UGbSettings>()->WardId;
	FGbWardSummary Ward;
	const bool bHaveWard = A->GetWard(WardId, Ward);

	const FString Name = bHaveWard ? Ward.Name : TEXT("Lantern Ward");
	const FString Time = S.bValid ? FString::Printf(TEXT("Day %d  %02d:00 %s"), S.Day, S.Hour, S.bNight ? TEXT("(night)") : TEXT("(day)")) : TEXT("Day -  --:--");
	WardTitle->SetText(FText::FromString(FString::Printf(TEXT("%s   %s"), *Name, *Time)));

	GrudgeTotal->SetText(FText::FromString(bHaveWard
		? FString::Printf(TEXT("Grudge %d / 100   (%d curses roaming)"), FMath::RoundToInt(Ward.Total), Ward.Wild)
		: FString(TEXT("Grudge -- / 100"))));
	RageText->SetText(FText::FromString(FString::Printf(TEXT("Rage %d"), FMath::RoundToInt(Ward.Grudge.Rage))));
	SorrowText->SetText(FText::FromString(FString::Printf(TEXT("Sorrow %d"), FMath::RoundToInt(Ward.Grudge.Sorrow))));
	DreadText->SetText(FText::FromString(FString::Printf(TEXT("Dread %d"), FMath::RoundToInt(Ward.Grudge.Dread))));
	RageBar->SetPercent(FMath::Clamp(Ward.Grudge.Rage / 100.f, 0.f, 1.f));
	SorrowBar->SetPercent(FMath::Clamp(Ward.Grudge.Sorrow / 100.f, 0.f, 1.f));
	DreadBar->SetPercent(FMath::Clamp(Ward.Grudge.Dread / 100.f, 0.f, 1.f));

	if (P)
	{
		Stats->SetText(FText::FromString(FString::Printf(TEXT("Points %d   Ember %d   Rules %d/10"),
			P->GetPoints(), P->GetEmber(), S.Rules.Num())));
		const FGbTalisman Partner = P->GetPartner();
		const FGbCurseInfo Curse = A->GetCurse(Partner.SpeciesId);
		PartnerText->SetText(FText::FromString(FString::Printf(TEXT("Partner: %s (%s)"), *Curse.Name,
			Curse.Emotion.IsEmpty() ? *Partner.Omen : *Capitalised(Curse.Emotion))));
	}

	ServerText->SetText(FText::FromString(A->IsServerOnline()
		? FString::Printf(TEXT("Server online  -  tick %d"), S.Tick)
		: FString::Printf(TEXT("Server offline: run  npm run server  (%s)"), *A->GetServerUrl())));
	ServerText->SetColorAndOpacity(FSlateColor(A->IsServerOnline() ? C::Leaf : C::Crimson));

	KessaText->SetText(FText::FromString(A->GetKessaLine()));
}

void UGbHudWidget::HandleWorldState(const FGbWorldState&) { Refresh(); }
void UGbHudWidget::HandleServerStatus(bool, int32) { Refresh(); }
void UGbHudWidget::HandleKessaLine(const FString&) { Refresh(); }
void UGbHudWidget::HandlePlayerChanged() { Refresh(); }

// ================================================================ encounter

void UGbEncounterWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Card = MakePanel(PaperPanel(), FMargin(24.f, 18.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Sized(Col, 460.f, 0.f));

	Title = MakeText(TEXT("A curse stirs in the tall grass!"), 22, C::Crimson, true);
	AddRow(Col, Title);
	Intro = MakeText(TEXT(""), 16, C::Ink);
	Intro->SetAutoWrapText(true);
	AddRow(Col, Intro, 6.f);
	Matchup = MakeText(TEXT(""), 14, C::Ink, true);
	AddRow(Col, Matchup, 6.f);
	Lore = MakeText(TEXT(""), 13, C::Ink);
	AddRow(Col, Lore, 2.f, 12.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Ignored = nullptr;
	UButton* Fight = MakeButton(TEXT("Fight!"), Ignored, C::Gold);
	Fight->OnClicked.AddDynamic(this, &UGbEncounterWidget::HandleFight);
	AddCol(Buttons, Sized(Fight, 0.f, 44.f), true, 12.f);
	UButton* Slip = MakeButton(TEXT("Slip away"), Ignored, C::Stone);
	Slip->OnClicked.AddDynamic(this, &UGbEncounterWidget::HandleSlipAway);
	AddCol(Buttons, Sized(Slip, 0.f, 44.f), true, 0.f);
	AddRow(Col, Buttons, 6.f);

	Centre(Root, Card);
}

void UGbEncounterWidget::Show(const FGbBattleStart& Battle)
{
	if (!Intro) return;
	// Every start line: the intro, then any curfew warning from Kessa.
	Intro->SetText(FText::FromString(Battle.Lines.Num() > 0 ? FString::Join(Battle.Lines, TEXT("\n")) : FString::Printf(TEXT("Wild %s!"), *Battle.Wild.Name)));

	const FString& M = Battle.Matchup;
	const FString Partner = Battle.View.Player.Name;
	FString MatchText = FString::Printf(TEXT("%s vs %s: even match."), *Partner, *Battle.Wild.Name);
	if (M == TEXT("advantage")) MatchText = FString::Printf(TEXT("Your %s has the advantage."), *Partner);
	else if (M == TEXT("disadvantage")) MatchText = FString::Printf(TEXT("Careful: %s resists your %s."), *Battle.Wild.Name, *Partner);
	Matchup->SetText(FText::FromString(FString::Printf(TEXT("%s %s  |  %s"),
		*Capitalised(Battle.Wild.Emotion), *Capitalised(Battle.Wild.Omen), *MatchText)));
	Matchup->SetColorAndOpacity(FSlateColor(EmotionColor(Battle.Wild.Emotion)));

	Lore->SetText(FText::FromString(Battle.Wild.BornFrom.IsEmpty() ? FString()
		: FString::Printf(TEXT("Born from %s."), *Battle.Wild.BornFrom)));
}

void UGbEncounterWidget::HandleFight() { OnFight.Broadcast(); }
void UGbEncounterWidget::HandleSlipAway() { OnSlipAway.Broadcast(); }

// ================================================================ battle

void UGbBattleWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Dim the Ward behind the battle and catch every click.
	UBorder* Backdrop = MakePanel(Dim(), FMargin(0.f));
	UCanvasPanelSlot* BackSlot = Root->AddChildToCanvas(Backdrop);
	BackSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BackSlot->SetOffsets(FMargin(0.f));

	UBorder* Card = MakePanel(PaperPanel(), FMargin(24.f, 18.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Sized(Col, 720.f, 0.f));

	auto SideBlock = [this, Col](TObjectPtr<UTextBlock>& OutName, TObjectPtr<UTextBlock>& OutTags, TObjectPtr<UProgressBar>& OutBar, TObjectPtr<UTextBlock>& OutHp)
	{
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		OutName = MakeText(TEXT(""), 20, C::Ink, true);
		OutName->SetAutoWrapText(false);
		AddCol(Top, OutName, false, 12.f);
		OutTags = MakeText(TEXT(""), 14, C::Ink, true);
		OutTags->SetAutoWrapText(false);
		AddCol(Top, OutTags, true, 0.f);
		AddRow(Col, Top, 6.f, 0.f);

		UHorizontalBox* Hp = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		OutBar = MakeBar(C::Leaf);
		AddCol(Hp, Sized(OutBar, 0.f, 16.f), true, 10.f);
		OutHp = MakeText(TEXT(""), 14, C::Ink, true);
		OutHp->SetAutoWrapText(false);
		AddCol(Hp, Sized(OutHp, 80.f, 0.f), false, 0.f);
		AddRow(Col, Hp, 2.f, 6.f);
	};
	SideBlock(WildName, WildTags, WildHp, WildHpText);
	SideBlock(PlayerName, PlayerTags, PlayerHp, PlayerHpText);

	UBorder* LogPanel = MakePanel(FLinearColor(C::Stone.R, C::Stone.G, C::Stone.B, 1.f), FMargin(12.f, 8.f));
	Log = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LogPanel->SetContent(Sized(Log, 0.f, 150.f));
	AddRow(Col, LogPanel, 8.f, 8.f);

	Problem = MakeText(TEXT(""), 13, C::Crimson, true);
	Problem->SetVisibility(ESlateVisibility::Collapsed);
	AddRow(Col, Problem);

	UTextBlock* Ignored = nullptr;
	auto Add = [this](UHorizontalBox* Row, UButton* B, bool bLast)
	{
		AddCol(Row, Sized(B, 0.f, 44.f), true, bLast ? 0.f : 10.f);
	};

	Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AttackButton = MakeButton(TEXT("Attack"), Ignored, C::Gold);
	AttackButton->OnClicked.AddDynamic(this, &UGbBattleWidget::HandleAttack);
	Add(Actions, AttackButton, false);
	WardButton = MakeButton(TEXT("Ward"), Ignored, C::Stone);
	WardButton->OnClicked.AddDynamic(this, &UGbBattleWidget::HandleWard);
	Add(Actions, WardButton, false);
	UTextBlock* SalveText = nullptr;
	SalveButton = MakeButton(TEXT("Salve"), SalveText, C::Stone);
	SalveLabel = SalveText;
	SalveButton->OnClicked.AddDynamic(this, &UGbBattleWidget::HandleSalve);
	Add(Actions, SalveButton, true);
	AddRow(Col, Actions, 4.f, 4.f);

	Actions2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* SealText = nullptr;
	SealButton = MakeButton(TEXT("Seal"), SealText, C::Crimson);
	SealLabel = SealText;
	SealLabel->SetColorAndOpacity(FSlateColor(C::Paper));
	SealButton->OnClicked.AddDynamic(this, &UGbBattleWidget::HandleSeal);
	Add(Actions2, SealButton, false);
	UTextBlock* GildedText = nullptr;
	GildedButton = MakeButton(TEXT("Gilded Seal"), GildedText, C::Gold);
	GildedLabel = GildedText;
	GildedButton->OnClicked.AddDynamic(this, &UGbBattleWidget::HandleGildedSeal);
	Add(Actions2, GildedButton, false);
	UTextBlock* FleeText = nullptr;
	FleeButton = MakeButton(TEXT("Flee"), FleeText, C::Stone);
	FleeLabel = FleeText;
	FleeButton->OnClicked.AddDynamic(this, &UGbBattleWidget::HandleFlee);
	Add(Actions2, FleeButton, true);
	AddRow(Col, Actions2, 4.f, 4.f);

	EndPanel = MakePanel(InkPanel(), FMargin(14.f, 10.f));
	UVerticalBox* EndCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EndText = MakeText(TEXT(""), 18, C::Paper, true);
	AddRow(EndCol, EndText, 0.f, 8.f);
	UButton* Back = MakeButton(TEXT("Back to the Ward"), Ignored, C::Gold);
	Back->OnClicked.AddDynamic(this, &UGbBattleWidget::HandleBack);
	AddRow(EndCol, Sized(Back, 0.f, 44.f));
	EndPanel->SetContent(EndCol);
	EndPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddRow(Col, EndPanel, 8.f);

	Centre(Root, Card);
}

void UGbBattleWidget::ShowStart(const FGbBattleStart& Battle)
{
	if (!Log) return;
	LogLines.Reset();
	Log->ClearChildren();
	EndPanel->SetVisibility(ESlateVisibility::Collapsed);
	Problem->SetVisibility(ESlateVisibility::Collapsed);
	Actions->SetVisibility(ESlateVisibility::Visible);
	Actions2->SetVisibility(ESlateVisibility::Visible);
	ShowView(Battle.View);
	AddLines(Battle.Lines);
	SetButtonsEnabled(true);
}

void UGbBattleWidget::ApplyTurn(const FGbBattleTurn& Turn)
{
	if (!Log) return;
	Problem->SetVisibility(ESlateVisibility::Collapsed);
	ShowView(Turn.View);
	AddLines(Turn.Lines);
	if (Turn.bEnded)
	{
		ShowEnd(Turn.Outcome, Turn.Points, Turn.View.Wild.Name);
	}
	else
	{
		SetButtonsEnabled(true);
	}
}

void UGbBattleWidget::ShowProblem(const FString& Message, bool bBattleOver)
{
	if (!Problem) return;
	Problem->SetText(FText::FromString(Message));
	Problem->SetVisibility(ESlateVisibility::Visible);
	if (bBattleOver)
	{
		Actions->SetVisibility(ESlateVisibility::Collapsed);
		Actions2->SetVisibility(ESlateVisibility::Collapsed);
		EndText->SetText(FText::FromString(TEXT("The curse slipped back into the grass.")));
		EndPanel->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		SetButtonsEnabled(true);
	}
}

void UGbBattleWidget::ShowView(const FGbBattleView& View)
{
	LastView = View;

	auto Fill = [](const FGbSide& S, UTextBlock* Name, UTextBlock* Tags, UProgressBar* Bar, UTextBlock* Hp, const TCHAR* Prefix)
	{
		Name->SetText(FText::FromString(FString::Printf(TEXT("%s%s"), Prefix, *S.Name)));
		Tags->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"), *Capitalised(S.Emotion), *Capitalised(S.Omen))));
		Tags->SetColorAndOpacity(FSlateColor(EmotionColor(S.Emotion)));
		const float F = HpFraction(S);
		Bar->SetPercent(F);
		Bar->SetFillColorAndOpacity(HpColor(F));
		Hp->SetText(FText::FromString(HpText(S)));
	};
	Fill(View.Wild, WildName, WildTags, WildHp, WildHpText, TEXT("Wild "));
	Fill(View.Player, PlayerName, PlayerTags, PlayerHp, PlayerHpText, TEXT("Your "));

	SalveLabel->SetText(FText::FromString(FString::Printf(TEXT("Salve (%d)"), View.Salves)));
	SealLabel->SetText(FText::FromString(FString::Printf(TEXT("Seal %s"), *Percent(View.SealChance))));
	GildedLabel->SetText(FText::FromString(FString::Printf(TEXT("Gilded Seal %s (%d)"), *Percent(View.GildedSealChance), View.GildedSeals)));
	FleeLabel->SetText(FText::FromString(FString::Printf(TEXT("Flee %s"), *Percent(View.FleeChance))));
}

void UGbBattleWidget::AddLines(const TArray<FString>& Lines)
{
	constexpr int32 MaxLines = 6;
	for (const FString& L : Lines) LogLines.Add(L);
	if (LogLines.Num() > MaxLines) LogLines.RemoveAt(0, LogLines.Num() - MaxLines);
	Log->ClearChildren();
	for (int32 i = 0; i < LogLines.Num(); ++i)
	{
		// Newest line in full ink, older ones faded.
		const float Alpha = i == LogLines.Num() - 1 ? 1.f : 0.65f;
		UTextBlock* T = MakeText(LogLines[i], 14, FLinearColor(C::Ink.R, C::Ink.G, C::Ink.B, Alpha), i == LogLines.Num() - 1);
		AddRow(Log, T, 1.f, 1.f);
	}
}

void UGbBattleWidget::SetButtonsEnabled(bool bEnabled)
{
	AttackButton->SetIsEnabled(bEnabled && LastView.bCanAttack);
	WardButton->SetIsEnabled(bEnabled && LastView.bCanWard);
	SalveButton->SetIsEnabled(bEnabled && LastView.bCanSalve);
	SealButton->SetIsEnabled(bEnabled && LastView.bCanSeal);
	GildedButton->SetIsEnabled(bEnabled && LastView.bCanGildedSeal);
	FleeButton->SetIsEnabled(bEnabled && LastView.bCanFlee);
}

void UGbBattleWidget::Send(EGbAction Action)
{
	UGbApiSubsystem* A = Api();
	if (!A || !A->IsBattleOpen() || A->IsBattleBusy()) return;
	SetButtonsEnabled(false);
	A->SendAction(Action);
}

void UGbBattleWidget::ShowEnd(EGbOutcome Outcome, int32 Points, const FString& Wild)
{
	Actions->SetVisibility(ESlateVisibility::Collapsed);
	Actions2->SetVisibility(ESlateVisibility::Collapsed);
	FString Text;
	switch (Outcome)
	{
	case EGbOutcome::Bound: Text = FString::Printf(TEXT("Bound! %s is sealed into a talisman.  +%d points"), *Wild, Points); break;
	case EGbOutcome::Won: Text = FString::Printf(TEXT("Exorcised! %s fades into ash.  +%d points"), *Wild, Points); break;
	case EGbOutcome::Fled: Text = TEXT("You got away. The curse still haunts the tall grass."); break;
	case EGbOutcome::Lost: Text = TEXT("Your partner is spent. You stagger back to the Ward."); break;
	default: Text = TEXT("The battle is over."); break;
	}
	EndText->SetText(FText::FromString(Text));
	EndPanel->SetVisibility(ESlateVisibility::Visible);
}

void UGbBattleWidget::HandleAttack() { Send(EGbAction::Attack); }
void UGbBattleWidget::HandleWard() { Send(EGbAction::Ward); }
void UGbBattleWidget::HandleSeal() { Send(EGbAction::Seal); }
void UGbBattleWidget::HandleGildedSeal() { Send(EGbAction::GildedSeal); }
void UGbBattleWidget::HandleSalve() { Send(EGbAction::Salve); }
void UGbBattleWidget::HandleFlee() { Send(EGbAction::Flee); }
void UGbBattleWidget::HandleBack() { OnBack.Broadcast(); }

// ================================================================ talisman case

void UGbTalismanRowWidget::BuildTree()
{
	UBorder* Row = MakePanel(FLinearColor(C::Stone.R, C::Stone.G, C::Stone.B, 1.f), FMargin(10.f, 6.f));
	WidgetTree->RootWidget = Row;
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(H);

	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Name = MakeText(TEXT(""), 16, C::Ink, true);
	AddRow(Text, Name, 0.f, 0.f);
	Details = MakeText(TEXT(""), 12, C::Ink);
	AddRow(Text, Details, 0.f, 0.f);
	AddCol(H, Text, true, 10.f);

	UTextBlock* Label = nullptr;
	PartnerButton = MakeButton(TEXT("Make partner"), Label, C::Gold);
	PartnerLabel = Label;
	PartnerButton->OnClicked.AddDynamic(this, &UGbTalismanRowWidget::HandleMakePartner);
	AddCol(H, Sized(PartnerButton, 150.f, 36.f), false, 0.f);
}

void UGbTalismanRowWidget::Setup(const FGbTalisman& Talisman, const FGbCurseInfo& Curse, bool bIsPartner)
{
	if (!Name) return;
	TalismanId = Talisman.Id;
	Name->SetText(FText::FromString(Curse.Name));
	Name->SetColorAndOpacity(FSlateColor(EmotionColor(Curse.Emotion)));
	const FString Origin = Talisman.Id == UGbPlayerSubsystem::StarterId ? FString(TEXT("Kessa's gift"))
		: Talisman.BoundDay < 0 ? FString(TEXT("from the chain")) : FString::Printf(TEXT("bound on day %d"), Talisman.BoundDay);
	const FString Purse = Talisman.Purse > 0 ? FString::Printf(TEXT(", purse %d Ember"), Talisman.Purse) : FString();
	const FString OnChain = Talisman.ChainTokenId.IsEmpty() ? FString() : FString::Printf(TEXT("  |  NEAR %s%s"), *Talisman.ChainTokenId, *Purse);
	Details->SetText(FText::FromString(FString::Printf(TEXT("%s %s  |  %s  |  %d wins%s"),
		*Capitalised(Curse.Emotion), *Capitalised(Talisman.Omen), *Origin, Talisman.Wins, *OnChain)));
	PartnerLabel->SetText(FText::FromString(bIsPartner ? TEXT("Partner") : TEXT("Make partner")));
	PartnerButton->SetIsEnabled(!bIsPartner);
}

void UGbTalismanRowWidget::HandleMakePartner()
{
	if (UGbPlayerSubsystem* P = Player()) P->SetPartner(TalismanId);
}

void UGbTalismanCaseWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Card = MakePanel(PaperPanel(), FMargin(22.f, 16.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Sized(Col, 560.f, 0.f));

	AddRow(Col, MakeText(TEXT("Talisman Case"), 22, C::Ink, true));
	Record = MakeText(TEXT(""), 13, C::Ink);
	AddRow(Col, Record, 2.f, 8.f);

	List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddRow(Col, Sized(List, 0.f, 360.f));

	UTextBlock* Ignored = nullptr;
	UButton* Close = MakeButton(TEXT("Close  (T)"), Ignored, C::Stone);
	Close->OnClicked.AddDynamic(this, &UGbTalismanCaseWidget::HandleClose);
	AddRow(Col, Sized(Close, 0.f, 40.f), 10.f);

	Centre(Root, Card);
}

void UGbTalismanCaseWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGbPlayerSubsystem* P = Player())
	{
		P->OnPlayerChanged.AddUniqueDynamic(this, &UGbTalismanCaseWidget::HandlePlayerChanged);
	}
	Refresh();
}

void UGbTalismanCaseWidget::NativeDestruct()
{
	if (UGbPlayerSubsystem* P = Player())
	{
		P->OnPlayerChanged.RemoveDynamic(this, &UGbTalismanCaseWidget::HandlePlayerChanged);
	}
	Super::NativeDestruct();
}

void UGbTalismanCaseWidget::Refresh()
{
	const UGbPlayerSubsystem* P = Player();
	const UGbApiSubsystem* A = Api();
	if (!P || !A || !List) return;

	int32 Won = 0, Bound = 0, Fled = 0, Lost = 0;
	P->GetRecord(Won, Bound, Fled, Lost);
	const TArray<FGbTalisman> Talismans = P->GetTalismans();
	Record->SetText(FText::FromString(FString::Printf(
		TEXT("%d talismans  |  bound %d, exorcised %d, fled %d, lost %d\n%d points, %d Ember%s"),
		Talismans.Num(), Bound, Won, Fled, Lost, P->GetPoints(), P->GetEmber(),
		P->IsOnChain() ? TEXT("  (read from NEAR)") : TEXT("  (this PC only)"))));

	List->ClearChildren();
	const FString PartnerId = P->GetPartner().Id;
	for (const FGbTalisman& T : Talismans)
	{
		UGbTalismanRowWidget* Row = CreateWidget<UGbTalismanRowWidget>(this, UGbTalismanRowWidget::StaticClass());
		if (!Row) continue;
		Row->Setup(T, A->GetCurse(T.SpeciesId), T.Id == PartnerId);
		List->AddChild(Row);
	}
}

void UGbTalismanCaseWidget::HandlePlayerChanged() { Refresh(); }

// ================================================================ the shop

void UGbShopRowWidget::BuildTree()
{
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = H;
	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleText = MakeText(TEXT(""), 15, C::Ink, true);
	DetailText = MakeText(TEXT(""), 12, Dim());
	DetailText->SetAutoWrapText(true);
	AddRow(Text, TitleText, 0.f, 0.f);
	AddRow(Text, DetailText, 0.f, 4.f);
	AddCol(H, Text, true);
	UTextBlock* Label = nullptr;
	Action = MakeButton(TEXT(""), Label, C::Gold);
	ActionLabel = Label;
	Action->OnClicked.AddDynamic(this, &UGbShopRowWidget::HandleClick);
	AddCol(H, Sized(Action, 120.f, 36.f), false, 0.f);
}

void UGbShopRowWidget::Setup(const FString& InTitle, const FString& Detail, const FString& Button, bool bEnabled, const FLinearColor& TitleColor)
{
	if (!TitleText) return;
	TitleText->SetText(FText::FromString(InTitle));
	TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
	DetailText->SetText(FText::FromString(Detail));
	ActionLabel->SetText(FText::FromString(Button));
	Action->SetIsEnabled(bEnabled);
}

void UGbShopRowWidget::HandleClick()
{
	if (OnClick) OnClick();
}

void UGbShopWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Card = MakePanel(PaperPanel(), FMargin(22.f, 16.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Sized(Col, 620.f, 0.f));

	Title = MakeText(TEXT("The Talisman Broker"), 22, C::Ink, true);
	AddRow(Col, Title);
	Purse = MakeText(TEXT("Counting your Ember..."), 13, C::Ink);
	AddRow(Col, Purse, 2.f, 8.f);

	AddRow(Col, MakeText(TEXT("For sale"), 14, C::Ink, true), 4.f);
	Stock = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddRow(Col, Sized(Stock, 0.f, 230.f));

	AddRow(Col, MakeText(TEXT("Your talismans on NEAR"), 14, C::Ink, true), 10.f);
	Yours = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddRow(Col, Sized(Yours, 0.f, 150.f));

	Status = MakeText(TEXT(""), 13, C::Ink);
	Status->SetAutoWrapText(true);
	AddRow(Col, Status, 8.f);

	UTextBlock* Ignored = nullptr;
	UButton* Close = MakeButton(TEXT("Close  (B)"), Ignored, C::Stone);
	Close->OnClicked.AddDynamic(this, &UGbShopWidget::HandleClose);
	AddRow(Col, Sized(Close, 0.f, 40.f), 10.f);

	Centre(Root, Card);
}

void UGbShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGbApiSubsystem* A = Api())
	{
		A->OnShop.AddUniqueDynamic(this, &UGbShopWidget::HandleShop);
		A->OnTrade.AddUniqueDynamic(this, &UGbShopWidget::HandleTrade);
		A->OnTradeReceipt.AddUniqueDynamic(this, &UGbShopWidget::HandleTradeReceipt);
		A->OnError.AddUniqueDynamic(this, &UGbShopWidget::HandleApiError);
	}
	if (UGbPlayerSubsystem* P = Player()) P->OnPlayerChanged.AddUniqueDynamic(this, &UGbShopWidget::HandlePlayerChanged);
}

void UGbShopWidget::NativeDestruct()
{
	if (UGbApiSubsystem* A = Api())
	{
		A->OnShop.RemoveDynamic(this, &UGbShopWidget::HandleShop);
		A->OnTrade.RemoveDynamic(this, &UGbShopWidget::HandleTrade);
		A->OnTradeReceipt.RemoveDynamic(this, &UGbShopWidget::HandleTradeReceipt);
		A->OnError.RemoveDynamic(this, &UGbShopWidget::HandleApiError);
	}
	if (UGbPlayerSubsystem* P = Player()) P->OnPlayerChanged.RemoveDynamic(this, &UGbShopWidget::HandlePlayerChanged);
	Super::NativeDestruct();
}

void UGbShopWidget::Open()
{
	if (UGbApiSubsystem* A = Api()) A->FetchShop(MerchantId);
}

void UGbShopWidget::HandleShop(const FGbShop& Shop)
{
	if (Shop.MerchantId != MerchantId) return;
	Data = Shop;
	Redraw();
}

void UGbShopWidget::HandlePlayerChanged()
{
	Redraw();
}

void UGbShopWidget::Redraw()
{
	if (!Stock) return;
	const UGbPlayerSubsystem* P = Player();
	const UGbApiSubsystem* A = Api();
	Title->SetText(FText::FromString(Data.MerchantName.IsEmpty() ? FString(TEXT("The Talisman Broker"))
		: Capitalised(Data.MerchantName) + (Data.bClosed ? TEXT("  (closed)") : TEXT(""))));
	Purse->SetText(FText::FromString(Data.Ember >= 0
		? FString::Printf(TEXT("Your Ember on NEAR: %d"), Data.Ember)
		: FString(TEXT("Your Ember isn't on chain here (no Player Account, or the server runs off-chain)."))));

	Stock->ClearChildren();
	for (const FGbShopItem& It : Data.Items)
	{
		const bool bTalisman = It.Id.StartsWith(TEXT("talisman-"));
		UGbShopRowWidget* Row = CreateWidget<UGbShopRowWidget>(this, UGbShopRowWidget::StaticClass());
		if (!Row) continue;
		const bool bCan = bTalisman && !bBusy && !Data.bClosed && It.Stock > 0 && Data.Ember >= It.Price;
		const FString Detail = FString::Printf(TEXT("%d Ember  |  %d in stock  |  %s"), It.Price, It.Stock, *It.Text);
		const FString Emotion = bTalisman ? It.Id.Mid(9) : FString();
		Row->Setup(It.Name, bTalisman ? Detail : Detail + TEXT("  (not sold to players yet)"),
			FString::Printf(TEXT("Buy  %d"), It.Price), bCan, bTalisman ? EmotionColor(Emotion) : C::Ink);
		const FString ItemId = It.Id;
		Row->OnClick = [this, ItemId]() { Buy(ItemId); };
		Stock->AddChild(Row);
	}

	Yours->ClearChildren();
	int32 Shown = 0;
	if (P && A)
	{
		for (const FGbTalisman& T : P->GetTalismans())
		{
			if (T.ChainTokenId.IsEmpty()) continue;
			const FGbCurseInfo Curse = A->GetCurse(T.SpeciesId);
			const FGbShopItem* Line = Data.Items.FindByPredicate([&Curse](const FGbShopItem& I) { return I.Id == TEXT("talisman-") + Curse.Emotion; });
			const int32 Offer = Line ? Line->BuyBack : 0;
			UGbShopRowWidget* Row = CreateWidget<UGbShopRowWidget>(this, UGbShopRowWidget::StaticClass());
			if (!Row) continue;
			const bool bPartner = T.Id == P->GetPartner().Id;
			Row->Setup(FString::Printf(TEXT("%s  (NEAR %s)"), *Curse.Name, *T.ChainTokenId),
				bPartner ? FString(TEXT("Your partner: pick another partner (T) before selling it."))
					: Offer > 0 ? FString::Printf(TEXT("The Broker offers %d Ember"), Offer) : FString(TEXT("The Broker doesn't buy these")),
				FString::Printf(TEXT("Sell  %d"), Offer), !bBusy && !bPartner && Offer > 0 && !Data.bClosed, EmotionColor(Curse.Emotion));
			const FString TokenId = T.ChainTokenId;
			Row->OnClick = [this, TokenId]() { Sell(TokenId); };
			Yours->AddChild(Row);
			++Shown;
		}
	}
	if (Shown == 0) Yours->AddChild(MakeText(TEXT("None yet: talismans you bind or buy on NEAR show up here."), 13, Dim()));
}

void UGbShopWidget::Buy(const FString& ItemId)
{
	UGbApiSubsystem* A = Api();
	if (bBusy || !A) return;
	bBusy = true;
	Status->SetText(FText::FromString(TEXT("The Broker wraps it up...")));
	Redraw();
	A->BuyItem(MerchantId, ItemId);
}

void UGbShopWidget::Sell(const FString& TokenId)
{
	UGbApiSubsystem* A = Api();
	if (bBusy || !A) return;
	bBusy = true;
	Status->SetText(FText::FromString(TEXT("The Broker weighs your talisman...")));
	Redraw();
	A->SellTalisman(MerchantId, TokenId);
}

void UGbShopWidget::HandleTrade(const FGbTrade& Trade)
{
	PendingJob = Trade.JobId;
	Status->SetText(FText::FromString(Trade.Text + TEXT(". Settling it on NEAR...")));
}

void UGbShopWidget::HandleTradeReceipt(const FString& JobId, const FGbChainReceipt& Receipt)
{
	if (JobId != PendingJob) return;
	PendingJob.Reset();
	bBusy = false;
	const FString Was = Status->GetText().ToString().Replace(TEXT(" Settling it on NEAR..."), TEXT(""));
	Status->SetText(FText::FromString(Receipt.Status == TEXT("done")
		? FString::Printf(TEXT("%s Settled on NEAR%s. (L to see it)"), *Was, Receipt.TokenId.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Receipt.TokenId))
		: FString::Printf(TEXT("The ledger refused the trade: %s"), *Receipt.Error)));
	Open();
}

void UGbShopWidget::HandleApiError(const FString& What, int32 StatusCode, const FString& Message)
{
	if (What == TEXT("shop/buy") || What == TEXT("shop/sell"))
	{
		bBusy = false;
		Status->SetText(FText::FromString(FString::Printf(TEXT("The Broker: %s"), *Message)));
		Redraw();
	}
	else if (What == TEXT("shop"))
	{
		Purse->SetText(FText::FromString(FString::Printf(TEXT("Could not reach the shop: %s"), *Message)));
	}
}

void UGbShopWidget::HandleClose() { OnClose.Broadcast(); }

// ================================================================ the Rite (rules panel)

void UGbRulesWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Card = MakePanel(PaperPanel(), FMargin(22.f, 16.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Sized(Col, 600.f, 0.f));

	AddRow(Col, MakeText(TEXT("The Rite"), 22, C::Ink, true));
	Info = MakeText(TEXT("Asking the Rite..."), 13, C::Ink);
	AddRow(Col, Info, 2.f, 8.f);

	AddRow(Col, MakeText(TEXT("In force"), 14, C::Ink, true), 4.f);
	ActiveList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UScrollBox* ActiveScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ActiveScroll->AddChild(ActiveList);
	AddRow(Col, Sized(ActiveScroll, 0.f, 120.f));

	AddRow(Col, MakeText(TEXT("Propose a rule"), 14, C::Ink, true), 10.f);

	UTextBlock* Ignored = nullptr;
	auto Arrow = [this, &Ignored](const TCHAR* Label) { return MakeButton(Label, Ignored, C::Stone); };

	// Template row: < R8 >
	UHorizontalBox* TemplateRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* PrevT = Arrow(TEXT("<"));
	UButton* NextT = Arrow(TEXT(">"));
	PrevT->OnClicked.AddDynamic(this, &UGbRulesWidget::HandlePrevTemplate);
	NextT->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleNextTemplate);
	AddCol(TemplateRow, MakeText(TEXT("Rule"), 13, Dim()), false);
	AddCol(TemplateRow, Sized(PrevT, 36.f, 32.f), false, 4.f);
	TemplateId = MakeText(TEXT("-"), 14, C::Ink, true);
	AddCol(TemplateRow, Sized(TemplateId, 60.f, 0.f), false, 4.f);
	AddCol(TemplateRow, Sized(NextT, 36.f, 32.f), false);
	AddRow(Col, TemplateRow, 4.f);

	// Parameter rows: key  < value >. Buttons bind by function name (AddDynamic needs a literal member).
	static const TCHAR* PrevName[MaxParams] = { TEXT("HandlePrev0"), TEXT("HandlePrev1"), TEXT("HandlePrev2") };
	static const TCHAR* NextName[MaxParams] = { TEXT("HandleNext0"), TEXT("HandleNext1"), TEXT("HandleNext2") };
	for (int32 i = 0; i < MaxParams; ++i)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Key = MakeText(TEXT(""), 13, Dim());
		UTextBlock* Value = MakeText(TEXT(""), 14, C::Ink, true);
		UButton* P = Arrow(TEXT("<"));
		UButton* N = Arrow(TEXT(">"));
		FScriptDelegate PD;
		PD.BindUFunction(this, FName(PrevName[i]));
		P->OnClicked.Add(PD);
		FScriptDelegate ND;
		ND.BindUFunction(this, FName(NextName[i]));
		N->OnClicked.Add(ND);
		AddCol(Row, Sized(Key, 110.f, 0.f), false);
		AddCol(Row, Sized(P, 36.f, 32.f), false, 4.f);
		AddCol(Row, Sized(Value, 260.f, 0.f), false, 4.f);
		AddCol(Row, Sized(N, 36.f, 32.f), false);
		AddRow(Col, Row, 2.f);
		ParamRows.Add(Row);
		ParamKeys.Add(Key);
		ParamValues.Add(Value);
	}

	Preview = MakeText(TEXT(""), 16, C::Ink, true);
	Preview->SetAutoWrapText(true);
	AddRow(Col, Preview, 8.f, 4.f);

	UTextBlock* ProposeLabel = nullptr;
	ProposeButton = MakeButton(TEXT("Propose"), ProposeLabel, C::Gold);
	ProposeButton->OnClicked.AddDynamic(this, &UGbRulesWidget::HandlePropose);
	AddRow(Col, Sized(ProposeButton, 0.f, 40.f), 4.f);

	Status = MakeText(TEXT(""), 13, C::Ink);
	Status->SetAutoWrapText(true);
	AddRow(Col, Status, 6.f);

	UButton* Close = MakeButton(TEXT("Close  (R)"), Ignored, C::Stone);
	Close->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleClose);
	AddRow(Col, Sized(Close, 0.f, 40.f), 10.f);

	Centre(Root, Card);
}

void UGbRulesWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGbApiSubsystem* A = Api())
	{
		A->OnRules.AddUniqueDynamic(this, &UGbRulesWidget::HandleRules);
		A->OnRuleVerdict.AddUniqueDynamic(this, &UGbRulesWidget::HandleVerdict);
		A->OnRuleReceipt.AddUniqueDynamic(this, &UGbRulesWidget::HandleRuleReceipt);
		A->OnError.AddUniqueDynamic(this, &UGbRulesWidget::HandleApiError);
	}
}

void UGbRulesWidget::NativeDestruct()
{
	if (UGbApiSubsystem* A = Api())
	{
		A->OnRules.RemoveDynamic(this, &UGbRulesWidget::HandleRules);
		A->OnRuleVerdict.RemoveDynamic(this, &UGbRulesWidget::HandleVerdict);
		A->OnRuleReceipt.RemoveDynamic(this, &UGbRulesWidget::HandleRuleReceipt);
		A->OnError.RemoveDynamic(this, &UGbRulesWidget::HandleApiError);
	}
	Super::NativeDestruct();
}

void UGbRulesWidget::Open()
{
	if (UGbApiSubsystem* A = Api()) A->FetchRules();
}

const FGbRuleTemplate* UGbRulesWidget::Current() const
{
	return Data.Templates.IsValidIndex(TemplateIndex) ? &Data.Templates[TemplateIndex] : nullptr;
}

void UGbRulesWidget::HandleRules(const FGbRulesOverview& Rules)
{
	const FString Was = Current() ? Current()->Id : FString();
	Data = Rules;
	// Keep the template being edited when the list comes back.
	const int32 Same = Data.Templates.IndexOfByPredicate([&Was](const FGbRuleTemplate& T) { return T.Id == Was; });
	if (Same == INDEX_NONE)
	{
		TemplateIndex = 0;
		Choice.Init(0, MaxParams);
	}
	else
	{
		TemplateIndex = Same;
	}
	if (Choice.Num() != MaxParams) Choice.Init(0, MaxParams);

	FString Line = FString::Printf(TEXT("The next rule costs %d points"), Data.Cost);
	if (Data.Points >= 0) Line += FString::Printf(TEXT("  |  you have %d"), Data.Points);
	if (!Data.bChain) Line = TEXT("This server runs off-chain, so no rules can be proposed.");
	Info->SetText(FText::FromString(Line));

	ActiveList->ClearChildren();
	if (Data.Active.Num() == 0) ActiveList->AddChild(MakeText(TEXT("No rules yet. The Rite is waiting."), 13, Dim()));
	for (const FGbActiveRule& R : Data.Active)
	{
		const FString Who = R.Author.Contains(TEXT(".")) ? R.Author.Left(R.Author.Find(TEXT("."))) : R.Author;
		UTextBlock* T = MakeText(FString::Printf(TEXT("#%d  %s   (%s, %d pts)"), R.Id, *R.Text, *Who, R.Cost), 13, C::Ink);
		T->SetAutoWrapText(true);
		ActiveList->AddChild(T);
	}
	Redraw();
}

void UGbRulesWidget::Redraw()
{
	const FGbRuleTemplate* T = Current();
	TemplateId->SetText(FText::FromString(T ? T->Id : TEXT("-")));
	FString Text = T ? T->Text : FString();
	for (int32 i = 0; i < MaxParams; ++i)
	{
		const bool bUsed = T && T->Params.IsValidIndex(i) && T->Params[i].Options.Num() > 0;
		ParamRows[i]->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bUsed) continue;
		const FGbRuleParam& P = T->Params[i];
		Choice[i] = FMath::Clamp(Choice[i], 0, P.Options.Num() - 1);
		const FGbRuleOption& O = P.Options[Choice[i]];
		ParamKeys[i]->SetText(FText::FromString(Capitalised(P.Key)));
		ParamValues[i]->SetText(FText::FromString(O.Label));
		Text = Text.Replace(*FString::Printf(TEXT("{%s}"), *P.Key), *O.Label);
	}
	if (!Text.IsEmpty()) Text = Text.Left(1).ToUpper() + Text.Mid(1);
	if (T && T->bCruel) Text += TEXT("   (cruel: angers the Ward)");
	Preview->SetText(FText::FromString(Text));
	ProposeButton->SetIsEnabled(!bBusy && T && Data.bChain);
}

void UGbRulesWidget::StepTemplate(int32 Delta)
{
	if (bBusy || Data.Templates.Num() == 0) return;
	TemplateIndex = (TemplateIndex + Delta + Data.Templates.Num()) % Data.Templates.Num();
	Choice.Init(0, MaxParams);
	Redraw();
}

void UGbRulesWidget::StepParam(int32 Index, int32 Delta)
{
	const FGbRuleTemplate* T = Current();
	if (bBusy || !T || !T->Params.IsValidIndex(Index)) return;
	const int32 N = T->Params[Index].Options.Num();
	if (N == 0) return;
	Choice[Index] = (Choice[Index] + Delta + N) % N;
	Redraw();
}

void UGbRulesWidget::SetBusy(bool bNowBusy)
{
	bBusy = bNowBusy;
	Redraw();
}

void UGbRulesWidget::HandlePropose()
{
	const FGbRuleTemplate* T = Current();
	UGbApiSubsystem* A = Api();
	if (bBusy || !T || !A) return;
	TMap<FString, FString> Params;
	for (int32 i = 0; i < T->Params.Num() && i < MaxParams; ++i)
	{
		const FGbRuleParam& P = T->Params[i];
		if (P.Options.IsValidIndex(Choice[i])) Params.Add(P.Key, P.Options[Choice[i]].Value);
	}
	PendingJob.Reset();
	Status->SetText(FText::FromString(TEXT("Kessa is reading your rule...")));
	SetBusy(true);
	A->ProposeRule(T->Id, Params);
}

void UGbRulesWidget::HandleVerdict(const FGbRuleVerdict& Verdict)
{
	PendingJob = Verdict.JobId;
	Status->SetText(FText::FromString(Verdict.bAccepted
		? FString::Printf(TEXT("Kessa: \"%s\" is accepted. Writing it into the Rite on NEAR..."), *Verdict.Text)
		: FString::Printf(TEXT("Kessa rejects it: %s  Writing her verdict on NEAR (your %d points come back)..."), *Verdict.Reason, Verdict.Cost)));
}

void UGbRulesWidget::HandleRuleReceipt(const FString& JobId, const FGbChainReceipt& Receipt)
{
	if (JobId != PendingJob) return;
	PendingJob.Reset();
	const FString Was = Status->GetText().ToString().Replace(TEXT("Writing it into the Rite on NEAR..."), TEXT("")).Replace(TEXT("Writing her verdict on NEAR"), TEXT("On NEAR"));
	Status->SetText(FText::FromString(Receipt.Status == TEXT("done")
		? FString::Printf(TEXT("%s  Recorded on NEAR %s as rule %d. (L to see it)"), *Was, *Receipt.Network, Receipt.RuleId)
		: FString::Printf(TEXT("The ledger refused it: %s"), *Receipt.Error)));
	SetBusy(false);
	Open();
}

void UGbRulesWidget::HandleApiError(const FString& What, int32 StatusCode, const FString& Message)
{
	if (What == TEXT("rules/propose"))
	{
		Status->SetText(FText::FromString(FString::Printf(TEXT("Kessa: %s"), *Message)));
		SetBusy(false);
	}
	else if (What == TEXT("rules"))
	{
		Info->SetText(FText::FromString(FString::Printf(TEXT("Could not reach the Rite: %s"), *Message)));
	}
}

void UGbRulesWidget::HandlePrevTemplate() { StepTemplate(-1); }
void UGbRulesWidget::HandleNextTemplate() { StepTemplate(1); }
void UGbRulesWidget::HandlePrev0() { StepParam(0, -1); }
void UGbRulesWidget::HandleNext0() { StepParam(0, 1); }
void UGbRulesWidget::HandlePrev1() { StepParam(1, -1); }
void UGbRulesWidget::HandleNext1() { StepParam(1, 1); }
void UGbRulesWidget::HandlePrev2() { StepParam(2, -1); }
void UGbRulesWidget::HandleNext2() { StepParam(2, 1); }
void UGbRulesWidget::HandleClose() { OnClose.Broadcast(); }
void UGbTalismanCaseWidget::HandleClose() { OnClose.Broadcast(); }
