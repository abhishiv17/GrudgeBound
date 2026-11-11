#include "GbVowWidget.h"
#include "GbApiSubsystem.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "GbGreyboxMaterials.h"
#include "GbUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	namespace VowColor = GbGreybox::Color;

	UVerticalBoxSlot* VowRow(UVerticalBox* Box, UWidget* W, float Top = 2.f, float Bottom = 2.f)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
		S->SetPadding(FMargin(0.f, Top, 0.f, Bottom));
		S->SetHorizontalAlignment(HAlign_Fill);
		return S;
	}

	UHorizontalBoxSlot* VowCol(UHorizontalBox* Box, UWidget* W, bool bFill, float Right = 8.f)
	{
		UHorizontalBoxSlot* S = Box->AddChildToHorizontalBox(W);
		S->SetPadding(FMargin(0.f, 0.f, Right, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
		if (bFill) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return S;
	}
}

void UGbVowWidget::BuildTree()
{
	UTextBlock* TitleText = nullptr; UHorizontalBox* Footer = nullptr;
	UVerticalBox* Body = MakeShell(TEXT("The Vow Shrine"), TEXT("wax_seal"), 1320.f, TitleText, Info, Footer);
	Info->SetText(FText::FromString(TEXT("Reading the Rite's ledger...")));

	UTextBlock* Ignored = nullptr;
	const auto Arrow = [this, &Ignored](const TCHAR* Label)
	{
		UButton* B = MakeKeyButton(Label, FString(), Ignored, GbUi::Paper2());
		StepButtons.Add(B);
		return B;
	};
	UHorizontalBox* Cols = WidgetTree->ConstructWidget<UHorizontalBox>();

	// Left: the Shrine's terms, the vow builder and the promise as it will be written.
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	VowRow(Left, MakeSection(TEXT("Swear a vow")), 0.f, 8.f);
	UTextBlock* Terms = MakeText(TEXT("Keep a promise until its end: your stake returns with a fifth added. Release it early: the stake burns. Promised actions stay closed while the vow holds."), 17, VowColor::Ink);
	Terms->SetAutoWrapText(true);
	VowRow(Left, Terms, 0.f, 12.f);

	UButton* PrevT = Arrow(TEXT("<")); UButton* NextT = Arrow(TEXT(">"));
	PrevT->OnClicked.AddDynamic(this, &UGbVowWidget::HandlePrevTemplate);
	NextT->OnClicked.AddDynamic(this, &UGbVowWidget::HandleNextTemplate);
	TemplateText = MakeText(TEXT("-"), 20, VowColor::Ink, true);
	VowRow(Left, MakeStepper(MakeText(TEXT("Vow"), 18, VowColor::Ink, true), PrevT, TemplateText, NextT), 0.f, 8.f);

	UButton* PrevP = Arrow(TEXT("<")); UButton* NextP = Arrow(TEXT(">"));
	PrevP->OnClicked.AddDynamic(this, &UGbVowWidget::HandlePrevParam);
	NextP->OnClicked.AddDynamic(this, &UGbVowWidget::HandleNextParam);
	ParamKey = MakeText(TEXT(""), 18, VowColor::Ink, true);
	ParamText = MakeText(TEXT("-"), 20, VowColor::Ink, true);
	ParamRow = MakeStepper(ParamKey, PrevP, ParamText, NextP);
	VowRow(Left, ParamRow, 0.f, 8.f);

	UButton* PrevD = Arrow(TEXT("<")); UButton* NextD = Arrow(TEXT(">"));
	PrevD->OnClicked.AddDynamic(this, &UGbVowWidget::HandlePrevDays);
	NextD->OnClicked.AddDynamic(this, &UGbVowWidget::HandleNextDays);
	DaysText = MakeText(TEXT("-"), 20, VowColor::Ink, true);
	VowRow(Left, MakeStepper(MakeText(TEXT("Town days"), 18, VowColor::Ink, true), PrevD, DaysText, NextD), 0.f, 8.f);

	UBorder* Promise = MakePanel(GbUi::PaperHi(), FMargin(16.f, 14.f));
	UHorizontalBox* PromiseRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Promise->SetContent(PromiseRow);
	UImage* Seal = nullptr;
	UWidget* SealArt = MakeArt(TEXT("wax_seal"), 40.f, Seal);
	VowCol(PromiseRow, SealArt, false, 14.f)->SetVerticalAlignment(VAlign_Top);
	Preview = MakeText(TEXT(""), 19, VowColor::Ink, true);
	Preview->SetAutoWrapText(true);
	VowCol(PromiseRow, Preview, true, 0.f)->SetVerticalAlignment(VAlign_Top);
	VowRow(Left, Sized(Promise, 0.f, 120.f), 4.f, 12.f);

	UTextBlock* Label = nullptr;
	Swear = MakeKeyButton(TEXT("Swear this vow"), FString(), Label, VowColor::Crimson);
	Swear->OnClicked.AddDynamic(this, &UGbVowWidget::HandleSwear);
	VowRow(Left, Sized(Swear, 0.f, 56.f));
	VowCol(Cols, Left, true, 28.f)->SetVerticalAlignment(VAlign_Top);

	// Right: every promise on the ledger, and the release stepper for the ones still holding.
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	VowRow(Right, MakeSection(TEXT("Your vows")), 0.f, 8.f);
	History = WidgetTree->ConstructWidget<UScrollBox>();
	VowRow(Right, Sized(History, 0.f, 360.f), 0.f, 12.f);

	UBorder* ReleaseStrip = MakePanel(GbUi::Paper2(), FMargin(14.f, 10.f));
	UVerticalBox* ReleaseCol = WidgetTree->ConstructWidget<UVerticalBox>();
	ReleaseStrip->SetContent(ReleaseCol);
	UButton* PrevA = Arrow(TEXT("<")); UButton* NextA = Arrow(TEXT(">"));
	PrevA->OnClicked.AddDynamic(this, &UGbVowWidget::HandlePrevActive);
	NextA->OnClicked.AddDynamic(this, &UGbVowWidget::HandleNextActive);
	ActiveText = MakeText(TEXT("-"), 18, VowColor::Ink, true);
	VowRow(ReleaseCol, MakeStepper(MakeText(TEXT("Vow"), 18, VowColor::Ink, true), PrevA, ActiveText, NextA, 80.f), 0.f, 8.f);
	Release = MakeKeyButton(TEXT("Release selected vow (burn its stake)"), FString(), Label, FLinearColor(0.65f, 0.34f, 0.26f));
	Label->SetFont(GbUi::Font(18, true));
	Release->OnClicked.AddDynamic(this, &UGbVowWidget::HandleRelease);
	VowRow(ReleaseCol, Sized(Release, 0.f, 52.f));
	ReleaseRow = ReleaseStrip;
	VowRow(Right, ReleaseStrip);
	VowCol(Cols, Right, true, 0.f)->SetVerticalAlignment(VAlign_Top);
	VowRow(Body, Cols);

	// Sealing progress and receipts go in the shell's ink note.
	Status = ShellStatus;

	UButton* Close = MakeKeyButton(TEXT("Return to the Ward"), KeyName(TEXT("Vows")), Label, GbUi::Paper2());
	Close->OnClicked.AddDynamic(this, &UGbVowWidget::HandleClose);
	VowCol(Footer, Sized(Close, 390.f, 56.f), false, 0.f);
}

void UGbVowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGbApiSubsystem* A = Api())
	{
		A->OnVows.AddUniqueDynamic(this, &UGbVowWidget::HandleVows);
		A->OnVowReceipt.AddUniqueDynamic(this, &UGbVowWidget::HandleReceipt);
		A->OnError.AddUniqueDynamic(this, &UGbVowWidget::HandleError);
	}
}

void UGbVowWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	if (UGbApiSubsystem* A = Api())
	{
		A->OnVows.RemoveDynamic(this, &UGbVowWidget::HandleVows);
		A->OnVowReceipt.RemoveDynamic(this, &UGbVowWidget::HandleReceipt);
		A->OnError.RemoveDynamic(this, &UGbVowWidget::HandleError);
	}
	Super::NativeDestruct();
}

void UGbVowWidget::Open()
{
	Data = FGbVowsOverview(); bBusy = false; bReady = false;
	History->ClearChildren(); Slips.Reset(); SlipIds.Reset(); ActiveIds.Reset();
	TemplateIndex = 0; ActiveIndex = 0;
	Info->SetText(FText::FromString(TEXT("Reading the Rite's ledger..."))); Status->SetText(FText::GetEmpty());
	RedrawChoices();
	if (UGbApiSubsystem* A = Api()) A->FetchVows();
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsVisible() && !bBusy) if (UGbApiSubsystem* A = Api()) A->FetchVows();
	}), 5.f, true);
}

const FGbVowTemplate* UGbVowWidget::Current() const
{
	return Data.Templates.IsValidIndex(TemplateIndex) ? &Data.Templates[TemplateIndex] : nullptr;
}

FString UGbVowWidget::ParamValue() const { return ParamValues.IsValidIndex(ParamIndex) ? ParamValues[ParamIndex] : FString(); }
FString UGbVowWidget::ParamLabel() const { return ParamLabels.IsValidIndex(ParamIndex) ? ParamLabels[ParamIndex] : FString(); }

