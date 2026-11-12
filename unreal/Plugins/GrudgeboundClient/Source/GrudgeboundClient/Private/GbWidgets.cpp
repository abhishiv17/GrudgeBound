#include "GbWidgets.h"

#include "GbApiSubsystem.h"
#include "GbAudio.h"
#include "Sound/SoundBase.h"
#include "GbAuthSubsystem.h"
#include "GbStorySubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbPlayerSubsystem.h"
#include "GbSettings.h"
#include "GbSatchelWidget.h"
#include "GbSettingsMenu.h"
#include "GbUiStyle.h"
#include "GbUserSettings.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Math/RandomStream.h"

namespace
{
	namespace C = GbGreybox::Color;

	// Kind 0: frame. 1: frame with crimson corner studs. 2: button. 16 x 16 art pixels, 3 texels each.
	UTexture2D* PixelFrame(int32 Kind)
	{
		static UTexture2D* Made[3] = {};
		if (Made[Kind]) return Made[Kind];
		const int32 N = 16, Px = 3, Size = N * Px;
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		Texture->AddToRoot(); Texture->SRGB = true; Texture->Filter = TF_Nearest;
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		const FColor Frame(26, 23, 38), Stud(176, 21, 58), Light(255, 255, 255);
		for (int32 Y = 0; Y < Size; ++Y) for (int32 X = 0; X < Size; ++X)
		{
			const int32 AX = X / Px, AY = Y / Px;
			const int32 FromX = FMath::Min(AX, N - 1 - AX), FromY = FMath::Min(AY, N - 1 - AY);
			FColor Color = Kind == 2 ? FColor(232, 232, 232) : FColor(246, 246, 246);
			if (FromX == 0 && FromY == 0) Color = FColor(0, 0, 0, 0);
			else if (FromX == 0 || FromY == 0) Color = Frame;
			else if (Kind == 2) { if (AY == 1) Color = Light; else if (AY >= N - 3) Color = FColor(176, 176, 176); }
			else if (FromX == 1 || FromY == 1) Color = Light;
			if (Kind == 1 && FMath::Abs(FromX - 3) + FMath::Abs(FromY - 3) <= 1) Color = Stud;
			Pixels[Y * Size + X] = Color;
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock(); Texture->UpdateResource();
		return Made[Kind] = Texture;
	}
	FSlateBrush FrameBrush(int32 Kind, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush; Brush.SetResourceObject(PixelFrame(Kind)); Brush.ImageSize = FVector2D(48, 48);
		Brush.DrawAs = ESlateBrushDrawType::Box; Brush.Margin = FMargin((Kind == 1 ? 6.f : Kind == 2 ? 4.f : 3.f) / 16.f);
		Brush.TintColor = FSlateColor(Tint); return Brush;
	}
	FSlateBrush PaperBrush() { return FrameBrush(0); }
	/** One of the pixel-art pictures in /Game/Grudgebound/UI/Battle (unreal/tools/export-battle-art.ts), drawn Scale
	    times its size. Nothing is drawn when the picture is not imported. */
	FSlateBrush ArtBrush(const FString& Name, float Scale)
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Grudgebound/UI/Battle/%s.%s"), *Name, *Name)))
		{
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(Texture->GetSizeX() * Scale, Texture->GetSizeY() * Scale);
		}
		else { Brush.DrawAs = ESlateBrushDrawType::NoDrawType; Brush.ImageSize = FVector2D(48.f * Scale, 48.f * Scale); }
		return Brush;
	}
	/** "riot-bell" -> "curse_riot_bell". */
	FString CurseArt(const FString& SpeciesId) { return TEXT("curse_") + SpeciesId.Replace(TEXT("-"), TEXT("_")); }
	/** The title screen's sky: a top-to-bottom wash, see-through so the Ward shows under it. */
	FSlateBrush DuskBrush()
	{
		static UTexture2D* Texture = nullptr;
		if (!Texture)
		{
			Texture = UTexture2D::CreateTransient(1, 64, PF_B8G8R8A8);
			Texture->AddToRoot(); Texture->SRGB = true; Texture->Filter = TF_Bilinear; Texture->AddressY = TA_Clamp;
			FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			const FLinearColor Top(14.f, 11.f, 38.f, 240.f), Mid(44.f, 27.f, 69.f, 170.f), Low(60.f, 30.f, 60.f, 120.f);
			for (int32 Y = 0; Y < 64; ++Y)
			{
				const float T = Y / 63.f;
				const FLinearColor V = T < 0.5f ? FMath::Lerp(Top, Mid, T * 2.f) : FMath::Lerp(Mid, Low, T * 2.f - 1.f);
				Pixels[Y] = FColor(uint8(V.R), uint8(V.G), uint8(V.B), uint8(V.A));
			}
			Texture->GetPlatformData()->Mips[0].BulkData.Unlock(); Texture->UpdateResource();
		}
		FSlateBrush Brush; Brush.SetResourceObject(Texture); Brush.ImageSize = FVector2D(1, 64); Brush.DrawAs = ESlateBrushDrawType::Image; return Brush;
	}
	/** bPixel: Pixelify Sans (Regular = semi-bold cut, Bold = bold cut). Otherwise Courier Prime. Before the new
	    faces are imported (UI/import-fonts.py) the earlier Mincho faces are used. */
	TSharedPtr<const FCompositeFont> UiFont(bool bPixel)
	{
		static TSharedPtr<FCompositeFont> Pixel, Body;
		TSharedPtr<FCompositeFont>& Font = bPixel ? Pixel : Body;
		if (!Font)
		{
			const auto Face = [](const TCHAR* Name) { return LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("/Game/Grudgebound/UI/Fonts/%s.%s"), Name, Name)); };
			UFontFace* R = Face(bPixel ? TEXT("PixelifySansSemiBold") : TEXT("CourierPrimeRegular"));
			UFontFace* B = bPixel ? Face(TEXT("PixelifySansBold")) : R;
			if (!R || !B)
			{
				R = Face(bPixel ? TEXT("YujiSyukuRegular") : TEXT("ShipporiMinchoRegular"));
				B = bPixel ? R : Face(TEXT("ShipporiMinchoBold"));
			}
			if (!R || !B) return nullptr;
			R->AddToRoot(); B->AddToRoot(); Font = MakeShared<FCompositeFont>();
			FTypefaceEntry RE(FName(TEXT("Regular"))); RE.Font = FFontData(R); Font->DefaultTypeface.Fonts.Add(RE);
			FTypefaceEntry BE(FName(TEXT("Bold"))); BE.Font = FFontData(B); Font->DefaultTypeface.Fonts.Add(BE);
		}
		return Font;
	}

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
		return Fraction > 0.5f ? C::Leaf : Fraction > 0.2f ? C::Gold : C::Rage;
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

FSlateFontInfo GbUi::Font(int32 Size, bool bBold, bool bTitle)
{
	FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold || bTitle ? "Bold" : "Regular", Size);
	if (const TSharedPtr<const FCompositeFont> Family = UiFont(bBold || bTitle)) Info = FSlateFontInfo(Family, Size);
	Info.Size = Size;
	Info.TypefaceFontName = bTitle ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	return Info;
}
FString GbUi::CurseArt(const FString& SpeciesId) { return TEXT("curse_") + SpeciesId.Replace(TEXT("-"), TEXT("_")); }
FSlateBrush GbUi::PanelBrush(const FLinearColor& Tint, bool bStuds) { return FrameBrush(bStuds ? 1 : 0, Tint); }
FSlateBrush GbUi::ButtonBrush(const FLinearColor& Tint) { return FrameBrush(2, Tint); }

// ================================================================ building panels

// The Mender: pick a talisman on the left (its ink at a glance), see it and the price on the right, restore.
void UGbMenderWidget::BuildTree()
{
 UTextBlock* TitleText=nullptr; UHorizontalBox* Footer=nullptr;
 UVerticalBox* Body=MakeShell(TEXT("Mender's House"),TEXT("icon_salve"),1100.f,TitleText,Subtitle,Footer);
 Status=ShellStatus; Status->SetText(FText::FromString(TEXT("Reading your pockets...")));
 UHorizontalBox* Split=WidgetTree->ConstructWidget<UHorizontalBox>(); AddRow(Body,Split,0.f,0.f);
 UVerticalBox* Left=WidgetTree->ConstructWidget<UVerticalBox>(); AddCol(Split,Left,true,24.f)->SetVerticalAlignment(VAlign_Top);
 AddRow(Left,MakeSection(TEXT("Your talismans")),0.f,10.f);
 Rows=WidgetTree->ConstructWidget<UScrollBox>(); AddRow(Left,Sized(Rows,0.f,440.f));
 UBorder* Detail=MakePanel(GbUi::Paper2(),FMargin(20.f,18.f)); AddCol(Split,Sized(Detail,400.f,0.f),false,0.f)->SetVerticalAlignment(VAlign_Top);
 UVerticalBox* D=WidgetTree->ConstructWidget<UVerticalBox>(); Detail->SetContent(D);
 UBorder* Plate=MakePanel(GbUi::PaperHi(),FMargin(8.f)); Plate->SetContent(MakeArt(FString(),176.f,PickedArt));
 AddRow(D,Plate,0.f,12.f)->SetHorizontalAlignment(HAlign_Center);
 PickedName=MakeText(TEXT(""),28,C::Ink,true); PickedName->SetJustification(ETextJustify::Center); AddRow(D,PickedName,0.f,8.f);
 PickedBar=MakeBar(C::Sorrow); AddRow(D,Sized(PickedBar,0.f,16.f),0.f,4.f);
 PickedInk=MakeText(TEXT(""),16,C::Ink); PickedInk->SetJustification(ETextJustify::Center); AddRow(D,PickedInk,0.f,14.f);
 UHorizontalBox* PriceRow=WidgetTree->ConstructWidget<UHorizontalBox>();
 AddCol(PriceRow,MakeText(TEXT("Price"),18,C::Ink,true),true,8.f); PriceChip=MakeChip(TEXT(""),C::Gold,PriceText); AddCol(PriceRow,PriceChip,false,0.f);
 AddRow(D,PriceRow,0.f,12.f);
 RestoreButton=MakeKeyButton(TEXT("Restore ink"),FString(),RestoreLabel,C::Crimson);
 RestoreButton->OnClicked.AddDynamic(this,&UGbMenderWidget::Restore); AddRow(D,Sized(RestoreButton,0.f,60.f));
 UTextBlock* Label=nullptr; UButton* Back=MakeKeyButton(TEXT("Return"),KeyName(TEXT("Settings")),Label,GbUi::Paper2());
 Back->OnClicked.AddDynamic(this,&UGbMenderWidget::Close); AddCol(Footer,Sized(Back,260.f,56.f),false,0.f);
 Redraw();
}
void UGbMenderWidget::NativeConstruct()
{
 Super::NativeConstruct(); if (Player()) Player()->OnPlayerChanged.AddUniqueDynamic(this,&UGbMenderWidget::Refresh);
 if (Api()) { Api()->OnSatchelAction.AddUniqueDynamic(this,&UGbMenderWidget::Action); Api()->OnError.AddUniqueDynamic(this,&UGbMenderWidget::Error); }
}
void UGbMenderWidget::NativeDestruct()
{
 if (Player()) Player()->OnPlayerChanged.RemoveDynamic(this,&UGbMenderWidget::Refresh);
 if (Api()) { Api()->OnSatchelAction.RemoveDynamic(this,&UGbMenderWidget::Action); Api()->OnError.RemoveDynamic(this,&UGbMenderWidget::Error); } Super::NativeDestruct();
}
void UGbMenderWidget::Open() { Status->SetText(FText::GetEmpty()); Refresh(); if (Player()) Player()->RefreshFromChain(); }
void UGbMenderWidget::Refresh()
{
 Data=Player()?Player()->GetSatchel():FGbSatchel();
 Names.Reset(); Species.Reset();
 if (Player() && Api()) for (const FGbTalisman& T:Player()->GetTalismans())
 {
  const FString Id=UGbPlayerSubsystem::ServerTokenId(T); Names.Add(Id,Api()->GetCurse(T.SpeciesId).Name); Species.Add(Id,T.SpeciesId);
 }
 Price=Api()?UGbSatchelWidget::MenderPriceFrom(Api()->GetWorldState()):-1;
 bBusy=!Api() || Api()->IsSatchelBusy();
 Redraw();
}
void UGbMenderWidget::Redraw()
{
 if (!Rows) return;
 Tokens=Data.Carried; for (const FString& Id:Data.Case) Tokens.AddUnique(Id);
 if (!Tokens.Contains(Selected))
 {
  // Start on the first talisman that needs the Mender.
  Selected=Tokens.Num()?Tokens[0]:FString();
  for (const FString& Id:Tokens) if (const FGbInk* I=Data.Ink.Find(Id)) if (I->Hp<I->MaxHp) { Selected=Id; break; }
 }
 const auto NameOf=[this](const FString& Id){ const FString* N=Names.Find(Id); return N && !N->IsEmpty()?*N:FString(TEXT("Sealed talisman")); };
 const auto ArtOf=[this](const FString& Id){ const FString* S=Species.Find(Id); return S?CurseArt(*S):FString(); };
 Rows->ClearChildren();
 for (const FString& Id:Tokens)
 {
  UGbPickRowWidget* Row=CreateWidget<UGbPickRowWidget>(this,UGbPickRowWidget::StaticClass()); if (!Row) continue;
  const FGbInk* Ink=Data.Ink.Find(Id);
  Row->Setup(ArtOf(Id),NameOf(Id),Data.Carried.Contains(Id)?TEXT("In your Satchel"):TEXT("In the Case"),
   Ink && Ink->MaxHp>0?float(Ink->Hp)/Ink->MaxHp:-1.f,Ink?FString::Printf(TEXT("%d / %d"),Ink->Hp,Ink->MaxHp):FString(),
   Ink && Ink->MaxHp>0 && Ink->Hp>=Ink->MaxHp?TEXT("Full"):TEXT(""),GbUi::Paper3(),Id==Selected);
  const FString Pick=Id; Row->OnClick=[this,Pick](){ Selected=Pick; Redraw(); };
  if (UScrollBoxSlot* RowSlot=Cast<UScrollBoxSlot>(Rows->AddChild(Row))) RowSlot->SetPadding(FMargin(0.f,0.f,8.f,8.f));
 }
 if (!Tokens.Num()) Rows->AddChild(MakeText(TEXT("No talismans to show yet."),16,C::Ink));
 const FGbInk* Ink=Data.Ink.Find(Selected); const bool bInk=Ink && Ink->MaxHp>0;
 SetArt(PickedArt,Selected.IsEmpty()?FString():ArtOf(Selected),176.f);
 PickedName->SetText(FText::FromString(Selected.IsEmpty()?FString():NameOf(Selected)));
 PickedBar->SetPercent(bInk?float(Ink->Hp)/Ink->MaxHp:0.f); PickedBar->SetFillColorAndOpacity(bInk && Ink->Hp<=0?C::Crimson:C::Sorrow);
 PickedInk->SetText(FText::FromString(bInk?FString::Printf(TEXT("Ink %d / %d"),Ink->Hp,Ink->MaxHp):FString(TEXT("Reading ink..."))));
 SetChip(PriceChip,PriceText,Price<0?FString():FString::Printf(TEXT("%d Ember"),Price),C::Gold);
 Subtitle->SetText(FText::FromString(Data.bValid && Data.Ember>=0?FString::Printf(TEXT("Your Ember: %d"),Data.Ember):FString()));
 RestoreButton->SetIsEnabled(Data.bValid && bInk && Ink->Hp<Ink->MaxHp && Price>=0 && Data.Ember>=Price && !bBusy);
}
void UGbMenderWidget::Restore()
{
 if (Api() && RestoreButton->GetIsEnabled() && !Selected.IsEmpty()) { RestoreButton->SetIsEnabled(false); Api()->RestoreInk(Selected); }
}
void UGbMenderWidget::Close() { OnClose.Broadcast(); }
void UGbMenderWidget::Action(const FString& Message) { if (IsVisible()) { Status->SetText(FText::FromString(Message)); Refresh(); } }
void UGbMenderWidget::Error(const FString& What,int32,const FString& Message)
{
 if (!IsVisible() || (What!=TEXT("mender/restore") && What!=TEXT("satchel"))) return;
 Status->SetText(FText::FromString(Message.Contains(TEXT("NEAR")) || Message.Contains(TEXT("chain"))?FString(TEXT("The town's ledger is unavailable. Refresh before trying again.")):Message)); Refresh();
}

