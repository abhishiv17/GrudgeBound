#include "GbSatchelWidget.h"
#include "GbApiSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"

namespace
{
	const FLinearColor& Paper=GbGreybox::Color::Paper; const FLinearColor& Ink=GbGreybox::Color::Ink;
	const FLinearColor& Gold=GbGreybox::Color::Gold; const FLinearColor& Red=GbGreybox::Color::Crimson; const FLinearColor& Blue=GbGreybox::Color::Sorrow;
	UVerticalBoxSlot* SatchelRow(UVerticalBox* Box, UWidget* W, float Top, float Bottom)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W); S->SetPadding(FMargin(0.f, Top, 0.f, Bottom)); S->SetHorizontalAlignment(HAlign_Fill); return S;
	}
	UHorizontalBoxSlot* SatchelCol(UHorizontalBox* Box, UWidget* W, bool bFill, float Right)
	{
		UHorizontalBoxSlot* S = Box->AddChildToHorizontalBox(W); S->SetPadding(FMargin(0.f, 0.f, Right, 0.f)); S->SetVerticalAlignment(VAlign_Center);
		if (bFill) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return S;
	}
}

void UGbSatchelWidget::BuildTree()
{
	UTextBlock* TitleText = nullptr; UTextBlock* Sub = nullptr; UHorizontalBox* Footer = nullptr;
	UVerticalBox* Body = MakeShell(TEXT("Your Satchel"), TEXT("talisman"), 1240.f, TitleText, Sub, Footer);
	Sub->SetText(FText::FromString(TEXT("Your talismans, equipment and discoveries. One piece of gear can be equipped.")));
	Status = ShellStatus;

	// The four pockets as tabs, each with its picture; the open one is crimson.
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	PocketTabs.Reset(); PocketLabels.Reset();
	const TCHAR* TabNames[] = { TEXT("Talismans"), TEXT("Supplies"), TEXT("Keepsakes"), TEXT("Ledger"), TEXT("Gear"), TEXT("Journal") };
	const TCHAR* TabIcons[] = { TEXT("talisman"), TEXT("icon_salve"), TEXT("wax_seal"), TEXT("stamp_tool"), TEXT("talisman_gilded"), TEXT("stamp_tool") };
	for (int32 I = 0; I < 6; ++I)
	{
		UTextBlock* Label = nullptr; UButton* B = MakeButton(TabNames[I], Label, GbUi::Paper2());
		Label->SetFont(GbUi::Font(17, true));
		UHorizontalBox* Face = WidgetTree->ConstructWidget<UHorizontalBox>(); B->SetContent(Face);
		UImage* Icon = nullptr; SatchelCol(Face, MakeArt(TabIcons[I], 32.f, Icon), false, 10.f);
		SatchelCol(Face, Label, false, 0.f);
		SatchelCol(Tabs, Sized(B, 0.f, 56.f), true, I < 5 ? 6.f : 0.f);
		PocketTabs.Add(B); PocketLabels.Add(Label);
	}
	PocketTabs[0]->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowTalismans);
	PocketTabs[1]->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowSupplies);
	PocketTabs[2]->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowKeepsakes);
	PocketTabs[3]->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowLedger);
	PocketTabs[4]->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowGear);
 PocketTabs[5]->OnClicked.AddDynamic(this, &UGbSatchelWidget::ShowJournal);
 SatchelRow(Body, Tabs, 0.f, 12.f);

	// What is where, at a glance; a plain sentence instead while the Satchel cannot be read.
	UHorizontalBox* Chips = WidgetTree->ConstructWidget<UHorizontalBox>();
	CarriedChip = MakeChip(TEXT(""), Gold, CarriedText); SatchelCol(Chips, CarriedChip, false, 10.f);
	CaseChip = MakeChip(TEXT(""), GbUi::Paper3(), CaseText); SatchelCol(Chips, CaseChip, false, 10.f);
	PartnerChip = MakeChip(TEXT(""), Gold, PartnerText); SatchelCol(Chips, PartnerChip, false, 0.f);
	SummaryChips = Chips; SatchelRow(Body, Chips, 0.f, 4.f);
	Summary = MakeText(TEXT("Reading your pockets..."), 16, Ink, true); SatchelRow(Body, Summary, 0.f, 8.f);

	Pockets = WidgetTree->ConstructWidget<UWidgetSwitcher>(); SatchelRow(Body, Sized(Pockets, 0.f, 470.f), 0.f, 0.f);

	// Talismans: the list on the left, the picked one on the right with what can be done with it.
	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(); Pockets->AddChild(Split);
	Rows = WidgetTree->ConstructWidget<UScrollBox>(); SatchelCol(Split, Sized(Rows, 0.f, 460.f), true, 24.f)->SetVerticalAlignment(VAlign_Top);
	UBorder* Detail = MakePanel(GbUi::Paper2(), FMargin(20.f, 16.f)); SatchelCol(Split, Sized(Detail, 420.f, 0.f), false, 0.f)->SetVerticalAlignment(VAlign_Top);
	UVerticalBox* D = WidgetTree->ConstructWidget<UVerticalBox>(); Detail->SetContent(D);
	UBorder* Plate = MakePanel(GbUi::PaperHi(), FMargin(8.f)); Plate->SetContent(MakeArt(FString(), 144.f, PickedArt));
	SatchelRow(D, Plate, 0.f, 10.f)->SetHorizontalAlignment(HAlign_Center);
	PickedName = MakeText(TEXT("Choose a talisman."), 26, Ink, true); PickedName->SetJustification(ETextJustify::Center); SatchelRow(D, PickedName, 0.f, 6.f);
	PlaceChip = MakeChip(TEXT(""), GbUi::Paper3(), PlaceText); SatchelRow(D, PlaceChip, 0.f, 10.f)->SetHorizontalAlignment(HAlign_Center);
	InkBar = MakeBar(Blue); SatchelRow(D, Sized(InkBar, 0.f, 16.f), 0.f, 4.f);
	InkText = MakeText(TEXT(""), 16, Ink); InkText->SetJustification(ETextJustify::Center); SatchelRow(D, InkText, 0.f, 12.f);
	UTextBlock* Label = nullptr;
	CarryButton = MakeKeyButton(TEXT("Carry"), FString(), CarryLabel, Gold); CarryButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::CarrySelected);
	SatchelRow(D, Sized(CarryButton, 0.f, 54.f), 0.f, 8.f);
	PartnerButton = MakeKeyButton(TEXT("Choose as partner"), FString(), Label, Red); PartnerButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::ChoosePartner);
	SatchelRow(D, Sized(PartnerButton, 0.f, 54.f), 0.f, 0.f);

	// Supplies: three counters, what they are for, and the salve.
	UVerticalBox* Supplies = WidgetTree->ConstructWidget<UVerticalBox>(); Pockets->AddChild(Supplies);
	UHorizontalBox* Counters = WidgetTree->ConstructWidget<UHorizontalBox>();
	const TCHAR* SupplyNames[] = { TEXT("Blank talismans"), TEXT("Gilded talismans"), TEXT("Ink Salves") };
	const TCHAR* SupplyIcons[] = { TEXT("talisman"), TEXT("talisman_gilded"), TEXT("icon_salve") };
	TObjectPtr<UTextBlock>* Counts[] = { &BlankCount, &GildedCount, &SalveCount };
	for (int32 I = 0; I < 3; ++I)
	{
		UBorder* Card = MakePanel(GbUi::Paper2(), FMargin(18.f, 14.f));
		UHorizontalBox* In = WidgetTree->ConstructWidget<UHorizontalBox>(); Card->SetContent(In);
		UImage* Icon = nullptr; SatchelCol(In, MakeArt(SupplyIcons[I], 96.f, Icon), false, 16.f);
		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(); SatchelCol(In, Text, true, 0.f);
		*Counts[I] = MakeText(TEXT("-"), 48, Ink, true); SatchelRow(Text, *Counts[I], 0.f, 0.f);
		SatchelRow(Text, MakeText(SupplyNames[I], 18, Ink, true), 0.f, 0.f);
		SatchelCol(Counters, Card, true, I < 2 ? 14.f : 0.f);
	}
	SatchelRow(Supplies, Counters, 0.f, 16.f);
	SatchelRow(Supplies, MakeText(TEXT("Each seal attempt uses one blank or gilded talisman. An Ink Salve restores part of a talisman's ink."), 17, Ink), 0.f, 16.f);
	SupplyTarget = MakeText(TEXT("Choose a talisman in the Talismans pocket."), 18, Ink, true); SatchelRow(Supplies, SupplyTarget, 0.f, 10.f);
	SalveButton = MakeKeyButton(TEXT("Use salve on selected talisman"), FString(), Label, Gold); SalveButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::UseSalve);
	SatchelRow(Supplies, Sized(SalveButton, 520.f, 56.f), 0.f, 0.f)->SetHorizontalAlignment(HAlign_Left);

	// Keepsakes: one slip each.
	Keepsakes = WidgetTree->ConstructWidget<UScrollBox>(); Pockets->AddChild(Keepsakes);

	// Ledger: the two balances large, then the debt notice.
	UVerticalBox* LedgerPocket = WidgetTree->ConstructWidget<UVerticalBox>(); Pockets->AddChild(LedgerPocket);
	UHorizontalBox* Balances = WidgetTree->ConstructWidget<UHorizontalBox>();
	const TCHAR* BalanceNames[] = { TEXT("Ember"), TEXT("Points") };
	TObjectPtr<UTextBlock>* BalanceCounts[] = { &EmberCount, &PointsCount };
	for (int32 I = 0; I < 2; ++I)
	{
		UBorder* Card = MakePanel(GbUi::Paper2(), FMargin(22.f, 14.f));
		UVerticalBox* In = WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(In);
		SatchelRow(In, MakeText(BalanceNames[I], 18, Ink, true), 0.f, 0.f);
		*BalanceCounts[I] = MakeText(TEXT("-"), 48, Ink, true); SatchelRow(In, *BalanceCounts[I], 0.f, 0.f);
		SatchelCol(Balances, Card, true, I == 0 ? 14.f : 0.f);
	}
	SatchelRow(LedgerPocket, Balances, 0.f, 14.f);
	// Your own page: what you hold is written under your name, not kept by the game. DRAFT wording for the lead.
	UBorder* PageSlip = MakePanel(GbUi::PaperHi(), FMargin(22.f, 14.f)); UHorizontalBox* PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(); PageSlip->SetContent(PageRow);
	UVerticalBox* PageCol = WidgetTree->ConstructWidget<UVerticalBox>(); SatchelCol(PageRow, PageCol, true, 16.f);
	SatchelRow(PageCol, MakeText(TEXT("Your page in the Rite's ledger"), 22, Ink, true), 0.f, 4.f);
	LedgerPage = MakeText(TEXT("-"), 17, Ink); SatchelRow(PageCol, LedgerPage, 0.f, 2.f);
	LedgerHoldings = MakeText(TEXT(""), 17, Ink); LedgerHoldings->SetAutoWrapText(true); SatchelRow(PageCol, LedgerHoldings, 0.f, 0.f);
	UTextBlock* PageLabel = nullptr;
	LedgerButton = MakeKeyButton(TEXT("Open my page"), FString(), PageLabel, Gold); LedgerButton->OnClicked.AddDynamic(this, &UGbSatchelWidget::OpenLedgerPage);
	SatchelCol(PageRow, Sized(LedgerButton, 260.f, 56.f), false, 0.f);
	SatchelRow(LedgerPocket, PageSlip, 0.f, 14.f);
	UBorder* Slip = MakePanel(GbUi::PaperHi(), FMargin(22.f, 16.f)); UVerticalBox* SlipCol = WidgetTree->ConstructWidget<UVerticalBox>(); Slip->SetContent(SlipCol);
	SatchelRow(SlipCol, MakeText(TEXT("Debt notice"), 22, Ink, true), 0.f, 6.f);
	SatchelRow(SlipCol, MakeText(TEXT("You arrived with a debt notice folded inside your coat."), 18, Ink), 0.f, 0.f);
	SatchelRow(LedgerPocket, Slip, 0.f, 0.f);

 GearRows = WidgetTree->ConstructWidget<UScrollBox>(); Pockets->AddChild(GearRows);
 JournalRows = WidgetTree->ConstructWidget<UScrollBox>(); Pockets->AddChild(JournalRows);
	UButton* B = MakeKeyButton(TEXT("Refresh"), FString(), Label, GbUi::Paper2()); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::Refresh);
	SatchelCol(Footer, Sized(B, 220.f, 56.f), false, 12.f);
	B = MakeKeyButton(TEXT("Return"), KeyName(TEXT("Satchel")), Label, GbUi::Paper2()); B->OnClicked.AddDynamic(this, &UGbSatchelWidget::Close);
	SatchelCol(Footer, Sized(B, 240.f, 56.f), false, 0.f);
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