void UGbVowWidget::HandleVows(const FGbVowsOverview& Overview)
{
	// Keep the vow being built, and the vow picked for release, across the 5 s refresh.
	const FString Previous = Current() ? Current()->Id : FString();
	const int32 PickedVow = ActiveIds.IsValidIndex(ActiveIndex) ? ActiveIds[ActiveIndex] : INDEX_NONE;
	const int32 OldPoints = Data.Points;
	Data = Overview; bReady = true;
	TemplateIndex = FMath::Max(0, Data.Templates.IndexOfByPredicate([&Previous](const FGbVowTemplate& T) { return T.Id == Previous; }));
	Info->SetText(FText::FromString(Data.bChain
		? FString::Printf(TEXT("Your points: %d  |  One town day: %.0f minutes"), Data.Points, Data.DaySeconds / 60.)
		: FString(TEXT("The Shrine is closed until the Rite's ledger is connected."))));

	History->ClearChildren(); Slips.Reset(); SlipIds.Reset(); ActiveIds.Reset();
	if (Data.Vows.IsEmpty()) History->AddChild(MakeText(TEXT("No promises written yet."), 17, VowColor::Ink));
	for (const FGbVow& V : Data.Vows)
	{
		const bool bActive = V.Status == TEXT("Active");
		const int64 Seconds = FMath::Max<int64>(0, static_cast<int64>(V.EndsAt / 1000.) - FDateTime::UtcNow().ToUnixTimestamp());
		const FString Time = bActive ? FString::Printf(TEXT(" | %lldm %llds left"), Seconds / 60, Seconds % 60) : FString();
		UBorder* Slip = MakePanel(GbUi::PaperHi(), FMargin(14.f, 10.f));
		UHorizontalBox* In = WidgetTree->ConstructWidget<UHorizontalBox>();
		Slip->SetContent(In);
		UTextBlock* ChipText = nullptr;
		UBorder* Chip = MakeChip(V.Status, bActive ? VowColor::Gold : V.Status == TEXT("Kept") ? VowColor::Leaf : GbUi::Paper3(), ChipText);
		VowCol(In, Chip, false, 12.f)->SetVerticalAlignment(VAlign_Top);
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Words = MakeText(V.Text, 18, VowColor::Ink, true);
		Words->SetAutoWrapText(true);
		VowRow(Lines, Words, 0.f, 4.f);
		VowRow(Lines, MakeText(FString::Printf(TEXT("stake %d, kept return %d%s"), V.Stake, V.Payout, *Time), 15, VowColor::Ink));
		VowCol(In, Lines, true, 0.f);
		if (UScrollBoxSlot* SlipSlot = Cast<UScrollBoxSlot>(History->AddChild(Slip))) SlipSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
		Slips.Add(Slip); SlipIds.Add(V.Id);
		if (bActive) ActiveIds.Add(V.Id);
	}
	ActiveIndex = FMath::Max(0, ActiveIds.IndexOfByKey(PickedVow));
	RedrawChoices();
	if (Data.Points != OldPoints) if (UGbPlayerSubsystem* P = Player()) P->RefreshFromChain();
}

