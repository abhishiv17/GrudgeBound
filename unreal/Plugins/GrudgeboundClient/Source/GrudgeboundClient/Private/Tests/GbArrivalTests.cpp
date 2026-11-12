#if WITH_DEV_AUTOMATION_TESTS
#include "GbArrivalWidget.h"
#include "GbExorcistLook.h"
#include "GbJson.h"
#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Dom/JsonObject.h"
#if WITH_EDITOR
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "UnrealClient.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbArrivalPanelTest,"Grudgebound.Arrival.ProfileAndPanels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbArrivalPanelTest::RunTest(const FString&)
{
 TSharedPtr<FJsonObject> O;
 TestTrue(TEXT("Versioned arrival parses"),GbJson::ParseObject(TEXT(R"({"carried":["starter"],"case":[],"supplies":{},"ink":{},"profile":{"version":1,"complete":false,"legacyStarter":false,"name":"Ari","look":"feminine","pronouns":"they","starter":"hushmoth","tokenId":"starter","mint":"virtual","choices":[{"id":"cinder-grin","name":"Cinder Grin","emotion":"rage","personality":"Bright and impatient.","stats":{"hp":38,"attack":12,"guard":6,"speed":11}},{"id":"weepwick","name":"Weepwick","emotion":"sorrow","personality":"A stubborn little flame.","stats":{"hp":44,"attack":8,"guard":10,"speed":8}},{"id":"hushmoth","name":"Hushmoth","emotion":"dread","personality":"Listens to everything.","stats":{"hp":34,"attack":10,"guard":7,"speed":13}}]}})"),O));
 const FGbArrivalProfile P=GbJson::Satchel(O).Profile;
 TestTrue(TEXT("Profile supported"),P.bValid); TestFalse(TEXT("First arrival incomplete"),P.bComplete);
 TestEqual(TEXT("All starter choices preserved"),P.Choices.Num(),3);
 TestEqual(TEXT("Chosen pronouns preserved"),P.Pronouns,FString(TEXT("they")));
 if (P.Choices.Num()==3) TestEqual(TEXT("Displayed starter stats are server values"),P.Choices[2].Hp,34);
 const auto Masculine=UGbExorcistLook::PresentationOutfit(TEXT("masculine")), Feminine=UGbExorcistLook::PresentationOutfit(TEXT("feminine"));
 TestTrue(TEXT("Presentations have distinct silhouettes"),Masculine.bPonytail!=Feminine.bPonytail && Masculine.bSkirt!=Feminine.bSkirt);
 TestEqual(TEXT("Looks do not change body scale or movement"),Masculine.Height,Feminine.Height);
 UGbArrivalWidget* W=NewObject<UGbArrivalWidget>(); W->AddToRoot(); W->Initialize(); W->TakeWidget(); W->Profile=P;
 W->ChosenName=TEXT("Ari"); W->ChosenStarter=TEXT("hushmoth");
 W->Page=5; W->Redraw(); TestNotNull(TEXT("Name field exists"),W->NameInput.Get()); W->NameInput->SetText(FText::FromString(TEXT("Ari"))); W->ChangeLook(); TestEqual(TEXT("Cycling look keeps typed name"),W->ChosenName,FString(TEXT("Ari")));
 W->Next(); TestEqual(TEXT("New player chooses a partner"),W->Page,6);
 W->Back(); W->Profile.bLegacyStarter=true; W->Next(); TestEqual(TEXT("Legacy player skips partner choice only"),W->Page,7);
 W->Profile.bLegacyStarter=false;
#if WITH_EDITOR
 FWidgetRenderer Renderer(false);
 for (const int32 Page:{0,5,6,7}) for (const int32 Height:{1080,720})
 {
  W->Page=Page; W->Redraw();
  const FIntPoint Size(Height*16/9,Height);
  const TSharedRef<SWidget> Scene=SNew(SDPIScaler).DPIScale(Height/1080.f)[W->TakeWidget()];
  Renderer.DrawWidget(Scene,FVector2D(Size)); Scene->SlatePrepass(1.f);
  UTextureRenderTarget2D* Target=Renderer.DrawWidget(Scene,FVector2D(Size)); if (!TestNotNull(TEXT("Arrival renders"),Target)) continue;
  TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
  if (!TestTrue(TEXT("Arrival pixels read"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))) continue;
  for (auto& Pixel:Pixels) Pixel.A=255;
  TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("ArrivalPreview")/FString::FromInt(Height); IFileManager::Get().MakeDirectory(*Folder,true);
  TestTrue(TEXT("Arrival preview saved"),FFileHelper::SaveArrayToFile(Png,*(Folder/FString::Printf(TEXT("page-%d.png"),Page))));
 }
#endif
 W->RemoveFromRoot();return true;
}
#endif
