#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "GbWidgets.h"
#include "GbUserSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbBattlePresentationTest,"Grudgebound.Ui.BattlePresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbBattlePresentationTest::RunTest(const FString&)
{
	UGbBattleWidget* W=NewObject<UGbBattleWidget>(); W->AddToRoot(); W->Initialize();
	if(!W->WidgetTree->RootWidget) W->BuildTree();
	UGbUserSettings* Settings=UGbUserSettings::Get(); const bool OriginalReduced=Settings->bReducedBattleMotion;
	Settings->bReducedBattleMotion=false;
	FGbBattleStart Start;
	Start.View.Wild.SpeciesId=TEXT("riot-bell"); Start.View.Wild.Name=TEXT("Riot Bell"); Start.View.Wild.Emotion=TEXT("rage");
	Start.View.Wild.Omen=TEXT("calamity"); Start.View.Wild.Hp=62; Start.View.Wild.MaxHp=80;
	Start.View.Player.SpeciesId=TEXT("weepwick"); Start.View.Player.Name=TEXT("Weepwick"); Start.View.Player.Emotion=TEXT("sorrow");
	Start.View.Player.Omen=TEXT("whisper"); Start.View.Player.Hp=31; Start.View.Player.MaxHp=40;
	Start.View.Salves=2; Start.View.GildedSeals=1; Start.View.SealChance=.42f; Start.View.GildedSealChance=.71f; Start.View.FleeChance=.8f;
	Start.View.bCanAttack=Start.View.bCanWard=Start.View.bCanSalve=Start.View.bCanSeal=Start.View.bCanGildedSeal=Start.View.bCanFlee=true;
	Start.Lines={TEXT("Battle log line"),TEXT("Battle log line")};
	const auto Advance=[W](float Seconds){for(float T=0;T<Seconds;T+=.02f)W->NativeTick(FGeometry(),.02f);};
	FWidgetRenderer Renderer(true);
	// Construct the user widget, not only its root: IsVisible and NativeTick need its Slate wrapper.
	const auto Slate=W->TakeWidget();
	W->SetVisibility(ESlateVisibility::Visible);
	FAssetCompilingManager::Get().FinishAllCompilation();
	TArray<UWidget*> Widgets; W->WidgetTree->GetAllWidgets(Widgets);
	for(UWidget* Widget:Widgets)if(UImage* Image=Cast<UImage>(Widget))if(UTexture2D* Texture=Cast<UTexture2D>(Image->GetBrush().GetResourceObject()))Texture->WaitForPendingInitOrStreaming();
	FlushRenderingCommands();
	const auto Capture=[&](const TCHAR* Name)
	{
		// ShowStart swaps in the species' textures: wait for those too, or the offscreen render shows empty frames.
		TArray<UWidget*> Now; W->WidgetTree->GetAllWidgets(Now);
		for(UWidget* Widget:Now)if(UImage* Image=Cast<UImage>(Widget))if(UTexture2D* Texture=Cast<UTexture2D>(Image->GetBrush().GetResourceObject()))Texture->WaitForPendingInitOrStreaming();
		FlushRenderingCommands();
		for(const FIntPoint Size:{FIntPoint(1920,1080),FIntPoint(1280,720)})
		{
			Renderer.DrawWidget(Slate,FVector2D(Size)); Slate->Invalidate(EInvalidateWidgetReason::Layout); Slate->SlatePrepass(1);
			UTextureRenderTarget2D* Target=Renderer.DrawWidget(Slate,FVector2D(Size));
			if(!TestNotNull(TEXT("Battle renders at target resolution"),Target))continue;
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			TestTrue(TEXT("Battle pixels read back"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
			TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);
			const FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BattleReview"),FString::FromInt(Size.Y)); IFileManager::Get().MakeDirectory(*Dir,true);
			const FString File=FPaths::Combine(Dir,FString(Name)+TEXT(".png"));
			TestTrue(TEXT("Review PNG saved"),FFileHelper::SaveArrayToFile(Png,*File));
		}
	};
	W->ShowStart(Start); Advance(.1f);
	for(UImage* I:{W->Sprites[0].Get(),W->Sprites[1].Get(),W->Portraits[0].Get(),W->EmotionIcons[0].Get(),W->SealStamp.Get(),W->WardImages[1].Get()})
	{
		const auto* Texture=Cast<UTexture2D>(I->GetBrush().GetResourceObject());
		if(TestNotNull(TEXT("Battle texture imported"),Texture))TestTrue(TEXT("Pixel art uses nearest filtering"),Texture->Filter==TF_Nearest);
	}
	TestTrue(TEXT("All legal opening commands available"),W->AttackButton->GetIsEnabled()&&W->SealButton->GetIsEnabled()&&W->FleeButton->GetIsEnabled());
	Capture(TEXT("battle-active"));
	FGbBattleTurn Turn; Turn.Action=TEXT("attack"); Turn.View=Start.View; Turn.View.Wild.Hp=48; Turn.View.Player.Hp=22;
	Turn.Lines={TEXT("Battle log line"),TEXT("Battle log line")}; W->ApplyTurn(Turn);
	TestFalse(TEXT("New command blocked while visual response plays"),W->AttackButton->GetIsEnabled());
	TestTrue(TEXT("HP bar waits for impact"),FMath::IsNearlyEqual(W->TargetHp[0],62.f/80));
	Advance(.28f); Capture(TEXT("battle-hit")); Advance(1.5f);
	TestTrue(TEXT("Commands restored after feedback"),W->AttackButton->GetIsEnabled());
	TestTrue(TEXT("HP settles to server result"),FMath::IsNearlyEqual(W->ShownHp[0],48.f/80,.01f));
	W->ShowStart(Start); Turn.Action=TEXT("ward"); Turn.View=Start.View; Turn.View.Player.Hp=28; W->ApplyTurn(Turn); Advance(.2f);
	TestTrue(TEXT("Ward has visible feedback"),W->WardImages[1]->GetVisibility()!=ESlateVisibility::Collapsed); Capture(TEXT("battle-ward"));
	W->ShowProblem(TEXT("Request failed"),false);
	TestTrue(TEXT("Recoverable request failure restores commands"),W->AttackButton->GetIsEnabled());
	W->ShowStart(Start); Turn.Action=TEXT("seal"); Turn.View=Start.View; Turn.bEnded=true; Turn.Outcome=EGbOutcome::Bound; Turn.Points=30;
	W->ApplyTurn(Turn); Advance(.25f); Capture(TEXT("battle-seal"));
	TestEqual(TEXT("Result does not cover seal animation"),W->EndPanel->GetVisibility(),ESlateVisibility::Collapsed);
	Advance(1.5f); TestEqual(TEXT("Result appears after animation"),W->EndPanel->GetVisibility(),ESlateVisibility::Visible);
	TestFalse(TEXT("Ended battle never re-enables actions"),W->AttackButton->GetIsEnabled()); Capture(TEXT("battle-result"));
	Settings->bReducedBattleMotion=true;
	W->ShowStart(Start); Turn.Action=TEXT("attack"); Turn.bEnded=false; Turn.Outcome=EGbOutcome::None; Turn.View.Wild.Hp=48; W->ApplyTurn(Turn); Advance(.28f);
	TestTrue(TEXT("Reduced motion stops sprite travel"),W->Sprites[0]->GetRenderTransform().Translation.IsNearlyZero()&&W->Sprites[1]->GetRenderTransform().Translation.IsNearlyZero());
	TestTrue(TEXT("Reduced motion keeps damage text stationary"),W->Numbers[0]->GetRenderTransform().Translation.IsNearlyZero()); Capture(TEXT("battle-reduced-motion"));
	Advance(1.5f); Turn.Action=TEXT("seal"); Turn.bEnded=true; Turn.Outcome=EGbOutcome::Bound; W->ApplyTurn(Turn); Advance(2);
	W->ShowStart(Start); Advance(.1f); TestEqual(TEXT("New battle restores previously sealed sprite opacity"),W->Sprites[0]->GetRenderOpacity(),1.f);
	Start.View.bCanSalve=false; Start.View.Salves=0; W->ShowStart(Start); Advance(.1f); Capture(TEXT("battle-disabled"));
	TestFalse(TEXT("Unavailable supply stays disabled"),W->SalveButton->GetIsEnabled());
	W->ShowProblem(TEXT("Battle ended"),true); Advance(2); TestFalse(TEXT("Terminal error leaves controls locked"),W->AttackButton->GetIsEnabled());
	TestEqual(TEXT("Terminal error offers return"),W->EndPanel->GetVisibility(),ESlateVisibility::Visible);
	Settings->bReducedBattleMotion=OriginalReduced; W->RemoveFromRoot(); return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
// The encounter poster: renders at 1080p and 720p, verdict chip follows the matchup, Enter starts the fight.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbEncounterPresentationTest,"Grudgebound.Ui.EncounterPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbEncounterPresentationTest::RunTest(const FString&)
{
	UGbEncounterWidget* W=NewObject<UGbEncounterWidget>(); W->AddToRoot(); W->Initialize();
	if(!W->WidgetTree->RootWidget) W->BuildTree();
	if(!TestNotNull(TEXT("Encounter built"),W->MatchChipText.Get())){W->RemoveFromRoot();return false;}
	const auto Slate=W->TakeWidget(); W->SetVisibility(ESlateVisibility::Visible);
	FGbBattleStart Start;
	Start.Wild.Id=TEXT("riot-bell"); Start.Wild.Name=TEXT("Riot Bell"); Start.Wild.Emotion=TEXT("rage"); Start.Wild.Omen=TEXT("calamity");
	Start.Wild.BornFrom=TEXT("a Ward where PvP never stops");
	Start.View.Wild.SpeciesId=TEXT("riot-bell"); Start.View.Player.SpeciesId=TEXT("weepwick"); Start.View.Player.Name=TEXT("Weepwick");
	Start.Lines={TEXT("Encounter line"),TEXT("Kessa line")};
	FWidgetRenderer Renderer(true);
	const auto Capture=[&](const TCHAR* Name)
	{
		// Let the shared open fade finish, or the capture is the panel at zero opacity.
		for(int32 T=0;T<40;++T) W->NativeTick(FGeometry(),.02f);
		TArray<UWidget*> Now; W->WidgetTree->GetAllWidgets(Now);
		for(UWidget* Widget:Now)if(UImage* Image=Cast<UImage>(Widget))if(UTexture2D* Texture=Cast<UTexture2D>(Image->GetBrush().GetResourceObject()))Texture->WaitForPendingInitOrStreaming();
		FlushRenderingCommands();
		for(const FIntPoint Size:{FIntPoint(1920,1080),FIntPoint(1280,720)})
		{
			Renderer.DrawWidget(Slate,FVector2D(Size)); Slate->SlatePrepass(1);
			UTextureRenderTarget2D* Target=Renderer.DrawWidget(Slate,FVector2D(Size));
			if(!TestNotNull(TEXT("Encounter renders"),Target))continue;
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
			TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Png);
			const FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BattleReview"),FString::FromInt(Size.Y)); IFileManager::Get().MakeDirectory(*Dir,true);
			TestTrue(TEXT("Encounter PNG saved"),FFileHelper::SaveArrayToFile(Png,*FPaths::Combine(Dir,FString(Name)+TEXT(".png"))));
		}
	};
	Start.Matchup=TEXT("disadvantage"); W->Show(Start);
	TestEqual(TEXT("Disadvantage chip"),W->MatchChipText->GetText().ToString(),FString(TEXT("Resists")));
	TestNotNull(TEXT("Wild art imported"),Cast<UTexture2D>(W->WildImage->GetBrush().GetResourceObject()));
	TestTrue(TEXT("Wild art fits its plate at a whole-number scale"),W->WildImage->GetBrush().ImageSize.X<=312.f&&FMath::Fmod(W->WildImage->GetBrush().ImageSize.X,64.f)==0.f);
	Capture(TEXT("encounter-disadvantage"));
	Start.Matchup=TEXT("advantage"); W->Show(Start);
	TestEqual(TEXT("Advantage chip"),W->MatchChipText->GetText().ToString(),FString(TEXT("Advantage")));
	Capture(TEXT("encounter-advantage"));
	bool bFought=false; W->OnFight.AddLambda([&bFought](){bFought=true;});
	W->NativeOnPreviewKeyDown(FGeometry(),FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
	TestTrue(TEXT("Enter starts the fight"),bFought);
	W->RemoveFromRoot(); return true;
}
#endif
