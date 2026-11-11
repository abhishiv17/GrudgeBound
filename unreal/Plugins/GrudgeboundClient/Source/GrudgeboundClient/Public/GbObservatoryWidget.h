#pragma once

#include "CoreMinimal.h"
#include "GbWidgets.h"
#include "Components/Widget.h"
#include "GbObservatoryWidget.generated.h"

struct FGbPlotLine
{
 TArray<FVector2D> Points; // tick, value
 FLinearColor Color = FLinearColor::Black;
 float Width = 1.f;
};

/** Slate paints only chart geometry; all labels use the shared paper UI text helper. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbObservatoryChart : public UWidget
{
 GENERATED_BODY()
public:
 TArray<FGbPlotLine> Lines;
 TArray<TPair<float,FLinearColor>> Markers;
 float FirstTick = 1.f;
 float LastTick = 168.f;
 static FVector2D ValueRange(const TArray<FGbPlotLine>& Series);
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** One button owns one proof URL, avoiding global selection races after a refresh. */
UCLASS()
class UGbObservatoryProof : public UObject
{
 GENERATED_BODY()
public:
 FString Url;
 UFUNCTION() void Open();
};

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbObservatoryWidget : public UGbWidgetBase
{
 GENERATED_BODY()
public:
 void Open();
 void Stop();
 FSimpleMulticastDelegate OnClose;
protected:
 virtual void BuildTree() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
 friend class FGbObservatoryRenderTest;
 friend class FGbPanelPreviewTest;
 UFUNCTION() void Live();
 UFUNCTION() void Preview();
 UFUNCTION() void Grudge();
 UFUNCTION() void Prices();
 UFUNCTION() void Rules();
 UFUNCTION() void Close();
 UFUNCTION() void WardLantern();
 UFUNCTION() void WardTide();
 UFUNCTION() void WardAsh();
 UFUNCTION() void WardBone();
 void SelectWard(const FString& Id);
 UFUNCTION() void Receive(const FGbObservatoryData& Incoming);
 UFUNCTION() void Error(const FString& What, int32 Code, const FString& Message);
 void SetMode(bool bNewPreview);
 void Refresh();
 void Redraw();
 void AddChart(const FString& Title, TArray<FGbPlotLine> Lines, float Height, const FString& Ward);
 bool bOpen = false;
 bool bPreview = false;
 int32 Tab = 0;
 float RefreshElapsed = 0.f;
 FGbObservatoryData Data;
 FString SelectedWard = TEXT("lantern");
 UPROPERTY() TObjectPtr<UTextBlock> Status;
 UPROPERTY() TObjectPtr<UVerticalBox> Content;
 /** The Ward buttons (Grudge tab): the picked Ward's chart also draws its three emotions. */
 UPROPERTY() TObjectPtr<UWidget> WardRow;
 UPROPERTY() TArray<TObjectPtr<UButton>> WardButtons;
 UPROPERTY() TArray<TObjectPtr<UTextBlock>> WardLabels;
 UPROPERTY() TArray<TObjectPtr<UButton>> ModeButtons;
 UPROPERTY() TArray<TObjectPtr<UTextBlock>> ModeLabels;
 UPROPERTY() TArray<TObjectPtr<UButton>> TabButtons;
 UPROPERTY() TArray<TObjectPtr<UTextBlock>> TabLabels;
 UPROPERTY() TArray<TObjectPtr<UGbObservatoryProof>> Proofs;
};
