#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "GbWidgets.h"
#include "GbIntroWidget.generated.h"

class SGbStoryArt;

/** Original ink illustrations, drawn by Slate without external texture assets. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbStoryArt : public UWidget
{
	GENERATED_BODY()
public:
	void SetPanel(int32 Index);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	int32 PanelIndex = 0;
	TSharedPtr<SGbStoryArt> Art;
};

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbIntroWidget : public UGbWidgetBase
{
	GENERATED_BODY()
public:
	void Open();
	FSimpleMulticastDelegate OnFinished;
protected:
	virtual void BuildTree() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
private:
	friend class FGbStoryIntroTest;
	UFUNCTION() void HandleNext();
	UFUNCTION() void HandleSkip();
	void Redraw();
	int32 PanelIndex = 0;
	UPROPERTY() TObjectPtr<UGbStoryArt> Illustration;
	UPROPERTY() TObjectPtr<UTextBlock> Page;
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UTextBlock> Body;
	UPROPERTY() TObjectPtr<UTextBlock> NextLabel;
};
