#pragma once

#include "CoreMinimal.h"
#include "GbWidgets.h"
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
	friend class FGbPanelPreviewTest;
	friend struct FGbPanelFixtures;
	void PullPlayer();
	static int32 MenderPriceFrom(const FGbWorldState& World);
	void Redraw();
	FString NameFor(const FString& TokenId) const;
	FString ArtFor(const FString& TokenId) const;
	const FGbTalisman* TalismanFor(const FString& TokenId) const;
	void Submit(const FString& Message);
	void ShowPocket(int32 Index);
	UFUNCTION() void HandlePlayerChanged();
	UFUNCTION() void HandleWorld(const FGbWorldState& World);
	UFUNCTION() void HandleAction(const FString& Message);
	UFUNCTION() void HandleError(const FString& What, int32 Code, const FString& Message);
	UFUNCTION() void ShowTalismans();
	UFUNCTION() void ShowSupplies();
	UFUNCTION() void ShowKeepsakes();
	UFUNCTION() void ShowLedger();
 UFUNCTION() void ShowGear();
 UFUNCTION() void ShowJournal();
 UPROPERTY() TObjectPtr<UScrollBox> GearRows;
 UPROPERTY() TObjectPtr<UScrollBox> JournalRows;
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
	int32 MenderPrice = -1;
	UPROPERTY() TObjectPtr<UTextBlock> Summary;
	UPROPERTY() TObjectPtr<UTextBlock> Status;
	UPROPERTY() TObjectPtr<UWidgetSwitcher> Pockets;
	UPROPERTY() TArray<TObjectPtr<UButton>> PocketTabs;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> PocketLabels;
	UPROPERTY() TObjectPtr<UWidget> SummaryChips;
	UPROPERTY() TObjectPtr<UBorder> CarriedChip;
	UPROPERTY() TObjectPtr<UTextBlock> CarriedText;
	UPROPERTY() TObjectPtr<UBorder> CaseChip;
	UPROPERTY() TObjectPtr<UTextBlock> CaseText;
	UPROPERTY() TObjectPtr<UBorder> PartnerChip;
	UPROPERTY() TObjectPtr<UTextBlock> PartnerText;
	UPROPERTY() TObjectPtr<UScrollBox> Rows;
	UPROPERTY() TObjectPtr<UImage> PickedArt;
	UPROPERTY() TObjectPtr<UTextBlock> PickedName;
	UPROPERTY() TObjectPtr<UBorder> PlaceChip;
	UPROPERTY() TObjectPtr<UTextBlock> PlaceText;
	UPROPERTY() TObjectPtr<UProgressBar> InkBar;
	UPROPERTY() TObjectPtr<UTextBlock> InkText;
	UPROPERTY() TObjectPtr<UButton> CarryButton;
	UPROPERTY() TObjectPtr<UTextBlock> CarryLabel;
	UPROPERTY() TObjectPtr<UButton> PartnerButton;
	UPROPERTY() TObjectPtr<UButton> SalveButton;
	UPROPERTY() TObjectPtr<UTextBlock> BlankCount;
	UPROPERTY() TObjectPtr<UTextBlock> GildedCount;
	UPROPERTY() TObjectPtr<UTextBlock> SalveCount;
	UPROPERTY() TObjectPtr<UTextBlock> SupplyTarget;
	UPROPERTY() TObjectPtr<UScrollBox> Keepsakes;
	UPROPERTY() TObjectPtr<UTextBlock> EmberCount;
	UPROPERTY() TObjectPtr<UTextBlock> PointsCount;
	/** Ledger pocket: the player's own page in the Rite's ledger (their account), and a button that opens it. */
	UFUNCTION() void OpenLedgerPage();
	UPROPERTY() TObjectPtr<UTextBlock> LedgerPage;
	UPROPERTY() TObjectPtr<UTextBlock> LedgerHoldings;
	UPROPERTY() TObjectPtr<UButton> LedgerButton;
};
