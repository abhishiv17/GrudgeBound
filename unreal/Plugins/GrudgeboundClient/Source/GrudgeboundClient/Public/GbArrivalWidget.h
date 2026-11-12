#pragma once
#include "CoreMinimal.h"
#include "GbWidgets.h"
#include "GbArrivalWidget.generated.h"

/** Draft pier conversation and durable arrival choices. No client grants or mutable combat numbers. */
UCLASS()
class GRUDGEBOUNDCLIENT_API UGbArrivalWidget : public UGbWidgetBase
{
 GENERATED_BODY()
public:
 void Open();
 FSimpleMulticastDelegate OnClose;
protected:
 virtual void BuildTree() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry&,const FKeyEvent&) override;
private:
 friend class FGbArrivalPanelTest;
 friend struct FGbPanelFixtures;
 void Redraw();
 void ReadName();
 void PreviewLook();
 UFUNCTION() void Next();
 UFUNCTION() void Back();
 UFUNCTION() void Later();
 UFUNCTION() void ChangeLook();
 UFUNCTION() void ChangePronouns();
 UFUNCTION() void Changed();
 UFUNCTION() void Action(const FString& Message);
 UFUNCTION() void Error(const FString& What,int32 Code,const FString& Message);
 UPROPERTY() TObjectPtr<UVerticalBox> Content;
 UPROPERTY() TObjectPtr<UTextBlock> Heading;
 UPROPERTY() TObjectPtr<UTextBlock> Status;
 UPROPERTY() TObjectPtr<UTextBlock> NextLabel;
 UPROPERTY() TObjectPtr<UButton> NextButton;
 UPROPERTY() TObjectPtr<UButton> BackButton;
 UPROPERTY() TObjectPtr<class UEditableTextBox> NameInput;
 FGbArrivalProfile Profile;
 FString ChosenName;
 FString ChosenLook=TEXT("masculine");
 FString ChosenPronouns=TEXT("they");
 FString ChosenStarter=TEXT("cinder-grin");
 int32 Page=0;
 bool bSubmitting=false;
};
