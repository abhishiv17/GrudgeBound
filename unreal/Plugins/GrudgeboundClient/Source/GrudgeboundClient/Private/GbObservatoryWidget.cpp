#include "GbObservatoryWidget.h"
#include "GbApiSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

namespace
{
 using namespace GbGreybox::Color;
 UVerticalBoxSlot* ObsRow(UVerticalBox* Box,UWidget* W,float Top=2.f,float Bottom=2.f)
 {
  UVerticalBoxSlot* S=Box->AddChildToVerticalBox(W); S->SetPadding(FMargin(0.f,Top,0.f,Bottom)); S->SetHorizontalAlignment(HAlign_Fill); return S;
 }
 UHorizontalBoxSlot* ObsCol(UHorizontalBox* Box,UWidget* W,bool bFill,float Right=8.f)
 {
  UHorizontalBoxSlot* S=Box->AddChildToHorizontalBox(W); S->SetPadding(FMargin(0.f,0.f,Right,0.f)); S->SetVerticalAlignment(VAlign_Center);
  if (bFill) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); return S;
 }
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
 UTextBlock* TitleText=nullptr; UHorizontalBox* Footer=nullptr;
 UVerticalBox* Body=MakeShell(TEXT("The Observatory"),TEXT("icon_ward"),1480.f,TitleText,Status,Footer);
 Status->SetText(FText::FromString(TEXT("Reading the town...")));

 // One row: which history (this town / seven days ahead) on the left, which view on the right.
 UHorizontalBox* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>();
 const auto AddTab=[this,Tabs](const TCHAR* Name,float Width,TArray<TObjectPtr<UButton>>& Buttons,TArray<TObjectPtr<UTextBlock>>& Labels,float Gap)
 {
  UTextBlock* Text=nullptr; UButton* B=MakeKeyButton(Name,FString(),Text,GbUi::Paper2());
  Buttons.Add(B); Labels.Add(Text); ObsCol(Tabs,Sized(B,Width,52.f),false,Gap); return B;
 };
 AddTab(TEXT("This town"),250.f,ModeButtons,ModeLabels,8.f)->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Live);
 AddTab(TEXT("Seven days ahead"),300.f,ModeButtons,ModeLabels,0.f)->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Preview);
 ObsCol(Tabs,WidgetTree->ConstructWidget<USpacer>(),true,0.f);
 AddTab(TEXT("Grudge"),176.f,TabButtons,TabLabels,8.f)->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Grudge);
 AddTab(TEXT("Prices"),176.f,TabButtons,TabLabels,8.f)->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Prices);
 AddTab(TEXT("Rules"),176.f,TabButtons,TabLabels,8.f)->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Rules);
 AddTab(TEXT("Agents"),176.f,TabButtons,TabLabels,0.f)->OnClicked.AddDynamic(this,&UGbObservatoryWidget::AgentsTab);
 ObsRow(Body,Tabs,0.f,12.f);

 // Ward buttons (Grudge only): the picked Ward's chart also draws rage, sorrow and dread.
 UHorizontalBox* Wards=WidgetTree->ConstructWidget<UHorizontalBox>();
 static const TCHAR* WardHandlers[]={TEXT("WardLantern"),TEXT("WardTide"),TEXT("WardAsh"),TEXT("WardBone")};
 for (int32 I=0;I<WardIds.Num();++I)
 {
  UTextBlock* Text=nullptr; UButton* B=MakeKeyButton(WardName(WardIds[I]),FString(),Text,GbUi::Paper2());
  Text->SetFont(GbUi::Font(17,true));
  FScriptDelegate Pick; Pick.BindUFunction(this,FName(WardHandlers[I])); B->OnClicked.Add(Pick);
  WardButtons.Add(B); WardLabels.Add(Text); ObsCol(Wards,Sized(B,210.f,44.f),false,8.f);
 }
 WardRow=Wards; ObsRow(Body,Wards,0.f,10.f);

 UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); ObsRow(Body,Sized(Scroll,0.f,540.f));
 Content=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Content);

 UTextBlock* Label=nullptr;
 UButton* Back=MakeKeyButton(TEXT("Return"),KeyName(TEXT("Observatory")),Label,GbUi::Paper2());
 Back->OnClicked.AddDynamic(this,&UGbObservatoryWidget::Close); ObsCol(Footer,Sized(Back,240.f,56.f),false,0.f);
 Redraw();
}
void UGbObservatoryWidget::NativeConstruct()
{
 Super::NativeConstruct(); if (auto* A=Api()) { A->OnObservatory.AddUniqueDynamic(this,&UGbObservatoryWidget::Receive); A->OnAgents.AddUniqueDynamic(this,&UGbObservatoryWidget::ReceiveAgents); A->OnError.AddUniqueDynamic(this,&UGbObservatoryWidget::Error); }
}
void UGbObservatoryWidget::NativeDestruct()
{
 Stop(); if (auto* A=Api()) { A->OnObservatory.RemoveDynamic(this,&UGbObservatoryWidget::Receive); A->OnAgents.RemoveDynamic(this,&UGbObservatoryWidget::ReceiveAgents); A->OnError.RemoveDynamic(this,&UGbObservatoryWidget::Error); } Super::NativeDestruct();
}
void UGbObservatoryWidget::Open() { bOpen=true; RefreshElapsed=0; Refresh(); }
void UGbObservatoryWidget::Stop() { bOpen=false; }
void UGbObservatoryWidget::NativeTick(const FGeometry& G,float Delta)
{
 Super::NativeTick(G,Delta); if (bOpen && (!bPreview || Tab==3) && IsVisible()) { RefreshElapsed+=Delta; if (RefreshElapsed>=10) { RefreshElapsed=0; Refresh(); } }
}
void UGbObservatoryWidget::Refresh() { if (auto* A=Api()) { A->FetchObservatory(bPreview); if (Tab==3) A->FetchAgents(); } }
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
void UGbObservatoryWidget::AgentsTab() { Tab=3; Redraw(); if (auto* A=Api()) A->FetchAgents(); }
void UGbObservatoryWidget::ReceiveAgents(const FGbAgentsData& Incoming) { if (!bOpen) return; Agents=Incoming; if (Tab==3) Redraw(); }
void UGbObservatoryWidget::Close() { Stop(); OnClose.Broadcast(); }
void UGbObservatoryWidget::SelectWard(const FString& Id) { SelectedWard=Id; Redraw(); }
void UGbObservatoryWidget::WardLantern() { SelectWard(TEXT("lantern")); }
void UGbObservatoryWidget::WardTide() { SelectWard(TEXT("tide")); }
void UGbObservatoryWidget::WardAsh() { SelectWard(TEXT("ash")); }
void UGbObservatoryWidget::WardBone() { SelectWard(TEXT("bone")); }
void UGbObservatoryWidget::Receive(const FGbObservatoryData& Incoming)
{
 if (!bOpen || Incoming.Mode!=(bPreview?TEXT("preview"):TEXT("live"))) return;
 Data=Incoming; Data.Rules.StableSort([](const auto& A,const auto& B){return A.Tick>B.Tick;}); Redraw();
}
void UGbObservatoryWidget::Error(const FString& What,int32,const FString& Message)
{
 if (bOpen && (What==TEXT("observatory") || (What==TEXT("agents") && Tab==3))) Status->SetText(FText::FromString(Message));
}
void UGbObservatoryWidget::AddChart(const FString& Title,TArray<FGbPlotLine> Lines,float Height,const FString& Ward)
{
 const FVector2D Range=UGbObservatoryChart::ValueRange(Lines);
 UBorder* Slip=MakePanel(GbUi::PaperHi(),FMargin(14.f,8.f,14.f,6.f)); UVerticalBox* In=WidgetTree->ConstructWidget<UVerticalBox>(); Slip->SetContent(In);
 ObsRow(In,MakeText(FString::Printf(TEXT("%s   %.0f - %.0f"),*Title,Range.X,Range.Y),17,Ink,true),0.f,2.f);
 auto* Chart=WidgetTree->ConstructWidget<UGbObservatoryChart>(); Chart->Lines=MoveTemp(Lines);
 Chart->LastTick=Data.Samples.IsEmpty()?168:FMath::Max(168,Data.Samples.Last().Tick); Chart->FirstTick=Chart->LastTick-168;
 for (const auto& Event:Data.Events) if ((Event.Type==TEXT("bankruptcy") || Event.Type==TEXT("calamity")) && (Ward.IsEmpty() || Event.Ward==Ward)) Chart->Markers.Emplace(Event.Tick,Event.Type==TEXT("calamity")?Crimson:Gold);
 ObsRow(In,Sized(Chart,0,Height),0.f,0.f);
 ObsRow(Content,Slip,0.f,8.f);
}
void UGbObservatoryWidget::Redraw()
{
 if (!Content) return;
 const auto Paint=[](UButton* B,UTextBlock* L,bool bOn,const FLinearColor& On)
 {
  if (!B || !L) return;
  B->SetBackgroundColor(bOn?On:GbUi::Paper2());
  L->SetColorAndOpacity(FSlateColor(bOn && On==Crimson?GbUi::PaperHi():Ink));
 };
 for (int32 I=0;I<ModeButtons.Num();++I) Paint(ModeButtons[I],ModeLabels[I],I==int32(bPreview),Crimson);
 for (int32 I=0;I<TabButtons.Num();++I) Paint(TabButtons[I],TabLabels[I],I==Tab,Crimson);
 for (int32 I=0;I<WardButtons.Num();++I) Paint(WardButtons[I],WardLabels[I],WardIds.IsValidIndex(I) && WardIds[I]==SelectedWard,Gold);
 WardRow->SetVisibility(Tab==0?ESlateVisibility::Visible:ESlateVisibility::Hidden);
 Content->ClearChildren(); Proofs.Reset();
 Status->SetText(FText::FromString(!Data.bValid?TEXT("Reading the town..."):Data.Samples.IsEmpty()?TEXT("No history yet."):FString::Printf(TEXT("%s | %d hours | day %d, %02d:00"),bPreview?TEXT("Seven days ahead"):TEXT("This town"),Data.Samples.Num(),Data.Samples.Last().Day,Data.Samples.Last().Hour)));
 if (Tab==0)
 {
  for (const auto& Id:WardIds)
  {
   TArray<FGbPlotLine> Lines; FGbPlotLine Total; Total.Color=Ink; Total.Width=2.5f;
   FGbPlotLine R,S,D; R.Color=Rage; S.Color=Sorrow; D.Color=Dread; R.Width=S.Width=D.Width=2.f;
   for (const auto& Sample:Data.Samples) for (const auto& W:Sample.Wards) if (W.Id==Id)
   { Total.Points.Emplace(Sample.Tick,W.Total); R.Points.Emplace(Sample.Tick,W.Grudge.Rage); S.Points.Emplace(Sample.Tick,W.Grudge.Sorrow); D.Points.Emplace(Sample.Tick,W.Grudge.Dread); }
   Lines.Add(Total); if (Id==SelectedWard) { Lines.Add(R); Lines.Add(S); Lines.Add(D); }
   AddChart(WardName(Id),MoveTemp(Lines),Id==SelectedWard?96.f:40.f,Id);
  }
  ObsRow(Content,MakeText(TEXT("Total (ink)   Rage (red)   Sorrow (blue)   Dread (violet)"),16,Ink),2.f,2.f);
 }
 else if (Tab==1)
 {
  TArray<FGbPlotLine> Lines;
  for (int32 I=0;I<ItemIds.Num();++I)
  {
   FGbPlotLine Line; Line.Color=ItemColors[I]; Line.Width=2.5f;
   for (const auto& Sample:Data.Samples)
   {
    const auto* Broker=Sample.Prices.Find(TEXT("broker")); const float* Price=Broker?Broker->Items.Find(ItemIds[I]):nullptr;
    if (Price && !Sample.Closed.Contains(TEXT("broker"))) Line.Points.Emplace(Sample.Tick,*Price);
    else if (!Line.Points.IsEmpty()) { Lines.Add(Line); Line.Points.Reset(); }
   }
   if (!Line.Points.IsEmpty()) Lines.Add(Line);
  }
  AddChart(TEXT("The Talisman Broker / Ember"),MoveTemp(Lines),260,TEXT(""));
  UHorizontalBox* Legend=WidgetTree->ConstructWidget<UHorizontalBox>(); ObsRow(Content,Legend,2.f,4.f);
  for (int32 I=0;I<ItemNames.Num();++I) ObsCol(Legend,MakeText(ItemNames[I],16,ItemColors[I],true),true,8.f);
  TArray<FString> Closed; for (const auto& Sample:Data.Samples) for (const auto& Id:Sample.Closed) Closed.AddUnique(Id);
  ObsRow(Content,MakeText(Closed.IsEmpty()?TEXT("No shop closures in this history."):TEXT("Closed during this history: ")+FString::Join(Closed,TEXT(", ")),16,Ink),2.f,2.f);
 }
 else if (Tab==3) DrawAgents();
 else
 {
  if (Data.Rules.IsEmpty()) ObsRow(Content,MakeText(TEXT("No rules in this history."),17,Ink));
  for (const auto& Rule:Data.Rules)
  {
   UBorder* Slip=MakePanel(GbUi::PaperHi(),FMargin(14.f,10.f)); UHorizontalBox* In=WidgetTree->ConstructWidget<UHorizontalBox>(); Slip->SetContent(In);
   const FString Mark=Rule.Status==TEXT("active")?TEXT("Accepted"):Rule.Status==TEXT("rejected")?TEXT("Rejected"):TEXT("Repealed");
   UTextBlock* ChipText=nullptr;
   UBorder* Chip=MakeChip(Mark,Rule.Status==TEXT("active")?Leaf:Rule.Status==TEXT("rejected")?Crimson:GbUi::Paper3(),ChipText);
   ObsCol(In,Chip,false,12.f)->SetVerticalAlignment(VAlign_Top);
   UVerticalBox* Row=WidgetTree->ConstructWidget<UVerticalBox>(); ObsCol(In,Row,true,12.f);
   UTextBlock* Words=MakeText(Rule.Text,18,Ink,true); Words->SetAutoWrapText(true); ObsRow(Row,Words,0.f,4.f);
   ObsRow(Row,MakeText(FString::Printf(TEXT("%s [%s] | day %d, %02d:00"),*Rule.Author,*Rule.AuthorKind,Rule.Tick/24+1,Rule.Tick%24),15,Ink),0.f,2.f);
   if (!Rule.Reason.IsEmpty()) { UTextBlock* Why=MakeText(Rule.Reason,15,Crimson); Why->SetAutoWrapText(true); ObsRow(Row,Why,2.f,0.f); }
   if (Rule.Url.StartsWith(TEXT("https://testnet.nearblocks.io/")) || Rule.Url.StartsWith(TEXT("https://explorer.testnet.near.org/")))
   {
    auto* Proof=NewObject<UGbObservatoryProof>(this); Proof->Url=Rule.Url; Proofs.Add(Proof);
    UTextBlock* Label=nullptr; UButton* B=MakeKeyButton(TEXT("Ledger proof"),FString(),Label,GbUi::Paper2()); Label->SetFont(GbUi::Font(17,true));
    B->OnClicked.AddDynamic(Proof,&UGbObservatoryProof::Open); ObsCol(In,Sized(B,210.f,48.f),false,0.f);
   }
   ObsRow(Content,Slip,0.f,8.f);
  }
 }
 if (Tab<2)
 {
  // Day labels centred under the chart's seven day columns (slip padding 14 + plot inset 6).
  UHorizontalBox* Axis=WidgetTree->ConstructWidget<UHorizontalBox>(); ObsRow(Content,Axis,0.f,2.f)->SetPadding(FMargin(20.f,0.f,20.f,2.f));
  for (int32 Day=1;Day<=7;++Day)
  {
   UTextBlock* DayText=MakeText(FString::Printf(TEXT("Day %d"),Day),15,Ink); DayText->SetJustification(ETextJustify::Center);
   ObsCol(Axis,DayText,true,0.f);
  }
  ObsRow(Content,MakeText(TEXT("Gold tick: bankruptcy   Crimson tick: Calamity"),16,Ink),2.f,2.f);
 }
}

