#pragma once

#include "CoreMinimal.h"
#include "GbWidgets.h"
#include "GbVowWidget.generated.h"

/** Native Vow Shrine. All stakes, ownership and settlement come from the game server. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbVowWidget : public UGbWidgetBase
{
	GENERATED_BODY()
public:
	void Open();
	FSimpleMulticastDelegate OnClose;
protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	friend class FGbPanelPreviewTest;
	friend struct FGbPanelFixtures;

	UFUNCTION() void HandleVows(const FGbVowsOverview& Overview);
	UFUNCTION() void HandleReceipt(const FString& JobId, const FGbChainReceipt& Receipt);
	UFUNCTION() void HandleError(const FString& What, int32 Code, const FString& Message);
	UFUNCTION() void HandlePrevTemplate();
	UFUNCTION() void HandleNextTemplate();
	UFUNCTION() void HandlePrevParam();
	UFUNCTION() void HandleNextParam();
	UFUNCTION() void HandlePrevDays();
	UFUNCTION() void HandleNextDays();
	UFUNCTION() void HandlePrevActive();
	UFUNCTION() void HandleNextActive();
	UFUNCTION() void HandleSwear();
	UFUNCTION() void HandleRelease();
	UFUNCTION() void HandleClose();
	/** Moves a stepper index round its options (ignored while a change is being sealed). */
	void Step(int32& Index, int32 Count, int32 Delta, bool bChoices);
	void RedrawChoices();
	void RedrawPreview();
	const FGbVowTemplate* Current() const;
	/** What the vow's {ward}/{talisman}/{omen} parameter sends, and how it reads. */
	FString ParamValue() const;
	FString ParamLabel() const;

	FGbVowsOverview Data;
	bool bBusy = false;
	bool bReady = false;
	FTimerHandle RefreshTimer;
	int32 TemplateIndex = 0;
	int32 ParamIndex = 0;
	int32 DaysIndex = 0;
	int32 ActiveIndex = 0;
	TArray<FString> ParamValues;
	TArray<FString> ParamLabels;
	TArray<int32> DayOptions;
	/** Active vows that can be released, in History order. */
	TArray<int32> ActiveIds;
	/** One slip per vow in History (same order as SlipIds); the one picked for release is marked. */
	TArray<int32> SlipIds;
	UPROPERTY() TArray<TObjectPtr<UBorder>> Slips;
	UPROPERTY() TArray<TObjectPtr<UButton>> StepButtons;
	UPROPERTY() TObjectPtr<UTextBlock> Info;
	UPROPERTY() TObjectPtr<UTextBlock> Preview;
	UPROPERTY() TObjectPtr<UTextBlock> Status;
	UPROPERTY() TObjectPtr<UTextBlock> TemplateText;
	UPROPERTY() TObjectPtr<UWidget> ParamRow;
	UPROPERTY() TObjectPtr<UTextBlock> ParamKey;
	UPROPERTY() TObjectPtr<UTextBlock> ParamText;
	UPROPERTY() TObjectPtr<UTextBlock> DaysText;
	UPROPERTY() TObjectPtr<UTextBlock> ActiveText;
	UPROPERTY() TObjectPtr<UWidget> ReleaseRow;
	UPROPERTY() TObjectPtr<UScrollBox> History;
	UPROPERTY() TObjectPtr<UButton> Swear;
	UPROPERTY() TObjectPtr<UButton> Release;
};
