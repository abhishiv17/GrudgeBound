// Phase 1 screens, built in C++ (no widget assets needed): HUD, encounter dialog, battle screen, Talisman Case.
// Plain paper-and-ink panels; the art pass can replace each with a Blueprint widget (see bCreateDefaultUi).
// Behaviour follows the web client: client/components/hud/*, client/game/scenes/BattleScene.ts, TalismanCase.tsx.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GbTypes.h"
#include "GbPlayerSubsystem.h"
#include "GbWidgets.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UGbApiSubsystem;
class UGbPlayerSubsystem;

/** Shared helpers: builds the widget tree in code when the widget is created. */
UCLASS(Abstract)
class GRUDGEBOUNDCLIENT_API UGbWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	/** Create the widgets and set WidgetTree->RootWidget. */
	virtual void BuildTree() {}

	UGbApiSubsystem* Api() const;
	UGbPlayerSubsystem* Player() const;

	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false);
	UBorder* MakePanel(const FLinearColor& Background, const FMargin& Padding);
	UButton* MakeButton(const FString& Label, UTextBlock*& OutLabel, const FLinearColor& Tint);
	UProgressBar* MakeBar(const FLinearColor& Fill);
	/** Wraps a widget in a size box of the given size (0 = leave that axis free). */
	UWidget* Sized(UWidget* Content, float Width, float Height);
};

/** Ward card (top left), Kessa's banner (bottom), key hint (top right). */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbHudWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	void Refresh();

protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandleWorldState(const FGbWorldState& State);
	UFUNCTION() void HandleServerStatus(bool bOnline, int32 Tick);
	UFUNCTION() void HandleKessaLine(const FString& Line);
	UFUNCTION() void HandlePlayerChanged();

	UPROPERTY() TObjectPtr<UTextBlock> WardTitle;
	UPROPERTY() TObjectPtr<UTextBlock> GrudgeTotal;
	UPROPERTY() TObjectPtr<UTextBlock> RageText;
	UPROPERTY() TObjectPtr<UTextBlock> SorrowText;
	UPROPERTY() TObjectPtr<UTextBlock> DreadText;
	UPROPERTY() TObjectPtr<UProgressBar> RageBar;
	UPROPERTY() TObjectPtr<UProgressBar> SorrowBar;
	UPROPERTY() TObjectPtr<UProgressBar> DreadBar;
	UPROPERTY() TObjectPtr<UTextBlock> Stats;
	UPROPERTY() TObjectPtr<UTextBlock> PartnerText;
	UPROPERTY() TObjectPtr<UTextBlock> ServerText;
	UPROPERTY() TObjectPtr<UTextBlock> KessaText;
};

/** "A curse stirs": Fight / Slip away. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbEncounterWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	void Show(const FGbBattleStart& Battle);

	FSimpleMulticastDelegate OnFight;
	FSimpleMulticastDelegate OnSlipAway;

protected:
	virtual void BuildTree() override;

private:
	UFUNCTION() void HandleFight();
	UFUNCTION() void HandleSlipAway();

	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UTextBlock> Intro;
	UPROPERTY() TObjectPtr<UTextBlock> Matchup;
	UPROPERTY() TObjectPtr<UTextBlock> Lore;
};

/** Turn-based battle on the server's /battle/turn: attack, ward, seal, gilded seal, salve, flee. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbBattleWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	void ShowStart(const FGbBattleStart& Battle);
	void ApplyTurn(const FGbBattleTurn& Turn);
	/** A failed request: show why; buttons come back if the battle can go on. */
	void ShowProblem(const FString& Message, bool bBattleOver);

	/** "Back to the Ward" after the battle ended. */
	FSimpleMulticastDelegate OnBack;

protected:
	virtual void BuildTree() override;

private:
	void ShowView(const FGbBattleView& View);
	void AddLines(const TArray<FString>& Lines);
	void SetButtonsEnabled(bool bEnabled);
	void Send(EGbAction Action);
	void ShowEnd(EGbOutcome Outcome, int32 Points, const FString& Wild);

	UFUNCTION() void HandleAttack();
	UFUNCTION() void HandleWard();
	UFUNCTION() void HandleSeal();
	UFUNCTION() void HandleGildedSeal();
	UFUNCTION() void HandleSalve();
	UFUNCTION() void HandleFlee();
	UFUNCTION() void HandleBack();

	UPROPERTY() TObjectPtr<UTextBlock> WildName;
	UPROPERTY() TObjectPtr<UTextBlock> WildTags;
	UPROPERTY() TObjectPtr<UProgressBar> WildHp;
	UPROPERTY() TObjectPtr<UTextBlock> WildHpText;
	UPROPERTY() TObjectPtr<UTextBlock> PlayerName;
	UPROPERTY() TObjectPtr<UTextBlock> PlayerTags;
	UPROPERTY() TObjectPtr<UProgressBar> PlayerHp;
	UPROPERTY() TObjectPtr<UTextBlock> PlayerHpText;
	UPROPERTY() TObjectPtr<UVerticalBox> Log;
	UPROPERTY() TObjectPtr<UTextBlock> Problem;
	UPROPERTY() TObjectPtr<UHorizontalBox> Actions;
	UPROPERTY() TObjectPtr<UHorizontalBox> Actions2;
	UPROPERTY() TObjectPtr<UButton> AttackButton;
	UPROPERTY() TObjectPtr<UButton> WardButton;
	UPROPERTY() TObjectPtr<UButton> SalveButton;
	UPROPERTY() TObjectPtr<UButton> SealButton;
	UPROPERTY() TObjectPtr<UButton> GildedButton;
	UPROPERTY() TObjectPtr<UButton> FleeButton;
	UPROPERTY() TObjectPtr<UTextBlock> SalveLabel;
	UPROPERTY() TObjectPtr<UTextBlock> SealLabel;
	UPROPERTY() TObjectPtr<UTextBlock> GildedLabel;
	UPROPERTY() TObjectPtr<UTextBlock> FleeLabel;
	UPROPERTY() TObjectPtr<UBorder> EndPanel;
	UPROPERTY() TObjectPtr<UTextBlock> EndText;

	FGbBattleView LastView;
	TArray<FString> LogLines;
};

/** One talisman in the case, with its "Make partner" button. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbTalismanRowWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	void Setup(const FGbTalisman& Talisman, const FGbCurseInfo& Curse, bool bIsPartner);

protected:
	virtual void BuildTree() override;

private:
	UFUNCTION() void HandleMakePartner();

	UPROPERTY() TObjectPtr<UTextBlock> Name;
	UPROPERTY() TObjectPtr<UTextBlock> Details;
	UPROPERTY() TObjectPtr<UButton> PartnerButton;
	UPROPERTY() TObjectPtr<UTextBlock> PartnerLabel;
	FString TalismanId;
};

/** Your talismans (T): pick the partner that fights for you. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbTalismanCaseWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	void Refresh();

	FSimpleMulticastDelegate OnClose;

protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandlePlayerChanged();
	UFUNCTION() void HandleClose();

	UPROPERTY() TObjectPtr<UTextBlock> Record;
	UPROPERTY() TObjectPtr<UScrollBox> List;
};
