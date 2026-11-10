#include "GbObservatoryWidget.h"
#include "GbApiSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

namespace
{
 using namespace GbGreybox::Color;
 FString WardName(const FString& Id)
 {
  if (Id==TEXT("lantern")) return TEXT("Lantern Ward");
  if (Id==TEXT("tide")) return TEXT("Tide Ward");
  if (Id==TEXT("ash")) return TEXT("Ash Ward");
  if (Id==TEXT("bone")) return TEXT("Bone Ward");
  return Id;
 }
 const TArray<FString> WardIds={TEXT("lantern"),TEXT("tide"),TEXT("ash"),TEXT("bone")};
 const TArray<FString> ItemIds={TEXT("talisman-rage"),TEXT("talisman-sorrow"),TEXT("talisman-dread"),TEXT("blank-talisman"),TEXT("gilded-talisman"),TEXT("ink-salve")};
 const TArray<FString> ItemNames={TEXT("Rage talisman"),TEXT("Sorrow talisman"),TEXT("Dread talisman"),TEXT("Blank talisman"),TEXT("Gilded talisman"),TEXT("Ink Salve")};
 const TArray<FLinearColor> ItemColors={Rage,Sorrow,Dread,Ink,Gold,Teal};

 class SGbPlot : public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SGbPlot) {} SLATE_END_ARGS()
  void Construct(const FArguments&,UGbObservatoryChart* InOwner) { Owner=InOwner; }
  virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800,80); }
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool) const override
  {
   const UGbObservatoryChart* C=Owner.Get(); if (!C) return Layer;
   const FVector2D Size=G.GetLocalSize(); const FVector2D Range=UGbObservatoryChart::ValueRange(C->Lines);
   const auto Point=[&](FVector2D P) { return FVector2D(6+(Size.X-12)*FMath::Clamp((P.X-C->FirstTick)/FMath::Max(1.f,C->LastTick-C->FirstTick),0.,1.),6+(Size.Y-18)*(1.-FMath::Clamp((P.Y-Range.X)/(Range.Y-Range.X),0.,1.))); };
   const auto Draw=[&](const TArray<FVector2D>& P,const FLinearColor& Color,float Width)
   { FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint(),true,Width); };
   for (int32 Day=0;Day<7;++Day) { const float X=6+(Size.X-12)*Day/7.f; Draw({FVector2D(X,6),FVector2D(X,Size.Y-12)},Ink.CopyWithNewOpacity(.18f),1); }
   Draw({FVector2D(6,6),FVector2D(6,Size.Y-12),FVector2D(Size.X-6,Size.Y-12)},Ink.CopyWithNewOpacity(.6f),1);
   for (const auto& Line:C->Lines)
   {
    TArray<FVector2D> P; for (const auto& V:Line.Points) if (FMath::IsFinite(V.Y)) P.Add(Point(V));
    if (P.Num()==1) P.Add(P[0]+FVector2D(2,0));
    if (P.Num()>1) Draw(P,Line.Color,Line.Width);
   }
   for (const auto& M:C->Markers) if (M.Key>=C->FirstTick && M.Key<=C->LastTick)
   { const float X=Point(FVector2D(M.Key,Range.X)).X; Draw({FVector2D(X,Size.Y-14),FVector2D(X,Size.Y-2)},M.Value,2); }
   return Layer+1;
  }
 private:
  TWeakObjectPtr<UGbObservatoryChart> Owner;
 };
}

FVector2D UGbObservatoryChart::ValueRange(const TArray<FGbPlotLine>& Series)
{
 double Low=DBL_MAX,High=-DBL_MAX;
 for (const auto& Line:Series) for (const auto& Point:Line.Points) if (FMath::IsFinite(Point.Y)) { Low=FMath::Min(Low,Point.Y); High=FMath::Max(High,Point.Y); }
 if (Low==DBL_MAX) return FVector2D(0,1);
 const double Pad=FMath::Max(1.,(High-Low)*.08);
 return FVector2D(Low-Pad,High+Pad);
}
TSharedRef<SWidget> UGbObservatoryChart::RebuildWidget() { return SNew(SGbPlot,this); }
void UGbObservatoryProof::Open()
{
 // Only explicit clicks open a supplied testnet explorer proof. No arbitrary URI schemes.
 if (Url.StartsWith(TEXT("https://testnet.nearblocks.io/")) || Url.StartsWith(TEXT("https://explorer.testnet.near.org/"))) FPlatformProcess::LaunchURL(*Url,nullptr,nullptr);
}

