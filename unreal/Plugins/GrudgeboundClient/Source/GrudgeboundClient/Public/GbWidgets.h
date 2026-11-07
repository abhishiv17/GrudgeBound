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

// A modal gate: the game itself never asks for an email, a code, or a wallet.

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
	UBorder* MakePanel(const FLinearColor& Background, const FMargin& Inner);
	UButton* MakeButton(const FString& Label, UTextBlock*& OutLabel, const FLinearColor& Tint);
	UProgressBar* MakeBar(const FLinearColor& Fill);
	/** Wraps a widget in a size box of the given size (0 = leave that axis free). */
	UWidget* Sized(UWidget* Content, float Width, float Height);
};

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbSignInWidget : public UGbWidgetBase
{
	GENERATED_BODY()
public:
	void Refresh();
protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void HandleSignIn();
	UFUNCTION() void HandleChanged();
	UPROPERTY() TObjectPtr<UTextBlock> StatusText;
	UPROPERTY() TObjectPtr<UButton> SignInButton;
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
	UFUNCTION() void HandleStoryChanged();

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
	UPROPERTY() TObjectPtr<UTextBlock> ChapterText;
	UPROPERTY() TObjectPtr<UTextBlock> ObjectiveText;
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

/** One line in the shop: a title, a detail line and one button. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbShopRowWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	void Setup(const FString& Title, const FString& Detail, const FString& Button, bool bEnabled, const FLinearColor& TitleColor);

	/** Called with nothing: the shop knows which row it built. */
	TFunction<void()> OnClick;

protected:
	virtual void BuildTree() override;

private:
	UFUNCTION() void HandleClick();

	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> DetailText;
	UPROPERTY() TObjectPtr<UButton> Action;
	UPROPERTY() TObjectPtr<UTextBlock> ActionLabel;
};

/** The Talisman Broker's shop (B): buy talismans for Ember, sell yours back; settled on NEAR (P2-G1). */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbShopWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	/** Fetch prices and your Ember (called when the shop opens). */
	void Open();

	FSimpleMulticastDelegate OnClose;

	/** The merchant in the Lantern Ward. */
	static constexpr const TCHAR* MerchantId = TEXT("broker");

protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandleShop(const FGbShop& Shop);
	UFUNCTION() void HandleTrade(const FGbTrade& Trade);
	UFUNCTION() void HandleTradeReceipt(const FString& JobId, const FGbChainReceipt& Receipt);
	UFUNCTION() void HandleApiError(const FString& What, int32 Status, const FString& Message);
	UFUNCTION() void HandlePlayerChanged();
	UFUNCTION() void HandleClose();

	void Redraw();
	void Buy(const FString& ItemId);
	void Sell(const FString& TokenId);

	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UTextBlock> Purse;
	UPROPERTY() TObjectPtr<UScrollBox> Stock;
	UPROPERTY() TObjectPtr<UScrollBox> Yours;
	UPROPERTY() TObjectPtr<UTextBlock> Status;

	FGbShop Data;
	FString PendingJob;
	bool bBusy = false;
};

/**
 * The Rite (R): rules in force, and a rule builder. Propose sends it as your Player Account; Kessa answers at
 * once, then the proposal and her verdict are written on NEAR (P2-G2). Repeal strikes a rule in force for twice
 * its cost, paid from your points on NEAR (P2-C5).
 */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbRulesWidget : public UGbWidgetBase
{
	GENERATED_BODY()

public:
	/** Fetch the latest rules and points (called when the panel opens). */
	void Open();

	FSimpleMulticastDelegate OnClose;

	static constexpr int32 MaxParams = 3;

protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandleRules(const FGbRulesOverview& Rules);
	UFUNCTION() void HandleVerdict(const FGbRuleVerdict& Verdict);
	UFUNCTION() void HandleRuleReceipt(const FString& JobId, const FGbChainReceipt& Receipt);
	UFUNCTION() void HandleRepealAnswer(const FGbRuleVerdict& Answer);
	UFUNCTION() void HandleApiError(const FString& What, int32 Status, const FString& Message);
	UFUNCTION() void HandlePrevRepeal();
	UFUNCTION() void HandleNextRepeal();
	UFUNCTION() void HandleRepeal();
	UFUNCTION() void HandlePrevTemplate();
	UFUNCTION() void HandleNextTemplate();
	UFUNCTION() void HandlePrev0();
	UFUNCTION() void HandleNext0();
	UFUNCTION() void HandlePrev1();
	UFUNCTION() void HandleNext1();
	UFUNCTION() void HandlePrev2();
	UFUNCTION() void HandleNext2();
	UFUNCTION() void HandlePropose();
	UFUNCTION() void HandleClose();

	void StepTemplate(int32 Delta);
	void StepRepeal(int32 Delta);
	/** Rules in force that live on the rite contract (only those can be repealed). */
	TArray<const FGbActiveRule*> Repealable() const;
	void StepParam(int32 Index, int32 Delta);
	void Redraw();
	void SetBusy(bool bNowBusy);
	const FGbRuleTemplate* Current() const;

	UPROPERTY() TObjectPtr<UTextBlock> Info;
	UPROPERTY() TObjectPtr<UVerticalBox> ActiveList;
	UPROPERTY() TObjectPtr<UTextBlock> TemplateId;
	UPROPERTY() TObjectPtr<UTextBlock> Preview;
	UPROPERTY() TArray<TObjectPtr<UWidget>> ParamRows;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> ParamKeys;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> ParamValues;
	UPROPERTY() TObjectPtr<UButton> ProposeButton;
	UPROPERTY() TObjectPtr<UWidget> RepealRow;
	UPROPERTY() TObjectPtr<UTextBlock> RepealPick;
	UPROPERTY() TObjectPtr<UButton> RepealButton;
	UPROPERTY() TObjectPtr<UTextBlock> RepealLabel;
	UPROPERTY() TObjectPtr<UTextBlock> Status;

	FGbRulesOverview Data;
	int32 TemplateIndex = 0;
	int32 RepealIndex = 0;
	TArray<int32> Choice;
	FString PendingJob;
	/** The pending job is a repeal (its receipt reads differently). */
	bool bPendingRepeal = false;
	bool bBusy = false;
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