// The town's agents and the accounts they act from. Every number and line here is the server's (GET /agents);
// nothing is made up on this side. DRAFT wording, for the lead's review.
void UGbObservatoryWidget::DrawAgents()
{
 if (!Agents.bValid) { Status->SetText(FText::FromString(TEXT("Reading the agents' ledger pages..."))); return; }
 Status->SetText(FText::FromString(Agents.bEnabled?FString::Printf(TEXT("%d agents, each with its own ledger page"),Agents.Agents.Num()):FString(TEXT("On this server the agents act in the town only, not on the ledger."))));
 UTextBlock* Lead=MakeText(TEXT("These traders are not scripted shopkeepers. Each holds its own page in the Rite's ledger, pays and proposes rules from it, and can do nothing else with it: a daily Ember cap, a few actions a day."),16,Ink);
 Lead->SetAutoWrapText(true); ObsRow(Content,Lead,0.f,10.f);
 if (Agents.Agents.IsEmpty()) ObsRow(Content,MakeText(TEXT("No agent pages yet."),17,Ink));
 for (const auto& Agent:Agents.Agents)
 {
  UBorder* Slip=MakePanel(GbUi::PaperHi(),FMargin(14.f,10.f)); UVerticalBox* In=WidgetTree->ConstructWidget<UVerticalBox>(); Slip->SetContent(In);
  UHorizontalBox* Head=WidgetTree->ConstructWidget<UHorizontalBox>(); ObsRow(In,Head,0.f,4.f);
  ObsCol(Head,MakeText(Agent.Name,20,Ink,true),false,14.f);
  ObsCol(Head,MakeText(Agent.Account,15,Ink),true,14.f);
  const auto Amount=[](int32 V){ return V<0?FString(TEXT("-")):FString::FromInt(V); };
  ObsCol(Head,MakeText(FString::Printf(TEXT("Points %s"),*Amount(Agent.Points)),17,Ink,true),false,18.f);
  ObsCol(Head,MakeText(FString::Printf(TEXT("Ember %s"),*Amount(Agent.Ember)),17,Ink,true),false,12.f);
  UTextBlock* ChipText=nullptr; ObsCol(Head,MakeChip(Agent.bReady?TEXT("On the ledger"):TEXT("Town only"),Agent.bReady?Leaf:GbUi::Paper3(),ChipText),false,0.f);
  if (Agent.Actions.IsEmpty()) ObsRow(In,MakeText(TEXT("Nothing written yet."),15,Ink),0.f,0.f);
  for (int32 I=0;I<Agent.Actions.Num() && I<3;++I)
  {
   const auto& Act=Agent.Actions[I];
   UHorizontalBox* Line=WidgetTree->ConstructWidget<UHorizontalBox>(); ObsRow(In,Line,2.f,2.f);
   ObsCol(Line,MakeText(FString::Printf(TEXT("Day %d, %02d:00"),Act.Tick/24+1,Act.Tick%24),15,Ink,true),false,12.f);
   UTextBlock* Words=MakeText(Act.Text,15,Ink); Words->SetAutoWrapText(true); ObsCol(Line,Words,true,12.f);
   const bool bProof=Act.Url.StartsWith(TEXT("https://testnet.nearblocks.io/")) || Act.Url.StartsWith(TEXT("https://explorer.testnet.near.org/"));
   if (bProof)
   {
    auto* Proof=NewObject<UGbObservatoryProof>(this); Proof->Url=Act.Url; Proofs.Add(Proof);
    UTextBlock* Label=nullptr; UButton* B=MakeKeyButton(TEXT("Ledger proof"),FString(),Label,GbUi::Paper2()); Label->SetFont(GbUi::Font(15,true));
    B->OnClicked.AddDynamic(Proof,&UGbObservatoryProof::Open); ObsCol(Line,Sized(B,180.f,40.f),false,0.f);
   }
   else ObsCol(Line,MakeText(Act.Status==TEXT("sim-only")?TEXT("town only"):Act.Status==TEXT("failed")?TEXT("not written"):Act.Status==TEXT("pending")?TEXT("being written"):TEXT("written"),15,Ink),false,0.f);
  }
  ObsRow(Content,Slip,0.f,8.f);
 }
}
