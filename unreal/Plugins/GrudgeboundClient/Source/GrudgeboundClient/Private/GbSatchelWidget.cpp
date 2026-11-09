#include "GbSatchelWidget.h"
#include "GbApiSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"

namespace
{
	const FLinearColor Paper(.86f, .80f, .65f), Ink(.12f, .10f, .09f), Gold(.72f, .56f, .25f), Red(.62f, .20f, .17f), Blue(.18f, .34f, .44f);
}

void UGbSatchelWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Root;
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
	UBorder* Card = MakePanel(Paper, FMargin(24));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Sized(Col, 760, 660)); Fit->AddChild(Card);
	UCanvasPanelSlot* FitSlot = Root->AddChildToCanvas(Fit);
	FitSlot->SetAnchors(FAnchors(.04f, .04f, .96f, .96f)); FitSlot->SetOffsets(FMargin(0));
	Col->AddChildToVerticalBox(MakeText(TEXT("Your Satchel"), 28, Ink, true));
	Col->AddChildToVerticalBox(MakeText(TEXT("Five talismans at hand. The rest wait in the Case."), 14, Ink));
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(); Col->AddChildToVerticalBox(Sized(Tabs, 0, 40));
	UTextBlock* Label = nullptr;
	PocketTabs.Reset();
	UButton* B = MakeButton(TEXT("Talismans"), Label, Gold); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowTalismans); Tabs->AddChildToHorizontalBox(Sized(B, 190, 36)); PocketTabs.Add(B);
	B = MakeButton(TEXT("Supplies"), Label, Paper); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowSupplies); Tabs->AddChildToHorizontalBox(Sized(B, 190, 36)); PocketTabs.Add(B);
	B = MakeButton(TEXT("Keepsakes"), Label, Paper); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowKeepsakes); Tabs->AddChildToHorizontalBox(Sized(B, 190, 36)); PocketTabs.Add(B);
	B = MakeButton(TEXT("Ledger"), Label, Paper); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowLedger); Tabs->AddChildToHorizontalBox(Sized(B, 190, 36)); PocketTabs.Add(B);
	Summary = MakeText(TEXT("Reading your pockets..."), 14, Ink, true); Col->AddChildToVerticalBox(Sized(Summary, 0, 28));
	Pockets = WidgetTree->ConstructWidget<UWidgetSwitcher>(); Col->AddChildToVerticalBox(Sized(Pockets, 0, 350));
	UVerticalBox* Tokens = WidgetTree->ConstructWidget<UVerticalBox>(); Pockets->AddChild(Tokens);
	Selection = WidgetTree->ConstructWidget<UComboBoxString>(); Tokens->AddChildToVerticalBox(Sized(Selection, 0, 30));
	Selection->OnSelectionChanged.AddDynamic(this, &UGbSatchelWidget::HandleSelection);
	SelectedInfo = MakeText(TEXT("Choose a talisman."), 14, Ink); Tokens->AddChildToVerticalBox(Sized(SelectedInfo, 0, 26));
	InkBar = MakeBar(Blue); Tokens->AddChildToVerticalBox(Sized(InkBar, 0, 10));
	InkText = MakeText(TEXT(""), 14, Ink); Tokens->AddChildToVerticalBox(Sized(InkText, 0, 26));
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(); Tokens->AddChildToVerticalBox(Sized(Actions, 0, 38));
	CarryButton = MakeButton(TEXT("Carry"), Label, Gold); CarryLabel = Label; CarryButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::CarrySelected); Actions->AddChildToHorizontalBox(Sized(CarryButton, 380, 34));
	PartnerButton = MakeButton(TEXT("Choose as partner"), Label, Paper); PartnerButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::ChoosePartner); Actions->AddChildToHorizontalBox(Sized(PartnerButton, 380, 34));
	Rows = WidgetTree->ConstructWidget<UScrollBox>(); Tokens->AddChildToVerticalBox(Sized(Rows, 0, 180));
	UHorizontalBox* Healing = WidgetTree->ConstructWidget<UHorizontalBox>(); Tokens->AddChildToVerticalBox(Sized(Healing, 0, 40));
	MenderButton = MakeButton(TEXT("Mender"), Label, Paper); MenderLabel = Label; MenderButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::RestoreInk); Healing->AddChildToHorizontalBox(Sized(MenderButton, 380, 36));
	RestButton = MakeButton(TEXT("Rest until morning (free)"), Label, Paper); RestButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::Rest); Healing->AddChildToHorizontalBox(Sized(RestButton, 380, 36));
	UVerticalBox* Supplies = WidgetTree->ConstructWidget<UVerticalBox>(); Pockets->AddChild(Supplies);
	SupplyText = MakeText(TEXT(""), 22, Ink, true); Supplies->AddChildToVerticalBox(Sized(SupplyText, 0, 125));
	UTextBlock* SupplyHelp = MakeText(TEXT("Each seal attempt uses one blank or gilded talisman. An Ink Salve restores part of a talisman's ink."), 15, Ink);
	SupplyHelp->SetWrapTextAt(740); Supplies->AddChildToVerticalBox(Sized(SupplyHelp, 0, 60));
	SupplyTarget = MakeText(TEXT("Choose a talisman in the Talismans pocket."), 16, Ink); Supplies->AddChildToVerticalBox(Sized(SupplyTarget, 0, 55));
	SalveButton = MakeButton(TEXT("Use salve on selected talisman"), Label, Gold); SalveButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::UseSalve); Supplies->AddChildToVerticalBox(Sized(SalveButton, 0, 40));
	Keepsakes = WidgetTree->ConstructWidget<UScrollBox>(); Pockets->AddChild(Keepsakes);
	UVerticalBox* LedgerPocket = WidgetTree->ConstructWidget<UVerticalBox>(); Pockets->AddChild(LedgerPocket);
	Ledger = MakeText(TEXT(""), 22, Ink, true); LedgerPocket->AddChildToVerticalBox(Sized(Ledger, 0, 100));
	LedgerPocket->AddChildToVerticalBox(MakeText(TEXT("Debt notice"), 19, Ink, true));
	LedgerPocket->AddChildToVerticalBox(MakeText(TEXT("You arrived with a debt notice folded inside your coat."), 16, Ink));
	Status = MakeText(TEXT(""), 14, Ink); Status->SetWrapTextAt(740); Col->AddChildToVerticalBox(Sized(Status, 0, 48));
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(); Col->AddChildToVerticalBox(Sized(Footer, 0, 40));
	BrokerButton = MakeButton(TEXT("Broker"), Label, Gold); BrokerButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::Broker); Footer->AddChildToHorizontalBox(Sized(BrokerButton, 250, 38));
	B = MakeButton(TEXT("Refresh"), Label, Paper); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::Refresh); Footer->AddChildToHorizontalBox(Sized(B, 250, 38));
	B = MakeButton(TEXT("Return to the Ward (B)"), Label, Paper); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::Close); Footer->AddChildToHorizontalBox(Sized(B, 260, 38));
	Redraw();
}

void UGbSatchelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGbPlayerSubsystem* P = Player()) P->OnPlayerChanged.AddUniqueDynamic(this, &UGbSatchelWidget::HandlePlayerChanged);
	if (UGbApiSubsystem* A = Api())
	{
		A->OnError.AddUniqueDynamic(this, &UGbSatchelWidget::HandleError);
		A->OnSatchelAction.AddUniqueDynamic(this, &UGbSatchelWidget::HandleAction);
		A->OnWorldState.AddUniqueDynamic(this, &UGbSatchelWidget::HandleWorld);
	}
}

void UGbSatchelWidget::NativeDestruct()
{
	if (UGbPlayerSubsystem* P = Player()) P->OnPlayerChanged.RemoveDynamic(this, &UGbSatchelWidget::HandlePlayerChanged);
	if (UGbApiSubsystem* A = Api())
	{
		A->OnError.RemoveDynamic(this, &UGbSatchelWidget::HandleError);
		A->OnSatchelAction.RemoveDynamic(this, &UGbSatchelWidget::HandleAction);
		A->OnWorldState.RemoveDynamic(this, &UGbSatchelWidget::HandleWorld);
	}
	Super::NativeDestruct();
}

void UGbSatchelWidget::Open()
{
	ShowTalismans();
	Status->SetText(FText::FromString(TEXT("Checking your pockets...")));
	Refresh();
}