void UGbObservatoryWidget::BuildTree()
{
 UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
 UScaleBox* Fit=WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
 UCanvasPanelSlot* FitSlot=Root->AddChildToCanvas(Fit); FitSlot->SetAnchors(FAnchors(.03f,.03f,.97f,.97f)); FitSlot->SetOffsets(FMargin(0));
 UBorder* Card=MakePanel(Paper,FMargin(20)); Fit->AddChild(Card);
 UVerticalBox* Col=WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Sized(Col,920,620));
 Col->AddChildToVerticalBox(MakeText(TEXT("The Observatory"),28,Ink,true));
 UTextBlock* Label=nullptr; UHorizontalBox* Modes=WidgetTree->ConstructWidget<UHorizontalBox>(); Col->AddChildToVerticalBox(Sized(Modes,0,38));
 UButton* B=MakeButton(TEXT("This town"),Label,Gold); B->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Live); Modes->AddChildToHorizontalBox(Sized(B,460,34)); ModeButtons.Add(B);
 B=MakeButton(TEXT("Seven days ahead"),Label,Paper); B->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Preview); Modes->AddChildToHorizontalBox(Sized(B,460,34)); ModeButtons.Add(B);
 UHorizontalBox* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>(); Col->AddChildToVerticalBox(Sized(Tabs,0,38));
 B=MakeButton(TEXT("Grudge"),Label,Gold); B->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Grudge); Tabs->AddChildToHorizontalBox(Sized(B,306,34)); TabButtons.Add(B);
 B=MakeButton(TEXT("Prices"),Label,Paper); B->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Prices); Tabs->AddChildToHorizontalBox(Sized(B,306,34)); TabButtons.Add(B);
 B=MakeButton(TEXT("Rules"),Label,Paper); B->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Rules); Tabs->AddChildToHorizontalBox(Sized(B,308,34)); TabButtons.Add(B);
 Status=MakeText(TEXT("Reading the town..."),14,Ink); Col->AddChildToVerticalBox(Sized(Status,0,30));
 WardPicker=WidgetTree->ConstructWidget<UComboBoxString>(); for (const FString& Id:WardIds) WardPicker->AddOption(WardName(Id));
 WardPicker->SetSelectedOption(WardName(SelectedWard)); WardPicker->OnSelectionChanged.AddDynamic(this,&UGbObservatoryWidget::SelectWard); Col->AddChildToVerticalBox(Sized(WardPicker,0,30));
 UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Col->AddChildToVerticalBox(Sized(Scroll,0,380)); Content=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Content);
 B=MakeButton(TEXT("Return (O)"),Label,Paper); B->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Close); Col->AddChildToVerticalBox(Sized(B,0,36));
 Redraw();
}
void UGbObservatoryWidget::NativeConstruct()
{
 Super::NativeConstruct(); if (auto* A=Api()) { A->OnObservatory.AddUniqueDynamic(this,&UGbObservatoryWidget::Receive); A->OnError.AddUniqueDynamic(this,&UGbObservatoryWidget::Error); }
}
void UGbObservatoryWidget::NativeDestruct()
{
 Stop(); if (auto* A=Api()) { A->OnObservatory.RemoveDynamic(this,&UGbObservatoryWidget::Receive); A->OnError.RemoveDynamic(this,&UGbObservatoryWidget::Error); } Super::NativeDestruct();
}
void UGbObservatoryWidget::Open() { bOpen=true; RefreshElapsed=0; Refresh(); }
void UGbObservatoryWidget::Stop() { bOpen=false; }
void UGbObservatoryWidget::NativeTick(const FGeometry& G,float Delta)
{
 Super::NativeTick(G,Delta); if (bOpen && !bPreview && IsVisible()) { RefreshElapsed+=Delta; if (RefreshElapsed>=10) { RefreshElapsed=0; Refresh(); } }
}
void UGbObservatoryWidget::Refresh() { if (auto* A=Api()) A->FetchObservatory(bPreview); }
void UGbObservatoryWidget::SetMode(bool Value)
{
 if (bPreview==Value) return;
 bPreview=Value; Data=FGbObservatoryData(); RefreshElapsed=0; Redraw(); Status->SetText(FText::FromString(TEXT("Reading the town..."))); Refresh();
}
void UGbObservatoryWidget::Live() { SetMode(false); }
void UGbObservatoryWidget::Preview() { SetMode(true); }
void UGbObservatoryWidget::Grudge() { Tab=0; Redraw(); }
void UGbObservatoryWidget::Prices() { Tab=1; Redraw(); }
void UGbObservatoryWidget::Rules() { Tab=2; Redraw(); }
void UGbObservatoryWidget::Close() { Stop(); OnClose.Broadcast(); }
void UGbObservatoryWidget::SelectWard(FString Option,ESelectInfo::Type)
{
 if (bUpdating) return;
 for (const auto& Id:WardIds) if (WardName(Id)==Option) SelectedWard=Id;
 Redraw();
}
void UGbObservatoryWidget::Receive(const FGbObservatoryData& Incoming)
{
 if (!bOpen || Incoming.Mode!=(bPreview?TEXT("preview"):TEXT("live"))) return;
 Data=Incoming; Data.Rules.StableSort([](const auto& A,const auto& B){return A.Tick>B.Tick;}); Redraw();
}
void UGbObservatoryWidget::Error(const FString& What,int32,const FString& Message)
{
 if (bOpen && What==TEXT("observatory")) Status->SetText(FText::FromString(Message));
}
void UGbObservatoryWidget::AddChart(const FString& Title,TArray<FGbPlotLine> Lines,float Height,const FString& Ward)
{
 const FVector2D Range=UGbObservatoryChart::ValueRange(Lines);
 Content->AddChildToVerticalBox(MakeText(FString::Printf(TEXT("%s   %.0f - %.0f"),*Title,Range.X,Range.Y),14,Ink,true));
 auto* Chart=WidgetTree->ConstructWidget<UGbObservatoryChart>(); Chart->Lines=MoveTemp(Lines);
 Chart->LastTick=Data.Samples.IsEmpty()?168:FMath::Max(168,Data.Samples.Last().Tick); Chart->FirstTick=Chart->LastTick-168;
 for (const auto& Event:Data.Events) if ((Event.Type==TEXT("bankruptcy") || Event.Type==TEXT("calamity")) && (Ward.IsEmpty() || Event.Ward==Ward)) Chart->Markers.Emplace(Event.Tick,Event.Type==TEXT("calamity")?Crimson:Gold);
 Content->AddChildToVerticalBox(Sized(Chart,0,Height));
}
void UGbObservatoryWidget::Redraw()
{
 if (!Content) return;
 for (int32 I=0;I<ModeButtons.Num();++I) ModeButtons[I]->SetBackgroundColor(I==int32(bPreview)?Gold:Paper);
 for (int32 I=0;I<TabButtons.Num();++I) TabButtons[I]->SetBackgroundColor(I==Tab?Gold:Paper);
 WardPicker->SetVisibility(Tab==0?ESlateVisibility::Visible:ESlateVisibility::Hidden);
 Content->ClearChildren(); Proofs.Reset();
 Status->SetText(FText::FromString(!Data.bValid?TEXT("Reading the town..."):Data.Samples.IsEmpty()?TEXT("No history yet."):FString::Printf(TEXT("%s | %d hours | day %d, %02d:00"),bPreview?TEXT("Seven days ahead"):TEXT("This town"),Data.Samples.Num(),Data.Samples.Last().Day,Data.Samples.Last().Hour)));
 if (Tab==0)
 {
  for (const auto& Id:WardIds)
  {
   TArray<FGbPlotLine> Lines; FGbPlotLine Total; Total.Color=Ink; Total.Width=2.5f;
   FGbPlotLine R,S,D; R.Color=Rage; S.Color=Sorrow; D.Color=Dread;
   for (const auto& Sample:Data.Samples) for (const auto& W:Sample.Wards) if (W.Id==Id)
   { Total.Points.Emplace(Sample.Tick,W.Total); R.Points.Emplace(Sample.Tick,W.Grudge.Rage); S.Points.Emplace(Sample.Tick,W.Grudge.Sorrow); D.Points.Emplace(Sample.Tick,W.Grudge.Dread); }
   Lines.Add(Total); if (Id==SelectedWard) { Lines.Add(R); Lines.Add(S); Lines.Add(D); }
   AddChart(WardName(Id),MoveTemp(Lines),40,Id);
  }
  Content->AddChildToVerticalBox(MakeText(TEXT("Total (ink)   Rage (red)   Sorrow (blue)   Dread (violet)"),14,Ink));
 }
 else if (Tab==1)
 {
  TArray<FGbPlotLine> Lines;
  for (int32 I=0;I<ItemIds.Num();++I)
  {
   FGbPlotLine Line; Line.Color=ItemColors[I]; Line.Width=2;
   for (const auto& Sample:Data.Samples)
   {
    const auto* Broker=Sample.Prices.Find(TEXT("broker")); const float* Price=Broker?Broker->Items.Find(ItemIds[I]):nullptr;
    if (Price && !Sample.Closed.Contains(TEXT("broker"))) Line.Points.Emplace(Sample.Tick,*Price);
    else if (!Line.Points.IsEmpty()) { Lines.Add(Line); Line.Points.Reset(); }
   }
   if (!Line.Points.IsEmpty()) Lines.Add(Line);
  }
  AddChart(TEXT("The Talisman Broker / Ember"),MoveTemp(Lines),125,TEXT(""));
  for (int32 I=0;I<ItemNames.Num();I+=2)
  {
   auto* Legend=WidgetTree->ConstructWidget<UHorizontalBox>(); Content->AddChildToVerticalBox(Legend);
   Legend->AddChildToHorizontalBox(Sized(MakeText(ItemNames[I],14,ItemColors[I],true),460,0));
   Legend->AddChildToHorizontalBox(Sized(MakeText(ItemNames[I+1],14,ItemColors[I+1],true),460,0));
  }
  TArray<FString> Closed; for (const auto& Sample:Data.Samples) for (const auto& Id:Sample.Closed) Closed.AddUnique(Id);
  Content->AddChildToVerticalBox(MakeText(Closed.IsEmpty()?TEXT("No shop closures in this history."):TEXT("Closed during this history: ")+FString::Join(Closed,TEXT(", ")),14,Ink));
 }
 else
 {
  if (Data.Rules.IsEmpty()) Content->AddChildToVerticalBox(MakeText(TEXT("No rules in this history."),16,Ink));
  for (const auto& Rule:Data.Rules)
  {
   UBorder* Frame=MakeFrame(Paper,FMargin(8)); UVerticalBox* Row=WidgetTree->ConstructWidget<UVerticalBox>(); Frame->SetContent(Row); Content->AddChildToVerticalBox(Frame);
   Row->AddChildToVerticalBox(MakeText(Rule.Text,16,Ink,true));
   const FString Mark=Rule.Status==TEXT("active")?TEXT("Accepted"):Rule.Status==TEXT("rejected")?TEXT("Rejected"):TEXT("Repealed");
   Row->AddChildToVerticalBox(MakeText(FString::Printf(TEXT("%s [%s] | day %d, %02d:00 | %s"),*Rule.Author,*Rule.AuthorKind,Rule.Tick/24+1,Rule.Tick%24,*Mark),14,Ink));
   if (!Rule.Reason.IsEmpty()) Row->AddChildToVerticalBox(MakeText(Rule.Reason,14,Crimson));
   if (Rule.Url.StartsWith(TEXT("https://testnet.nearblocks.io/")) || Rule.Url.StartsWith(TEXT("https://explorer.testnet.near.org/")))
   {
    auto* Proof=NewObject<UGbObservatoryProof>(this); Proof->Url=Rule.Url; Proofs.Add(Proof);
    UTextBlock* Label=nullptr; UButton* B=MakeButton(TEXT("Ledger proof"),Label,Paper); B->OnClicked.AddDynamic(Proof,&UGbObservatoryProof::Open); Row->AddChildToVerticalBox(Sized(B,0,32));
   }
  }
 }
 if (Tab!=2)
 {
  UHorizontalBox* Axis=WidgetTree->ConstructWidget<UHorizontalBox>(); Content->AddChildToVerticalBox(Axis);
  for (int32 Day=1;Day<=7;++Day) Axis->AddChildToHorizontalBox(Sized(MakeText(FString::Printf(TEXT("Day %d"),Day),14,Ink),920.f/7.f,0));
  Content->AddChildToVerticalBox(MakeText(TEXT("Gold tick: bankruptcy   Crimson tick: Calamity"),14,Ink));
 }
}
