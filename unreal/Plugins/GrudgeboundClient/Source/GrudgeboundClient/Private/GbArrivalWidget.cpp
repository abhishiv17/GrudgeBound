#include "GbArrivalWidget.h"
#include "GbApiSubsystem.h"
#include "GbExorcistLook.h"
#include "GbGreyboxMaterials.h"
#include "GbUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
 // Draft dialogue: about a minute aloud; the player controls its pace and can leave at any time.
 const TCHAR* ArrivalLines[]={
  TEXT("Welcome to Hollowmere. I'm Kessa, keeper of the Rite. Keep that debt notice safe. Before you walk into our market, there are a few things you should know."),
  TEXT("A stolen wage leaves Rage. A lost home leaves Sorrow. A rumour in the dark leaves Dread. We call what gathers here Grudge. When enough gathers, discarded things wake up as curses."),
  TEXT("You can drive a curse away, or weaken it and bind it into a talisman. A bound curse becomes your partner. The talisman belongs to you: keep it, train it, or trade it. Every attempt needs a blank talisman."),
  TEXT("The Rite is our shared rulebook. Earn points and you can propose a rule or make a vow. The merchants have their own money and make their own bargains. Their choices change prices, livelihoods, and the Grudge."),
  TEXT("You don't have to solve this town at once. Meet its people, check the corners, and keep what you find. Your Satchel carries your partners and supplies; its Journal remembers the next step. Now, who have I welcomed?")
 };
 void Row(UVerticalBox* Box,UWidget* Widget,float Gap=10.f) { auto* S=Box->AddChildToVerticalBox(Widget); S->SetPadding(FMargin(0,0,0,Gap)); }
}