void UGbSatchelWidget::PullPlayer()
{
	if (const UGbPlayerSubsystem* P = Player())
	{
		Data = P->GetSatchel(); Talismans = P->GetTalismans(); PartnerToken = P->GetPartnerTokenId();
	}
	if (const UGbApiSubsystem* A = Api())
	{
		bBusy = A->IsSatchelBusy() || A->IsBattleOpen() || A->IsBattleBusy();
		Names.Reset();
		for (const FGbTalisman& T : Talismans)
		{
			const FGbCurseInfo C = A->GetCurse(T.SpeciesId);
			Names.Add(UGbPlayerSubsystem::ServerTokenId(T), C.Name.IsEmpty() || C.Name == T.SpeciesId
				? (T.Id == UGbPlayerSubsystem::StarterId ? FString(TEXT("Cinder Grin")) : FString(TEXT("Sealed talisman"))) : C.Name);
		}
		MenderPrice = MenderPriceFrom(A->GetWorldState());
	}
	Redraw();
}

int32 UGbSatchelWidget::MenderPriceFrom(const FGbWorldState& World)
{
	if (!World.bValid) return -1;
	for (const FGbWardSummary& W : World.Wards) if (W.Id == TEXT("lantern")) return FMath::RoundToInt(8.f * (1.f + W.Grudge.Sorrow / 100.f));
	return -1;
}

const FGbTalisman* UGbSatchelWidget::TalismanFor(const FString& Id) const
{
	return Talismans.FindByPredicate([&Id](const FGbTalisman& T) { return UGbPlayerSubsystem::ServerTokenId(T) == Id; });
}
FString UGbSatchelWidget::NameFor(const FString& Id) const
{
	const FString* Name = Names.Find(Id);
	return Name ? *Name : Id == TEXT("starter") ? FString(TEXT("Cinder Grin")) : FString(TEXT("Sealed talisman"));
}

