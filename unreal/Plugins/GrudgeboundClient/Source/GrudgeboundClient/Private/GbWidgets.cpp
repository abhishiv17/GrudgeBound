#include "GbWidgets.h"

#include "GbApiSubsystem.h"
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

FSlateFontInfo GbUi::Font(int32 Size, bool bBold, bool bTitle)
{
	FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold || bTitle ? "Bold" : "Regular", Size);
	if (const TSharedPtr<const FCompositeFont> Family = UiFont(bBold || bTitle)) Info = FSlateFontInfo(Family, Size);
	Info.Size = Size;
	Info.TypefaceFontName = bTitle ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	return Info;
}
FSlateBrush GbUi::PanelBrush(const FLinearColor& Tint, bool bStuds) { return FrameBrush(bStuds ? 1 : 0, Tint); }
FSlateBrush GbUi::ButtonBrush(const FLinearColor& Tint) { return FrameBrush(2, Tint); }

// ================================================================ building panels
void UGbMenderWidget::BuildTree()
{
 UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
 UBorder* Card=MakePanel(C::Paper,FMargin(24)); UVerticalBox* Col=WidgetTree->ConstructWidget<UVerticalBox>();
 Card->SetContent(Sized(Col,600,400)); Centre(Root,Card);
 AddRow(Col,MakeText(TEXT("Mender's House"),28,C::Ink,true));
 Selection=WidgetTree->ConstructWidget<UComboBoxString>(); AddRow(Col,Sized(Selection,0,38),12,12);
 Selection->OnSelectionChanged.AddDynamic(this,&UGbMenderWidget::Select);
 UTextBlock* Label=nullptr; RestoreButton=MakeButton(TEXT("Restore ink"),Label,C::Gold); RestoreLabel=Label;
 RestoreButton->OnClicked.AddDynamic(this,&UGbMenderWidget::Restore); AddRow(Col,Sized(RestoreButton,0,44));
 Status=MakeText(TEXT("Reading your pockets..."),16,C::Ink); Status->SetWrapTextAt(590); AddRow(Col,Sized(Status,0,140));
 UButton* Back=MakeButton(TEXT("Return"),Label,C::Paper); Back->OnClicked.AddDynamic(this,&UGbMenderWidget::Close); AddRow(Col,Sized(Back,0,40));
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
 if (!Selection) return;
 const int32 Old=Selection->GetSelectedIndex(); const FString Selected=Tokens.IsValidIndex(Old)?Tokens[Old]:FString();
 bUpdating=true; Selection->ClearOptions(); Tokens.Reset();
 const FGbSatchel Data=Player()?Player()->GetSatchel():FGbSatchel();
 Tokens=Data.Carried; for (const FString& Id:Data.Case) Tokens.AddUnique(Id);
 for (const FString& Id:Tokens)
 {
  FString Name=TEXT("Sealed talisman");
  if (Player() && Api()) for (const FGbTalisman& T:Player()->GetTalismans()) if (UGbPlayerSubsystem::ServerTokenId(T)==Id) { Name=Api()->GetCurse(T.SpeciesId).Name; break; }
  const FGbInk* Ink=Data.Ink.Find(Id);
  Selection->AddOption(Ink?FString::Printf(TEXT("%s   %d / %d"),*Name,Ink->Hp,Ink->MaxHp):Name);
 }
 const int32 Index=Tokens.Contains(Selected)?Tokens.IndexOfByKey(Selected):0;
 Selection->SetSelectedIndex(Index); bUpdating=false;
 const FString Id=Tokens.IsValidIndex(Index)?Tokens[Index]:FString(); const FGbInk* Ink=Data.Ink.Find(Id);
 const int32 Price=Api()?UGbSatchelWidget::MenderPriceFrom(Api()->GetWorldState()):-1;
 RestoreLabel->SetText(FText::FromString(Price<0?FString(TEXT("Restore ink")):FString::Printf(TEXT("Restore ink (%d Ember)"),Price)));
 RestoreButton->SetIsEnabled(Data.bValid && Ink && Ink->Hp<Ink->MaxHp && Price>=0 && Data.Ember>=Price && Api() && !Api()->IsSatchelBusy());
}
void UGbMenderWidget::Select(FString,ESelectInfo::Type) { if (!bUpdating) Refresh(); }
void UGbMenderWidget::Restore()
{
 const int32 Index=Selection->GetSelectedIndex();
 if (Api() && RestoreButton->GetIsEnabled() && Tokens.IsValidIndex(Index)) { RestoreButton->SetIsEnabled(false); Api()->RestoreInk(Tokens[Index]); }
}
void UGbMenderWidget::Close() { OnClose.Broadcast(); }
void UGbMenderWidget::Action(const FString& Message) { if (IsVisible()) { Status->SetText(FText::FromString(Message)); Refresh(); } }
void UGbMenderWidget::Error(const FString& What,int32,const FString& Message)
{
 if (!IsVisible() || (What!=TEXT("mender/restore") && What!=TEXT("satchel"))) return;
 Status->SetText(FText::FromString(Message.Contains(TEXT("NEAR")) || Message.Contains(TEXT("chain"))?FString(TEXT("The town's ledger is unavailable. Refresh before trying again.")):Message)); Refresh();
}