// Your room: the debt notice pinned up as a slip of paper, then the two things to do here.
void UGbRoomWidget::BuildTree()
{
 UTextBlock* TitleText=nullptr; UTextBlock* Sub=nullptr; UHorizontalBox* Footer=nullptr;
 UVerticalBox* Body=MakeShell(TEXT("Your room"),TEXT("talisman"),900.f,TitleText,Sub,Footer);
 Sub->SetVisibility(ESlateVisibility::Collapsed); Status=ShellStatus;
 UBorder* Slip=MakePanel(GbUi::PaperHi(),FMargin(22.f,16.f)); UVerticalBox* SlipCol=WidgetTree->ConstructWidget<UVerticalBox>(); Slip->SetContent(SlipCol);
 AddRow(SlipCol,MakeText(TEXT("Debt notice"),22,C::Ink,true),0.f,6.f);
 AddRow(SlipCol,MakeText(TEXT("You arrived with a debt notice folded inside your coat."),18,C::Ink),0.f,0.f);
 AddRow(Body,Slip,0.f,18.f);
 UHorizontalBox* Choices=WidgetTree->ConstructWidget<UHorizontalBox>(); AddRow(Body,Choices,0.f,0.f);
 UTextBlock* Label=nullptr;
 RestButton=MakeKeyButton(TEXT("Rest until morning (free)"),FString(),Label,C::Crimson); RestButton->OnClicked.AddDynamic(this,&UGbRoomWidget::Rest);
 AddCol(Choices,Sized(RestButton,0.f,64.f),true,16.f);
 UButton* Chest=MakeKeyButton(TEXT("Talisman Case"),FString(),Label,C::Gold); Chest->OnClicked.AddDynamic(this,&UGbRoomWidget::Case);
 AddCol(Choices,Sized(Chest,0.f,64.f),true,0.f);
 UButton* Back=MakeKeyButton(TEXT("Return"),KeyName(TEXT("Settings")),Label,GbUi::Paper2()); Back->OnClicked.AddDynamic(this,&UGbRoomWidget::Close);
 AddCol(Footer,Sized(Back,260.f,56.f),false,0.f);
}
void UGbRoomWidget::NativeConstruct()
{
 Super::NativeConstruct(); if (Player()) Player()->OnPlayerChanged.AddUniqueDynamic(this,&UGbRoomWidget::RefreshState);
 if (Api()) { Api()->OnSatchelAction.AddUniqueDynamic(this,&UGbRoomWidget::Action); Api()->OnError.AddUniqueDynamic(this,&UGbRoomWidget::Error); }
}
void UGbRoomWidget::NativeDestruct()
{
 if (Player()) Player()->OnPlayerChanged.RemoveDynamic(this,&UGbRoomWidget::RefreshState);
 if (Api()) { Api()->OnSatchelAction.RemoveDynamic(this,&UGbRoomWidget::Action); Api()->OnError.RemoveDynamic(this,&UGbRoomWidget::Error); } Super::NativeDestruct();
}
void UGbRoomWidget::RefreshState() { if (RestButton) RestButton->SetIsEnabled(Api() && !Api()->IsSatchelBusy()); }
void UGbRoomWidget::Open() { Status->SetText(FText::GetEmpty()); RefreshState(); }
void UGbRoomWidget::Rest() { if (Api() && RestButton->GetIsEnabled()) { RestButton->SetIsEnabled(false); Api()->Rest(); } }
void UGbRoomWidget::Case() { OnCase.Broadcast(); }
void UGbRoomWidget::Close() { OnClose.Broadcast(); }
void UGbRoomWidget::Action(const FString& Message) { if (IsVisible()) { Status->SetText(FText::FromString(Message)); RestButton->SetIsEnabled(Api() && !Api()->IsSatchelBusy()); } }
void UGbRoomWidget::Error(const FString& What,int32,const FString& Message) { if (IsVisible() && What==TEXT("rest")) { Status->SetText(FText::FromString(Message)); RestButton->SetIsEnabled(Api() && !Api()->IsSatchelBusy()); } }

// ================================================================ base

bool UGbWidgetBase::Initialize()
{
	const bool bDone = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
	return bDone;
}

void UGbWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	if (WidgetTree)
	{
		TArray<UWidget*> Widgets; WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* W:Widgets) if (UComboBoxString* Combo=Cast<UComboBoxString>(W))
		{
			if (!Combo->OnGenerateWidgetEvent.IsBound()) Combo->OnGenerateWidgetEvent.BindDynamic(this,&UGbWidgetBase::MakeChoice);
			FComboBoxStyle Style=Combo->GetWidgetStyle();
			Style.ComboButtonStyle.ButtonStyle.Normal=PaperBrush(); Style.ComboButtonStyle.ButtonStyle.Normal.TintColor=FSlateColor(C::Paper);
			Style.ComboButtonStyle.ButtonStyle.Hovered=Style.ComboButtonStyle.ButtonStyle.Normal;
			Style.ComboButtonStyle.ButtonStyle.Pressed=Style.ComboButtonStyle.ButtonStyle.Normal;
			Style.ComboButtonStyle.DownArrowImage.TintColor=FSlateColor(C::Ink);
			Style.ComboButtonStyle.MenuBorderBrush=Style.ComboButtonStyle.ButtonStyle.Normal; Combo->SetWidgetStyle(Style);
			FTableRowStyle Row=Combo->GetItemStyle(); Row.EvenRowBackgroundBrush=PaperBrush(); Row.OddRowBackgroundBrush=PaperBrush();
			Row.EvenRowBackgroundBrush.TintColor=FSlateColor(C::Paper); Row.OddRowBackgroundBrush.TintColor=FSlateColor(C::Paper);
			Row.TextColor=FSlateColor(C::Ink); Row.SelectedTextColor=FSlateColor(C::Ink); Combo->SetItemStyle(Row);
		}
	}
}

UWidget* UGbWidgetBase::MakeChoice(FString Item) { return MakeText(Item,15,C::Ink); }

void UGbWidgetBase::SetVisibility(ESlateVisibility InVisibility)
{
	TargetVisibility=InVisibility;
	const bool bOpening=InVisibility!=ESlateVisibility::Collapsed && InVisibility!=ESlateVisibility::Hidden;
	if (!bOpening && !bHasOpened)
	{
		VisibilityBlend=0; SetRenderOpacity(0); Super::SetVisibility(InVisibility); return;
	}
	if (bOpening) { if (!bHasOpened) { VisibilityBlend=0; SetRenderOpacity(0); } bHasOpened=true; Super::SetVisibility(InVisibility); }
	else if (IsVisible()) Super::SetVisibility(ESlateVisibility::HitTestInvisible);
	bFadingVisibility=true; SetRenderTransformPivot(FVector2D(.5,.5));
}
void UGbWidgetBase::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	// The shell's ink note shows only while it says something (its space stays, so nothing jumps).
	if (ShellNote && ShellStatus) ShellNote->SetVisibility(ShellStatus->GetText().IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	if (!bFadingVisibility) return;
	const bool bOpening=TargetVisibility!=ESlateVisibility::Collapsed && TargetVisibility!=ESlateVisibility::Hidden;
	const float Target=bOpening?1.f:0.f;
	VisibilityBlend=FMath::FInterpConstantTo(VisibilityBlend,Target,DeltaTime,1.f/.15f);
	SetRenderOpacity(VisibilityBlend); SetRenderScale(FVector2D(.975f+.025f*VisibilityBlend));
	if (FMath::IsNearlyEqual(VisibilityBlend,Target)) { bFadingVisibility=false; if (!bOpening) Super::SetVisibility(TargetVisibility); }
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
	FLinearColor Readable=Color; Readable.A=FMath::Max(Readable.A,.85f);
	T->SetColorAndOpacity(FSlateColor(Readable));
	FSlateFontInfo Font = T->GetFont();
	if (const TSharedPtr<const FCompositeFont> Family=UiFont(bBold)) Font=FSlateFontInfo(Family,FMath::Max(Size,14));
	Font.Size = FMath::Max(Size,14);
	Font.TypefaceFontName = bBold && Size>=22 ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	T->SetFont(Font);
	T->SetAutoWrapText(true);
	return T;
}

UBorder* UGbWidgetBase::MakePanel(const FLinearColor& Background, const FMargin& Inner)
{
	return MakeFrame(Background,Inner);
}
UBorder* UGbWidgetBase::MakeFrame(const FLinearColor& Background, const FMargin& Inner)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	// Big cards get the studded frame; chips and rows the plain one.
	B->SetBrush(FrameBrush(Inner.Left>=20.f && Inner.Top>=14.f ? 1 : 0));
	B->SetBrushColor(Background);
	B->SetPadding(Inner);
	return B;
}

UButton* UGbWidgetBase::MakeButton(const FString& Label, UTextBlock*& OutLabel, const FLinearColor& Tint)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style=B->GetStyle(); Style.Normal=FrameBrush(2); Style.Hovered=Style.Normal; Style.Pressed=Style.Normal; Style.Disabled=Style.Normal;
	Style.Hovered.TintColor=FSlateColor(FLinearColor::White*1.08f); Style.Pressed.TintColor=FSlateColor(FLinearColor::White*.83f);
	Style.Disabled.TintColor=FSlateColor(FLinearColor::White*.55f); Style.NormalPadding=FMargin(12,6,12,9); Style.PressedPadding=FMargin(12,8,12,7);
	if (UGbAudio* Audio = UGbAudio::Get(this)) if (USoundBase* Click = Audio->Find(TEXT("ui_click"))) Style.PressedSlateSound.SetResourceObject(Click);
	B->SetStyle(Style);
	B->SetBackgroundColor(Tint);
	OutLabel = MakeText(Label, 15, Tint == C::Crimson ? GbUi::PaperHi() : C::Ink, true);
	OutLabel->SetJustification(ETextJustify::Center);
	OutLabel->SetAutoWrapText(false);
	B->AddChild(OutLabel);
	return B;
}

UProgressBar* UGbWidgetBase::MakeBar(const FLinearColor& Fill)
{
	UProgressBar* P = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style = P->GetWidgetStyle();
	Style.BackgroundImage = FrameBrush(0, GbUi::Paper3());
	Style.FillImage = FSlateBrush(); Style.FillImage.DrawAs = ESlateBrushDrawType::Image; Style.FillImage.SetResourceObject(nullptr);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	P->SetWidgetStyle(Style);
	P->SetFillColorAndOpacity(Fill);
	P->SetPercent(0.f);
	return P;
}

UWidget* UGbWidgetBase::Sized(UWidget* Content, float Width, float Height)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (Width > 0.f) Box->SetWidthOverride(Width);
	if (Height > 0.f)
	{
		// Text and framed content may grow for font metrics/wrapping; lists keep their scroll bounds.
		if (Content && (Content->IsA<UTextBlock>() || Content->IsA<UBorder>() || Content->IsA<UVerticalBox>())) Box->SetMinDesiredHeight(Height);
		else Box->SetHeightOverride(Height);
	}
	Box->SetContent(Content);
	return Box;
}

// ================================================================ panel shell

UVerticalBox* UGbWidgetBase::MakeShell(const FString& TitleText, const FString& Icon, float Width, UTextBlock*& OutTitle, UTextBlock*& OutSubtitle, UHorizontalBox*& OutFooter)
{
	// The encounter poster's language for every panel: the Ward dimmed, a crimson ribbon naming the place, a paper
	// card under it. Centred; the DPI curve scales the 1080p measurements to the window.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(); Shade->SetBrushColor(FLinearColor(C::Ink.R, C::Ink.G, C::Ink.B, 0.55f));
	UCanvasPanelSlot* ShadeSlot = Root->AddChildToCanvas(Shade); ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ShadeSlot->SetOffsets(FMargin(0));

	UVerticalBox* Poster = WidgetTree->ConstructWidget<UVerticalBox>();
	UBorder* Ribbon = MakePanel(C::Crimson, FMargin(28.f, 10.f));
	UHorizontalBox* RibbonRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Ribbon->SetContent(RibbonRow);
	if (!Icon.IsEmpty()) { UImage* Ignored = nullptr; AddCol(RibbonRow, MakeArt(Icon, 40.f, Ignored), false, 14.f); }
	OutTitle = MakeText(TitleText, 30, GbUi::PaperHi(), true); OutTitle->SetAutoWrapText(false);
	AddCol(RibbonRow, OutTitle, false, 0.f);
	AddRow(Poster, Ribbon, 0.f, 0.f)->SetHorizontalAlignment(HAlign_Center);

	UBorder* Card = MakePanel(PaperPanel(), FMargin(28.f, 22.f));
	AddRow(Poster, Sized(Card, Width, 0.f), -6.f, 0.f);
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Col);
	OutSubtitle = MakeText(TEXT(""), 18, C::Ink); AddRow(Col, OutSubtitle, 6.f, 12.f);
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(); AddRow(Col, Body, 0.f, 0.f);

	// The ink note: replies from the server and the ledger. Hidden (not collapsed) while empty.
	UBorder* Note = MakePanel(InkPanel(), FMargin(14.f, 8.f));
	UHorizontalBox* NoteRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Note->SetContent(NoteRow);
	UImage* Seal = nullptr; AddCol(NoteRow, MakeArt(TEXT("wax_seal"), 26.f, Seal), false, 12.f);
	ShellStatus = MakeText(TEXT(""), 17, GbUi::PaperHi()); AddCol(NoteRow, ShellStatus, true, 0.f);
	ShellNote = Note;
	AddRow(Col, Sized(Note, 0.f, 50.f), 14.f, 0.f);

	OutFooter = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddCol(OutFooter, WidgetTree->ConstructWidget<USpacer>(), true, 0.f);
	AddRow(Col, OutFooter, 14.f, 0.f);
	Centre(Root, Poster);
	return Body;
}

UWidget* UGbWidgetBase::MakeSection(const FString& Label)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UTextBlock* Text = MakeText(Label, 18, C::Ink, true); Text->SetAutoWrapText(false);
	AddCol(Row, Text, false, 12.f);
	UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(); Rule->SetBrushColor(FLinearColor(C::Ink.R, C::Ink.G, C::Ink.B, 0.35f));
	AddCol(Row, Sized(Rule, 0.f, 3.f), true, 0.f);
	return Row;
}

UButton* UGbWidgetBase::MakeKeyButton(const FString& Label, const FString& Key, UTextBlock*& OutLabel, const FLinearColor& Tint)
{
	UButton* B = MakeButton(Label, OutLabel, Tint);
	OutLabel->SetFont(GbUi::Font(20, true));
	if (Key.IsEmpty()) return B;
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); B->SetContent(Row);
	AddCol(Row, OutLabel, true, 12.f);
	UBorder* Cap = MakePanel(GbUi::PaperHi(), FMargin(8.f, 2.f));
	UTextBlock* KeyText = MakeText(Key, 16, C::Ink, true); KeyText->SetAutoWrapText(false); Cap->SetContent(KeyText);
	AddCol(Row, Cap, false, 0.f);
	return B;
}

UWidget* UGbWidgetBase::MakeArt(const FString& Name, float Box, UImage*& OutImage)
{
	OutImage = WidgetTree->ConstructWidget<UImage>();
	SetArt(OutImage, Name, Box);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(Box); Size->SetHeightOverride(Box);
	UScaleBox* Keep = WidgetTree->ConstructWidget<UScaleBox>(); Keep->SetStretch(EStretch::None); Keep->SetContent(OutImage);
	Size->SetContent(Keep);
	return Size;
}

