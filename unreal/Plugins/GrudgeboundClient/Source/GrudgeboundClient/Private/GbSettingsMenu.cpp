#include "GbSettingsMenu.h"

#include "GbAuthSubsystem.h"
#include "GbGreyboxMaterials.h"
#include "GbUiDirector.h"
#include "GbUiStyle.h"
#include "GbUserSettings.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

extern ENGINE_API float GAverageFPS;

namespace
{
	namespace C = GbGreybox::Color;

	FLinearColor Alpha(FLinearColor Color, float A) { Color.A = A; return Color; }
	FLinearColor Faint() { return Alpha(C::Ink, 0.16f); }
	FLinearColor Soft() { return GbUi::InkSoft(); }

	const TCHAR* TabNames[] = { TEXT("Game"), TEXT("Controls"), TEXT("Camera"), TEXT("Display"), TEXT("Sound") };
	const TCHAR* TabNotes[] = {
		TEXT("Hollowmere keeps turning while this menu is open."),
		TEXT("Click a key, then press the new one. A key already in use swaps places."),
		TEXT("How the camera follows you through the Ward."),
		TEXT("Window, resolution and picture quality."),
		TEXT("Volume."),
	};
	const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };

	class FGbSettingsInput : public IInputProcessor
	{
	public:
		explicit FGbSettingsInput(AGbSettingsHost* InHost) : Host(InHost) {}
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			return Host.IsValid() && !Event.IsRepeat() && Host->HandleKey(Event.GetKey());
		}
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
		{
			return Host.IsValid() && Host->HandleMouseKey(Event.GetEffectingButton());
		}
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent& Event, const FPointerEvent*) override
		{
			return Host.IsValid() && Event.GetWheelDelta() != 0.f
				&& Host->HandleMouseKey(Event.GetWheelDelta() > 0.f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
		}
		virtual const TCHAR* GetDebugName() const override { return TEXT("GbSettings"); }
	private:
		TWeakObjectPtr<AGbSettingsHost> Host;
	};
}

// ================================================================ the menu

class SGbSettingsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGbSettingsMenu) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, AGbSettingsHost* InHost)
	{
		Host = InHost;
		// The web client's look: cream paper in a notched ink frame, crimson for what matters, paper for the rest.
		PanelBrush = GbUi::PanelBrush(C::Paper, true);
		RailBrush = GbUi::PanelBrush(GbUi::Paper2());
		const auto MakeStyle = [](const FLinearColor& Face)
		{
			const auto Shade = [&Face](float K) { return FLinearColor(Face.R * K, Face.G * K, Face.B * K, 1.f); };
			FButtonStyle Style;
			Style.SetNormal(GbUi::ButtonBrush(Face));
			Style.SetHovered(GbUi::ButtonBrush(Shade(1.1f)));
			Style.SetPressed(GbUi::ButtonBrush(Shade(0.84f)));
			Style.SetDisabled(GbUi::ButtonBrush(Alpha(Face, 0.4f)));
			Style.SetNormalPadding(FMargin(14.f, 7.f, 14.f, 10.f));
			Style.SetPressedPadding(FMargin(14.f, 9.f, 14.f, 8.f));
			return Style;
		};
		GhostStyle = MakeStyle(GbUi::Paper2());
		TabOnStyle = MakeStyle(C::Crimson);
		GoldStyle = MakeStyle(C::Gold);
		CrimsonStyle = MakeStyle(C::Crimson);
		TabStyle.SetNormal(FSlateRoundedBoxBrush(Alpha(C::Ink, 0.f), 0.f));
		TabStyle.SetHovered(FSlateRoundedBoxBrush(Alpha(C::Ink, 0.10f), 0.f));
		TabStyle.SetPressed(FSlateRoundedBoxBrush(Alpha(C::Ink, 0.16f), 0.f));
		TabStyle.SetNormalPadding(FMargin(14.f, 7.f, 14.f, 10.f));
		TabStyle.SetPressedPadding(FMargin(14.f, 9.f, 14.f, 8.f));

		TSharedRef<SVerticalBox> Tabs = SNew(SVerticalBox);
		TabButtons.SetNum(UE_ARRAY_COUNT(TabNames));
		for (int32 I = 0; I < UE_ARRAY_COUNT(TabNames); ++I)
		{
			Tabs->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				SAssignNew(TabButtons[I], SButton).ButtonStyle(&TabStyle)
				.OnClicked_Lambda([this, I]() { ShowTab(I); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(FText::FromString(TabNames[I])).Font(GbUi::Font(19, true))
					.ColorAndOpacity_Lambda([this, I]() { return FSlateColor(Tab == I ? GbUi::PaperHi() : C::Ink); })
				]
			];
		}

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.01f, 0.66f))
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(1080.f).HeightOverride(680.f)
				[
					SNew(SBorder).BorderImage(&PanelBrush).Padding(18.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(250.f)
							[
								SNew(SBorder).BorderImage(&RailBrush).Padding(FMargin(18.f, 20.f, 18.f, 16.f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("GRUDGEBOUND"))).Font(GbUi::Font(14, true)).ColorAndOpacity(FSlateColor(C::Crimson))
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 22.f)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("Menu"))).Font(GbUi::Font(38, true, true)).ColorAndOpacity(FSlateColor(C::Ink))
									]
									+ SVerticalBox::Slot().AutoHeight()[ Tabs ]
									+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SSpacer) ]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(GbUi::Font(14)).ColorAndOpacity(FSlateColor(Soft())).AutoWrapText(true)
										.Text_Lambda([]() { return FText::FromString(UGbUserSettings::Get()->KeyLabel(TEXT("Settings")) + TEXT(" or F1 closes the menu")); })
									]
								]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(26.f, 8.f, 10.f, 4.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(GbUi::Font(28, true, true)).ColorAndOpacity(FSlateColor(C::Ink))
								.Text_Lambda([this]() { return FText::FromString(TabNames[Tab]); })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 12.f)
							[
								SNew(STextBlock).Font(GbUi::Font(15)).ColorAndOpacity(FSlateColor(Soft())).AutoWrapText(true)
								.Text_Lambda([this]() { return FText::FromString(TabNotes[Tab]); })
							]
							+ SVerticalBox::Slot().AutoHeight()[ Line() ]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 6.f)[ SAssignNew(Body, SBox) ]
							+ SVerticalBox::Slot().AutoHeight()[ Line() ]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[ SAssignNew(Footer, SBox) ]
						]
					]
				]
			]
		];
		ShowTab(0);
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	void ShowTab(int32 InTab)
	{
		Tab = FMath::Clamp(InTab, 0, static_cast<int32>(UE_ARRAY_COUNT(TabNames)) - 1);
		Listening = NAME_None; Note.Reset(); bQuitArmed = false;
		for (int32 I = 0; I < TabButtons.Num(); ++I) if (TabButtons[I]) TabButtons[I]->SetButtonStyle(I == Tab ? &TabOnStyle : &TabStyle);
		TSharedRef<SWidget> Content = Tab == 0 ? BuildGame() : Tab == 1 ? BuildControls() : Tab == 2 ? BuildCamera() : Tab == 3 ? BuildDisplay() : BuildSound();
		Body->SetContent(Content);

		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		if (Tab != 0)
		{
			Row->AddSlot().AutoWidth()[ Button(Tab == 3 ? TEXT("Auto-detect quality") : TEXT("Restore defaults"), &GhostStyle, C::Ink, [this]() { RestoreDefaults(); }) ];
		}
		Row->AddSlot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f)
		[
			SNew(STextBlock).Font(GbUi::Font(14)).ColorAndOpacity(FSlateColor(C::Crimson)).AutoWrapText(true)
			.Text_Lambda([this]() { return FText::FromString(Listening != NAME_None ? FString(TEXT("Press a key or mouse button.  Esc cancels.")) : Note); })
		];
		if (Tab == 3)
		{
			Row->AddSlot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this]() { return bPendingDisplay ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					Button(TEXT("Apply"), &GoldStyle, C::Ink, [this]() { ApplyDisplay(); })
				]
			];
		}
		Row->AddSlot().AutoWidth()[ Button(TEXT("Back to the Ward"), &GhostStyle, C::Ink, [this]() { if (Host.IsValid()) Host->Close(); }) ];
		Footer->SetContent(Row);
	}

	bool IsListening() const { return Listening != NAME_None; }

	/** A key or mouse button arrived while a binding is waiting for one. */
	void Capture(const FKey& Key)
	{
		const FName Action = Listening;
		Listening = NAME_None;
		if (Key == EKeys::Escape || Key == EKeys::LeftMouseButton) { Note = TEXT("Binding unchanged."); return; }
		if (Key == EKeys::F1) { Note = TEXT("F1 always opens this menu; pick another key."); return; }
		if (Action == TEXT("Settings") && Key.IsMouseButton()) { Note = TEXT("The menu needs a keyboard key."); return; }
		const bool bHeld = Action.ToString().StartsWith(TEXT("Move")) || Action == TEXT("RotateLeft") || Action == TEXT("RotateRight") || Action == TEXT("RotateHold");
		if (bHeld && (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)) { Note = TEXT("The wheel cannot be held; pick a key or button."); return; }
		UGbUserSettings* S = UGbUserSettings::Get();
		const FKey Old = S->KeyFor(Action);
		const FName Swapped = S->SetKey(Action, Key);
		Note = Swapped == NAME_None
			? FString::Printf(TEXT("%s is now %s."), *LabelOf(Action), *UGbUserSettings::KeyName(Key))
			: FString::Printf(TEXT("%s is now %s.  %s moved to %s."), *LabelOf(Action), *UGbUserSettings::KeyName(Key), *LabelOf(Swapped), *UGbUserSettings::KeyName(Old));
	}

	void CancelListening() { Listening = NAME_None; }

	/** Leaving with an unapplied window mode or resolution puts the selectors back. */
	void DropPendingDisplay()
	{
		if (bPendingDisplay && GEngine && GEngine->GetGameUserSettings()) GEngine->GetGameUserSettings()->ResetToCurrentSettings();
		bPendingDisplay = false;
	}