void UGbVowWidget::RedrawChoices()
{
	const FGbVowTemplate* T = Current();
	const FString OldParameter = ParamValue();
	const int32 OldDays = DayOptions.IsValidIndex(DaysIndex) ? DayOptions[DaysIndex] : INDEX_NONE;
	ParamValues.Reset(); ParamLabels.Reset(); DayOptions.Reset();
	FString Key;
	if (T)
	{
		if (T->Id == TEXT("V1")) { Key = TEXT("Ward"); ParamValues.Add(GetDefault<UGbSettings>()->WardId); ParamLabels.Add(GetDefault<UGbSettings>()->WardId); } // Vow is made where you stand.
		if (T->Id == TEXT("V3"))
		{
			Key = TEXT("Talisman");
			for (const FGbRuleOption& Token : Data.Talismans)
			{
				ParamValues.Add(Token.Value);
				ParamLabels.Add(Token.Label.IsEmpty() ? Token.Value : FString::Printf(TEXT("%s (%s)"), *Token.Label, *Token.Value));
			}
		}
		if (T->Id == TEXT("V4")) { Key = TEXT("Omen"); for (const TCHAR* Omen : { TEXT("whisper"), TEXT("wail"), TEXT("calamity") }) { ParamValues.Add(Omen); ParamLabels.Add(Omen); } }
		for (int32 D = T->MinDays; D <= T->MaxDays; ++D) DayOptions.Add(D);
	}
	ParamIndex = FMath::Max(0, ParamValues.IndexOfByKey(OldParameter));
	DaysIndex = FMath::Max(0, DayOptions.IndexOfByKey(OldDays));
	ParamKey->SetText(FText::FromString(Key));
	ParamRow->SetVisibility(T && T->Id != TEXT("V2") ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RedrawPreview();
}

void UGbVowWidget::RedrawPreview()
{
	const FGbVowTemplate* T = Current();
	const FString Param = ParamLabel();
	const FString DaysWord = DayOptions.IsValidIndex(DaysIndex) ? FString::FromInt(DayOptions[DaysIndex]) : FString();
	TemplateText->SetText(FText::FromString(T ? T->Id : TEXT("-")));
	ParamText->SetText(FText::FromString(Param.IsEmpty() ? TEXT("-") : Param));
	DaysText->SetText(FText::FromString(DaysWord.IsEmpty() ? TEXT("-") : DaysWord));
	FString Words = T ? T->Text : FString();
	Words = Words.Replace(TEXT("{ward}"), *Param).Replace(TEXT("{talisman}"), *Param).Replace(TEXT("{omen}"), *Param);
	Preview->SetText(FText::FromString(T ? FString::Printf(TEXT("%s, for %s town days. Stake %d points; keep it to receive %d."), *Words, *DaysWord, T->Stake, T->Stake + T->Stake * 20 / 100) : TEXT("")));
	const bool bHasParam = T && (T->Id == TEXT("V2") || ParamValues.IsValidIndex(ParamIndex));
	Swear->SetIsEnabled(bReady && !bBusy && Data.bChain && T && bHasParam && Data.Points >= T->Stake);
	Release->SetIsEnabled(bReady && !bBusy && Data.bChain && ActiveIds.Num() > 0);
	for (UButton* B : StepButtons) if (B) B->SetIsEnabled(!bBusy);

	ReleaseRow->SetVisibility(ActiveIds.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const int32 Marked = ActiveIds.IsValidIndex(ActiveIndex) ? ActiveIds[ActiveIndex] : INDEX_NONE;
	const FGbVow* Picked = Data.Vows.FindByPredicate([Marked](const FGbVow& V) { return V.Id == Marked; });
	ActiveText->SetText(FText::FromString(Picked ? FString::Printf(TEXT("%d of %d"), ActiveIndex + 1, ActiveIds.Num()) : FString(TEXT("-"))));
	for (int32 i = 0; i < Slips.Num(); ++i) if (Slips[i]) Slips[i]->SetBrushColor(SlipIds[i] == Marked ? GbUi::Paper2() : GbUi::PaperHi());
}

void UGbVowWidget::Step(int32& Index, int32 Count, int32 Delta, bool bChoices)
{
	if (bBusy || Count == 0) return;
	Index = (Index + Delta + Count) % Count;
	if (bChoices) RedrawChoices(); else RedrawPreview();
}

void UGbVowWidget::HandlePrevTemplate() { Step(TemplateIndex, Data.Templates.Num(), -1, true); }
void UGbVowWidget::HandleNextTemplate() { Step(TemplateIndex, Data.Templates.Num(), 1, true); }
void UGbVowWidget::HandlePrevParam() { Step(ParamIndex, ParamValues.Num(), -1, false); }
void UGbVowWidget::HandleNextParam() { Step(ParamIndex, ParamValues.Num(), 1, false); }
void UGbVowWidget::HandlePrevDays() { Step(DaysIndex, DayOptions.Num(), -1, false); }
void UGbVowWidget::HandleNextDays() { Step(DaysIndex, DayOptions.Num(), 1, false); }
void UGbVowWidget::HandlePrevActive() { Step(ActiveIndex, ActiveIds.Num(), -1, false); }
void UGbVowWidget::HandleNextActive() { Step(ActiveIndex, ActiveIds.Num(), 1, false); }

void UGbVowWidget::HandleSwear()
{
	const FGbVowTemplate* T = Current(); UGbApiSubsystem* A = Api();
	if (!T || !A || bBusy || !Swear->GetIsEnabled()) return;
	TMap<FString, FString> Params;
	if (T->Id == TEXT("V1")) Params.Add(TEXT("ward"), ParamValue());
	if (T->Id == TEXT("V3")) Params.Add(TEXT("talisman"), ParamValue());
	if (T->Id == TEXT("V4")) Params.Add(TEXT("omen"), ParamValue());
	bBusy = true; RedrawPreview(); Status->SetText(FText::FromString(TEXT("Sealing your promise in the Rite's ledger...")));
	A->SwearVow(T->Id, DayOptions.IsValidIndex(DaysIndex) ? DayOptions[DaysIndex] : 0, Params);
}

void UGbVowWidget::HandleRelease()
{
	if (bBusy || !ActiveIds.IsValidIndex(ActiveIndex) || !Api()) return;
	bBusy = true; RedrawPreview(); Status->SetText(FText::FromString(TEXT("Releasing your promise. Its stake will burn...")));
	Api()->ReleaseVow(ActiveIds[ActiveIndex]);
}

void UGbVowWidget::HandleReceipt(const FString&, const FGbChainReceipt& Receipt)
{
	bBusy = false; RedrawPreview();
	Status->SetText(FText::FromString(Receipt.Status == TEXT("done") ? TEXT("Sealed in the Rite's ledger. L opens the proof.") : TEXT("The Rite could not record that change. Refreshing your vows...")));
}

void UGbVowWidget::HandleError(const FString& What, int32, const FString& Message)
{
	if (What != TEXT("vows")) return;
	bBusy = false; bReady = false; RedrawPreview(); Status->SetText(FText::FromString(Message));
}

void UGbVowWidget::HandleClose() { GetWorld()->GetTimerManager().ClearTimer(RefreshTimer); OnClose.Broadcast(); }