void UGbWidgetBase::SetArt(UImage* Image, const FString& Name, float Box)
{
	if (!Image) return;
	if (Name.IsEmpty()) { Image->SetVisibility(ESlateVisibility::Hidden); return; }
	const FSlateBrush One = ArtBrush(Name, 1.f);
	const float Scale = FMath::Max(1.f, FMath::FloorToFloat(Box / FMath::Max(1.f, FMath::Max(One.ImageSize.X, One.ImageSize.Y))));
	Image->SetBrush(ArtBrush(Name, Scale));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

UBorder* UGbWidgetBase::MakeChip(const FString& Text, const FLinearColor& Tint, UTextBlock*& OutText)
{
	UBorder* Chip = MakePanel(Tint, FMargin(10.f, 4.f));
	OutText = MakeText(Text, 16, C::Ink, true); OutText->SetAutoWrapText(false); Chip->SetContent(OutText);
	SetChip(Chip, OutText, Text, Tint);
	return Chip;
}

void UGbWidgetBase::SetChip(UBorder* Chip, UTextBlock* Text, const FString& Label, const FLinearColor& Tint)
{
	if (!Chip || !Text) return;
	Chip->SetBrushColor(Tint);
	// Cream on dark tags (crimson, ink), ink on light ones (gold, leaf, paper).
	const float Luminance = 0.2126f * Tint.R + 0.7152f * Tint.G + 0.0722f * Tint.B;
	Text->SetColorAndOpacity(FSlateColor(Luminance < 0.15f ? GbUi::PaperHi() : C::Ink));
	Text->SetText(FText::FromString(Label));
	Chip->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

FString UGbWidgetBase::KeyName(const TCHAR* Action) { return UGbUserSettings::Get()->KeyLabel(FName(Action)); }

// ---------------------------------------------------------------- a talisman to pick

void UGbPickRowWidget::BuildTree()
{
	UTextBlock* Ignored = nullptr;
	Button = MakeButton(TEXT(""), Ignored, GbUi::PaperHi());
	WidgetTree->RootWidget = Button;
	FButtonStyle Style = Button->GetStyle(); Style.NormalPadding = FMargin(10, 8, 14, 10); Style.PressedPadding = FMargin(10, 10, 14, 8); Button->SetStyle(Style);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UButtonSlot* Content = Cast<UButtonSlot>(Button->SetContent(Row))) { Content->SetHorizontalAlignment(HAlign_Fill); Content->SetVerticalAlignment(VAlign_Fill); }
	UBorder* Plate = MakePanel(GbUi::Paper2(), FMargin(4.f)); Plate->SetContent(MakeArt(FString(), 64.f, Art));
	AddCol(Row, Plate, false, 14.f);
	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(); AddCol(Row, Text, true, 12.f);
	Title = MakeText(TEXT(""), 20, C::Ink, true); Title->SetAutoWrapText(false); AddRow(Text, Title, 0.f, 0.f);
	Line = MakeText(TEXT(""), 15, C::Ink); AddRow(Text, Line, 0.f, 4.f);
	UHorizontalBox* Ink = WidgetTree->ConstructWidget<UHorizontalBox>();
	InkBar = MakeBar(C::Sorrow); AddCol(Ink, Sized(InkBar, 0.f, 12.f), true, 10.f);
	InkText = MakeText(TEXT(""), 15, C::Ink, true); InkText->SetAutoWrapText(false); AddCol(Ink, InkText, false, 0.f);
	AddRow(Text, Ink, 0.f, 0.f); InkRow = Ink;
	Chip = MakeChip(TEXT(""), C::Gold, ChipText); AddCol(Row, Chip, false, 0.f)->SetVerticalAlignment(VAlign_Top);
	Button->OnClicked.AddDynamic(this, &UGbPickRowWidget::HandleClick);
}

void UGbPickRowWidget::Setup(const FString& InArt, const FString& InTitle, const FString& InLine, float Ink, const FString& InInkText, const FString& InChip, const FLinearColor& ChipTint, bool bSelected)
{
	if (!Button) return;
	// The picked row turns gold, like the focused command in battle.
	Button->SetBackgroundColor(bSelected ? C::Gold : GbUi::PaperHi());
	SetArt(Art, InArt, 64.f);
	Title->SetText(FText::FromString(InTitle));
	Line->SetText(FText::FromString(InLine));
	InkRow->SetVisibility(Ink < 0.f ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	InkBar->SetPercent(FMath::Clamp(Ink, 0.f, 1.f)); InkBar->SetFillColorAndOpacity(Ink <= 0.f ? C::Crimson : C::Sorrow);
	InkText->SetText(FText::FromString(InInkText));
	SetChip(Chip, ChipText, InChip, ChipTint);
}

void UGbPickRowWidget::HandleClick() { if (OnClick) OnClick(); }

// ================================================================ HUD

// The title screen: the one front door. Enter signs you in the first time (browser), then goes to the Ward.
void UGbSignInWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Root;
	SetIsFocusable(true);

	// Night over the live Ward: dark behind the logo, clearer toward the streets.
	UBorder* Sky = WidgetTree->ConstructWidget<UBorder>(); Sky->SetBrush(DuskBrush());
	UCanvasPanelSlot* SkySlot = Root->AddChildToCanvas(Sky); SkySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); SkySlot->SetOffsets(FMargin(0.f));

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	const auto Middle = [Col](UWidget* W, float Top, float Bottom) { AddRow(Col, W, Top, Bottom)->SetHorizontalAlignment(HAlign_Center); };

	UTextBlock* Logo = MakeText(TEXT("GRUDGEBOUND"), 96, GbUi::PaperHi(), true);
	FSlateFontInfo LogoFont = Logo->GetFont(); LogoFont.OutlineSettings = FFontOutlineSettings(4, C::Ink); LogoFont.LetterSpacing = 40; Logo->SetFont(LogoFont);
	Logo->SetShadowOffset(FVector2D(0.f, 8.f)); Logo->SetShadowColorAndOpacity(C::Crimson); Logo->SetAutoWrapText(false);
	Middle(Logo, 0.f, 10.f);

	UBorder* Ribbon = MakePanel(C::Crimson, FMargin(40.f, 7.f, 34.f, 8.f));
	UTextBlock* Sub = MakeText(TEXT("THE RITE"), 20, GbUi::PaperHi(), true); Sub->SetAutoWrapText(false);
	FSlateFontInfo SubFont = Sub->GetFont(); SubFont.LetterSpacing = 400; Sub->SetFont(SubFont);
	Ribbon->SetContent(Sub); Middle(Ribbon, 0.f, 16.f);

	UTextBlock* Tagline = MakeText(TEXT("Greed makes monsters. You rewrite the rules."), 20, GbUi::PaperHi(), true);
	Tagline->SetShadowOffset(FVector2D(0.f, 2.f)); Tagline->SetShadowColorAndOpacity(C::Ink); Tagline->SetAutoWrapText(false);
	Middle(Tagline, 0.f, 18.f);

	UBorder* Panel = MakePanel(PaperPanel(), FMargin(28.f, 24.f, 28.f, 20.f));
	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(); Panel->SetContent(Sized(Menu, 460.f, 0.f));
	UTextBlock* Label = nullptr;
	// The panels' buttons and ink note, so the front door reads like the rooms behind it.
	SignInButton = MakeKeyButton(TEXT("Enter Lantern Ward"), FString(), Label, C::Crimson);
	Label->SetFont(GbUi::Font(22, true));
	SignInButton->OnClicked.AddDynamic(this, &UGbSignInWidget::HandleSignIn); AddRow(Menu, Sized(SignInButton, 0.f, 64.f), 0.f, 10.f);
	UHorizontalBox* Pair = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* Help = MakeKeyButton(TEXT("How to play"), FString(), Label, GbUi::Paper2()); Help->OnClicked.AddDynamic(this, &UGbSignInWidget::HandleHelp);
	UButton* Settings = MakeKeyButton(TEXT("Settings"), FString(), Label, GbUi::Paper2()); Settings->OnClicked.AddDynamic(this, &UGbSignInWidget::HandleSettings);
	AddCol(Pair, Sized(Help, 0.f, 52.f), true, 10.f); AddCol(Pair, Sized(Settings, 0.f, 52.f), true, 0.f);
	AddRow(Menu, Pair, 0.f, 0.f);
	HelpBox = WidgetTree->ConstructWidget<UVerticalBox>(); HelpBox->SetVisibility(ESlateVisibility::Collapsed); AddRow(Menu, HelpBox, 14.f, 0.f);
	// What the sign-in says, on the shell's ink note; collapsed while there is nothing to say.
	UBorder* Note = MakePanel(InkPanel(), FMargin(14.f, 8.f));
	UHorizontalBox* NoteRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Note->SetContent(NoteRow);
	UImage* Seal = nullptr; AddCol(NoteRow, MakeArt(TEXT("wax_seal"), 26.f, Seal), false, 12.f);
	StatusText = MakeText(TEXT(""), 16, GbUi::PaperHi()); AddCol(NoteRow, StatusText, true, 0.f);
	StatusNote = Note; Note->SetVisibility(ESlateVisibility::Collapsed); AddRow(Menu, Note, 14.f, 0.f);
	Middle(Panel, 0.f, 0.f);

	UCanvasPanelSlot* ColSlot = Centre(Root, Col); ColSlot->SetPosition(FVector2D(0.f, -20.f));

	const auto Foot = [this, Root](const TCHAR* Text, float Side)
	{
		UTextBlock* T = MakeText(Text, 14, C::Paper, true); T->SetAutoWrapText(false);
		T->SetShadowOffset(FVector2D(0.f, 2.f)); T->SetShadowColorAndOpacity(C::Ink);
		UCanvasPanelSlot* S = Root->AddChildToCanvas(T); S->SetAnchors(FAnchors(Side, 1.f)); S->SetAlignment(FVector2D(Side, 1.f));
		S->SetPosition(FVector2D(Side > 0.f ? -22.f : 22.f, -14.f)); S->SetAutoSize(true);
	};
	Foot(TEXT("Hollowmere, a harbor town that ran on debt"), 0.f);
	Foot(TEXT("NEAR HACKATHON 2026"), 1.f);
}

void UGbSignInWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->OnSessionChanged.AddUniqueDynamic(this, &UGbSignInWidget::HandleChanged);
	UGbUserSettings::Get()->OnChanged.AddUObject(this, &UGbSignInWidget::FillHelp);
	FillHelp(); Refresh();
}

void UGbSignInWidget::NativeDestruct()
{
	UGbUserSettings::Get()->OnChanged.RemoveAll(this);
	GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->OnSessionChanged.RemoveDynamic(this, &UGbSignInWidget::HandleChanged);
	Super::NativeDestruct();
}

void UGbSignInWidget::Refresh()
{
	const UGbAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>();
	if (StatusText) StatusText->SetText(FText::FromString(Auth->GetMessage()));
	if (StatusNote) StatusNote->SetVisibility(Auth->GetMessage().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (SignInButton) SignInButton->SetIsEnabled(!Auth->IsBusy());
}

/** The keys as they are bound right now, drawn as keycaps. */
void UGbSignInWidget::FillHelp()
{
	if (!HelpBox) return;
	HelpBox->ClearChildren();
	AddRow(HelpBox, MakeSection(TEXT("How to play")), 0.f, 8.f);
	const UGbUserSettings* S = UGbUserSettings::Get();
	const auto Row = [this, S](std::initializer_list<const TCHAR*> Actions, FString Text)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		for (const TCHAR* Action : Actions)
		{
			UBorder* Cap = MakePanel(GbUi::PaperHi(), FMargin(8.f, 2.f));
			UTextBlock* Key = MakeText(S->KeyLabel(Action), 16, C::Ink, true); Key->SetAutoWrapText(false); Cap->SetContent(Key);
			AddCol(Line, Cap, false, 4.f);
			if (Text.IsEmpty()) for (const FGbActionDef& Def : UGbUserSettings::Actions()) if (Def.Id == Action) Text = Def.Label;
		}
		UTextBlock* What = MakeText(Text, 17, C::Ink); What->SetAutoWrapText(false);
		AddCol(Line, What, true, 0.f)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		AddRow(HelpBox, Line, 3.f, 3.f);
	};
	Row({ TEXT("MoveUp"), TEXT("MoveLeft"), TEXT("MoveDown"), TEXT("MoveRight") }, TEXT("Walk"));
	for (const TCHAR* Action : { TEXT("Interact"), TEXT("Satchel"), TEXT("Case"), TEXT("Rite"), TEXT("Vows"), TEXT("Settings") }) Row({ Action }, FString());
}

void UGbSignInWidget::HandleSignIn() { OnEnter.Broadcast(); }
void UGbSignInWidget::HandleChanged() { Refresh(); }
void UGbSignInWidget::HandleHelp()
{
	if (HelpBox) HelpBox->SetVisibility(HelpBox->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
void UGbSignInWidget::HandleSettings()
{
	if (AGbSettingsHost* Menu = AGbSettingsHost::Find(GetWorld())) Menu->Open(1);
}

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
	KeyHint = MakeText(TEXT(""), 13, C::Paper, true); KeyHint->SetAutoWrapText(false);
	Hint->SetContent(KeyHint); RefreshKeyHint();
	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(Hint);
	HintSlot->SetAnchors(FAnchors(1.f, 0.f));
	HintSlot->SetAlignment(FVector2D(1.f, 0.f));
	HintSlot->SetPosition(FVector2D(-24.f, 24.f));
	HintSlot->SetAutoSize(true);

	UBorder* ObjectiveCard = MakePanel(PaperPanel(), FMargin(14.f, 10.f));
	UVerticalBox* ObjectiveCol = WidgetTree->ConstructWidget<UVerticalBox>();
	ObjectiveCard->SetContent(Sized(ObjectiveCol, 340.f, 0.f));
	ChapterText = MakeText(TEXT(""), 14, C::Crimson, true); AddRow(ObjectiveCol, ChapterText);
	ObjectiveText = MakeText(TEXT(""), 14, C::Ink); AddRow(ObjectiveCol, ObjectiveText, 6.f);
	UCanvasPanelSlot* ObjectiveSlot = Root->AddChildToCanvas(ObjectiveCard);
	ObjectiveSlot->SetAnchors(FAnchors(1.f, 0.f)); ObjectiveSlot->SetAlignment(FVector2D(1.f, 0.f));
	ObjectiveSlot->SetPosition(FVector2D(-24.f, 82.f)); ObjectiveSlot->SetAutoSize(true);

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

void UGbHudWidget::RefreshKeyHint()
{
	if (!KeyHint) return;
	const UGbUserSettings* S = UGbUserSettings::Get();
	const TPair<const TCHAR*, const TCHAR*> Keys[] = { { TEXT("Settings"), TEXT("Menu") }, { TEXT("Satchel"), TEXT("Satchel") }, { TEXT("Case"), TEXT("Case") },
		{ TEXT("Rite"), TEXT("Rite") }, { TEXT("Vows"), TEXT("Vows") }, { TEXT("Observatory"), TEXT("Observatory") }, { TEXT("Proof"), TEXT("Proof") } };
	FString Text;
	for (const auto& K : Keys) Text += FString::Printf(TEXT("%s%s %s"), Text.IsEmpty() ? TEXT("") : TEXT("    "), *S->KeyLabel(K.Key), K.Value);
	KeyHint->SetText(FText::FromString(Text));
}

void UGbHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UGbUserSettings::Get()->OnChanged.AddUObject(this, &UGbHudWidget::RefreshKeyHint);
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
	GetGameInstance()->GetSubsystem<UGbStorySubsystem>()->OnStoryChanged.AddUniqueDynamic(this, &UGbHudWidget::HandleStoryChanged);
}

void UGbHudWidget::NativeDestruct()
{
	UGbUserSettings::Get()->OnChanged.RemoveAll(this);
	GetGameInstance()->GetSubsystem<UGbStorySubsystem>()->OnStoryChanged.RemoveDynamic(this, &UGbHudWidget::HandleStoryChanged);
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
	const FString WardId = P ? P->GetWardId() : GetDefault<UGbSettings>()->WardId;
	FGbWardSummary Ward;
	const bool bHaveWard = A->GetWard(WardId, Ward);

	const FString Name = bHaveWard ? Ward.Name : WardId == TEXT("tide") ? TEXT("Tide Ward") : TEXT("Lantern Ward");
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
		? FString(TEXT("Connected to Hollowmere"))
		: FString(TEXT("Hollowmere is unreachable. Retrying..."))));
	ServerText->SetColorAndOpacity(FSlateColor(A->IsServerOnline() ? C::Leaf : C::Crimson));

	KessaText->SetText(FText::FromString(A->GetKessaLine()));
	const UGbStorySubsystem* Story = GetGameInstance()->GetSubsystem<UGbStorySubsystem>();
	ChapterText->SetText(FText::FromString(Story->GetChapterTitle()));
	FString Objective=Story->GetObjective();
 if (P) for (const FGbJournalEntry& J : P->GetSatchel().Journal)
  if (J.Id==P->GetSatchel().Tracked && J.Status!=TEXT("complete")) {Objective+=TEXT("\n\n")+J.Title+TEXT("\n")+J.Objective; break;}
 ObjectiveText->SetText(FText::FromString(Objective));
}