FString UGbSatchelWidget::ArtFor(const FString& Id) const
{
	const FGbTalisman* T = TalismanFor(Id);
	return T ? GbUi::CurseArt(T->SpeciesId) : Id == TEXT("starter") ? GbUi::CurseArt(TEXT("cinder-grin")) : FString();
}

void UGbSatchelWidget::Redraw()
{
	if (!Rows || !Pockets) return;
	for (int32 I = 0; I < PocketTabs.Num(); ++I)
	{
		const bool bOpen = I == Pockets->GetActiveWidgetIndex();
		PocketTabs[I]->SetBackgroundColor(bOpen ? Red : GbUi::Paper2());
		PocketLabels[I]->SetColorAndOpacity(FSlateColor(bOpen ? GbUi::PaperHi() : Ink));
	}
 GearRows->ClearChildren(); JournalRows->ClearChildren();
 if (!Data.bExplorationAvailable)
 {
  GearRows->AddChild(MakeText(TEXT("Equipment is unavailable on this server. Refresh after connecting to the updated server."),18,Ink));
  JournalRows->AddChild(MakeText(TEXT("The exploration journal is unavailable on this server."),18,Ink));
 }
 else
 {
  GearRows->AddChild(MakeText(TEXT("Your equipment strengthens your partner in the next battle. Select a piece to equip it; select it again to remove it."),18,Ink));
  if (Data.Gear.IsEmpty()) GearRows->AddChild(MakeText(TEXT("No equipment yet. Speak with the Lamplighter in Lantern Ward."),20,Ink,true));
  for (const FGbGear& G : Data.Gear)
  {
   UGbPickRowWidget* R=CreateWidget<UGbPickRowWidget>(this); if (!R) continue;
   R->Setup(TEXT("talisman_gilded"),G.Name,FString::Printf(TEXT("+%d attack  +%d guard\n%s"),G.Attack,G.Guard,*G.Source),-1.f,FString(),Data.Equipped==G.Id?TEXT("Equipped"):TEXT("Equip"),Gold,Data.Equipped==G.Id);
   R->SetIsEnabled(!bBusy);
   const FString Id=Data.Equipped==G.Id?FString():G.Id;
   R->OnClick=[this,Id](){if (!bBusy) if (UGbApiSubsystem* A=Api()) {Submit(TEXT("Changing equipment...")); A->Equip(Id);}};
   GearRows->AddChild(R);
  }
  JournalRows->AddChild(MakeText(TEXT("Select a task to track it on the town HUD. Hints appear beneath each objective."),17,Ink));
  for (const FGbJournalEntry& J : Data.Journal)
  {
   UGbPickRowWidget* R=CreateWidget<UGbPickRowWidget>(this); if (!R) continue;
   const bool bDone=J.Status==TEXT("complete");
   R->Setup(TEXT("stamp_tool"),J.Title,J.Status+TEXT("\n")+J.Objective,-1.f,FString(),bDone?TEXT("Done"):Data.Tracked==J.Id?TEXT("Tracking"):TEXT("Track"),Gold,!bDone&&Data.Tracked==J.Id);
   R->SetIsEnabled(!bBusy);
   const FString Id=J.Id; if (!bDone) R->OnClick=[this,Id](){if (!bBusy) if (UGbApiSubsystem* A=Api()) {Submit(TEXT("Tracking task...")); A->Explore(TEXT("track:")+Id);}};
   JournalRows->AddChild(R);
   JournalRows->AddChild(MakeText(bDone?FString():TEXT("Hint: ")+J.Hint+TEXT("\n"),bDone?8:16,Ink));
  }
  if (Data.Lore.Num()) JournalRows->AddChild(MakeText(TEXT("Hollowmere"),18,Ink,true));
  for (const FGbKeepsake& L : Data.Lore) JournalRows->AddChild(MakeText(L.Title+TEXT("\n")+L.Text+TEXT("\n"),15,Ink));
  bool bTips=false;
  for (const FGbTip& T : Data.Tips) if (T.bSeen)
  {
   if (!bTips) { JournalRows->AddChild(MakeText(TEXT("Tips you have seen"),18,Ink,true)); bTips=true; }
   JournalRows->AddChild(MakeText(TEXT("- ")+T.Text,15,Ink));
  }
 }
	Options = Data.Carried;
	for (const FString& Id : Data.Case) Options.AddUnique(Id);
	if (!Options.Contains(SelectedToken)) SelectedToken = Options.Contains(PartnerToken) ? PartnerToken : Options.Num() ? Options[0] : FString();

	SummaryChips->SetVisibility(Data.bValid ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Summary->SetVisibility(Data.bValid ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Summary->SetText(FText::FromString(TEXT("Your Satchel is unavailable. Refresh before choosing a partner or using supplies.")));
	SetChip(CarriedChip, CarriedText, FString::Printf(TEXT("Carried %d / %d"), Data.Carried.Num(), FGbSatchel::CarryLimit), Gold);
	SetChip(CaseChip, CaseText, FString::Printf(TEXT("Case %d"), Data.Case.Num()), GbUi::Paper3());
	SetChip(PartnerChip, PartnerText, FString::Printf(TEXT("Partner: %s"), PartnerToken.IsEmpty() ? TEXT("Choose one") : *NameFor(PartnerToken)), Gold);

	Rows->ClearChildren();
	for (int32 I = 0; I < Options.Num(); ++I)
	{
		const FString Id = Options[I]; const FGbInk* Value = Data.Ink.Find(Id);
		UGbPickRowWidget* Row = CreateWidget<UGbPickRowWidget>(this, UGbPickRowWidget::StaticClass()); if (!Row) continue;
		Row->Setup(ArtFor(Id), NameFor(Id), FString::Printf(TEXT("Lv %d  |  %s %d"), Data.LevelOf(Id), Data.Carried.Contains(Id) ? TEXT("Carried") : TEXT("Case"), I + 1),
			Value && Value->MaxHp > 0 ? static_cast<float>(Value->Hp) / Value->MaxHp : -1.f,
			Value ? FString::Printf(TEXT("%d / %d ink"), Value->Hp, Value->MaxHp) : FString(),
			Id == PartnerToken ? TEXT("Partner") : TEXT(""), Gold, Id == SelectedToken);
		Row->OnClick = [this, Id]() { if (!bBusy) { SelectedToken = Id; Redraw(); } };
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Rows->AddChild(Row))) RowSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
	}
	if (!Options.Num()) Rows->AddChild(MakeText(TEXT("No talismans to show yet."), 16, Ink));

	const FGbInk* Value = Data.Ink.Find(SelectedToken);
	const bool bHasInk = Value && Value->MaxHp > 0;
	const bool bCarried = Data.Carried.Contains(SelectedToken);
	const bool bReady = Data.bValid && !bBusy;
	SetArt(PickedArt, SelectedToken.IsEmpty() ? FString() : ArtFor(SelectedToken), 144.f);
	PickedName->SetText(FText::FromString(SelectedToken.IsEmpty() ? FString(TEXT("Choose a talisman.")) : NameFor(SelectedToken)));
	SetChip(PlaceChip, PlaceText, SelectedToken.IsEmpty() ? FString() : bCarried ? FString(TEXT("In your Satchel")) : FString(TEXT("In the Case")), bCarried ? Gold : GbUi::Paper3());
	InkBar->SetPercent(bHasInk ? static_cast<float>(Value->Hp) / Value->MaxHp : 0.f);
	InkBar->SetFillColorAndOpacity(bHasInk && Value->Hp <= 0 ? Red : Blue);
	const FGbLevel* Level = Data.Levels.Find(SelectedToken);
	const FString Growth = !Level ? FString() : Level->Next < 0 ? FString::Printf(TEXT("Level %d (highest)  |  "), Level->Level)
		: FString::Printf(TEXT("Level %d  |  %d / %d experience  |  "), Level->Level, Level->Xp, Level->Next);
	InkText->SetText(FText::FromString(bHasInk
		? Growth + FString::Printf(TEXT("Ink %d / %d%s"), Value->Hp, Value->MaxHp, Value->Hp <= 0 ? TEXT("  |  Visit the Mender or rest") : TEXT(""))
		: FString(TEXT("Reading ink..."))));
	CarryLabel->SetText(FText::FromString(bCarried ? TEXT("Leave in the Case") : TEXT("Carry in Satchel")));
	CarryButton->SetIsEnabled(bReady && (bCarried || Data.CanCarry(SelectedToken)));
	PartnerButton->SetIsEnabled(bReady && bCarried && SelectedToken != PartnerToken && TalismanFor(SelectedToken));
	SalveButton->SetIsEnabled(bReady && Data.CanUseSalve(SelectedToken));

	const auto Count = [this](int32 N) { return FText::FromString(Data.bValid ? FString::FromInt(N) : FString(TEXT("-"))); };
	BlankCount->SetText(Count(Data.Blank)); GildedCount->SetText(Count(Data.Gilded)); SalveCount->SetText(Count(Data.Salves));
	SupplyTarget->SetText(FText::FromString(SelectedToken.IsEmpty() ? FString(TEXT("Choose a talisman in the Talismans pocket."))
		: FString::Printf(TEXT("Selected: %s%s"), *NameFor(SelectedToken), bHasInk ? *FString::Printf(TEXT("  |  Ink %d / %d"), Value->Hp, Value->MaxHp) : TEXT(""))));

	Keepsakes->ClearChildren();
	if (Data.Keepsakes.IsEmpty()) Keepsakes->AddChild(MakeText(TEXT("Nothing yet."), 18, Ink));
	else for (const FString& Item : Data.Keepsakes)
	{
		UBorder* Slip = MakePanel(GbUi::PaperHi(), FMargin(16.f, 10.f));
		UHorizontalBox* In = WidgetTree->ConstructWidget<UHorizontalBox>(); Slip->SetContent(In);
		UImage* Icon = nullptr; SatchelCol(In, MakeArt(TEXT("wax_seal"), 32.f, Icon), false, 14.f);
		const FGbKeepsake* Note = Data.Notes.FindByPredicate([&Item](const FGbKeepsake& N){return N.Id==Item;});
        SatchelCol(In, MakeText(Note ? Note->Title + TEXT("\n\n") + Note->Text : Item, 18, Ink), true, 0.f);
		if (UScrollBoxSlot* SlipSlot = Cast<UScrollBoxSlot>(Keepsakes->AddChild(Slip))) SlipSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
	}
	EmberCount->SetText(FText::FromString(Data.bValid && Data.Ember >= 0 ? FString::FromInt(Data.Ember) : FString(TEXT("Unavailable"))));
	PointsCount->SetText(FText::FromString(Data.bValid && Data.Points >= 0 ? FString::FromInt(Data.Points) : FString(TEXT("Unavailable"))));
	if (LedgerPage && LedgerHoldings && LedgerButton)
	{
		const UGbPlayerSubsystem* P = Player();
		const FString Account = P ? P->GetActiveAccount() : FString();
		const bool bPage = P && P->IsOnChain() && !P->GetAccountUrl().IsEmpty();
		int32 Sealed = 0, Wins = 0;
		for (const FGbTalisman& T : Talismans) if (!T.ChainTokenId.IsEmpty()) { ++Sealed; Wins += T.Wins; }
		LedgerPage->SetText(FText::FromString(Account.IsEmpty() ? FString(TEXT("Sign in to be given a page.")) : Account));
		LedgerHoldings->SetText(FText::FromString(Account.IsEmpty() ? FString() : !bPage ? FString(TEXT("The ledger has not answered yet. Nothing here is lost; it is read again on Refresh."))
			: FString::Printf(TEXT("%d talisman%s sealed under your name, %d win%s written on them."), Sealed, Sealed == 1 ? TEXT("") : TEXT("s"), Wins, Wins == 1 ? TEXT("") : TEXT("s"))));
		LedgerButton->SetIsEnabled(bPage);
	}
}