void UGbArrivalWidget::BuildTree()
{
 SetIsFocusable(true);
 UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
 UBorder* Paper=MakePanel(GbUi::PaperHi(),FMargin(26,20));
 UVerticalBox* Stack=WidgetTree->ConstructWidget<UVerticalBox>(); Paper->SetContent(Sized(Stack,1040,0));
 Heading=MakeText(TEXT("Kessa / The harbour pier"),23,GbGreybox::Color::Crimson,true); Row(Stack,Heading);
 Content=WidgetTree->ConstructWidget<UVerticalBox>(); Row(Stack,Content);
 Status=MakeText(TEXT(""),17,GbGreybox::Color::Crimson); Row(Stack,Status);
 UHorizontalBox* Footer=WidgetTree->ConstructWidget<UHorizontalBox>(); Row(Stack,Footer,0);
 UTextBlock* Label=nullptr;
 UButton* Leave=MakeKeyButton(TEXT("Later"),TEXT("Esc"),Label,GbUi::Paper2()); Leave->OnClicked.AddDynamic(this,&UGbArrivalWidget::Later);
 Footer->AddChildToHorizontalBox(Sized(Leave,200,48))->SetPadding(FMargin(0,0,12,0));
 BackButton=MakeButton(TEXT("Back"),Label,GbUi::Paper2()); BackButton->OnClicked.AddDynamic(this,&UGbArrivalWidget::Back);
 Footer->AddChildToHorizontalBox(Sized(BackButton,160,48))->SetPadding(FMargin(0,0,12,0));
 NextButton=MakeButton(TEXT("Continue"),Label,GbGreybox::Color::Gold); NextLabel=Label; NextButton->OnClicked.AddDynamic(this,&UGbArrivalWidget::Next);
 Footer->AddChildToHorizontalBox(Sized(NextButton,0,48))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto* Place=Root->AddChildToCanvas(Paper); Place->SetAnchors(FAnchors(.5f,1)); Place->SetAlignment(FVector2D(.5f,1)); Place->SetPosition(FVector2D(0,-30)); Place->SetAutoSize(true);
 Redraw();
}
void UGbArrivalWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (Player()) Player()->OnPlayerChanged.AddUniqueDynamic(this,&UGbArrivalWidget::Changed);
 if (Api()) {Api()->OnSatchelAction.AddUniqueDynamic(this,&UGbArrivalWidget::Action); Api()->OnError.AddUniqueDynamic(this,&UGbArrivalWidget::Error);}
}
void UGbArrivalWidget::NativeDestruct()
{
 if (Player()) Player()->OnPlayerChanged.RemoveDynamic(this,&UGbArrivalWidget::Changed);
 if (Api()) {Api()->OnSatchelAction.RemoveDynamic(this,&UGbArrivalWidget::Action); Api()->OnError.RemoveDynamic(this,&UGbArrivalWidget::Error);}
 Super::NativeDestruct();
}
void UGbArrivalWidget::Open()
{
 Profile=Player()?Player()->GetSatchel().Profile:FGbArrivalProfile(); Page=0; bSubmitting=false;
 ChosenName=Profile.Name; ChosenLook=Profile.Look; ChosenPronouns=Profile.Pronouns; ChosenStarter=Profile.Starter;
 if (Profile.bComplete && Profile.Mint==TEXT("pending")) Page=7;
 Status->SetText(FText::GetEmpty()); Redraw(); if (Api()) Api()->FetchSatchel();
}
void UGbArrivalWidget::ReadName() {if (NameInput) ChosenName=NameInput->GetText().ToString();}
void UGbArrivalWidget::PreviewLook() {if (GetOwningPlayer()) if (UGbExorcistLook* Look=UGbExorcistLook::Ensure(GetOwningPlayer()->GetPawn())) Look->SetPresentation(ChosenLook);}
void UGbArrivalWidget::Redraw()
{
 if (!Content) return; Content->ClearChildren(); NameInput=nullptr;
 const auto Text=[this](const FString& Line){Row(Content,MakeText(Line,20,GbGreybox::Color::Ink));};
 BackButton->SetIsEnabled(Page>0 && !bSubmitting && !Profile.bComplete);
 NextButton->SetIsEnabled(!bSubmitting);
 NextLabel->SetText(FText::FromString(Page==7?TEXT("Confirm arrival"):TEXT("Continue")));
 if (Page<5) {Text(ArrivalLines[Page]); return;}
 if (!Profile.bValid) {Text(TEXT("Your arrival record isn't ready. Return to the pier after reconnecting, or continue exploring for now.")); NextLabel->SetText(FText::FromString(TEXT("Return to town"))); return;}
 if (Profile.bComplete && Profile.Mint!=TEXT("pending")) {Text(TEXT("Welcome back, ")+Profile.Name+TEXT(". Your first partner is already recorded. Check the Journal for your next step.")); NextLabel->SetText(FText::FromString(TEXT("Return to town"))); return;}
 if (Page==5)
 {
  Text(TEXT("Your name (2-24 letters). Your look and pronouns do not change your stats."));
  NameInput=WidgetTree->ConstructWidget<UEditableTextBox>();
  FEditableTextBoxStyle NameStyle=NameInput->GetWidgetStyle(); NameStyle.SetFont(GbUi::Font(22)).SetForegroundColor(GbGreybox::Color::Ink).SetBackgroundColor(GbUi::Paper2()); NameInput->SetWidgetStyle(NameStyle);
  NameInput->SetText(FText::FromString(ChosenName)); NameInput->SetHintText(FText::FromString(TEXT("Your name"))); Row(Content,Sized(NameInput,0,52));
  UTextBlock* L=nullptr; UButton* B=MakeButton(TEXT("Look: ")+ChosenLook+TEXT("  /  Change"),L,GbUi::Paper2()); B->OnClicked.AddDynamic(this,&UGbArrivalWidget::ChangeLook); Row(Content,Sized(B,0,48));
  B=MakeButton(TEXT("Refer to me as: ")+ChosenPronouns+TEXT("  /  Change"),L,GbUi::Paper2()); B->OnClicked.AddDynamic(this,&UGbArrivalWidget::ChangePronouns); Row(Content,Sized(B,0,48));
  if (Profile.bLegacyStarter) Text(TEXT("Your existing partner stays with you. You won't receive a second starter."));
  PreviewLook();
 }
 else if (Page==6)
 {
  Text(TEXT("Choose your first partner. This choice is permanent."));
  for (const FGbStarterChoice& C:Profile.Choices)
  {
   UGbPickRowWidget* R=CreateWidget<UGbPickRowWidget>(this); if (!R) continue;
   R->Setup(GbUi::CurseArt(C.Id),C.Name+TEXT(" / ")+C.Emotion,FString::Printf(TEXT("Ink %d  Attack %d  Guard %d  Speed %d\n%s"),C.Hp,C.Attack,C.Guard,C.Speed,*C.Personality),-1,FString(),ChosenStarter==C.Id?TEXT("Chosen"):TEXT("Choose"),GbGreybox::Color::Gold,ChosenStarter==C.Id);
   const FString Id=C.Id; R->OnClick=[this,Id](){ChosenStarter=Id;Redraw();}; Row(Content,R,8);
  }
  NextButton->SetIsEnabled(!Profile.Choices.IsEmpty());
 }
 else
 {
  const FGbStarterChoice* C=Profile.Choices.FindByPredicate([this](const FGbStarterChoice& S){return S.Id==ChosenStarter;});
  Text(FString::Printf(TEXT("%s  /  %s  /  %s\nFirst partner: %s"),*ChosenName,*ChosenLook,*ChosenPronouns,C?*C->Name:*ChosenStarter));
  Text(Profile.Mint==TEXT("pending")?TEXT("Your choice is saved. Retry to finish recording the same partner."):TEXT("Kessa: Ready? Once recorded, your first partner cannot be chosen again."));
 }
}
void UGbArrivalWidget::Next()
{
 if (bSubmitting) return;
 if (Page>=5 && (!Profile.bValid || (Profile.bComplete && Profile.Mint!=TEXT("pending")))) {Later();return;}
 if (Page==5) {ReadName(); if (ChosenName.TrimStartAndEnd().Len()<2 || ChosenName.Len()>24) {Status->SetText(FText::FromString(TEXT("Choose a name between 2 and 24 letters.")));return;}}
 if (Page<7) {Page=Page==5 && Profile.bLegacyStarter?7:Page+1;Status->SetText(FText::GetEmpty());Redraw();return;}
 if (Api()) {bSubmitting=true;Status->SetText(FText::FromString(TEXT("Recording your arrival...")));Redraw();Api()->CompleteProfile(ChosenName,ChosenLook,ChosenPronouns,ChosenStarter);}
}
void UGbArrivalWidget::Back() {ReadName(); Page=Page==7 && Profile.bLegacyStarter?5:FMath::Max(0,Page-1);Redraw();}
void UGbArrivalWidget::Later() {bSubmitting=false;OnClose.Broadcast();}
void UGbArrivalWidget::ChangeLook() {ReadName();ChosenLook=ChosenLook==TEXT("masculine")?TEXT("feminine"):TEXT("masculine");Redraw();}
void UGbArrivalWidget::ChangePronouns() {ReadName();ChosenPronouns=ChosenPronouns==TEXT("they")?TEXT("he"):ChosenPronouns==TEXT("he")?TEXT("she"):TEXT("they");Redraw();}
void UGbArrivalWidget::Changed()
{
 if (Player())
 {
  const bool WasValid=Profile.bValid; Profile=Player()->GetSatchel().Profile;
  if (!bSubmitting && (!WasValid || Profile.bComplete))
  {
   ChosenName=Profile.Name; ChosenLook=Profile.Look; ChosenPronouns=Profile.Pronouns; ChosenStarter=Profile.Starter;
   NameInput=nullptr;
   if (Profile.Mint==TEXT("pending")) Page=7;
  }
 }
 if (IsVisible()) {ReadName();Redraw();}
}
void UGbArrivalWidget::Action(const FString& Message) {if (!bSubmitting) return;bSubmitting=false;Status->SetText(FText::FromString(Message)); if (Profile.bComplete && Profile.Mint!=TEXT("pending")) Later();else Redraw();}
void UGbArrivalWidget::Error(const FString& What,int32,const FString& Message) {if (What!=TEXT("profile/complete") && What!=TEXT("satchel")) return;bSubmitting=false;Status->SetText(FText::FromString(Message));Redraw();}
FReply UGbArrivalWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{if (Event.GetKey()==EKeys::Escape) {Later();return FReply::Handled();} return Super::NativeOnPreviewKeyDown(Geometry,Event);}