private:
	static FString LabelOf(FName Action)
	{
		for (const FGbActionDef& Def : UGbUserSettings::Actions()) if (Def.Id == Action) return Def.Label;
		return Action.ToString();
	}

	TSharedRef<SWidget> Line() const
	{
		return SNew(SBox).HeightOverride(1.f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Faint()) ];
	}

	TSharedRef<SWidget> Button(const FString& Label, const FButtonStyle* Style, const FLinearColor& Text, TFunction<void()> OnClick, int32 Size = 16) const
	{
		return SNew(SButton).ButtonStyle(Style).HAlign(HAlign_Center).VAlign(VAlign_Center)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(GbUi::Font(Size, true)).ColorAndOpacity(FSlateColor(Text))
			];
	}

	TSharedRef<SWidget> Heading(const FString& Text) const
	{
		return SNew(SBox).Padding(FMargin(0.f, 14.f, 0.f, 4.f))
		[
			SNew(STextBlock).Text(FText::FromString(Text.ToUpper())).Font(GbUi::Font(14, true)).ColorAndOpacity(FSlateColor(C::Crimson))
		];
	}

	/** Label on the left, control on the right, a faint rule under it. */
	TSharedRef<SWidget> Row(const FString& Label, TSharedRef<SWidget> Control, const FString& Help = FString()) const
	{
		TSharedRef<SVerticalBox> Text = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(Label)).Font(GbUi::Font(17)).ColorAndOpacity(FSlateColor(C::Ink)) ];
		if (!Help.IsEmpty())
		{
			Text->AddSlot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(Help)).Font(GbUi::Font(13)).ColorAndOpacity(FSlateColor(Soft())).AutoWrapText(true) ];
		}
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Text ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(330.f)[ Control ] ]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(1.f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Alpha(C::Ink, 0.05f)) ]
			];
	}

	/** "<  value  >": Step gets -1 or +1. */
	TSharedRef<SWidget> Stepper(TFunction<FString()> Value, TFunction<void(int32)> Step) const
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(44.f)[ Button(TEXT("<"), &GhostStyle, C::Ink, [Step]() { Step(-1); }) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(GbUi::Font(16, true)).ColorAndOpacity(FSlateColor(C::Ink))
				.Text_Lambda([Value]() { return FText::FromString(Value()); })
			]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(44.f)[ Button(TEXT(">"), &GhostStyle, C::Ink, [Step]() { Step(1); }) ] ];
	}

	TSharedRef<SWidget> Toggle(TFunction<bool()> Get, TFunction<void(bool)> Set) const
	{
		return Stepper([Get]() { return FString(Get() ? TEXT("On") : TEXT("Off")); }, [Get, Set](int32) { Set(!Get()); });
	}

	TSharedRef<SWidget> Slider(float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FString(float)> Format, TFunction<void()> Commit) const
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider).MinValue(Min).MaxValue(Max)
				.SliderBarColor(Alpha(C::Ink, 0.45f)).SliderHandleColor(C::Crimson)
				.Value_Lambda([Get]() { return Get(); })
				.OnValueChanged_Lambda([Set](float V) { Set(V); })
				.OnMouseCaptureEnd_Lambda([Commit]() { Commit(); })
				.OnControllerCaptureEnd_Lambda([Commit]() { Commit(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(64.f).HAlign(HAlign_Right)
				[
					SNew(STextBlock).Font(GbUi::Font(15, true)).ColorAndOpacity(FSlateColor(C::Ink))
					.Text_Lambda([Get, Format]() { return FText::FromString(Format(Get())); })
				]
			];
	}

	TSharedRef<SWidget> Scroll(TSharedRef<SVerticalBox> Rows) const
	{
		return SNew(SScrollBox).ScrollBarThickness(FVector2D(6.f, 6.f)) + SScrollBox::Slot().Padding(0.f, 0.f, 14.f, 0.f)[ Rows ];
	}

	// ---------------------------------------------------------------- tabs

	TSharedRef<SWidget> BuildGame()
	{
		const auto Big = [this](const FString& Label, const FButtonStyle* Style, const FLinearColor& Text, TFunction<void()> OnClick)
		{
			return SNew(SBox).WidthOverride(360.f).HeightOverride(52.f)[ Button(Label, Style, Text, MoveTemp(OnClick), 19) ];
		};
		TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
		Col->AddSlot().AutoHeight().Padding(0.f, 26.f, 0.f, 8.f)[ Big(TEXT("Back to the Ward"), &CrimsonStyle, GbUi::PaperHi(), [this]() { if (Host.IsValid()) Host->Close(); }) ];
		Col->AddSlot().AutoHeight().Padding(0.f, 8.f)[ Big(TEXT("Controls"), &GhostStyle, C::Ink, [this]() { ShowTab(1); }) ];
		Col->AddSlot().AutoHeight().Padding(0.f, 8.f)[ Big(TEXT("Sign out"), &GhostStyle, C::Ink, [this]() { SignOut(); }) ];
		Col->AddSlot().AutoHeight().Padding(0.f, 8.f)
		[
			SNew(SBox).WidthOverride(360.f).HeightOverride(52.f)
			[
				SNew(SButton).ButtonStyle(&GhostStyle).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.OnClicked_Lambda([this]() { Quit(); return FReply::Handled(); })
				[
					SNew(STextBlock).Font(GbUi::Font(19, true))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(bQuitArmed ? C::Crimson : C::Ink); })
					.Text_Lambda([this]() { return FText::FromString(bQuitArmed ? TEXT("Click again to quit") : TEXT("Quit to desktop")); })
				]
			]
		];
		return SNew(SBox).HAlign(HAlign_Left)[ Col ];
	}

	TSharedRef<SWidget> BuildControls()
	{
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		FString Group;
		for (const FGbActionDef& Def : UGbUserSettings::Actions())
		{
			if (Def.Group != Group) { Group = Def.Group; Rows->AddSlot().AutoHeight()[ Heading(Group) ]; }
			const FName Id = Def.Id;
			Rows->AddSlot().AutoHeight()
			[
				Row(Def.Label,
					SNew(SButton).ButtonStyle(&GhostStyle).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.OnClicked_Lambda([this, Id]() { Listening = Id; Note.Reset(); return FReply::Handled(); })
					[
						SNew(STextBlock).Font(GbUi::Font(16, true))
						.ColorAndOpacity_Lambda([this, Id]() { return FSlateColor(Listening == Id ? C::Crimson : C::Ink); })
						.Text_Lambda([this, Id]() { return FText::FromString(Listening == Id ? FString(TEXT("Press a key...")) : UGbUserSettings::Get()->KeyLabel(Id)); })
					])
			];
		}
		Rows->AddSlot().AutoHeight()[ Heading(TEXT("Mouse")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Walk to a spot"), FixedKey(TEXT("Left Mouse"))) ];
		return Scroll(Rows);
	}

	TSharedRef<SWidget> FixedKey(const FString& Text) const
	{
		return SNew(SBox).HAlign(HAlign_Center).Padding(FMargin(0.f, 7.f))
		[
			SNew(STextBlock).Text(FText::FromString(Text)).Font(GbUi::Font(16, true)).ColorAndOpacity(FSlateColor(Soft()))
		];
	}

	TSharedRef<SWidget> BuildCamera()
	{
		UGbUserSettings* S = UGbUserSettings::Get();
		const auto Commit = [S]() { S->Save(); };
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Distance"), Slider(700.f, UGbUserSettings::MaxDistance, [S]() { return S->CameraDistance; }, [S](float V) { S->CameraDistance = V; },
			[](float V) { return FString::Printf(TEXT("%.0f m"), V / 100.f); }, Commit), TEXT("Also on the mouse wheel.")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Angle"), Slider(UGbUserSettings::MinPitch, UGbUserSettings::MaxPitch, [S]() { return S->CameraPitch; }, [S](float V) { S->CameraPitch = V; },
			[](float V) { return FString::Printf(TEXT("%.0f deg"), V); }, Commit), TEXT("Drag vertically with the orbit key held. Rooms keep a higher angle to see over walls.")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Rotate speed"), Slider(40.f, 240.f, [S]() { return S->CameraRotateSpeed; }, [S](float V) { S->CameraRotateSpeed = V; },
			[](float V) { return FString::Printf(TEXT("%.0f"), V); }, Commit)) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Mouse rotate sensitivity"), Slider(0.2f, 3.f, [S]() { return S->MouseRotateSensitivity; }, [S](float V) { S->MouseRotateSensitivity = V; },
			[](float V) { return FString::Printf(TEXT("%.1fx"), V); }, Commit)) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Invert mouse orbit / tilt"), Toggle([S]() { return S->bInvertMouseRotate; }, [S](bool B) { S->bInvertMouseRotate = B; S->Save(); })) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Smooth follow"), Toggle([S]() { return S->bSmoothCamera; }, [S](bool B) { S->bSmoothCamera = B; S->Save(); }),
			TEXT("The camera eases after you instead of locking to you.")) ];
		return Scroll(Rows);
	}

	TSharedRef<SWidget> BuildDisplay()
	{
		UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (!G) return SNew(STextBlock).Text(FText::FromString(TEXT("Display settings are unavailable.")));
		UGbUserSettings* S = UGbUserSettings::Get();

		Resolutions.Reset();
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		if (Resolutions.Num() == 0) Resolutions = { FIntPoint(1280, 720), FIntPoint(1366, 768), FIntPoint(1600, 900), FIntPoint(1920, 1080) };
		Resolutions.AddUnique(G->GetScreenResolution());
		Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });

		const auto Now = [G]() { G->ApplyNonResolutionSettings(); G->SaveSettings(); };
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		Rows->AddSlot().AutoHeight()[ Heading(TEXT("Window")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Window mode"), Stepper(
			[G]() { const EWindowMode::Type M = G->GetFullscreenMode(); return FString(M == EWindowMode::Fullscreen ? TEXT("Fullscreen") : M == EWindowMode::WindowedFullscreen ? TEXT("Borderless") : TEXT("Windowed")); },
			[this, G](int32 Dir)
			{
				const EWindowMode::Type Order[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
				int32 I = 0; for (int32 K = 0; K < 3; ++K) if (Order[K] == G->GetFullscreenMode()) I = K;
				G->SetFullscreenMode(Order[(I + Dir + 3) % 3]); bPendingDisplay = true;
			})) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Resolution"), Stepper(
			[G]() { const FIntPoint R = G->GetScreenResolution(); return FString::Printf(TEXT("%d x %d"), R.X, R.Y); },
			[this, G](int32 Dir)
			{
				const int32 I = FMath::Max(0, Resolutions.IndexOfByKey(G->GetScreenResolution()));
				G->SetScreenResolution(Resolutions[FMath::Clamp(I + Dir, 0, Resolutions.Num() - 1)]); bPendingDisplay = true;
			}), TEXT("Window mode and resolution change when you press Apply.")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("V-Sync"), Toggle([G]() { return G->IsVSyncEnabled(); }, [G, Now](bool B) { G->SetVSyncEnabled(B); Now(); })) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Frame rate limit"), Stepper(
			[G]() { const int32 L = FMath::RoundToInt(G->GetFrameRateLimit()); return L <= 0 ? FString(TEXT("Unlimited")) : FString::Printf(TEXT("%d fps"), L); },
			[G, Now](int32 Dir)
			{
				const int32 Limits[] = { 30, 45, 60, 90, 120, 144, 0 };
				const int32 L = FMath::RoundToInt(G->GetFrameRateLimit());
				int32 I = 6; for (int32 K = 0; K < 7; ++K) if (Limits[K] == L) I = K;
				G->SetFrameRateLimit(Limits[FMath::Clamp(I + Dir, 0, 6)]); Now();
			})) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Show frame rate"), Toggle([S]() { return S->bShowFps; }, [S](bool B) { S->bShowFps = B; S->Save(); })) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Reduce battle motion"), Toggle([S]() { return S->bReducedBattleMotion; }, [S](bool B) { S->bReducedBattleMotion = B; S->Save(); })) ];

		Rows->AddSlot().AutoHeight()[ Heading(TEXT("Quality")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Preset"), Stepper(
			[G]() { const int32 L = G->GetOverallScalabilityLevel(); return FString(L < 0 ? TEXT("Custom") : QualityNames[FMath::Clamp(L, 0, 3)]); },
			[G, Now](int32 Dir) { const int32 L = G->GetOverallScalabilityLevel(); G->SetOverallScalabilityLevel(FMath::Clamp((L < 0 ? 2 : L) + Dir, 0, 3)); Now(); }),
			TEXT("Sets every line below at once.")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Render scale"), Slider(50.f, 100.f,
			[G]() { float N, V, Min, Max; G->GetResolutionScaleInformationEx(N, V, Min, Max); return V; },
			[G](float V) { G->SetResolutionScaleValueEx(FMath::RoundToFloat(V)); },
			[](float V) { return FString::Printf(TEXT("%.0f%%"), V); }, Now), TEXT("Lower it for more frames on a small graphics card.")) ];

		struct FQuality { const TCHAR* Label; int32 (UGameUserSettings::*Get)() const; void (UGameUserSettings::*Set)(int32); };
		const FQuality Qualities[] = {
			{ TEXT("Shadows"), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
			{ TEXT("Lighting"), &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
			{ TEXT("Reflections"), &UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality },
			{ TEXT("Textures"), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
			{ TEXT("Effects"), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
			{ TEXT("Post-processing"), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
			{ TEXT("Anti-aliasing"), &UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality },
			{ TEXT("View distance"), &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality },
		};
		for (const FQuality& Q : Qualities)
		{
			const auto Getter = Q.Get; const auto Setter = Q.Set;
			Rows->AddSlot().AutoHeight()[ Row(Q.Label, Stepper(
				[G, Getter]() { return FString(QualityNames[FMath::Clamp((G->*Getter)(), 0, 3)]); },
				[G, Getter, Setter, Now](int32 Dir) { (G->*Setter)(FMath::Clamp((G->*Getter)() + Dir, 0, 3)); Now(); })) ];
		}
		return Scroll(Rows);
	}

	TSharedRef<SWidget> BuildSound()
	{
		UGbUserSettings* S = UGbUserSettings::Get();
		TWeakObjectPtr<AGbSettingsHost> H = Host;
		const auto Apply = [S, H]() { if (H.IsValid()) S->ApplySound(H->GetWorld()); };
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Master volume"), Slider(0.f, 1.f, [S]() { return S->MasterVolume; }, [S, Apply](float V) { S->MasterVolume = V; Apply(); },
			[](float V) { return FString::Printf(TEXT("%.0f%%"), V * 100.f); }, [S]() { S->Save(); })) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Market and music"), Slider(0.f, 1.f, [S]() { return S->AmbienceVolume; }, [S](float V) { S->AmbienceVolume = V; },
			[](float V) { return FString::Printf(TEXT("%.0f%%"), V * 100.f); }, [S]() { S->Save(); }), TEXT("The night market you hear in the Wards.")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Effects"), Slider(0.f, 1.f, [S]() { return S->EffectsVolume; }, [S](float V) { S->EffectsVolume = V; },
			[](float V) { return FString::Printf(TEXT("%.0f%%"), V * 100.f); }, [S]() { S->Save(); }), TEXT("Footsteps, doors, panels and battles.")) ];
		Rows->AddSlot().AutoHeight()[ Row(TEXT("Mute in the background"), Toggle([S]() { return S->bMuteInBackground; }, [S, Apply](bool B) { S->bMuteInBackground = B; Apply(); S->Save(); }),
			TEXT("Silence the game while another window is in front.")) ];
		return Scroll(Rows);
	}

	// ---------------------------------------------------------------- actions

	void RestoreDefaults()
	{
		UGbUserSettings* S = UGbUserSettings::Get();
		if (Tab == 1) { S->ResetControls(); Note = TEXT("Default controls restored."); }
		else if (Tab == 2) { S->ResetCamera(); Note = TEXT("Default camera restored."); }
		else if (Tab == 4) { S->ResetSound(); if (Host.IsValid()) S->ApplySound(Host->GetWorld()); Note = TEXT("Default sound restored."); }
		else if (Tab == 3)
		{
			if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
			{
				G->RunHardwareBenchmark(); G->ApplyHardwareBenchmarkResults();
				Note = TEXT("Quality set for this machine.");
			}
		}
		Listening = NAME_None;
	}

	void ApplyDisplay()
	{
		if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr) G->ApplySettings(false);
		bPendingDisplay = false; Note = TEXT("Display settings applied.");
	}

	void SignOut()
	{
		if (!Host.IsValid()) return;
		UGameInstance* GI = Host->GetGameInstance();
		Host->Close();
		if (UGbAuthSubsystem* Auth = GI ? GI->GetSubsystem<UGbAuthSubsystem>() : nullptr) Auth->SignOut();
	}

	void Quit()
	{
		if (!bQuitArmed) { bQuitArmed = true; return; }
		if (!Host.IsValid()) return;
		UKismetSystemLibrary::QuitGame(Host->GetWorld(), Host->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	}

	TWeakObjectPtr<AGbSettingsHost> Host;
	int32 Tab = 0;
	FName Listening;
	FString Note;
	bool bQuitArmed = false;
	bool bPendingDisplay = false;
	TArray<FIntPoint> Resolutions;
	TArray<TSharedPtr<SButton>> TabButtons;
	TSharedPtr<SBox> Body;
	TSharedPtr<SBox> Footer;
	FSlateBrush PanelBrush, RailBrush;
	FButtonStyle GhostStyle, TabStyle, TabOnStyle, GoldStyle, CrimsonStyle;
};

