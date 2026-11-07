#include "GbVowWidget.h"
#include "GbApiSubsystem.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor Ink(0.12f, 0.10f, 0.09f);
	const FLinearColor Paper(0.86f, 0.80f, 0.65f);
	const FLinearColor Gold(0.72f, 0.56f, 0.25f);
}

void UGbVowWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Root;
	UBorder* Card = MakePanel(Paper, FMargin(24.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Sized(Col, 610.f, 0.f));
	auto Add = [Col](UWidget* W) { Col->AddChildToVerticalBox(W); };
	Add(MakeText(TEXT("The Vow Shrine"), 26, Ink, true));
	Add(MakeText(TEXT("Keep a promise until its end: your stake returns with a fifth added. Release it early: the stake burns. Promised actions stay closed while the vow holds."), 14, Ink));
	Info = MakeText(TEXT("Reading the Rite's ledger..."), 14, Ink); Add(Info);
	Templates = WidgetTree->ConstructWidget<UComboBoxString>(); Add(Templates);
	Templates->OnSelectionChanged.AddDynamic(this, &UGbVowWidget::HandleTemplate);
	Parameter = WidgetTree->ConstructWidget<UComboBoxString>(); Add(Parameter);
	Parameter->OnSelectionChanged.AddDynamic(this, &UGbVowWidget::HandleChoice);
	Days = WidgetTree->ConstructWidget<UComboBoxString>(); Add(Days);
	Days->OnSelectionChanged.AddDynamic(this, &UGbVowWidget::HandleChoice);
	Preview = MakeText(TEXT(""), 16, Ink, true); Add(Preview);
	UTextBlock* Label = nullptr;
	Swear = MakeButton(TEXT("Swear this vow"), Label, Gold); Swear->OnClicked.AddDynamic(this, &UGbVowWidget::HandleSwear); Add(Sized(Swear, 0, 38));
	History = WidgetTree->ConstructWidget<UScrollBox>(); Add(Sized(History, 0, 165));
	Active = WidgetTree->ConstructWidget<UComboBoxString>(); Add(Active);
	Release = MakeButton(TEXT("Release selected vow (burn its stake)"), Label, FLinearColor(0.65f, 0.34f, 0.26f));
	Release->OnClicked.AddDynamic(this, &UGbVowWidget::HandleRelease); Add(Sized(Release, 0, 38));
	Status = MakeText(TEXT(""), 14, Ink); Add(Status);
	UButton* Close = MakeButton(TEXT("Return to the Ward  (V)"), Label, Paper);
	Close->OnClicked.AddDynamic(this, &UGbVowWidget::HandleClose); Add(Sized(Close, 0, 38));
	UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Card);
	CardSlot->SetAnchors(FAnchors(0.5f)); CardSlot->SetAlignment(FVector2D(0.5f)); CardSlot->SetAutoSize(true);
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
	History->ClearChildren(); Active->ClearOptions(); Templates->ClearOptions();
	Info->SetText(FText::FromString(TEXT("Reading the Rite's ledger..."))); Status->SetText(FText::GetEmpty());
	RedrawPreview();
	if (UGbApiSubsystem* A = Api()) A->FetchVows();
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsVisible() && !bBusy) if (UGbApiSubsystem* A = Api()) A->FetchVows();
	}), 5.f, true);
}

const FGbVowTemplate* UGbVowWidget::Current() const
{
	const int32 Index = Templates->GetSelectedIndex();
	return Data.Templates.IsValidIndex(Index) ? &Data.Templates[Index] : nullptr;
}

void UGbVowWidget::HandleVows(const FGbVowsOverview& Overview)
{
	const FString Previous = Templates->GetSelectedOption();
	const int32 OldPoints = Data.Points;
	Data = Overview; bReady = true; bUpdating = true;
	Templates->ClearOptions();
	for (const FGbVowTemplate& T : Data.Templates) Templates->AddOption(T.Id + TEXT("  ") + T.Text);
	if (Templates->FindOptionIndex(Previous) != INDEX_NONE) Templates->SetSelectedOption(Previous);
	else if (Data.Templates.Num()) Templates->SetSelectedIndex(0);
	bUpdating = false;
	Info->SetText(FText::FromString(Data.bChain
		? FString::Printf(TEXT("Your points: %d  |  One town day: %.0f minutes"), Data.Points, Data.DaySeconds / 60.)
		: FString(TEXT("The Shrine is closed until the Rite's ledger is connected."))));
	History->ClearChildren(); Active->ClearOptions(); ActiveIds.Reset();
	if (Data.Vows.IsEmpty()) History->AddChild(MakeText(TEXT("No promises written yet."), 14, Ink));
	for (const FGbVow& V : Data.Vows)
	{
		const int64 Seconds = FMath::Max<int64>(0, static_cast<int64>(V.EndsAt / 1000.) - FDateTime::UtcNow().ToUnixTimestamp());
		const FString Time = V.Status == TEXT("Active") ? FString::Printf(TEXT(" | %lldm %llds left"), Seconds / 60, Seconds % 60) : FString();
		History->AddChild(MakeText(FString::Printf(TEXT("%s  |  %s  |  stake %d, kept return %d%s"), *V.Text, *V.Status, V.Stake, V.Payout, *Time), 14, Ink));
		if (V.Status == TEXT("Active")) { Active->AddOption(V.Text); ActiveIds.Add(V.Id); }
	}
	if (ActiveIds.Num()) Active->SetSelectedIndex(0);
	RedrawChoices();
	if (Data.Points != OldPoints) if (UGbPlayerSubsystem* P = Player()) P->RefreshFromChain();
}