void UGbRoomWidget::BuildTree()
{
 UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
 UBorder* Card=MakePanel(C::Paper,FMargin(24)); UVerticalBox* Col=WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Sized(Col,600,420)); Centre(Root,Card);
 AddRow(Col,MakeText(TEXT("Your room"),28,C::Ink,true)); AddRow(Col,MakeText(TEXT("Debt notice"),20,C::Ink,true));
 AddRow(Col,MakeText(TEXT("You arrived with a debt notice folded inside your coat."),16,C::Ink));
 UTextBlock* Label=nullptr; RestButton=MakeButton(TEXT("Rest until morning (free)"),Label,C::Gold); RestButton->OnClicked.AddDynamic(this,&UGbRoomWidget::Rest); AddRow(Col,Sized(RestButton,0,44),15,8);
 UButton* Chest=MakeButton(TEXT("Talisman Case"),Label,C::Paper); Chest->OnClicked.AddDynamic(this,&UGbRoomWidget::Case); AddRow(Col,Sized(Chest,0,40));
 Status=MakeText(TEXT(""),16,C::Ink); Status->SetWrapTextAt(590); AddRow(Col,Sized(Status,0,110));
 UButton* Back=MakeButton(TEXT("Return"),Label,C::Paper); Back->OnClicked.AddDynamic(this,&UGbRoomWidget::Close); AddRow(Col,Sized(Back,0,40));
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
	SignInButton = MakeButton(TEXT("Enter Lantern Ward"), Label, C::Crimson);
	FSlateFontInfo Big = Label->GetFont(); Big.Size = 21; Label->SetFont(Big);
	SignInButton->OnClicked.AddDynamic(this, &UGbSignInWidget::HandleSignIn); AddRow(Menu, Sized(SignInButton, 0.f, 60.f), 0.f, 8.f);
	UHorizontalBox* Pair = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* Help = MakeButton(TEXT("How to play"), Label, GbUi::Paper2()); Help->OnClicked.AddDynamic(this, &UGbSignInWidget::HandleHelp);
	UButton* Settings = MakeButton(TEXT("Settings"), Label, GbUi::Paper2()); Settings->OnClicked.AddDynamic(this, &UGbSignInWidget::HandleSettings);
	AddCol(Pair, Sized(Help, 0.f, 46.f), true); AddCol(Pair, Sized(Settings, 0.f, 46.f), true, 0.f);
	AddRow(Menu, Pair, 0.f, 6.f);
	HelpBox = WidgetTree->ConstructWidget<UVerticalBox>(); HelpBox->SetVisibility(ESlateVisibility::Collapsed); AddRow(Menu, HelpBox, 4.f, 4.f);
	StatusText = MakeText(TEXT(""), 14, GbUi::InkSoft()); StatusText->SetJustification(ETextJustify::Center); AddRow(Menu, StatusText, 8.f, 0.f);
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
	if (SignInButton) SignInButton->SetIsEnabled(!Auth->IsBusy());
}

/** The keys as they are bound right now, drawn as keycaps. */
void UGbSignInWidget::FillHelp()
{
	if (!HelpBox) return;
	HelpBox->ClearChildren();
	const UGbUserSettings* S = UGbUserSettings::Get();
	const auto Row = [this, S](std::initializer_list<const TCHAR*> Actions, FString Text)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		for (const TCHAR* Action : Actions)
		{
			UBorder* Cap = WidgetTree->ConstructWidget<UBorder>(); Cap->SetBrush(FrameBrush(2)); Cap->SetBrushColor(GbUi::PaperHi()); Cap->SetPadding(FMargin(9.f, 3.f, 9.f, 7.f));
			UTextBlock* Key = MakeText(S->KeyLabel(Action), 14, C::Ink, true); Key->SetAutoWrapText(false); Cap->SetContent(Key);
			AddCol(Line, Cap, false, 4.f);
			if (Text.IsEmpty()) for (const FGbActionDef& Def : UGbUserSettings::Actions()) if (Def.Id == Action) Text = Def.Label;
		}
		UTextBlock* What = MakeText(Text, 15, C::Ink); What->SetAutoWrapText(false);
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
		: FString(TEXT("Hollowmere is unreachable. Retrying..."))));
	ServerText->SetColorAndOpacity(FSlateColor(A->IsServerOnline() ? C::Leaf : C::Crimson));

	KessaText->SetText(FText::FromString(A->GetKessaLine()));
	const UGbStorySubsystem* Story = GetGameInstance()->GetSubsystem<UGbStorySubsystem>();
	ChapterText->SetText(FText::FromString(Story->GetChapterTitle()));
	ObjectiveText->SetText(FText::FromString(Story->GetObjective()));
}