void UGbHudWidget::SetIndoor(bool bIndoor)
{
 const ESlateVisibility V=bIndoor?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible;
 if (!GrudgeTotal) return;
 GrudgeTotal->SetVisibility(V); Stats->SetVisibility(V); ServerText->SetVisibility(V);
 RageText->GetParent()->SetVisibility(V); SorrowText->GetParent()->SetVisibility(V); DreadText->GetParent()->SetVisibility(V);
}
void UGbHudWidget::HandleWorldState(const FGbWorldState&) { Refresh(); }
void UGbHudWidget::HandleServerStatus(bool, int32) { Refresh(); }
void UGbHudWidget::HandleKessaLine(const FString&) { Refresh(); }
void UGbHudWidget::HandlePlayerChanged() { Refresh(); }
void UGbHudWidget::HandleStoryChanged() { Refresh(); }

// ================================================================ encounter

void UGbEncounterWidget::BuildTree()
{
	// The Ward stays visible but dimmed behind a "wanted poster" for the curse, in the battle screen's language:
	// crimson ribbon, paper card, the curse's own pixel art on a cream plate, ink text, crimson primary button.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	SetIsFocusable(true);
	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(); Shade->SetBrushColor(FLinearColor(C::Ink.R, C::Ink.G, C::Ink.B, 0.68f));
	auto* ShadeSlot = Root->AddChildToCanvas(Shade); ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ShadeSlot->SetOffsets(FMargin(0));

	UVerticalBox* Poster = WidgetTree->ConstructWidget<UVerticalBox>();
	UBorder* Ribbon = MakePanel(C::Crimson, FMargin(28.f, 10.f));
	Title = MakeText(TEXT("A curse stirs in the tall grass!"), 30, GbUi::PaperHi(), true);
	Title->SetJustification(ETextJustify::Center); Ribbon->SetContent(Title);
	AddRow(Poster, Ribbon, 0.f, 0.f)->SetHorizontalAlignment(HAlign_Center);

	UBorder* Card = MakePanel(PaperPanel(), FMargin(28.f, 24.f));
	AddRow(Poster, Sized(Card, 1080.f, 0.f), -6.f, 0.f);
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(); Card->SetContent(Body);

	// Left: the curse, big, integer-scaled so its pixels stay square.
	UBorder* Plate = MakePanel(GbUi::PaperHi(), FMargin(8.f));
	WildImage = WidgetTree->ConstructWidget<UImage>(); WildImage->SetRenderTransformPivot(FVector2D(.5f, 1.f));
	USizeBox* PlateBox = WidgetTree->ConstructWidget<USizeBox>(); PlateBox->SetWidthOverride(312.f); PlateBox->SetHeightOverride(312.f);
	UScaleBox* Keep = WidgetTree->ConstructWidget<UScaleBox>(); Keep->SetStretch(EStretch::None); Keep->SetContent(WildImage);
	PlateBox->SetContent(Keep); Plate->SetContent(PlateBox);
	AddCol(Body, Plate, false, 28.f)->SetVerticalAlignment(VAlign_Top);

	// Right: who it is, how your partner fares, what it does, and the two choices.
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	AddCol(Body, Col, true, 0.f)->SetVerticalAlignment(VAlign_Fill);
	WildName = MakeText(TEXT(""), 44, C::Ink, true); WildName->SetAutoWrapText(false);
	AddRow(Col, WildName, 0.f, 2.f);
	UHorizontalBox* TagRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	EmotionImage = WidgetTree->ConstructWidget<UImage>(); AddCol(TagRow, Sized(EmotionImage, 33.f, 33.f), false, 10.f);
	Tags = MakeText(TEXT(""), 22, C::Ink, true); AddCol(TagRow, Tags, true, 0.f);
	AddRow(Col, TagRow, 0.f, 10.f);
	Intro = MakeText(TEXT(""), 20, C::Ink); Intro->SetAutoWrapText(true);
	AddRow(Col, Intro, 0.f, 12.f);

	// The matchup: your partner's face, the plain sentence, and a coloured verdict chip (gold good, crimson bad).
	UBorder* MatchPanel = MakePanel(GbUi::Paper2(), FMargin(12.f, 8.f));
	UHorizontalBox* MatchRow = WidgetTree->ConstructWidget<UHorizontalBox>(); MatchPanel->SetContent(MatchRow);
	UBorder* Face = MakePanel(GbUi::PaperHi(), FMargin(4.f));
	PartnerImage = WidgetTree->ConstructWidget<UImage>(); Face->SetContent(Sized(PartnerImage, 56.f, 56.f));
	AddCol(MatchRow, Face, false, 14.f);
	Matchup = MakeText(TEXT(""), 20, C::Ink, true); Matchup->SetAutoWrapText(true);
	AddCol(MatchRow, Matchup, true, 12.f);
	MatchChip = MakePanel(C::Gold, FMargin(12.f, 6.f));
	MatchChipText = MakeText(TEXT(""), 20, C::Ink, true); MatchChip->SetContent(MatchChipText);
	AddCol(MatchRow, MatchChip, false, 0.f);
	AddRow(Col, MatchPanel, 0.f, 12.f);

	// No signature move line: battles do not use signature moves yet (server/src/battles.ts only passes them on).
	Lore = MakeText(TEXT(""), 18, C::Ink); Lore->SetAutoWrapText(true);
	AddRow(Col, Lore, 0.f, 16.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* FightLabel = nullptr;
	FightButton = MakeButton(TEXT("Fight!"), FightLabel, C::Crimson);
	FightLabel->SetFont(GbUi::Font(28, true));
	UHorizontalBox* FightRow = WidgetTree->ConstructWidget<UHorizontalBox>(); FightButton->SetContent(FightRow);
	AddCol(FightRow, FightLabel, true, 14.f);
	UBorder* Cap = MakePanel(GbUi::PaperHi(), FMargin(6.f, 3.f)); Cap->SetContent(MakeText(TEXT("Enter"), 18, C::Ink, true));
	AddCol(FightRow, Cap, false, 0.f);
	FightButton->OnClicked.AddDynamic(this, &UGbEncounterWidget::HandleFight);
	AddCol(Buttons, Sized(FightButton, 0.f, 72.f), true, 16.f);
	UTextBlock* SlipLabel = nullptr;
	UButton* Slip = MakeButton(TEXT("Slip away"), SlipLabel, GbUi::Paper2()); SlipButton = Slip;
	SlipLabel->SetFont(GbUi::Font(24, true));
	Slip->OnClicked.AddDynamic(this, &UGbEncounterWidget::HandleSlipAway);
	AddCol(Buttons, Sized(Slip, 300.f, 72.f), false, 0.f);
	UVerticalBoxSlot* ButtonSlot = AddRow(Col, Buttons, 0.f, 0.f);
	ButtonSlot->SetVerticalAlignment(VAlign_Bottom); ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	Centre(Root, Poster);
}

void UGbEncounterWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!WildImage) return;
	Clock += DeltaTime;
	// The same slow idle bob as on the battle stage; none when the player asked for less motion.
	const bool bStill = UGbUserSettings::Get()->bReducedBattleMotion;
	WildImage->SetRenderTranslation(FVector2D(0.f, bStill ? 0.f : FMath::Sin(Clock * 3.5f) * 4.f));
}

FReply UGbEncounterWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Enter && !Event.IsRepeat()) { HandleFight(); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UGbEncounterWidget::Show(const FGbBattleStart& Battle)
{
	if (!Intro) return;
	// Every start line: the intro, then any curfew warning from Kessa.
	Intro->SetText(FText::FromString(Battle.Lines.Num() > 0 ? FString::Join(Battle.Lines, TEXT("\n")) : FString::Printf(TEXT("Wild %s!"), *Battle.Wild.Name)));

	// A rival steps up instead of a curse stirring: no slipping away from them.
	if (Title) Title->SetText(FText::FromString(Battle.RivalStage > 0 ? FString::Printf(TEXT("%s wants a rematch!"), *Battle.RivalName)
		: FString(TEXT("A curse stirs in the tall grass!"))));
	if (Battle.RivalStage == 1 && Title) Title->SetText(FText::FromString(FString::Printf(TEXT("%s blocks the pier!"), *Battle.RivalName)));
	if (SlipButton) SlipButton->SetVisibility(Battle.RivalStage > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	const FString& M = Battle.Matchup;
	const FString Partner = Battle.View.Player.Name;
	FString MatchText = FString::Printf(TEXT("%s vs %s: even match."), *Partner, *Battle.Wild.Name);
	if (M == TEXT("advantage")) MatchText = FString::Printf(TEXT("Your %s has the advantage."), *Partner);
	else if (M == TEXT("disadvantage")) MatchText = FString::Printf(TEXT("Careful: %s resists your %s."), *Battle.Wild.Name, *Partner);
	Matchup->SetText(FText::FromString(MatchText));

	// Verdict chip: ink on gold (6.5:1), cream on crimson (7.3:1), ink on paper (11:1).
	const bool bGood = M == TEXT("advantage"), bBad = M == TEXT("disadvantage");
	MatchChip->SetBrush(GbUi::PanelBrush(bGood ? C::Gold : bBad ? C::Crimson : GbUi::Paper3(), false));
	MatchChipText->SetText(FText::FromString(bGood ? TEXT("Advantage") : bBad ? TEXT("Resists") : TEXT("Even")));
	MatchChipText->SetColorAndOpacity(FSlateColor(bBad ? GbUi::PaperHi() : C::Ink));

	// The curse itself: as large as fits the 312 plate at a whole-number scale (48 px -> x6, 56 -> x5, 64 -> x4).
	const FString Species = Battle.View.Wild.SpeciesId.IsEmpty() ? Battle.Wild.Id : Battle.View.Wild.SpeciesId;
	FSlateBrush Art = ArtBrush(CurseArt(Species), 1.f);
	const float Scale = FMath::Max(1.f, FMath::FloorToFloat(312.f / FMath::Max(Art.ImageSize.X, Art.ImageSize.Y)));
	WildImage->SetBrush(ArtBrush(CurseArt(Species), Scale));
	PartnerImage->SetBrush(ArtBrush(CurseArt(Battle.View.Player.SpeciesId), 1.f));
	WildName->SetText(FText::FromString(FString::Printf(TEXT("%s  Lv %d"), *Battle.Wild.Name, Battle.View.Wild.Level)));
	EmotionImage->SetBrush(ArtBrush(TEXT("emo_") + (Battle.Wild.Emotion.IsEmpty() ? FString(TEXT("rage")) : Battle.Wild.Emotion), 3.f));
	Tags->SetText(FText::FromString(FString::Printf(TEXT("%s  -  %s"), *Capitalised(Battle.Wild.Emotion), *Capitalised(Battle.Wild.Omen))));

	Lore->SetText(FText::FromString(Battle.Wild.BornFrom.IsEmpty() ? FString()
		: FString::Printf(TEXT("Born from %s."), *Battle.Wild.BornFrom)));
	Lore->SetVisibility(Battle.Wild.BornFrom.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (FightButton) FightButton->SetKeyboardFocus();
}

void UGbEncounterWidget::HandleFight() { OnFight.Broadcast(); }
void UGbEncounterWidget::HandleSlipAway() { OnSlipAway.Broadcast(); }

// ================================================================ battle

// Fixed 1080p composition: market stage, symmetric portrait cards, six commands and a two-line log.
// The outer ScaleBox fits the whole composition at 720p without changing information order.
void UGbBattleWidget::BuildTree()
{
	// One measured 1920x1080 composition, uniformly fitted inside the viewport's DPI scaler.
	UCanvasPanel* Outer = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Outer;
	UBorder* Ink = WidgetTree->ConstructWidget<UBorder>(); Ink->SetBrushColor(GbUi::Night());
	auto* InkSlot = Outer->AddChildToCanvas(Ink); InkSlot->SetAnchors(FAnchors(0,0,1,1)); InkSlot->SetOffsets(FMargin(0));
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
	auto* FitSlot = Outer->AddChildToCanvas(Fit); FitSlot->SetAnchors(FAnchors(0,0,1,1)); FitSlot->SetOffsets(FMargin(0));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(1920); Size->SetHeightOverride(1080); Fit->SetContent(Size);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(); Size->SetContent(Root);
	Root->SetClipping(EWidgetClipping::ClipToBounds); SetIsFocusable(true);
	const auto Place = [](UCanvasPanel* Canvas, UWidget* W, float X, float Y, float Width, float Height)
	{
		auto* S = Canvas->AddChildToCanvas(W); S->SetPosition(FVector2D(X,Y)); S->SetSize(FVector2D(Width,Height)); return S;
	};
	const auto Art = [this](const FString& Name)
	{
		auto* I = WidgetTree->ConstructWidget<UImage>(); I->SetBrush(ArtBrush(Name,1)); I->SetVisibility(ESlateVisibility::HitTestInvisible); return I;
	};
	UCanvasPanel* Stage = WidgetTree->ConstructWidget<UCanvasPanel>(); Stage->SetClipping(EWidgetClipping::ClipToBounds);
	Place(Root,Stage,0,0,1920,720); StagePanel=Stage; Stage->SetRenderTransformPivot(FVector2D(.5,.5));
	Place(Stage,Art(TEXT("backdrop")),0,0,1920,1080);
	KessaImage = Art(TEXT("kessa")); Place(Stage,KessaImage,884,42,112,128);
	// Fixed bounds prevent species texture dimensions from changing the layout.
	const FVector2D Feet[2] = { FVector2D(1416,510), FVector2D(468,690) };
	const float Extent[2] = {448,288};
	for(int32 Side=0;Side<2;++Side)
	{
		Sprites[Side]=Art(TEXT("curse_weepwick")); Sprites[Side]->SetRenderTransformPivot(FVector2D(.5,1));
		Place(Stage,Sprites[Side],Feet[Side].X-Extent[Side]/2,Feet[Side].Y-Extent[Side],Extent[Side],Extent[Side]);
		WardImages[Side]=Art(TEXT("icon_ward")); Place(Stage,WardImages[Side],Feet[Side].X+Extent[Side]/2-40,Feet[Side].Y-140,112,112);
		WardImages[Side]->SetVisibility(ESlateVisibility::Collapsed);
		// The number pops on the curse's body (ink outline keeps it readable on any sprite), not up in the lantern strings.
		Numbers[Side]=MakeText(TEXT(""),48,GbUi::PaperHi(),true); Numbers[Side]->SetAutoWrapText(false); Numbers[Side]->SetJustification(ETextJustify::Center);
		auto Font=Numbers[Side]->GetFont(); Font.OutlineSettings=FFontOutlineSettings(4,C::Ink); Numbers[Side]->SetFont(Font);
		Place(Stage,Numbers[Side],Feet[Side].X-120,Feet[Side].Y-Extent[Side]*.62f-34,240,68);
		Numbers[Side]->SetVisibility(ESlateVisibility::Collapsed);
	}
	// One ring where a blow lands, one where the seal comes down, and a single soft flash over the stage.
	for(int32 Side=0;Side<2;++Side)
	{
		Rings[Side]=WidgetTree->ConstructWidget<UImage>(); Rings[Side]->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent,80.f,GbUi::PaperHi(),7.f,FVector2D(160,160)));
		Place(Stage,Rings[Side],Feet[Side].X-80,Feet[Side].Y-Extent[Side]*.5f-80,160,160); Rings[Side]->SetVisibility(ESlateVisibility::Collapsed);
	}
	SealRing=WidgetTree->ConstructWidget<UImage>(); SealRing->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent,110.f,C::Gold,12.f,FVector2D(220,220)));
	Place(Stage,SealRing,1306,238,220,220); SealRing->SetVisibility(ESlateVisibility::Collapsed);
	Flash=WidgetTree->ConstructWidget<UBorder>(); Flash->SetBrushColor(GbUi::PaperHi()); Place(Stage,Flash,0,0,1920,720); Flash->SetVisibility(ESlateVisibility::Collapsed);
	SealStamp=Art(TEXT("wax_seal")); Place(Stage,SealStamp,1336,260,160,176); SealStamp->SetVisibility(ESlateVisibility::Collapsed);
	SealStamp->SetRenderTransformPivot(FVector2D(.5,.5));
	Place(Root,MakePanel(C::Paper,FMargin(24,20)),0,720,1920,360);
	for(int32 Side=0;Side<2;++Side)
	{
		const float X=Side==1?24:1476;
		Place(Root,MakePanel(GbUi::Paper2(),FMargin(12)),X,744,420,228);
		UTextBlock* Name=MakeText(TEXT(""),28,C::Ink,true); Name->SetAutoWrapText(false); Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Place(Root,Name,X+24,756,372,42);
		Place(Root,MakePanel(GbUi::PaperHi(),FMargin(6)),X+24,804,96,96);
		Portraits[Side]=Art(TEXT("curse_weepwick")); Place(Root,Portraits[Side],X+36,816,72,72);
		EmotionIcons[Side]=Art(TEXT("emo_rage")); Place(Root,EmotionIcons[Side],X+138,810,32,32);
		UTextBlock* Tags=MakeText(TEXT(""),22,C::Ink,true); Tags->SetAutoWrapText(false);
		Place(Root,Tags,X+182,806,220,82);
		UProgressBar* Bar=MakeBar(C::Leaf);
		// Empty HP is ink, so green/gold/crimson all remain distinguishable from the track.
		auto BarStyle=Bar->GetWidgetStyle(); BarStyle.BackgroundImage.TintColor=FSlateColor(C::Ink); Bar->SetWidgetStyle(BarStyle);
		Place(Root,Bar,X+24,912,372,21);
		UTextBlock* Hp=MakeText(TEXT(""),24,C::Ink); Hp->SetAutoWrapText(false); Place(Root,Hp,X+24,938,372,30);
		if(Side==0){WildName=Name;WildTags=Tags;WildHp=Bar;WildHpText=Hp;}
		else {PlayerName=Name;PlayerTags=Tags;PlayerHp=Bar;PlayerHpText=Hp;}
	}
	// Six commands in a stable 3x2 grid; the two-line log spans the entire strip.
	const TCHAR* Icons[]={TEXT("icon_attack"),TEXT("icon_ward"),TEXT("icon_salve"),TEXT("icon_seal"),TEXT("icon_gilded"),TEXT("icon_flee")};
	const TCHAR* Names[]={TEXT("Attack"),TEXT("Ward"),TEXT("Salve"),TEXT("Seal"),TEXT("Gilded Seal"),TEXT("Flee")};
	UButton* Buttons[6];
	for(int32 I=0;I<6;++I)
	{
		const float X=468+(I%3)*336,Y=744+(I/3)*120;
		FocusFrames[I]=WidgetTree->ConstructWidget<UBorder>(); FocusFrames[I]->SetBrushColor(C::Gold);
		Place(Root,FocusFrames[I],X-3,Y-3,318,114); FocusFrames[I]->SetVisibility(ESlateVisibility::Hidden);
	}
	Actions=WidgetTree->ConstructWidget<UHorizontalBox>(); Place(Root,Actions,468,744,984,108);
	Actions2=WidgetTree->ConstructWidget<UHorizontalBox>(); Place(Root,Actions2,468,864,984,108);
	for(int32 I=0;I<6;++I)
	{
		const bool Primary=I==0||I==3;
		UTextBlock* Label=nullptr; Buttons[I]=MakeButton(Names[I],Label,Primary?C::Crimson:I==4?C::Gold:GbUi::Paper2());
		UCanvasPanel* Face=WidgetTree->ConstructWidget<UCanvasPanel>(); Buttons[I]->SetContent(Face);
		auto Style=Buttons[I]->GetStyle(); Style.NormalPadding=FMargin(0); Style.PressedPadding=FMargin(0,3,0,-3); Buttons[I]->SetStyle(Style);
		Place(Face,MakePanel(GbUi::PaperHi(),FMargin(6)),12,18,72,72); Place(Face,Art(Icons[I]),16,22,64,64);
		auto Font=Label->GetFont(); Font.Size=26; Label->SetFont(Font); Label->SetJustification(ETextJustify::Left);
		Place(Face,Label,96,18,204,40);
		if(I>=2)
		{
			UTextBlock* Note=MakeText(TEXT(""),22,Primary?GbUi::PaperHi():C::Ink,true); Note->SetAutoWrapText(false);
			Place(Face,Note,96,62,162,32);
			if(I==2)SalveLabel=Note;else if(I==3)SealLabel=Note;else if(I==4)GildedLabel=Note;else FleeLabel=Note;
		}
		UBorder* Cap=MakePanel(GbUi::PaperHi(),FMargin(3)); UTextBlock* Key=MakeText(FString::FromInt(I+1),22,C::Ink,true); Key->SetJustification(ETextJustify::Center); Cap->SetContent(Key);
		Place(Face,Cap,264,58,36,42);
		AddCol(I<3?Actions:Actions2,Sized(Buttons[I],312,108),false,I%3==2?0:24);
	}
	AttackButton=Buttons[0]; WardButton=Buttons[1]; SalveButton=Buttons[2]; SealButton=Buttons[3]; GildedButton=Buttons[4]; FleeButton=Buttons[5];
	AttackButton->OnClicked.AddDynamic(this,&UGbBattleWidget::HandleAttack); WardButton->OnClicked.AddDynamic(this,&UGbBattleWidget::HandleWard);
	SalveButton->OnClicked.AddDynamic(this,&UGbBattleWidget::HandleSalve); SealButton->OnClicked.AddDynamic(this,&UGbBattleWidget::HandleSeal);
	GildedButton->OnClicked.AddDynamic(this,&UGbBattleWidget::HandleGildedSeal); FleeButton->OnClicked.AddDynamic(this,&UGbBattleWidget::HandleFlee);
	UBorder* LogPanel=MakePanel(C::Ink,FMargin(18,4)); Place(Root,LogPanel,24,990,1872,72);
	Log=WidgetTree->ConstructWidget<UVerticalBox>(); LogPanel->SetContent(Log);
	Problem=MakeText(TEXT(""),24,GbUi::PaperHi(),true); UBorder* Error=MakePanel(C::Ink,FMargin(20,14)); Error->SetContent(Problem);
	Place(Root,Error,384,620,1152,76); Error->SetVisibility(ESlateVisibility::SelfHitTestInvisible); Problem->SetVisibility(ESlateVisibility::Collapsed);
	// Hide the entire error backing with the label (ShowProblem toggles its parent).
	Error->SetVisibility(ESlateVisibility::Collapsed);
	// The result covers the (now locked) command grid, so the stage keeps the seal, the fade or the fallen partner in view.
	EndPanel=MakePanel(C::Ink,FMargin(30,20)); Place(Root,EndPanel,468,744,984,228);
	UVerticalBox* EndCol=WidgetTree->ConstructWidget<UVerticalBox>(); EndPanel->SetContent(EndCol);
	EndText=MakeText(TEXT(""),28,GbUi::PaperHi(),true); EndText->SetJustification(ETextJustify::Center); AddRow(EndCol,EndText,0,14);
	UTextBlock* BackLabel=nullptr; BackButton=MakeButton(TEXT("Back to the Ward"),BackLabel,C::Gold);
	BackLabel->SetFont(GbUi::Font(26,true)); BackButton->OnClicked.AddDynamic(this,&UGbBattleWidget::HandleBack);
	UHorizontalBox* ReturnRow=WidgetTree->ConstructWidget<UHorizontalBox>(); BackButton->SetContent(ReturnRow);
	AddCol(ReturnRow,BackLabel,true,18);
	UBorder* ReturnCap=MakePanel(GbUi::PaperHi(),FMargin(6,3));
	ReturnCap->SetContent(MakeText(TEXT("Enter"),22,C::Ink,true)); AddCol(ReturnRow,ReturnCap,false,0);
	AddRow(EndCol,Sized(BackButton,600,64),0,0)->SetHorizontalAlignment(HAlign_Center);
	EndPanel->SetVisibility(ESlateVisibility::Collapsed);
}


