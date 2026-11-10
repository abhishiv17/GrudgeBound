#if WITH_DEV_AUTOMATION_TESTS
#include "GbJson.h"
#include "GbObservatoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#if WITH_EDITOR
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"
#include "UnrealClient.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbObservatoryJsonTest,"Grudgebound.Observatory.JsonAndOrdering",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbObservatoryJsonTest::RunTest(const FString&)
{
 TSharedPtr<FJsonObject> O;
 TestTrue(TEXT("Empty live history parses"),GbJson::ParseObject(TEXT(R"({"mode":"live","seed":2,"samples":[],"rules":[],"events":[]})"),O));
 auto D=GbJson::Observatory(O); TestTrue(TEXT("Empty history is valid"),D.bValid); TestEqual(TEXT("No fake samples"),D.Samples.Num(),0);
 TestTrue(TEXT("Populated response parses"),GbJson::ParseObject(TEXT(R"({"mode":"preview","seed":2,"samples":[{"tick":49,"day":3,"hour":1,"wards":[{"id":"lantern","rage":8,"sorrow":9,"dread":10,"total":27}],"prices":{"broker":{"blank-talisman":15}},"closed":["fence"]}],"rules":[{"id":"old","text":"first","author":"Fen","authorKind":"agent","tick":5,"status":"active"},{"id":"new","text":"second","author":"player","authorKind":"player","tick":8,"status":"rejected","reason":"existing server reason","url":"https://testnet.nearblocks.io/txns/example"}],"events":[{"tick":49,"type":"bankruptcy","ward":"tide","text":"milestone"}]})"),O));
 D=GbJson::Observatory(O); TestEqual(TEXT("Newest rule first"),D.Rules[0].Id,FString(TEXT("new")));
 TestEqual(TEXT("Rejection reason retained"),D.Rules[0].Reason,FString(TEXT("existing server reason")));
 TestEqual(TEXT("Agent author retained"),D.Rules[1].AuthorKind,FString(TEXT("agent")));
 TestEqual(TEXT("Flat ward emotions"),D.Samples[0].Wards[0].Grudge.Sorrow,9.f);
 TestEqual(TEXT("Nested prices"),D.Samples[0].Prices[TEXT("broker")].Items[TEXT("blank-talisman")],15.f);
 TestEqual(TEXT("Closed shop"),D.Samples[0].Closed[0],FString(TEXT("fence")));
 TestEqual(TEXT("Milestone"),D.Events[0].Tick,49);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbObservatoryScaleTest,"Grudgebound.Observatory.FlatAndEmptyScale",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbObservatoryScaleTest::RunTest(const FString&)
{
 const auto Empty=UGbObservatoryChart::ValueRange({}); TestTrue(TEXT("Empty finite range"),Empty.Y>Empty.X);
 FGbPlotLine Flat; Flat.Points={FVector2D(1,15),FVector2D(168,15)};
 const auto Range=UGbObservatoryChart::ValueRange({Flat});
 TestTrue(TEXT("Flat series gets finite padding"),FMath::IsFinite(Range.X)&&FMath::IsFinite(Range.Y)&&Range.X<15&&Range.Y>15);
 Flat.Points={FVector2D(1,-10),FVector2D(168,90)}; const auto Variable=UGbObservatoryChart::ValueRange({Flat});
 TestTrue(TEXT("Variable range contains extremes"),Variable.X<-10&&Variable.Y>90);
 return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbObservatoryRenderTest,"Grudgebound.Observatory.ThreeTabs1040x720",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbObservatoryRenderTest::RunTest(const FString&)
{
 UGbObservatoryWidget* W=NewObject<UGbObservatoryWidget>(); W->AddToRoot(); W->Initialize(); W->NativeOnInitialized();
 W->bOpen=true;
 FGbObservatoryData D; D.bValid=true; D.Mode=TEXT("live"); D.Seed=2;
 for (int32 Tick=1;Tick<=168;++Tick)
 {
  auto& S=D.Samples.AddDefaulted_GetRef(); S.Tick=Tick; S.Day=Tick/24+1; S.Hour=Tick%24;
  for (const FString& Id:{FString(TEXT("lantern")),FString(TEXT("tide")),FString(TEXT("ash")),FString(TEXT("bone"))})
  { auto& Ward=S.Wards.AddDefaulted_GetRef(); Ward.Id=Id; Ward.Grudge.Rage=10+Tick%12; Ward.Grudge.Sorrow=20; Ward.Grudge.Dread=8; Ward.Total=Ward.Grudge.Rage+28; }
  auto& Broker=S.Prices.Add(TEXT("broker")); Broker.Items={{TEXT("blank-talisman"),15.f},{TEXT("talisman-rage"),float(40+Tick%12)},{TEXT("talisman-sorrow"),35.f},{TEXT("talisman-dread"),44.f},{TEXT("gilded-talisman"),30.f},{TEXT("ink-salve"),10.f}};
  if (Tick>=49) S.Closed.Add(TEXT("fence"));
 }
 FGbObservatoryEvent E; E.Tick=49; E.Type=TEXT("bankruptcy"); E.Ward=TEXT("tide"); D.Events.Add(E); E.Tick=73; E.Type=TEXT("calamity"); E.Ward=TEXT("lantern"); D.Events.Add(E);
 FGbObservatoryRule R; R.Id=TEXT("1"); R.Tick=24; R.Author=TEXT("Fen"); R.AuthorKind=TEXT("agent"); R.Text=TEXT("Fixture rule supplied by the server."); R.Status=TEXT("active"); D.Rules.Add(R);
 R.Id=TEXT("2"); R.Tick=72; R.Status=TEXT("rejected"); R.Reason=TEXT("Fixture rejection reason supplied by the server."); R.Url=TEXT("https://testnet.nearblocks.io/txns/fixture"); D.Rules.Add(R);
 W->Receive(D); TestEqual(TEXT("UI keeps newest rule first"),W->Data.Rules[0].Id,FString(TEXT("2")));
 const auto Slate=W->WidgetTree->RootWidget->TakeWidget(); FWidgetRenderer Renderer(false);
 for (int32 Tab=0;Tab<3;++Tab)
 {
  W->Tab=Tab; W->Redraw();
  Renderer.DrawWidget(Slate,FVector2D(1040,720)); Slate->Invalidate(EInvalidateWidgetReason::Layout); Slate->SlatePrepass(1.f);
  auto* Target=Renderer.DrawWidget(Slate,FVector2D(1040,720));
  if (!TestNotNull(TEXT("Tab renders natively"),Target)) { W->RemoveFromRoot(); return false; }
  TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
  TestTrue(TEXT("Pixels readable"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
  int32 Visible=0; for (const auto& P:Pixels) if (P.A && int32(P.R)+P.G+P.B>30) ++Visible;
  TestTrue(TEXT("Tab has visible content"),Visible>500);
  TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1040,720,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);
  const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ObservatoryPreview")); IFileManager::Get().MakeDirectory(*Folder,true);
  const FString File=FPaths::Combine(Folder,FString::Printf(TEXT("Tab-%d.png"),Tab)); TestTrue(TEXT("Preview saved"),FFileHelper::SaveArrayToFile(Png,*File)); AddInfo(File);
 }
 W->RemoveFromRoot(); return true;
}
#endif
#endif