void UGbVowWidget::RedrawChoices()
{
	const FGbVowTemplate* T = Current();
	const FString OldParameter = Parameter->GetSelectedOption(), OldDays = Days->GetSelectedOption();
	bUpdating = true; Parameter->ClearOptions(); Days->ClearOptions();
	if (T)
	{
		if (T->Id == TEXT("V1")) Parameter->AddOption(GetDefault<UGbSettings>()->WardId); // Vow is made where you stand.
		if (T->Id == TEXT("V3")) for (const auto& Token : Data.Talismans) Parameter->AddOption(Token.Value);
		if (T->Id == TEXT("V4")) { Parameter->AddOption(TEXT("whisper")); Parameter->AddOption(TEXT("wail")); Parameter->AddOption(TEXT("calamity")); }
		for (int32 D = T->MinDays; D <= T->MaxDays; ++D) Days->AddOption(FString::FromInt(D));
	}
	if (Parameter->FindOptionIndex(OldParameter) != INDEX_NONE) Parameter->SetSelectedOption(OldParameter);
	else if (Parameter->GetOptionCount()) Parameter->SetSelectedIndex(0);
	if (Days->FindOptionIndex(OldDays) != INDEX_NONE) Days->SetSelectedOption(OldDays); else if (Days->GetOptionCount()) Days->SetSelectedIndex(0);
	Parameter->SetVisibility(T && T->Id != TEXT("V2") ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	bUpdating = false; RedrawPreview();
}

void UGbVowWidget::RedrawPreview()
{
	const FGbVowTemplate* T = Current();
	FString Words = T ? T->Text : FString();
	Words = Words.Replace(TEXT("{ward}"), *Parameter->GetSelectedOption()).Replace(TEXT("{talisman}"), *Parameter->GetSelectedOption()).Replace(TEXT("{omen}"), *Parameter->GetSelectedOption());
	Preview->SetText(FText::FromString(T ? FString::Printf(TEXT("%s, for %s town days. Stake %d points; keep it to receive %d."), *Words, *Days->GetSelectedOption(), T->Stake, T->Stake + T->Stake * 20 / 100) : TEXT("")));
	const bool bHasParam = T && (T->Id == TEXT("V2") || Parameter->GetSelectedIndex() != INDEX_NONE);
	Swear->SetIsEnabled(bReady && !bBusy && Data.bChain && T && bHasParam && Data.Points >= T->Stake);
	Release->SetIsEnabled(bReady && !bBusy && Data.bChain && ActiveIds.Num() > 0);
	Templates->SetIsEnabled(!bBusy); Parameter->SetIsEnabled(!bBusy); Days->SetIsEnabled(!bBusy); Active->SetIsEnabled(!bBusy);
}

void UGbVowWidget::HandleTemplate(FString, ESelectInfo::Type) { if (!bUpdating) RedrawChoices(); }
void UGbVowWidget::HandleChoice(FString, ESelectInfo::Type) { if (!bUpdating) RedrawPreview(); }

void UGbVowWidget::HandleSwear()
{
	const FGbVowTemplate* T = Current(); UGbApiSubsystem* A = Api();
	if (!T || !A || bBusy || !Swear->GetIsEnabled()) return;
	TMap<FString, FString> Params;
	if (T->Id == TEXT("V1")) Params.Add(TEXT("ward"), Parameter->GetSelectedOption());
	if (T->Id == TEXT("V3")) Params.Add(TEXT("talisman"), Parameter->GetSelectedOption());
	if (T->Id == TEXT("V4")) Params.Add(TEXT("omen"), Parameter->GetSelectedOption());
	bBusy = true; RedrawPreview(); Status->SetText(FText::FromString(TEXT("Sealing your promise in the Rite's ledger...")));
	A->SwearVow(T->Id, FCString::Atoi(*Days->GetSelectedOption()), Params);
}

void UGbVowWidget::HandleRelease()
{
	const int32 Index = Active->GetSelectedIndex();
	if (bBusy || !ActiveIds.IsValidIndex(Index) || !Api()) return;
	bBusy = true; RedrawPreview(); Status->SetText(FText::FromString(TEXT("Releasing your promise. Its stake will burn...")));
	Api()->ReleaseVow(ActiveIds[Index]);
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