void UGbBattleWidget::ShowStart(const FGbBattleStart& Battle)
{
	if (!Log) return;
	LastStart = Battle; bRivalEnd = false; EndExtra.Reset();
	LogLines.Reset();
	Log->ClearChildren();
	EndPanel->SetVisibility(ESlateVisibility::Collapsed);
	Problem->SetVisibility(ESlateVisibility::Collapsed);
	Actions->SetVisibility(ESlateVisibility::Visible);
	Actions2->SetVisibility(ESlateVisibility::Visible);

	// A fresh stage: both curses step in, nothing left over from the last fight.
	Cues.Reset(); SealTime = 0.f; bSealHolds = false;
	Enter = UGbUserSettings::Get()->bReducedBattleMotion ? 0.f : 1.f;
	FlashTime = Punch = Squash = SealRingTime = SealPop = 0.f; bSealLanded = false;
	Flash->SetVisibility(ESlateVisibility::Collapsed); SealRing->SetVisibility(ESlateVisibility::Collapsed);
	StagePanel->SetRenderScale(FVector2D(1)); StagePanel->SetRenderTranslation(FVector2D::ZeroVector);
	SealStamp->SetRenderTranslation(FVector2D::ZeroVector); SealStamp->SetRenderTransformAngle(0.f);
	FeedbackRemaining=0; bBattleEnded=false; bWaitingForReply=false; Clock=0;
	Problem->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
	SealStamp->SetVisibility(ESlateVisibility::Collapsed);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Lunge[Side] = Hit[Side] = NumberTime[Side] = Impact[Side] = 0.f;
		Rings[Side]->SetVisibility(ESlateVisibility::Collapsed);
		Shrink[Side] = ShrinkTo[Side] = FadeTo[Side] = Fade[Side] = WardTime[Side] = 0.f;
		WardImages[Side]->SetVisibility(ESlateVisibility::Collapsed);
		Sprites[Side]->SetRenderScale(FVector2D(1)); Sprites[Side]->SetRenderTranslation(FVector2D::ZeroVector);
		Sprites[Side]->SetColorAndOpacity(FLinearColor::White);
		Sprites[Side]->SetRenderOpacity(1.f);
		Numbers[Side]->SetVisibility(ESlateVisibility::Collapsed);
	}
	const FGbSide* Sides[2] = { &Battle.View.Wild, &Battle.View.Player };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Sprites[Side]->SetBrush(ArtBrush(CurseArt(Sides[Side]->SpeciesId),1));
		Portraits[Side]->SetBrush(ArtBrush(CurseArt(Sides[Side]->SpeciesId),1));
		ShownHp[Side] = HpFraction(*Sides[Side]);
	}
	ShowView(Battle.View);
	AddLines(Battle.Lines);
	SetButtonsEnabled(true);
}

void UGbBattleWidget::ApplyTurn(const FGbBattleTurn& Turn)
{
	if (!Log) return;
	bWaitingForReply=false;
	Problem->SetVisibility(ESlateVisibility::Collapsed);
	Problem->GetParent()->SetVisibility(ESlateVisibility::Collapsed);

	// The server sends what the screen should show, not a script; the stage plays what the numbers imply.
	const int32 WildLost = LastView.Wild.Hp - Turn.View.Wild.Hp;
	const int32 PartnerLost = LastView.Player.Hp - Turn.View.Player.Hp;
	float Reply = 0.15f; // when the wild curse answers
	if (Turn.Action.Contains(TEXT("seal"), ESearchCase::IgnoreCase))
	{
		bSealHolds = Turn.Outcome == EGbOutcome::Bound;
		Cue(3, 0, 0.f); Reply = 0.9f;
		if (!bSealHolds) Cue(9, 0, 0.8f);
	}
	else if (Turn.Action.Contains(TEXT("ward"), ESearchCase::IgnoreCase)) { Cue(6,1,0); Reply=.55f; }
	else if (Turn.Action.Contains(TEXT("salve"), ESearchCase::IgnoreCase)) Reply = 0.5f;
	else if (WildLost > 0) { Cue(0, 1, 0.f); Reply = 0.6f; }
	if (WildLost > 0) Cue(1, 0, 0.16f, WildLost);
	if (PartnerLost < 0) Cue(2, 1, 0.1f, -PartnerLost);
	if (PartnerLost > 0) { Cue(0, 0, Reply); Cue(1, 1, Reply + 0.16f, PartnerLost); }
	if (Turn.bEnded)
	{
		if (Turn.Outcome == EGbOutcome::Bound) { Cue(4, 0, 0.45f); Cue(8, 0, 0.55f, 0); }
		else if (Turn.Outcome == EGbOutcome::Won) { Cue(5, 0, 0.5f); Cue(8, 0, 0.5f, 1); }
		else if (Turn.Outcome == EGbOutcome::Lost) { Cue(5, 1, Reply + 0.5f); Cue(8, 1, Reply + 0.5f, 2); }
	}

	ShowView(Turn.View);
	// The authoritative HP target changes when its hit/heal lands, not before the lunge.
	TargetHp[0]=ShownHp[0]; TargetHp[1]=ShownHp[1];
	if(WildLost!=0) Cue(7,0,.16f); else TargetHp[0]=HpFraction(Turn.View.Wild);
	if(PartnerLost!=0) Cue(7,1,PartnerLost>0?Reply+.16f:.1f); else TargetHp[1]=HpFraction(Turn.View.Player);
	AddLines(Turn.Lines);
	FeedbackRemaining=Turn.Action.Contains(TEXT("seal"),ESearchCase::IgnoreCase)?1.45f:Reply+.72f;
	SetButtonsEnabled(false);
	if (Turn.bEnded)
	{
		EndExtra = Turn.XpGained > 0 ? FString::Printf(TEXT("\n%s gained %d experience%s"), *Turn.View.Player.Name, Turn.XpGained,
			Turn.Level > Turn.LevelBefore ? *FString::Printf(TEXT(" and grew to level %d!"), Turn.Level) : TEXT(".")) : FString();
		bRivalEnd = LastStart.RivalStage > 0;
		ShowEnd(Turn.Outcome, Turn.Points, Turn.View.Wild.Name);
	}
}

void UGbBattleWidget::Cue(int32 Type, int32 Side, float Delay, int32 Amount)
{
	FCue C; C.Type = Type; C.Side = Side; C.Delay = Delay; C.Amount = Amount;
	if (Delay <= 0.f) Play(C); else Cues.Add(C);
}

void UGbBattleWidget::Play(const FCue& C)
{
	switch (C.Type)
	{
	case 0: Lunge[C.Side] = 1.f; UGbAudio::Play(this, TEXT("whoosh"), 0.7f, C.Side == 1 ? 1.05f : 0.9f); break;
	case 1:
		Hit[C.Side] = Impact[C.Side] = FlashTime = Punch = 1.f;
		ShowNumber(C.Side, FString::Printf(TEXT("-%d"), C.Amount), GbUi::PaperHi());
		UGbAudio::Play(this, C.Side == 0 ? TEXT("hit_1") : TEXT("hit_2"), 0.9f, FMath::FRandRange(0.95f, 1.05f));
		break;
	case 2: ShowNumber(C.Side, FString::Printf(TEXT("+%d"), C.Amount), FLinearColor(0.55f, 0.95f, 0.45f)); UGbAudio::Play(this, TEXT("heal"), 0.8f); break;
	case 3: SealTime = 1.4f; bSealLanded = false; SealPop = 0.f; SealStamp->SetVisibility(ESlateVisibility::HitTestInvisible); UGbAudio::Play(this, TEXT("whoosh"), 0.6f, 0.8f); break;
	case 4: ShrinkTo[C.Side] = 1.f; break;
	case 5: FadeTo[C.Side] = 1.f; break;
	case 6: WardTime[C.Side]=1.1f; WardImages[C.Side]->SetVisibility(ESlateVisibility::HitTestInvisible); UGbAudio::Play(this, TEXT("ward"), 0.8f); break;
	case 7: TargetHp[C.Side]=HpFraction(C.Side==0?LastView.Wild:LastView.Player); break;
	case 8: UGbAudio::Play(this, C.Amount == 0 ? TEXT("bound") : C.Amount == 1 ? TEXT("exorcised") : TEXT("lost"), 0.9f); break;
	case 9: SealPop = 0.001f; UGbAudio::Play(this, TEXT("seal_break"), 0.9f); break;
	default: break;
	}
}

void UGbBattleWidget::ShowNumber(int32 Side, const FString& Text, const FLinearColor& Color)
{
	Numbers[Side]->SetText(FText::FromString(Text));
	Numbers[Side]->SetColorAndOpacity(FSlateColor(Color));
	Numbers[Side]->SetVisibility(ESlateVisibility::HitTestInvisible);
	NumberTime[Side] = 1.f;
}

void UGbBattleWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!Sprites[0] || !IsVisible()) return;
	const bool Reduced=UGbUserSettings::Get()->bReducedBattleMotion;
	Clock += DeltaTime;
	for (int32 I = Cues.Num() - 1; I >= 0; --I)
	{
		if ((Cues[I].Delay -= DeltaTime) > 0.f) continue;
		const FCue C = Cues[I]; Cues.RemoveAt(I); Play(C);
	}

	// Staging clocks. Enter runs 1 -> 0 as the curses step in; the rest are short one-shot pulses.
	Enter = FMath::Max(0.f, Enter - DeltaTime / 0.55f);
	FlashTime = FMath::Max(0.f, FlashTime - DeltaTime / 0.14f);
	Punch = FMath::Max(0.f, Punch - DeltaTime / 0.3f);
	Squash = FMath::Max(0.f, Squash - DeltaTime / 0.28f);
	SealRingTime = FMath::Max(0.f, SealRingTime - DeltaTime / 0.5f);
	if (SealPop > 0.f) SealPop = FMath::Min(1.f, SealPop + DeltaTime / 0.45f);
	const float Arrive = Reduced ? 0.f : FMath::Square(Enter);
	const float Pop = Reduced ? 0.f : SealPop;

	UProgressBar* Bars[2] = { WildHp, PlayerHp };
	if(FeedbackRemaining>0)
	{
		FeedbackRemaining=FMath::Max(0.f,FeedbackRemaining-DeltaTime);
		if(FeedbackRemaining==0 && !bBattleEnded && !bWaitingForReply) SetButtonsEnabled(true);
		if(FeedbackRemaining==0 && bBattleEnded) { EndPanel->SetVisibility(ESlateVisibility::Visible); BackButton->SetKeyboardFocus(); }
	}
	UButton* Buttons[]={AttackButton,WardButton,SalveButton,SealButton,GildedButton,FleeButton};
	for(int32 I=0;I<6;++I) FocusFrames[I]->SetVisibility(Buttons[I]->GetIsEnabled() && (Buttons[I]->IsHovered() || Buttons[I]->HasKeyboardFocus())?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Lunge[Side] = FMath::Max(0.f, Lunge[Side] - DeltaTime / 0.32f);
		Hit[Side] = FMath::Max(0.f, Hit[Side] - DeltaTime / 0.5f);
		Shrink[Side] = FMath::FInterpConstantTo(Shrink[Side], ShrinkTo[Side], DeltaTime, 2.2f);
		Fade[Side] = FMath::FInterpConstantTo(Fade[Side], FadeTo[Side], DeltaTime, 2.2f);

		// Idle bob; a lunge goes toward the other circle and back; a hit shakes and blinks red.
		const float Toward = Side == 1 ? 1.f : -1.f;
		const float Reach = FMath::Sin(Lunge[Side] * PI);
		const float Shake = Hit[Side] > 0.f ? FMath::Sin(Clock * 45.f) * 6.f * Hit[Side] : 0.f;
		const float Bob = FMath::Sin(Clock * (Side == 1 ? 2.9f : 3.5f) + Side) * 4.f;
		// Stepping in from its own side, knocked back by a blow, and (the wild curse) a hop when it throws the seal off.
		const float Recoil = -Toward * 24.f * FMath::Square(Hit[Side]);
		const float Hop = Side == 0 ? -22.f * FMath::Sin(Pop * PI) : 0.f;
		Sprites[Side]->SetRenderTranslation(Reduced?FVector2D::ZeroVector:FVector2D(Toward * 72.f * Reach + Shake + Recoil - Toward * 340.f * Arrive, Bob + Hop));
		const float Press = Side == 0 && !Reduced ? Squash : 0.f;
		Sprites[Side]->SetRenderScale(Reduced?FVector2D(1.f):FVector2D((1.f-Shrink[Side])*(1.f+.07f*Press),(1.f-Shrink[Side])*(1.f-.14f*Press)));
		// One sustained tint, never a repeated red/white flash. A sealed curse turns the gold of the wax.
		const bool bRed = Hit[Side] > .55f;
		const bool bGold = Side == 0 && bSealLanded && bSealHolds && SealTime > 0.f;
		Sprites[Side]->SetColorAndOpacity(FLinearColor(1.f, bRed ? 0.3f : bGold ? 0.86f : 1.f, bRed ? 0.3f : bGold ? 0.5f : 1.f, (1.f - Fade[Side]) * (1.f - Arrive)));
		if (Impact[Side] > 0.f)
		{
			Impact[Side] = FMath::Max(0.f, Impact[Side] - DeltaTime / 0.28f);
			Rings[Side]->SetVisibility(Impact[Side] > 0.f && !Reduced ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Rings[Side]->SetRenderScale(FVector2D(FMath::Lerp(1.9f, 0.5f, Impact[Side])));
			Rings[Side]->SetRenderOpacity(Impact[Side]);
		}
		if(Reduced && Shrink[Side]>=.99f) Sprites[Side]->SetRenderOpacity(0);
		WardTime[Side]=FMath::Max(0.f,WardTime[Side]-DeltaTime);
		if(WardTime[Side]==0) WardImages[Side]->SetVisibility(ESlateVisibility::Collapsed);

		ShownHp[Side] = FMath::FInterpTo(ShownHp[Side], TargetHp[Side], DeltaTime, 7.f);
		Bars[Side]->SetPercent(ShownHp[Side]);
		Bars[Side]->SetFillColorAndOpacity(HpColor(ShownHp[Side]));

		if (NumberTime[Side] > 0.f)
		{
			NumberTime[Side] -= DeltaTime / 0.95f;
			Numbers[Side]->SetRenderTranslation(FVector2D(0.f, Reduced?0.f:-24.f * (1.f - NumberTime[Side])));
			Numbers[Side]->SetRenderOpacity(FMath::Clamp(NumberTime[Side] * 3.f, 0.f, 1.f));
			if (NumberTime[Side] <= 0.f) Numbers[Side]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// The wax seal comes down on the wild curse: it stays when the binding held, and lifts when it broke.
	if (SealTime > 0.f)
	{
		SealTime = FMath::Max(bSealHolds ? 0.6f : 0.f, SealTime - DeltaTime);
		const float In = FMath::Clamp((1.4f - SealTime) / 0.22f, 0.f, 1.f);
		// It falls faster as it comes, tilted, and lands square: a thud, a ring of gold and the curse pressed flat.
		const float Drop = In * In;
		if (In >= 1.f && !bSealLanded)
		{
			bSealLanded = true; SealRingTime = Punch = Squash = 1.f;
			UGbAudio::Play(this, TEXT("seal_stamp"), 1.f);
		}
		// A seal that breaks is thrown up and aside; one that holds stays on the curse.
		SealStamp->SetRenderTranslation(Reduced?FVector2D::ZeroVector:FVector2D(90.f * Pop, -280.f * (1.f - Drop) - 210.f * Pop * (2.f - Pop)));
		SealStamp->SetRenderTransformAngle(Reduced?0.f:-16.f * (1.f - Drop) + 55.f * Pop);
		SealStamp->SetRenderScale(FVector2D(Reduced?1.f:FMath::Lerp(1.7f, 1.f, Drop) + .1f * Squash));
		SealStamp->SetRenderOpacity(SealPop > 0.f ? 1.f - SealPop : In * FMath::Clamp(SealTime / 0.3f, 0.f, 1.f));
		if (SealTime <= 0.f) SealStamp->SetVisibility(ESlateVisibility::Collapsed);
	}
	SealRing->SetVisibility(SealRingTime > 0.f && !Reduced ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SealRing->SetRenderScale(FVector2D(FMath::Lerp(2.3f, 0.6f, SealRingTime)));
	SealRing->SetRenderOpacity(SealRingTime);
	Flash->SetVisibility(FlashTime > 0.f && !Reduced ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Flash->SetRenderOpacity(0.16f * FlashTime);

	// The camera: wide as the curses step in, a short punch on a blow, and in on the wild curse while the seal is on it.
	const float SealFocus = Reduced || SealTime <= 0.f ? 0.f : FMath::Clamp((1.4f - SealTime) / 0.3f, 0.f, 1.f) * (SealPop > 0.f ? 1.f - SealPop : 1.f);
	const float Zoom = Reduced ? 1.f : 1.f + .06f * Arrive + .018f * Punch + .07f * SealFocus;
	const float Rattle = FMath::Sin(Clock * 55.f) * FMath::Min(6.f, (Zoom - 1.f) * 800.f) * Punch;
	StagePanel->SetRenderScale(FVector2D(Zoom));
	StagePanel->SetRenderTranslation(Reduced?FVector2D::ZeroVector:FVector2D(Rattle - (1416.f - 960.f) * .07f * SealFocus, -(286.f - 360.f) * .07f * SealFocus));
	KessaImage->SetRenderTranslation(FVector2D(0.f, Reduced?0.f:FMath::Sin(Clock * 1.7f) * 4.f));
}

void UGbBattleWidget::ShowProblem(const FString& Message, bool bBattleOver)
{
	if (!Problem) return;
	Problem->SetText(FText::FromString(Message));
	Problem->SetVisibility(ESlateVisibility::Visible);
	Problem->GetParent()->SetVisibility(ESlateVisibility::Visible);
	bWaitingForReply=false; FeedbackRemaining=0; Cues.Reset();
	if (bBattleOver)
	{
		bBattleEnded=true; SetButtonsEnabled(false);
		EndText->SetText(FText::FromString(TEXT("The curse slipped back into the grass.")));
		EndPanel->SetVisibility(ESlateVisibility::Visible);
		BackButton->SetKeyboardFocus();
	}
	else
	{
		SetButtonsEnabled(true);
	}
}

void UGbBattleWidget::ShowView(const FGbBattleView& View)
{
	LastView = View;

	const FGbSide* Sides[2] = { &View.Wild, &View.Player };
	UTextBlock* Names[2] = { WildName, PlayerName };
	UTextBlock* Tags[2] = { WildTags, PlayerTags };
	UTextBlock* Hps[2] = { WildHpText, PlayerHpText };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FGbSide& S = *Sides[Side];
		Names[Side]->SetText(FText::FromString(FString::Printf(TEXT("%s  Lv %d"), *S.Name, S.Level)));
		Tags[Side]->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *Capitalised(S.Emotion), *Capitalised(S.Omen))));
		Tags[Side]->SetColorAndOpacity(FSlateColor(C::Ink));
		EmotionIcons[Side]->SetBrush(ArtBrush(TEXT("emo_") + (S.Emotion.IsEmpty() ? FString(TEXT("rage")) : S.Emotion), 3.f));
		// The bar itself eases to this in NativeTick.
		TargetHp[Side] = HpFraction(S);
		Hps[Side]->SetText(FText::FromString(HpText(S)));
	}

	SalveLabel->SetText(FText::FromString(FString::Printf(TEXT("x%d"), View.Salves)));
	SealLabel->SetText(FText::FromString(Percent(View.SealChance)));
	GildedLabel->SetText(FText::FromString(FString::Printf(TEXT("%s  x%d"), *Percent(View.GildedSealChance), View.GildedSeals)));
	FleeLabel->SetText(FText::FromString(Percent(View.FleeChance)));
}

void UGbBattleWidget::AddLines(const TArray<FString>& Lines)
{
	constexpr int32 MaxLines = 2;
	for (const FString& L : Lines) LogLines.Add(L);
	if (LogLines.Num() > MaxLines) LogLines.RemoveAt(0, LogLines.Num() - MaxLines);
	Log->ClearChildren();
	for (int32 i = 0; i < LogLines.Num(); ++i)
	{
		// Opaque cream on ink: both lines carry readable turn information.
		UTextBlock* T = MakeText(LogLines[i], 22, GbUi::PaperHi());
		T->SetAutoWrapText(false); T->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); T->SetToolTipText(FText::FromString(LogLines[i]));
		AddRow(Log, T, 0.f, 0.f);
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
	if(bBattleEnded || bWaitingForReply || FeedbackRemaining>0) return;
	const bool Allowed = Action==EGbAction::Attack?LastView.bCanAttack:Action==EGbAction::Ward?LastView.bCanWard:Action==EGbAction::Salve?LastView.bCanSalve:Action==EGbAction::Seal?LastView.bCanSeal:Action==EGbAction::GildedSeal?LastView.bCanGildedSeal:LastView.bCanFlee;
	if(!Allowed) return;
	UGbApiSubsystem* A = Api();
	if (!A || !A->IsBattleOpen() || A->IsBattleBusy()) return;
	SetButtonsEnabled(false);
	bWaitingForReply=true;
	A->SendAction(Action);
}

void UGbBattleWidget::ShowEnd(EGbOutcome Outcome, int32 Points, const FString& Wild)
{
	bBattleEnded=true; SetButtonsEnabled(false);
	FString Text;
	switch (Outcome)
	{
	case EGbOutcome::Bound: Text = FString::Printf(TEXT("Bound! %s is sealed into a talisman.\n+%d points"), *Wild, Points); break;
	case EGbOutcome::Won: Text = bRivalEnd ? FString::Printf(TEXT("You beat %s!\n+%d points"), *LastStart.RivalName, Points)
		: FString::Printf(TEXT("Exorcised! %s fades into ash.\n+%d points"), *Wild, Points); break;
	case EGbOutcome::Fled: Text = TEXT("You got away. The curse still haunts the tall grass."); break;
	case EGbOutcome::Lost: Text = bRivalEnd ? FString::Printf(TEXT("%s wins this round. Rest your partner and try again."), *LastStart.RivalName)
		: FString(TEXT("Your partner is spent. You stagger back to the Ward.")); break;
	default: Text = TEXT("The battle is over."); break;
	}
	Text += EndExtra; EndExtra.Reset();
	EndText->SetText(FText::FromString(Text));
	if(FeedbackRemaining<=0) EndPanel->SetVisibility(ESlateVisibility::Visible);
}

FReply UGbBattleWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
	const FKey Key=Event.GetKey();
	if(bBattleEnded && EndPanel->GetVisibility()==ESlateVisibility::Visible && Key==EKeys::Enter) { HandleBack(); return FReply::Handled(); }
	const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six};
	const EGbAction Moves[]={EGbAction::Attack,EGbAction::Ward,EGbAction::Salve,EGbAction::Seal,EGbAction::GildedSeal,EGbAction::Flee};
	UButton* Buttons[]={AttackButton,WardButton,SalveButton,SealButton,GildedButton,FleeButton};
	for(int32 I=0;I<6;++I) if(Key==Keys[I]) { if(!Event.IsRepeat() && Buttons[I]->GetIsEnabled()) Send(Moves[I]); return FReply::Handled(); }
	if(Key==EKeys::Left||Key==EKeys::Right||Key==EKeys::Up||Key==EKeys::Down)
	{
		for(int32 I=0;I<6;++I)if(Buttons[I]->HasKeyboardFocus())FocusedAction=I;
		const int32 Step=Key==EKeys::Left?-1:Key==EKeys::Right?1:3;
		for(int32 I=0;I<6;++I){FocusedAction=(FocusedAction+Step+6)%6;if(Buttons[FocusedAction]->GetIsEnabled()){Buttons[FocusedAction]->SetKeyboardFocus();break;}}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry,Event);
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
	UBorder* Row = MakePanel(GbUi::PaperHi(), FMargin(12.f, 10.f));
	WidgetTree->RootWidget = Row;
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(H);
	UBorder* Plate = MakePanel(GbUi::Paper2(), FMargin(4.f)); Plate->SetContent(MakeArt(FString(), 72.f, Art));
	AddCol(H, Plate, false, 16.f);

	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Name = MakeText(TEXT(""), 22, C::Ink, true); Name->SetAutoWrapText(false);
	AddRow(Text, Name, 0.f, 2.f);
	UHorizontalBox* TagRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddCol(TagRow, MakeArt(FString(), 22.f, Emotion), false, 12.f);
	Tags = MakeText(TEXT(""), 16, C::Ink, true); AddCol(TagRow, Tags, true, 0.f);
	AddRow(Text, TagRow, 0.f, 2.f);
	Details = MakeText(TEXT(""), 15, C::Ink);
	AddRow(Text, Details, 0.f, 0.f);
	AddCol(H, Text, true, 12.f);

	UTextBlock* Label = nullptr;
	PartnerButton = MakeKeyButton(TEXT("Make partner"), FString(), Label, C::Gold);
	Label->SetFont(GbUi::Font(18, true));
	PartnerLabel = Label;
	PartnerButton->OnClicked.AddDynamic(this, &UGbTalismanRowWidget::HandleMakePartner);
	AddCol(H, Sized(PartnerButton, 230.f, 52.f), false, 0.f);
}