// ================================================================ the host

AGbSettingsHost::AGbSettingsHost()
{
	PrimaryActorTick.bCanEverTick = false;
}

AGbSettingsHost* AGbSettingsHost::Find(const UWorld* World)
{
	if (World) for (TActorIterator<AGbSettingsHost> It(World); It; ++It) return *It;
	return nullptr;
}

void AGbSettingsHost::BeginPlay()
{
	Super::BeginPlay();
	UGbUserSettings::Get()->ApplySound(GetWorld());
	if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender() || GetNetMode() == NM_DedicatedServer) return;

	Input = MakeShared<FGbSettingsInput>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(Input);

	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		FpsRoot = SNew(SBox).Visibility(EVisibility::HitTestInvisible) // already inside the engine's DPI scaler
		[
			SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(26.f, 0.f, 0.f, 20.f))
			[
				SNew(STextBlock).Font(GbUi::Font(15, true)).ColorAndOpacity(FSlateColor(GbGreybox::Color::Lantern))
				.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				.Visibility_Lambda([]() { return UGbUserSettings::Get()->bShowFps ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%.0f fps"), GAverageFPS)); })
			]
		];
		Viewport->AddViewportWidgetContent(FpsRoot.ToSharedRef(), 400);
	}
}

void AGbSettingsHost::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bOpen) Close();
	if (Input && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Input);
	Input.Reset();
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		if (FpsRoot) Viewport->RemoveViewportWidgetContent(FpsRoot.ToSharedRef());
	}
	FpsRoot.Reset(); MenuRoot.Reset(); Menu.Reset();
	Super::EndPlay(Reason);
}

