#pragma once

#include "CoreMinimal.h"
#include "GbWidgets.h"
#include "Components/ComboBoxString.h"
#include "GbSatchelWidget.generated.h"

class UWidgetSwitcher;

/** Four pockets backed by the signed-in player's server Satchel. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbSatchelWidget : public UGbWidgetBase
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
	friend class UGbMenderWidget;
	friend class FGbSatchelCarryTest;
	friend class FGbSatchelSalveTest;
	friend class FGbSatchelVisualTest;
	void PullPlayer();
	static int32 MenderPriceFrom(const FGbWorldState& World);
	void Redraw();
	FString NameFor(const FString& TokenId) const;
	const FGbTalisman* TalismanFor(const FString& TokenId) const;
	void Submit(const FString& Message);
	UFUNCTION() void HandlePlayerChanged();
	UFUNCTION() void HandleWorld(const FGbWorldState& World);
	UFUNCTION() void HandleAction(const FString& Message);
	UFUNCTION() void HandleError(const FString& What, int32 Code, const FString& Message);
	UFUNCTION() void HandleSelection(FString Option, ESelectInfo::Type Selection);
	UFUNCTION() void ShowTalismans();
	UFUNCTION() void ShowSupplies();
	UFUNCTION() void ShowKeepsakes();
	UFUNCTION() void ShowLedger();
	UFUNCTION() void CarrySelected();
	UFUNCTION() void ChoosePartner();
	UFUNCTION() void UseSalve();
	UFUNCTION() void Refresh();
	UFUNCTION() void Close();
	FGbSatchel Data;
	TArray<FGbTalisman> Talismans;
	TMap<FString, FString> Names;
	TArray<FString> Options;
	FString SelectedToken;
	FString PartnerToken;
	bool bBusy = false;
	bool bUpdating = false;
	int32 MenderPrice = -1;
	UPROPERTY() TObjectPtr<UTextBlock> Summary;
	UPROPERTY() TObjectPtr<UTextBlock> Status;
	UPROPERTY() TObjectPtr<UWidgetSwitcher> Pockets;
	UPROPERTY() TArray<TObjectPtr<UButton>> PocketTabs;
	UPROPERTY() TObjectPtr<UComboBoxString> Selection;
	UPROPERTY() TObjectPtr<UScrollBox> Rows;
	UPROPERTY() TObjectPtr<UProgressBar> InkBar;
	UPROPERTY() TObjectPtr<UTextBlock> InkText;
	UPROPERTY() TObjectPtr<UTextBlock> SelectedInfo;
	UPROPERTY() TObjectPtr<UButton> CarryButton;
	UPROPERTY() TObjectPtr<UTextBlock> CarryLabel;
	UPROPERTY() TObjectPtr<UButton> PartnerButton;
	UPROPERTY() TObjectPtr<UButton> SalveButton;
	UPROPERTY() TObjectPtr<UTextBlock> SupplyText;
	UPROPERTY() TObjectPtr<UTextBlock> SupplyTarget;
	UPROPERTY() TObjectPtr<UScrollBox> Keepsakes;
	UPROPERTY() TObjectPtr<UTextBlock> Ledger;
};