void UGbSatchelWidget::Redraw()
{
	if (!Selection || !Pockets) return;
	for (int32 I = 0; I < PocketTabs.Num(); ++I) PocketTabs[I]->SetBackgroundColor(I == Pockets->GetActiveWidgetIndex() ? Gold : Paper);
	bUpdating = true; Options = Data.Carried;
	for (const FString& Id : Data.Case) Options.AddUnique(Id);
	if (!Options.Contains(SelectedToken)) SelectedToken = Options.Contains(PartnerToken) ? PartnerToken : Options.Num() ? Options[0] : FString();
	Selection->ClearOptions();
	for (int32 I = 0; I < Options.Num(); ++I)
	{
		const FString& Id = Options[I];
		Selection->AddOption(FString::Printf(TEXT("%s %d  |  %s%s"), Data.Carried.Contains(Id) ? TEXT("Carried") : TEXT("Case"), I + 1,
			*NameFor(Id), Id == PartnerToken ? TEXT("  (partner)") : TEXT("")));
	}
	if (Options.Contains(SelectedToken)) Selection->SetSelectedIndex(Options.IndexOfByKey(SelectedToken));
	bUpdating = false;
	Summary->SetText(FText::FromString(Data.bValid
		? FString::Printf(TEXT("Carried %d / 5    |    Case %d    |    Partner: %s"), Data.Carried.Num(), Data.Case.Num(), PartnerToken.IsEmpty() ? TEXT("Choose one") : *NameFor(PartnerToken))
		: FString(TEXT("Your Satchel is unavailable. Refresh before choosing a partner or using supplies."))));
	Rows->ClearChildren();
	for (int32 I = 0; I < Options.Num(); ++I)
	{
		const FString& Id = Options[I]; const FGbInk* Value = Data.Ink.Find(Id);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(Sized(MakeText(FString::Printf(TEXT("%s %d  |  %s%s"), Data.Carried.Contains(Id) ? TEXT("Carried") : TEXT("Case"), I + 1, *NameFor(Id), Id == PartnerToken ? TEXT("  (partner)") : TEXT("")), 13, Ink, Id == SelectedToken), 450, 26));
		UProgressBar* Bar = MakeBar(Value && Value->Hp <= 0 ? Red : Blue);
		Bar->SetPercent(Value && Value->MaxHp > 0 ? static_cast<float>(Value->Hp) / Value->MaxHp : 0.f);
		Row->AddChildToHorizontalBox(Sized(Bar, 180, 14));
		Row->AddChildToHorizontalBox(Sized(MakeText(Value ? FString::Printf(TEXT("  %d / %d ink"), Value->Hp, Value->MaxHp) : FString(TEXT("  Reading ink...")), 13, Ink), 130, 26));
		Rows->AddChild(Row);
	}
	if (!Options.Num()) Rows->AddChild(MakeText(TEXT("No talismans to show yet."), 14, Ink));
	const FGbInk* Value = Data.Ink.Find(SelectedToken);
	const bool bHasInk = Value && Value->MaxHp > 0;
	const bool bCarried = Data.Carried.Contains(SelectedToken);
	const bool bReady = Data.bValid && !bBusy;
	SelectedInfo->SetText(FText::FromString(SelectedToken.IsEmpty() ? FString(TEXT("Choose a talisman."))
		: NameFor(SelectedToken) + (bCarried ? TEXT("  |  In your Satchel") : TEXT("  |  In the Case"))));
	InkBar->SetPercent(bHasInk ? static_cast<float>(Value->Hp) / Value->MaxHp : 0.f);
	InkBar->SetFillColorAndOpacity(bHasInk && Value->Hp <= 0 ? Red : Blue);
	InkText->SetText(FText::FromString(bHasInk
		? FString::Printf(TEXT("Ink %d / %d%s"), Value->Hp, Value->MaxHp, Value->Hp <= 0 ? TEXT("  |  Visit the Mender or rest") : TEXT(""))
		: FString(TEXT("Reading ink..."))));
	CarryLabel->SetText(FText::FromString(bCarried ? TEXT("Leave in the Case") : TEXT("Carry in Satchel")));
	CarryButton->SetIsEnabled(bReady && (bCarried || Data.CanCarry(SelectedToken)));
	PartnerButton->SetIsEnabled(bReady && bCarried && SelectedToken != PartnerToken && TalismanFor(SelectedToken));
	SalveButton->SetIsEnabled(bReady && Data.CanUseSalve(SelectedToken));
	Selection->SetIsEnabled(bReady);
	MenderLabel->SetText(FText::FromString(MenderPrice >= 0 ? FString::Printf(TEXT("Mender: restore ink (%d Ember)"), MenderPrice) : FString(TEXT("Mender: price unavailable"))));
	MenderButton->SetIsEnabled(bReady && bHasInk && Value->Hp < Value->MaxHp && MenderPrice >= 0 && Data.Ember >= MenderPrice);
	RestButton->SetIsEnabled(bReady); BrokerButton->SetIsEnabled(bReady);
	SupplyText->SetText(FText::FromString(Data.bValid ? FString::Printf(TEXT("Blank talismans     %d\nGilded talismans    %d\nInk Salves               %d"), Data.Blank, Data.Gilded, Data.Salves) : FString(TEXT("Supplies unavailable"))));
	SupplyTarget->SetText(FText::FromString(SelectedToken.IsEmpty() ? FString(TEXT("Choose a talisman in the Talismans pocket."))
		: FString::Printf(TEXT("Selected: %s%s"), *NameFor(SelectedToken), bHasInk ? *FString::Printf(TEXT("  |  Ink %d / %d"), Value->Hp, Value->MaxHp) : TEXT(""))));
	Keepsakes->ClearChildren();
	if (Data.Keepsakes.IsEmpty()) Keepsakes->AddChild(MakeText(TEXT("Nothing yet."), 18, Ink));
	else for (const FString& Item : Data.Keepsakes) Keepsakes->AddChild(MakeText(Item, 18, Ink));
	Ledger->SetText(FText::FromString(FString::Printf(TEXT("Ember   %s\nPoints   %s"), Data.bValid && Data.Ember >= 0 ? *FString::FromInt(Data.Ember) : TEXT("Unavailable"), Data.bValid && Data.Points >= 0 ? *FString::FromInt(Data.Points) : TEXT("Unavailable"))));
}