void UGbTalismanRowWidget::Setup(const FGbTalisman& Talisman, const FGbCurseInfo& Curse, bool bIsPartner)
{
	if (!Name) return;
	TalismanId = Talisman.Id;
	Name->SetText(FText::FromString(Curse.Name.IsEmpty() ? FString(TEXT("Sealed talisman")) : Curse.Name));
	SetArt(Art, CurseArt(Talisman.SpeciesId), 72.f);
	SetArt(Emotion, Curse.Emotion.IsEmpty() ? FString() : TEXT("emo_") + Curse.Emotion, 22.f);
	Tags->SetText(FText::FromString(FString::Printf(TEXT("%s  -  %s"), *Capitalised(Curse.Emotion), *Capitalised(Talisman.Omen))));
	const FString Origin = Talisman.Id == UGbPlayerSubsystem::StarterId ? FString(TEXT("Kessa's gift"))
		: Talisman.BoundDay < 0 ? FString(TEXT("from the ledger")) : FString::Printf(TEXT("bound on day %d"), Talisman.BoundDay);
	const FString Purse = Talisman.Purse > 0 ? FString::Printf(TEXT(", purse %d Ember"), Talisman.Purse) : FString();
	const FString OnChain = Talisman.ChainTokenId.IsEmpty() ? FString() : FString::Printf(TEXT("  |  Ledger %s%s"), *Talisman.ChainTokenId, *Purse);
	Details->SetText(FText::FromString(FString::Printf(TEXT("%s  |  %d wins%s"), *Origin, Talisman.Wins, *OnChain)));
	// The partner's row is gold-edged paper and its button says so instead of offering the swap.
	if (UBorder* Row = Cast<UBorder>(WidgetTree->RootWidget)) Row->SetBrushColor(bIsPartner ? GbUi::Paper2() : GbUi::PaperHi());
	PartnerLabel->SetText(FText::FromString(bIsPartner ? TEXT("Partner") : TEXT("Make partner")));
	PartnerButton->SetBackgroundColor(bIsPartner ? C::Leaf : C::Gold);
	PartnerButton->SetIsEnabled(!bIsPartner);
}

void UGbTalismanRowWidget::HandleMakePartner()
{
	if (UGbPlayerSubsystem* P = Player()) P->SetPartner(TalismanId);
}

void UGbTalismanCaseWidget::BuildTree()
{
	UTextBlock* TitleText = nullptr; UHorizontalBox* Footer = nullptr;
	UVerticalBox* Body = MakeShell(TEXT("Talisman Case"), TEXT("talisman_gilded"), 1120.f, TitleText, Record, Footer);
	UHorizontalBox* Chips = WidgetTree->ConstructWidget<UHorizontalBox>();
	PointsChip = MakeChip(TEXT(""), C::Gold, PointsText); AddCol(Chips, PointsChip, false, 10.f);
	EmberChip = MakeChip(TEXT(""), C::Gold, EmberText); AddCol(Chips, EmberChip, false, 10.f);
	LedgerChip = MakeChip(TEXT(""), C::Leaf, LedgerText); AddCol(Chips, LedgerChip, false, 0.f);
	AddRow(Body, Chips, 0.f, 14.f);
	List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddRow(Body, Sized(List, 0.f, 520.f));
	UTextBlock* Label = nullptr;
	UButton* Close = MakeKeyButton(TEXT("Close"), KeyName(TEXT("Case")), Label, GbUi::Paper2());
	Close->OnClicked.AddDynamic(this, &UGbTalismanCaseWidget::HandleClose);
	AddCol(Footer, Sized(Close, 240.f, 56.f), false, 0.f);
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
	TMap<FString, FGbCurseInfo> Curses;
	for (const FGbTalisman& T : Talismans) Curses.Add(T.SpeciesId, A->GetCurse(T.SpeciesId));
	Show(Talismans, Curses, P->GetPartner().Id, Won, Bound, Fled, Lost, P->GetPoints(), P->GetEmber(), P->IsOnChain());
}

void UGbTalismanCaseWidget::Show(const TArray<FGbTalisman>& Talismans, const TMap<FString, FGbCurseInfo>& Curses, const FString& PartnerId,
	int32 Won, int32 Bound, int32 Fled, int32 Lost, int32 Points, int32 Ember, bool bOnChain)
{
	if (!List) return;
	Record->SetText(FText::FromString(FString::Printf(TEXT("%d talismans  |  bound %d, exorcised %d, fled %d, lost %d"),
		Talismans.Num(), Bound, Won, Fled, Lost)));
	SetChip(PointsChip, PointsText, Points >= 0 ? FString::Printf(TEXT("%d points"), Points) : FString(), C::Gold);
	SetChip(EmberChip, EmberText, Ember >= 0 ? FString::Printf(TEXT("%d Ember"), Ember) : FString(), C::Gold);
	SetChip(LedgerChip, LedgerText, bOnChain ? TEXT("Rite's ledger") : TEXT("This PC only"), bOnChain ? C::Leaf : GbUi::Paper3());
	List->ClearChildren();
	for (const FGbTalisman& T : Talismans)
	{
		UGbTalismanRowWidget* Row = CreateWidget<UGbTalismanRowWidget>(this, UGbTalismanRowWidget::StaticClass());
		if (!Row) continue;
		Row->Setup(T, Curses.FindRef(T.SpeciesId), T.Id == PartnerId);
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(List->AddChild(Row))) RowSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 10.f));
	}
	if (!Talismans.Num()) List->AddChild(MakeText(TEXT("No talismans to show yet."), 16, C::Ink));
}

void UGbTalismanCaseWidget::HandlePlayerChanged() { Refresh(); }

// ================================================================ the shop

void UGbShopRowWidget::BuildTree()
{
	UBorder* Row = MakePanel(GbUi::PaperHi(), FMargin(12.f, 10.f));
	WidgetTree->RootWidget = Row;
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(H);
	UBorder* Plate = MakePanel(GbUi::Paper2(), FMargin(4.f)); Plate->SetContent(MakeArt(FString(), 56.f, ArtImage));
	if (UHorizontalBoxSlot* PlateSlot = Cast<UHorizontalBoxSlot>(H->AddChildToHorizontalBox(Plate)))
	{
		PlateSlot->SetVerticalAlignment(VAlign_Top); PlateSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
	}
	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleText = MakeText(TEXT(""), 20, C::Ink, true);
	DetailText = MakeText(TEXT(""), 15, C::Ink);
	DetailText->SetAutoWrapText(true);
	AddRow(Text, TitleText, 0.f, 4.f);
	AddRow(Text, DetailText, 0.f, 0.f);
	AddCol(H, Text, true, 12.f);
	UTextBlock* Label = nullptr;
	Action = MakeKeyButton(TEXT(""), FString(), Label, C::Gold);
	Label->SetFont(GbUi::Font(18, true));
	ActionLabel = Label;
	Action->OnClicked.AddDynamic(this, &UGbShopRowWidget::HandleClick);
	if (UHorizontalBoxSlot* ButtonSlot = Cast<UHorizontalBoxSlot>(H->AddChildToHorizontalBox(Sized(Action, 150.f, 52.f))))
	{
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UGbShopRowWidget::Setup(const FString& Art, const FString& InTitle, const FString& Detail, const FString& Button, bool bEnabled, const FLinearColor& TitleColor)
{
	if (!TitleText) return;
	SetArt(ArtImage, Art, 56.f);
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
	UTextBlock* TitleText = nullptr; UHorizontalBox* Footer = nullptr;
	UVerticalBox* Body = MakeShell(TEXT("The Talisman Broker"), TEXT("icon_seal"), 1280.f, TitleText, Purse, Footer);
	Title = TitleText;
	Purse->SetText(FText::FromString(TEXT("Counting your Ember...")));

	// Two columns: the Broker's stock on the left, your sealed talismans (to sell back) on the right.
	UHorizontalBox* Cols = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	AddRow(Left, MakeSection(TEXT("For sale")), 0.f, 8.f);
	Stock = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddRow(Left, Sized(Stock, 0.f, 500.f));
	AddCol(Cols, Left, true, 24.f);
	if (UHorizontalBoxSlot* LeftSlot = Cast<UHorizontalBoxSlot>(Left->Slot))
	{
		FSlateChildSize Wide(ESlateSizeRule::Fill); Wide.Value = 1.4f; LeftSlot->SetSize(Wide);
	}
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	AddRow(Right, MakeSection(TEXT("Your sealed talismans")), 0.f, 8.f);
	Yours = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddRow(Right, Sized(Yours, 0.f, 500.f));
	AddCol(Cols, Right, true, 0.f);
	AddRow(Body, Cols);

	// Trade progress and receipts go in the shell's ink note.
	Status = ShellStatus;

	UTextBlock* Label = nullptr;
	UButton* Close = MakeKeyButton(TEXT("Back to Satchel"), KeyName(TEXT("Satchel")), Label, GbUi::Paper2());
	Close->OnClicked.AddDynamic(this, &UGbShopWidget::HandleClose);
	AddCol(Footer, Sized(Close, 320.f, 56.f), false, 0.f);
}

FString UGbShopWidget::ItemArt(const FString& ItemId)
{
	if (ItemId.StartsWith(TEXT("talisman-"))) return TEXT("emo_") + ItemId.Mid(9);
	if (ItemId == TEXT("blank-talisman") || ItemId == TEXT("binding-seal")) return TEXT("talisman");
	if (ItemId == TEXT("gilded-talisman")) return TEXT("talisman_gilded");
	if (ItemId == TEXT("ink-salve")) return TEXT("icon_salve");
	return TEXT("wax_seal");
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
		? FString::Printf(TEXT("Your Ember in the ledger: %d"), Data.Ember)
		: FString(TEXT("Your Ember balance is unavailable. The Broker cannot trade yet."))));

	Stock->ClearChildren();
	for (const FGbShopItem& It : Data.Items)
	{
		const bool bTalisman = It.Id.StartsWith(TEXT("talisman-"));
		const bool bSupply = It.Id == TEXT("blank-talisman") || It.Id == TEXT("gilded-talisman") || It.Id == TEXT("ink-salve") || It.Id == TEXT("binding-seal");
		UGbShopRowWidget* Row = CreateWidget<UGbShopRowWidget>(this, UGbShopRowWidget::StaticClass());
		if (!Row) continue;
		const bool bCan = (bTalisman || bSupply) && !bBusy && A && !A->IsSatchelBusy() && !Data.bClosed && It.Stock > 0 && Data.Ember >= It.Price;
		const FString Detail = FString::Printf(TEXT("%d Ember  |  %d in stock\n%s"), It.Price, It.Stock, *It.Text);
		const FString Emotion = bTalisman ? It.Id.Mid(9) : FString();
		Row->Setup(ItemArt(It.Id), It.Name, bTalisman || bSupply ? Detail : Detail + TEXT("  (not sold to players yet)"),
			FString::Printf(TEXT("Buy  %d"), It.Price), bCan, bTalisman ? EmotionColor(Emotion) : C::Ink);
		const FString ItemId = It.Id;
		Row->OnClick = [this, ItemId]() { Buy(ItemId); };
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Stock->AddChild(Row))) RowSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
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
			Row->Setup(CurseArt(T.SpeciesId), FString::Printf(TEXT("%s  (ledger %s)"), *Curse.Name, *T.ChainTokenId),
				bPartner ? FString(TEXT("Your partner: choose another partner in the Satchel (B) before selling it."))
					: Offer > 0 ? FString::Printf(TEXT("The Broker offers %d Ember"), Offer) : FString(TEXT("The Broker doesn't buy these")),
				FString::Printf(TEXT("Sell  %d"), Offer), !bBusy && !A->IsSatchelBusy() && !bPartner && Offer > 0 && !Data.bClosed, EmotionColor(Curse.Emotion));
			const FString TokenId = T.ChainTokenId;
			Row->OnClick = [this, TokenId]() { Sell(TokenId); };
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Yours->AddChild(Row))) RowSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
			++Shown;
		}
	}
	if (Shown == 0)
	{
		UTextBlock* None = MakeText(TEXT("None yet: talismans sealed in the ledger show up here."), 17, C::Ink);
		None->SetAutoWrapText(true);
		Yours->AddChild(None);
	}
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
	Status->SetText(FText::FromString(Trade.Text + TEXT(". Sealing it in the ledger...")));
}

void UGbShopWidget::HandleTradeReceipt(const FString& JobId, const FGbChainReceipt& Receipt)
{
	if (JobId != PendingJob) return;
	PendingJob.Reset();
	bBusy = false;
	const FString Was = Status->GetText().ToString().Replace(TEXT(" Sealing it in the ledger..."), TEXT(""));
	Status->SetText(FText::FromString(Receipt.Status == TEXT("done")
		? FString::Printf(TEXT("%s Sealed in the Rite's ledger. (%s opens the proof)"), *Was, *UGbUserSettings::Get()->KeyLabel(TEXT("Proof")))
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
	UTextBlock* TitleText = nullptr; UHorizontalBox* Footer = nullptr;
	UVerticalBox* Body = MakeShell(TEXT("The Rite"), TEXT("stamp_tool"), 1320.f, TitleText, Info, Footer);
	Info->SetText(FText::FromString(TEXT("Asking the Rite...")));

	UTextBlock* Ignored = nullptr;
	auto Arrow = [this, &Ignored](const TCHAR* Label) { return MakeKeyButton(Label, FString(), Ignored, GbUi::Paper2()); };
	UHorizontalBox* Cols = WidgetTree->ConstructWidget<UHorizontalBox>();

	// Left: the rules in force as slips, and the repeal stepper under them.
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	AddRow(Left, MakeSection(TEXT("In force")), 0.f, 8.f);
	ActiveList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UScrollBox* ActiveScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ActiveScroll->AddChild(ActiveList);
	AddRow(Left, Sized(ActiveScroll, 0.f, 400.f), 0.f, 12.f);

	UBorder* StrikeStrip = MakePanel(GbUi::Paper2(), FMargin(14.f, 10.f));
	UHorizontalBox* Strike = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StrikeStrip->SetContent(Strike);
	UButton* PrevR = Arrow(TEXT("<"));
	UButton* NextR = Arrow(TEXT(">"));
	PrevR->OnClicked.AddDynamic(this, &UGbRulesWidget::HandlePrevRepeal);
	NextR->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleNextRepeal);
	RepealPick = MakeText(TEXT("-"), 20, C::Ink, true);
	AddCol(Strike, MakeStepper(MakeText(TEXT("Strike"), 18, C::Ink, true), PrevR, RepealPick, NextR, 80.f), true, 14.f);
	RepealButton = MakeKeyButton(TEXT("Repeal"), FString(), RepealLabel, C::Crimson);
	RepealButton->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleRepeal);
	AddCol(Strike, Sized(RepealButton, 230.f, 52.f), false, 0.f);
	RepealRow = StrikeStrip;
	AddRow(Left, StrikeStrip);
	AddCol(Cols, Left, true, 28.f)->SetVerticalAlignment(VAlign_Top);

	// Right: the rule builder. Parameter buttons bind by function name (AddDynamic needs a literal member).
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	AddRow(Right, MakeSection(TEXT("Propose a rule")), 0.f, 8.f);
	UButton* PrevT = Arrow(TEXT("<"));
	UButton* NextT = Arrow(TEXT(">"));
	PrevT->OnClicked.AddDynamic(this, &UGbRulesWidget::HandlePrevTemplate);
	NextT->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleNextTemplate);
	TemplateId = MakeText(TEXT("-"), 20, C::Ink, true);
	AddRow(Right, MakeStepper(MakeText(TEXT("Rule"), 18, C::Ink, true), PrevT, TemplateId, NextT), 0.f, 8.f);

	static const TCHAR* PrevName[MaxParams] = { TEXT("HandlePrev0"), TEXT("HandlePrev1"), TEXT("HandlePrev2") };
	static const TCHAR* NextName[MaxParams] = { TEXT("HandleNext0"), TEXT("HandleNext1"), TEXT("HandleNext2") };
	for (int32 i = 0; i < MaxParams; ++i)
	{
		UTextBlock* Key = MakeText(TEXT(""), 18, C::Ink, true);
		UTextBlock* Value = MakeText(TEXT(""), 20, C::Ink, true);
		UButton* P = Arrow(TEXT("<"));
		UButton* N = Arrow(TEXT(">"));
		FScriptDelegate PD;
		PD.BindUFunction(this, FName(PrevName[i]));
		P->OnClicked.Add(PD);
		FScriptDelegate ND;
		ND.BindUFunction(this, FName(NextName[i]));
		N->OnClicked.Add(ND);
		UHorizontalBox* Row = MakeStepper(Key, P, Value, N);
		AddRow(Right, Row, 0.f, 8.f);
		ParamRows.Add(Row);
		ParamKeys.Add(Key);
		ParamValues.Add(Value);
	}

	// The decree as it would read, on a sealed slip.
	UBorder* Decree = MakePanel(GbUi::PaperHi(), FMargin(16.f, 14.f));
	UHorizontalBox* DecreeRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Decree->SetContent(DecreeRow);
	UImage* Seal = nullptr;
	UWidget* SealArt = MakeArt(TEXT("wax_seal"), 40.f, Seal);
	AddCol(DecreeRow, SealArt, false, 14.f);
	if (UHorizontalBoxSlot* SealSlot = Cast<UHorizontalBoxSlot>(SealArt->Slot)) SealSlot->SetVerticalAlignment(VAlign_Top);
	Preview = MakeText(TEXT(""), 20, C::Ink, true);
	Preview->SetAutoWrapText(true);
	AddCol(DecreeRow, Preview, true, 0.f);
	AddRow(Right, Sized(Decree, 0.f, 120.f), 4.f, 12.f);

	UTextBlock* ProposeLabel = nullptr;
	ProposeButton = MakeKeyButton(TEXT("Propose"), FString(), ProposeLabel, C::Crimson);
	ProposeButton->OnClicked.AddDynamic(this, &UGbRulesWidget::HandlePropose);
	AddRow(Right, Sized(ProposeButton, 0.f, 56.f));
	AddCol(Cols, Right, true, 0.f)->SetVerticalAlignment(VAlign_Top);
	AddRow(Body, Cols);

	// Kessa's answers and ledger receipts go in the shell's ink note.
	Status = ShellStatus;

	UTextBlock* Label = nullptr;
	UButton* Close = MakeKeyButton(TEXT("Close"), KeyName(TEXT("Rite")), Label, GbUi::Paper2());
	Close->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleClose);
	AddCol(Footer, Sized(Close, 240.f, 56.f), false, 0.f);
}