void UGbHudWidget::HandleWorldState(const FGbWorldState&) { Refresh(); }
void UGbHudWidget::HandleServerStatus(bool, int32) { Refresh(); }
void UGbHudWidget::HandleKessaLine(const FString&) { Refresh(); }
void UGbHudWidget::HandlePlayerChanged() { Refresh(); }
void UGbHudWidget::HandleStoryChanged() { Refresh(); }

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
		: Talisman.BoundDay < 0 ? FString(TEXT("from the ledger")) : FString::Printf(TEXT("bound on day %d"), Talisman.BoundDay);
	const FString Purse = Talisman.Purse > 0 ? FString::Printf(TEXT(", purse %d Ember"), Talisman.Purse) : FString();
	const FString OnChain = Talisman.ChainTokenId.IsEmpty() ? FString() : FString::Printf(TEXT("  |  Ledger %s%s"), *Talisman.ChainTokenId, *Purse);
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
		P->IsOnChain() ? TEXT("  (Rite's ledger)") : TEXT("  (this PC only)"))));

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

	AddRow(Col, MakeText(TEXT("Your sealed talismans"), 14, C::Ink, true), 10.f);
	Yours = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddRow(Col, Sized(Yours, 0.f, 150.f));

	Status = MakeText(TEXT(""), 13, C::Ink);
	Status->SetAutoWrapText(true);
	AddRow(Col, Status, 8.f);

	UTextBlock* Ignored = nullptr;
	UButton* Close = MakeButton(TEXT("Back to Satchel  (B)"), Ignored, C::Stone);
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
		const FString Detail = FString::Printf(TEXT("%d Ember  |  %d in stock  |  %s"), It.Price, It.Stock, *It.Text);
		const FString Emotion = bTalisman ? It.Id.Mid(9) : FString();
		Row->Setup(It.Name, bTalisman || bSupply ? Detail : Detail + TEXT("  (not sold to players yet)"),
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
			Row->Setup(FString::Printf(TEXT("%s  (ledger %s)"), *Curse.Name, *T.ChainTokenId),
				bPartner ? FString(TEXT("Your partner: choose another partner in the Satchel (B) before selling it."))
					: Offer > 0 ? FString::Printf(TEXT("The Broker offers %d Ember"), Offer) : FString(TEXT("The Broker doesn't buy these")),
				FString::Printf(TEXT("Sell  %d"), Offer), !bBusy && !A->IsSatchelBusy() && !bPartner && Offer > 0 && !Data.bClosed, EmotionColor(Curse.Emotion));
			const FString TokenId = T.ChainTokenId;
			Row->OnClick = [this, TokenId]() { Sell(TokenId); };
			Yours->AddChild(Row);
			++Shown;
		}
	}
	if (Shown == 0) Yours->AddChild(MakeText(TEXT("None yet: talismans sealed in the ledger show up here."), 13, Dim()));
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

	UTextBlock* Ignored = nullptr;
	auto Arrow = [this, &Ignored](const TCHAR* Label) { return MakeButton(Label, Ignored, C::Stone); };

	// Repeal row: Strike  < #3 >  [Repeal (100 pts)]
	UHorizontalBox* Strike = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* PrevR = Arrow(TEXT("<"));
	UButton* NextR = Arrow(TEXT(">"));
	PrevR->OnClicked.AddDynamic(this, &UGbRulesWidget::HandlePrevRepeal);
	NextR->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleNextRepeal);
	AddCol(Strike, Sized(MakeText(TEXT("Strike"), 13, Dim()), 60.f, 0.f), false);
	AddCol(Strike, Sized(PrevR, 36.f, 32.f), false, 4.f);
	RepealPick = MakeText(TEXT("-"), 14, C::Ink, true);
	AddCol(Strike, Sized(RepealPick, 60.f, 0.f), false, 4.f);
	AddCol(Strike, Sized(NextR, 36.f, 32.f), false, 8.f);
	UTextBlock* RepealText = nullptr;
	RepealButton = MakeButton(TEXT("Repeal"), RepealText, C::Stone);
	RepealLabel = RepealText;
	RepealButton->OnClicked.AddDynamic(this, &UGbRulesWidget::HandleRepeal);
	AddCol(Strike, Sized(RepealButton, 200.f, 32.f), false);
	RepealRow = Strike;
	AddRow(Col, Strike, 4.f);

	AddRow(Col, MakeText(TEXT("Propose a rule"), 14, C::Ink, true), 10.f);

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
