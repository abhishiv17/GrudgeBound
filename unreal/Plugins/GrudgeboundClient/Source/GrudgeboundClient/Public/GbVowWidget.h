#pragma once

#include "CoreMinimal.h"
#include "GbWidgets.h"
#include "Components/ComboBoxString.h"
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
	UFUNCTION() void HandleVows(const FGbVowsOverview& Overview);
	UFUNCTION() void HandleReceipt(const FString& JobId, const FGbChainReceipt& Receipt);
	UFUNCTION() void HandleError(const FString& What, int32 Code, const FString& Message);
	UFUNCTION() void HandleTemplate(FString Selected, ESelectInfo::Type Selection);
	UFUNCTION() void HandleChoice(FString Selected, ESelectInfo::Type Selection);
	UFUNCTION() void HandleSwear();
	UFUNCTION() void HandleRelease();
	UFUNCTION() void HandleClose();
	void RedrawChoices();
	void RedrawPreview();
	const FGbVowTemplate* Current() const;
	FGbVowsOverview Data;
	bool bBusy = false;
	bool bUpdating = false;
	bool bReady = false;
	FTimerHandle RefreshTimer;
	TArray<int32> ActiveIds;
	UPROPERTY() TObjectPtr<UTextBlock> Info;
	UPROPERTY() TObjectPtr<UTextBlock> Preview;
	UPROPERTY() TObjectPtr<UTextBlock> Status;
	UPROPERTY() TObjectPtr<UComboBoxString> Templates;
	UPROPERTY() TObjectPtr<UComboBoxString> Parameter;
	UPROPERTY() TObjectPtr<UComboBoxString> Days;
	UPROPERTY() TObjectPtr<UComboBoxString> Active;
	UPROPERTY() TObjectPtr<UScrollBox> History;
	UPROPERTY() TObjectPtr<UButton> Swear;
	UPROPERTY() TObjectPtr<UButton> Release;
};