UHorizontalBox* UGbWidgetBase::MakeStepper(UTextBlock* Key, UButton* Prev, UTextBlock* Value, UButton* Next, float KeyWidth)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddCol(Row, Sized(Key, KeyWidth, 0.f), false, 8.f);
	AddCol(Row, Sized(Prev, 52.f, 52.f), false, 6.f);
	UBorder* Face = MakePanel(GbUi::PaperHi(), FMargin(12.f, 0.f));
	Face->SetHorizontalAlignment(HAlign_Center); Face->SetVerticalAlignment(VAlign_Center);
	Value->SetJustification(ETextJustify::Center);
	Face->SetContent(Value);
	AddCol(Row, Sized(Face, 0.f, 52.f), true, 6.f);
	AddCol(Row, Sized(Next, 52.f, 52.f), false, 0.f);
	return Row;
}

void UGbRulesWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGbApiSubsystem* A = Api())
	{
		A->OnRules.AddUniqueDynamic(this, &UGbRulesWidget::HandleRules);
		A->OnRuleVerdict.AddUniqueDynamic(this, &UGbRulesWidget::HandleVerdict);
		A->OnRuleReceipt.AddUniqueDynamic(this, &UGbRulesWidget::HandleRuleReceipt);
		A->OnRuleRepeal.AddUniqueDynamic(this, &UGbRulesWidget::HandleRepealAnswer);
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
		A->OnRuleRepeal.RemoveDynamic(this, &UGbRulesWidget::HandleRepealAnswer);
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

TArray<const FGbActiveRule*> UGbRulesWidget::Repealable() const
{
	TArray<const FGbActiveRule*> Out;
	for (const FGbActiveRule& R : Data.Active) if (R.ChainId >= 0) Out.Add(&R);
	return Out;
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
	if (!Data.bChain) Line = TEXT("The Rite's ledger is unavailable. Rules cannot be proposed yet.");
	Info->SetText(FText::FromString(Line));

	ActiveList->ClearChildren();
	ActiveSlips.Reset();
	ActiveIds.Reset();
	if (Data.Active.Num() == 0) ActiveList->AddChild(MakeText(TEXT("No rules yet. The Rite is waiting."), 17, C::Ink));
	for (const FGbActiveRule& R : Data.Active)
	{
		const FString Who = R.Author.Contains(TEXT(".")) ? R.Author.Left(R.Author.Find(TEXT("."))) : R.Author;
		UBorder* Slip = MakePanel(GbUi::PaperHi(), FMargin(14.f, 10.f));
		UHorizontalBox* In = WidgetTree->ConstructWidget<UHorizontalBox>();
		Slip->SetContent(In);
		UTextBlock* NumText = nullptr;
		UBorder* Num = MakeChip(FString::Printf(TEXT("#%d"), R.Id), C::Crimson, NumText);
		AddCol(In, Num, false, 12.f);
		if (UHorizontalBoxSlot* NumSlot = Cast<UHorizontalBoxSlot>(Num->Slot)) NumSlot->SetVerticalAlignment(VAlign_Top);
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* T = MakeText(R.Text, 18, C::Ink, true);
		T->SetAutoWrapText(true);
		AddRow(Lines, T, 0.f, 4.f);
		AddRow(Lines, MakeText(FString::Printf(TEXT("%s, %d pts"), *Who, R.Cost), 15, C::Ink));
		AddCol(In, Lines, true, 0.f);
		if (UVerticalBoxSlot* SlipSlot = Cast<UVerticalBoxSlot>(ActiveList->AddChild(Slip))) SlipSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
		ActiveSlips.Add(Slip);
		ActiveIds.Add(R.Id);
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

	const TArray<const FGbActiveRule*> Strikable = Repealable();
	RepealRow->SetVisibility(Data.bChain && Strikable.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Strikable.Num() > 0)
	{
		RepealIndex = FMath::Clamp(RepealIndex, 0, Strikable.Num() - 1);
		const FGbActiveRule* R = Strikable[RepealIndex];
		RepealPick->SetText(FText::FromString(FString::Printf(TEXT("#%d"), R->Id)));
		RepealLabel->SetText(FText::FromString(FString::Printf(TEXT("Repeal (%d pts)"), R->RepealCost)));
		const bool bAfford = Data.Points < 0 || Data.Points >= R->RepealCost;
		RepealButton->SetIsEnabled(!bBusy && bAfford);
	}
	// Mark the slip the repeal stepper points at.
	const int32 Marked = Data.bChain && Strikable.Num() > 0 ? Strikable[RepealIndex]->Id : INDEX_NONE;
	for (int32 i = 0; i < ActiveSlips.Num(); ++i)
	{
		if (ActiveSlips[i]) ActiveSlips[i]->SetBrushColor(ActiveIds[i] == Marked ? GbUi::Paper2() : GbUi::PaperHi());
	}
}

void UGbRulesWidget::StepRepeal(int32 Delta)
{
	const int32 N = Repealable().Num();
	if (bBusy || N == 0) return;
	RepealIndex = (RepealIndex + Delta + N) % N;
	Redraw();
}

void UGbRulesWidget::HandleRepeal()
{
	const TArray<const FGbActiveRule*> Strikable = Repealable();
	UGbApiSubsystem* A = Api();
	if (bBusy || !A || !Strikable.IsValidIndex(RepealIndex)) return;
	const FGbActiveRule* R = Strikable[RepealIndex];
	PendingJob.Reset();
	bPendingRepeal = true;
	Status->SetText(FText::FromString(FString::Printf(TEXT("Asking the Rite to strike #%d..."), R->Id)));
	SetBusy(true);
	A->RepealRule(R->ChainId);
}

void UGbRulesWidget::HandleRepealAnswer(const FGbRuleVerdict& Answer)
{
	PendingJob = Answer.JobId;
	bPendingRepeal = true;
	Status->SetText(FText::FromString(FString::Printf(TEXT("Kessa: \"%s\" will be struck for %d points. Sealing it in the ledger..."), *Answer.Text, Answer.Cost)));
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
	bPendingRepeal = false;
	Status->SetText(FText::FromString(TEXT("Kessa is reading your rule...")));
	SetBusy(true);
	A->ProposeRule(T->Id, Params);
}

void UGbRulesWidget::HandleVerdict(const FGbRuleVerdict& Verdict)
{
	PendingJob = Verdict.JobId;
	Status->SetText(FText::FromString(Verdict.bAccepted
		? FString::Printf(TEXT("Kessa: \"%s\" is accepted. Sealing it in the Rite's ledger..."), *Verdict.Text)
		: FString::Printf(TEXT("Kessa rejects it: %s  Sealing her verdict (your %d points come back)..."), *Verdict.Reason, Verdict.Cost)));
}

void UGbRulesWidget::HandleRuleReceipt(const FString& JobId, const FGbChainReceipt& Receipt)
{
	if (JobId != PendingJob) return;
	PendingJob.Reset();
	if (bPendingRepeal)
	{
		bPendingRepeal = false;
		Status->SetText(FText::FromString(Receipt.Status == TEXT("done")
			? FString::Printf(TEXT("Rule %d was struck from the Rite's ledger. (%s opens the proof)"), Receipt.RuleId, *UGbUserSettings::Get()->KeyLabel(TEXT("Proof")))
			: FString::Printf(TEXT("The ledger refused the repeal: %s"), *Receipt.Error)));
		SetBusy(false);
		Open();
		return;
	}
	const FString Was = Status->GetText().ToString().Replace(TEXT("Sealing it in the Rite's ledger..."), TEXT("")).Replace(TEXT("Sealing her verdict"), TEXT("Verdict"));
	Status->SetText(FText::FromString(Receipt.Status == TEXT("done")
		? FString::Printf(TEXT("%s  Sealed in the Rite's ledger as rule %d. (%s opens the proof)"), *Was, Receipt.RuleId, *UGbUserSettings::Get()->KeyLabel(TEXT("Proof")))
		: FString::Printf(TEXT("The ledger refused it: %s"), *Receipt.Error)));
	SetBusy(false);
	Open();
}

void UGbRulesWidget::HandleApiError(const FString& What, int32 StatusCode, const FString& Message)
{
	if (What == TEXT("rules/propose") || What == TEXT("rules/repeal"))
	{
		bPendingRepeal = false;
		Status->SetText(FText::FromString(FString::Printf(TEXT("Kessa: %s"), *Message)));
		SetBusy(false);
	}
	else if (What == TEXT("rules"))
	{
		Info->SetText(FText::FromString(FString::Printf(TEXT("Could not reach the Rite: %s"), *Message)));
	}
}

void UGbRulesWidget::HandlePrevRepeal() { StepRepeal(-1); }
void UGbRulesWidget::HandleNextRepeal() { StepRepeal(1); }
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

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GbVowWidget.h"
#include "GbIntroWidget.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbUiMotionTest,"Grudgebound.Ui.SharedOpenCloseMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbUiMotionTest::RunTest(const FString&)
{
 UGbWidgetBase* W=NewObject<UGbMenderWidget>(); W->AddToRoot(); W->Initialize(); W->NativeOnInitialized(); W->TakeWidget();
 W->SetVisibility(ESlateVisibility::Collapsed); W->SetVisibility(ESlateVisibility::Visible);
 W->NativeTick(FGeometry(),.075f); TestTrue(TEXT("Open opacity interpolates over 0.15 seconds"),FMath::IsNearlyEqual(W->GetRenderOpacity(),.5f,.01f));
 W->NativeTick(FGeometry(),.075f); TestTrue(TEXT("Open finishes at full opacity"),FMath::IsNearlyEqual(W->GetRenderOpacity(),1.f));
 W->SetVisibility(ESlateVisibility::Collapsed); W->NativeTick(FGeometry(),.15f);
 TestEqual(TEXT("Close collapses only after the fade"),W->GetVisibility(),ESlateVisibility::Collapsed);
 W->RemoveFromRoot(); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbUiFontsTest,"Grudgebound.Ui.InlineFontsAndContrast",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbUiFontsTest::RunTest(const FString&)
{
 for (const TCHAR* Path:{TEXT("/Game/Grudgebound/UI/Fonts/YujiSyukuRegular.YujiSyukuRegular"),TEXT("/Game/Grudgebound/UI/Fonts/ShipporiMinchoRegular.ShipporiMinchoRegular"),TEXT("/Game/Grudgebound/UI/Fonts/ShipporiMinchoBold.ShipporiMinchoBold"),
  TEXT("/Game/Grudgebound/UI/Fonts/PixelifySansSemiBold.PixelifySansSemiBold"),TEXT("/Game/Grudgebound/UI/Fonts/PixelifySansBold.PixelifySansBold"),TEXT("/Game/Grudgebound/UI/Fonts/CourierPrimeRegular.CourierPrimeRegular")})
 {
  const UFontFace* Face=LoadObject<UFontFace>(nullptr,Path);
  if (TestNotNull(TEXT("OFL font face imported"),Face)) TestEqual(TEXT("Font bytes are inline"),Face->GetLoadingPolicy(),EFontLoadingPolicy::Inline);
 }
 const auto L=[](const FLinearColor& Color){return .2126f*Color.R+.7152f*Color.G+.0722f*Color.B;};
 TestTrue(TEXT("Shared ink/paper contrast exceeds 4.5:1"),(L(C::Paper)+.05f)/(L(C::Ink)+.05f)>=4.5f);
 return true;
}
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbUiRenderTest,"Grudgebound.Ui.AllPanels720And1080",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbUiRenderTest::RunTest(const FString&)
{
 const TArray<UClass*> Classes={UGbHudWidget::StaticClass(),UGbEncounterWidget::StaticClass(),UGbBattleWidget::StaticClass(),UGbSatchelWidget::StaticClass(),UGbTalismanCaseWidget::StaticClass(),UGbShopWidget::StaticClass(),UGbRulesWidget::StaticClass(),UGbVowWidget::StaticClass(),UGbSignInWidget::StaticClass(),UGbIntroWidget::StaticClass(),UGbMenderWidget::StaticClass(),UGbRoomWidget::StaticClass()};
 FWidgetRenderer Renderer(false);
 for (UClass* Class:Classes)
 {
  UGbWidgetBase* W=NewObject<UGbWidgetBase>(GetTransientPackage(),Class); W->AddToRoot(); W->Initialize(); W->NativeOnInitialized();
  if (!TestNotNull(TEXT("Native panel tree built"),W->WidgetTree->RootWidget.Get())) { W->RemoveFromRoot(); return false; }
  // Render the actual constructed tree without invoking live-game/session subscriptions in NativeConstruct.
  const TSharedRef<SWidget> Slate=W->WidgetTree->RootWidget->TakeWidget();
  if (UGbIntroWidget* Intro=Cast<UGbIntroWidget>(W)) Intro->Open();
  TArray<UWidget*> Widgets; W->WidgetTree->GetAllWidgets(Widgets);
  for (const UWidget* Widget:Widgets) if (const UTextBlock* Text=Cast<UTextBlock>(Widget)) TestTrue(TEXT("Shared text never falls below 14 pt"),Text->GetFont().Size>=14);
  for (const FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1920,1080)})
  {
   Renderer.DrawWidget(Slate,FVector2D(Size.X,Size.Y)); // settle AutoWrap's measured width before capture
   Slate->Invalidate(EInvalidateWidgetReason::Layout); Slate->SlatePrepass(1.f);
   UTextureRenderTarget2D* Target=Renderer.DrawWidget(Slate,FVector2D(Size.X,Size.Y));
   if (!TestNotNull(TEXT("Native panel renders"),Target)) { W->RemoveFromRoot(); return false; }
   TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
   TestTrue(TEXT("Panel pixels readable"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
   int32 VisiblePixels=0; for (const FColor& Pixel:Pixels) if (Pixel.A && int32(Pixel.R)+Pixel.G+Pixel.B>30) ++VisiblePixels;
   TestTrue(TEXT("Preview contains visible panel content"),VisiblePixels>50);
   TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);
   const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("UiPreview"),FString::FromInt(Size.Y)); IFileManager::Get().MakeDirectory(*Folder,true);
   const FString File=FPaths::Combine(Folder,Class->GetName()+TEXT(".png")); TestTrue(TEXT("Panel preview saved"),FFileHelper::SaveArrayToFile(Png,*File)); AddInfo(File);
  }
  W->RemoveFromRoot();
 }
 return true;
}
#endif
#endif