void UGbSatchelWidget::OpenLedgerPage()
{
	// Only the server-supplied testnet explorer page of the signed-in account. No arbitrary URI schemes.
	const UGbPlayerSubsystem* P = Player();
	const FString Url = P ? P->GetAccountUrl() : FString();
	if (Url.StartsWith(TEXT("https://testnet.nearblocks.io/")) || Url.StartsWith(TEXT("https://explorer.testnet.near.org/"))) FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

void UGbSatchelWidget::ShowPocket(int32 Index) { Pockets->SetActiveWidgetIndex(Index); Redraw(); }
void UGbSatchelWidget::ShowTalismans() { ShowPocket(0); }
void UGbSatchelWidget::ShowSupplies() { ShowPocket(1); }
void UGbSatchelWidget::ShowKeepsakes() { ShowPocket(2); }
void UGbSatchelWidget::ShowGear() { ShowPocket(4); }
void UGbSatchelWidget::ShowJournal() { ShowPocket(5); }
void UGbSatchelWidget::ShowLedger() { ShowPocket(3); }
void UGbSatchelWidget::HandlePlayerChanged() { PullPlayer(); }
// The world polls every 2 s; only the Mender's price depends on it, so redraw (and rebuild the talisman list) only when it changes.
void UGbSatchelWidget::HandleWorld(const FGbWorldState& World) { if (IsVisible() && MenderPriceFrom(World) != MenderPrice) PullPlayer(); }
void UGbSatchelWidget::HandleAction(const FString& Message) { Status->SetText(FText::FromString(Message)); PullPlayer(); }
void UGbSatchelWidget::HandleError(const FString& What, int32 Code, const FString& Message)
{
	if (What != TEXT("satchel/equip") && What != TEXT("exploration/interact") && What != TEXT("satchel") && What != TEXT("satchel/carry") && What != TEXT("satchel/salve") && What != TEXT("mender/restore") && What != TEXT("rest") && What != TEXT("shop/buy") && What != TEXT("shop/sell")) return;
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
void UGbSatchelWidget::Refresh()
{
	PullPlayer();
	if (UGbPlayerSubsystem* P = Player()) P->RefreshFromChain();
}
void UGbSatchelWidget::Close() { OnClose.Broadcast(); }