void UGbSatchelWidget::HandleSelection(FString, ESelectInfo::Type)
{
	if (bUpdating) return;
	const int32 Index = Selection->GetSelectedIndex(); SelectedToken = Options.IsValidIndex(Index) ? Options[Index] : FString(); Redraw();
}
void UGbSatchelWidget::ShowTalismans() { Pockets->SetActiveWidgetIndex(0); Redraw(); }
void UGbSatchelWidget::ShowSupplies() { Pockets->SetActiveWidgetIndex(1); Redraw(); }
void UGbSatchelWidget::ShowKeepsakes() { Pockets->SetActiveWidgetIndex(2); Redraw(); }
void UGbSatchelWidget::ShowLedger() { Pockets->SetActiveWidgetIndex(3); Redraw(); }
void UGbSatchelWidget::HandlePlayerChanged() { PullPlayer(); }
// The world polls every 2 s; only the Mender's price depends on it, so redraw (and rebuild the talisman list) only when it changes.
void UGbSatchelWidget::HandleWorld(const FGbWorldState& World) { if (IsVisible() && MenderPriceFrom(World) != MenderPrice) PullPlayer(); }
void UGbSatchelWidget::HandleAction(const FString& Message) { Status->SetText(FText::FromString(Message)); PullPlayer(); }
void UGbSatchelWidget::HandleError(const FString& What, int32 Code, const FString& Message)
{
	if (What != TEXT("satchel") && What != TEXT("satchel/carry") && What != TEXT("satchel/salve") && What != TEXT("mender/restore") && What != TEXT("rest") && What != TEXT("shop/buy") && What != TEXT("shop/sell")) return;
	Status->SetText(FText::FromString(Code == 0 ? FString(TEXT("Hollowmere is unreachable. Refresh your Satchel to check the result."))
		: Message.Contains(TEXT("chain")) || Message.Contains(TEXT("NEAR")) ? FString(TEXT("The town's ledger is unavailable. Refresh before trying again.")) : Message));
	PullPlayer();
}
void UGbSatchelWidget::Submit(const FString& Message) { bBusy = true; Status->SetText(FText::FromString(Message)); Redraw(); }
void UGbSatchelWidget::CarrySelected()
{
	if (UGbApiSubsystem* A = Api()) if (CarryButton->GetIsEnabled()) { const bool bCarry = !Data.Carried.Contains(SelectedToken); Submit(TEXT("Moving your talisman...")); A->SetCarried(SelectedToken, bCarry); }
}
void UGbSatchelWidget::ChoosePartner()
{
	if (!PartnerButton->GetIsEnabled()) return;
	const FGbTalisman* T = TalismanFor(SelectedToken);
	if (UGbPlayerSubsystem* P = Player()) if (T) { const FString Id = T->Id; P->SetPartner(Id); Status->SetText(FText::FromString(TEXT("Your partner is chosen."))); PullPlayer(); }
}
void UGbSatchelWidget::UseSalve()
{
	if (UGbApiSubsystem* A = Api()) if (SalveButton->GetIsEnabled()) { Submit(TEXT("Applying an Ink Salve...")); A->UseSalve(SelectedToken); }
}
void UGbSatchelWidget::RestoreInk()
{
	if (UGbApiSubsystem* A = Api()) if (MenderButton->GetIsEnabled()) { Submit(TEXT("Asking the Mender to restore your ink...")); A->RestoreInk(SelectedToken); }
}
void UGbSatchelWidget::Rest()
{
	if (UGbApiSubsystem* A = Api()) if (RestButton->GetIsEnabled()) { Submit(TEXT("Resting until morning...")); A->Rest(); }
}
void UGbSatchelWidget::Refresh()
{
	PullPlayer();
	if (UGbPlayerSubsystem* P = Player()) P->RefreshFromChain();
}
void UGbSatchelWidget::Close() { OnClose.Broadcast(); }
void UGbSatchelWidget::Broker() { if (BrokerButton->GetIsEnabled()) OnBroker.Broadcast(); }