bool AGbSettingsHost::IsViewportActive() const
{
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	return Window.IsValid() && FSlateApplication::Get().GetActiveTopLevelWindow() == Window;
}

void AGbSettingsHost::Open(int32 Tab)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (bOpen || !Viewport || !PC || !PC->IsLocalController()) return;

	if (!Menu)
	{
		MenuRoot = SAssignNew(Menu, SGbSettingsMenu, this);
	}
	Menu->ShowTab(Tab);
	Viewport->AddViewportWidgetContent(MenuRoot.ToSharedRef(), 500);
	bOpen = true;

	PC->StopMovement();
	PC->SetIgnoreMoveInput(true); bAddedIgnore = true;
	PC->SetShowMouseCursor(true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu);
	PC->SetInputMode(Mode);
}

void AGbSettingsHost::Close()
{
	if (!bOpen) return;
	bOpen = false;
	if (Menu) { Menu->CancelListening(); Menu->DropPendingDisplay(); }
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		if (MenuRoot) Viewport->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
	}
	UGbUserSettings::Get()->Save();

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;
	if (bAddedIgnore) { PC->SetIgnoreMoveInput(false); bAddedIgnore = false; }
	AGbUiDirector* Director = nullptr;
	for (TActorIterator<AGbUiDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
	if (Director) Director->RestoreInput();
	else
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
}

bool AGbSettingsHost::HandleKey(const FKey& Key)
{
	if (!IsViewportActive()) return false;
	const FKey MenuKey = UGbUserSettings::Get()->KeyFor(TEXT("Settings"));
	if (bOpen)
	{
		if (Menu && Menu->IsListening()) { Menu->Capture(Key); return true; }
		if (Key == EKeys::Escape || Key == EKeys::F1 || Key == MenuKey) { Close(); return true; }
		return false;
	}
	if (Key == MenuKey && !bKeyBusy)
	{
		// Esc first closes whatever panel is open, like any other game; on the bare Ward it opens the menu.
		for (TActorIterator<AGbUiDirector> It(GetWorld()); It; ++It) if (It->CloseTopPanel()) return true;
		Open(0);
		return true;
	}
	if (Key == EKeys::F1) { Open(0); return true; }
	return false;
}

bool AGbSettingsHost::HandleMouseKey(const FKey& Key)
{
	if (!bOpen || !Menu || !Menu->IsListening() || !IsViewportActive()) return false;
	Menu->Capture(Key);
	return true;
}
