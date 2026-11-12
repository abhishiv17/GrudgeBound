#if WITH_DEV_AUTOMATION_TESTS
#include "GbStory.h"
#include "GbStorySubsystem.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "GbIntroWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"
#include "UnrealClient.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbStoryOrderTest, "Grudgebound.Story.ChapterOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbStoryOrderTest::RunTest(const FString&)
{
	FGbStoryProgress P;
	FGbWorldState W; W.bValid = true; W.Day = 99;
	TestFalse(TEXT("A day number cannot invent a bankruptcy"), GbStory::Observe(P, W));
	FGbTownEvent Calamity; Calamity.Type = TEXT("calamity"); Calamity.Ward = TEXT("tide");
	FGbTownEvent Debt; Debt.Type = TEXT("bankruptcy"); Debt.Ward = TEXT("tide");
	TestTrue(TEXT("A late player remembers a Calamity"), GbStory::Observe(P, Calamity));
	TestTrue(TEXT("A late player remembers a bankruptcy"), GbStory::Observe(P, Debt));
	TestEqual(TEXT("Arrival comes first even in an old world"), GbStory::Chapter(P), 1);
	TestFalse(TEXT("An early action cannot finish the final chapter"), GbStory::Respond(P));
	P.bFirstBind = true;
	TestEqual(TEXT("The actual debt beat follows the first binding"), GbStory::Chapter(P), 2);
	TestTrue(TEXT("Objective asks for the Broker"), GbStory::Objective(P).Contains(TEXT("Broker")));
	P.bVisitedBroker = true;
	TestEqual(TEXT("An earlier Broker visit cannot acknowledge a later bankruptcy"), GbStory::Chapter(P), 2);
	P.bDebtAcknowledged = true;
	TestEqual(TEXT("Chapter 3 follows acknowledgement of the debt beat"), GbStory::Chapter(P), 3);
	TestTrue(TEXT("The objective names the actual Ward"), GbStory::Objective(P).Contains(TEXT("Tide Ward")));
	TestTrue(TEXT("A subsequent successful action responds"), GbStory::Respond(P));
	TestFalse(TEXT("Duplicate responses do not repeat the transition"), GbStory::Respond(P));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbStoryFactsTest, "Grudgebound.Story.RealWorldFacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbStoryFactsTest::RunTest(const FString&)
{
	FGbStoryProgress P;
	FGbTownEvent E; E.Type = TEXT("bind"); E.Text = TEXT("A Calamity and bankruptcy mentioned in unrelated dialogue");
	TestFalse(TEXT("Dialogue keywords do not create a chapter event"), GbStory::Observe(P, E));
	FGbWorldState W; FGbMerchantSummary Merchant; Merchant.bClosed = true; Merchant.Ward = TEXT("ash"); W.Merchants.Add(Merchant);
	TestFalse(TEXT("An invalid snapshot is ignored"), GbStory::Observe(P, W));
	W.bValid = true;
	TestTrue(TEXT("A genuinely closed merchant restores a missed debt beat"), GbStory::Observe(P, W));
	TestEqual(TEXT("The debt retains its real Ward"), P.DebtWard, FString(TEXT("ash")));
	TestFalse(TEXT("Repeated snapshots do not repeat the beat"), GbStory::Observe(P, W));
	TestFalse(TEXT("A closed shop does not invent a distinct Calamity event"), P.bCalamity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbStorySaveTest, "Grudgebound.Story.SaveIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbStorySaveTest::RunTest(const FString&)
{
	const FString A = GbStory::SaveSlot(TEXT("a.testnet"), TEXT("http://localhost:8787"));
	TestNotEqual(TEXT("Accounts have separate story slots"), A, GbStory::SaveSlot(TEXT("b.testnet"), TEXT("http://localhost:8787")));
	TestNotEqual(TEXT("Servers have separate story slots"), A, GbStory::SaveSlot(TEXT("a.testnet"), TEXT("https://game.example")));
	TestEqual(TEXT("The same identity resumes its slot"), A, GbStory::SaveSlot(TEXT("a.testnet"), TEXT("http://localhost:8787")));
	TestFalse(TEXT("No URL punctuation leaks into a path"), A.Contains(TEXT("/")));
	UGbStorySaveGame* Save = NewObject<UGbStorySaveGame>();
	Save->Progress.bIntroSeen = true; Save->Progress.bFirstBind = true; Save->Progress.bBankruptcy = true; Save->Progress.DebtWard = TEXT("tide");
	Save->Progress.bDebtAcknowledged = true;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Narrative history serializes without touching real player saves"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	UGbStorySaveGame* Restored = Cast<UGbStorySaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Story save restores"), Restored)) return false;
	TestTrue(TEXT("Intro remains completed after restart"), Restored->Progress.bIntroSeen);
	TestTrue(TEXT("Binding remains remembered"), Restored->Progress.bFirstBind);
	TestTrue(TEXT("The acknowledged debt beat survives restart"), Restored->Progress.bDebtAcknowledged);
	TestEqual(TEXT("World history remains remembered"), Restored->Progress.DebtWard, FString(TEXT("tide")));
	TestFalse(TEXT("Uncompleted responses remain incomplete"), Restored->Progress.bResponded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbStoryReceiptsTest, "Grudgebound.Story.ConfirmedActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbStoryReceiptsTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UGbStorySubsystem* Story = NewObject<UGbStorySubsystem>(GI);
	Story->Data = NewObject<UGbStorySaveGame>(); // No active save slot: no writes to anyone's real story.
	FGbBattleTurn Turn; Turn.bEnded = true; Turn.Outcome = EGbOutcome::Bound; Turn.Points = 5; Turn.bHasChain = true; Turn.BattleId = TEXT("b1");
	Story->HandleBattle(Turn);
	TestFalse(TEXT("An unconfirmed online bind does not advance the story"), Story->Data->Progress.bFirstBind);
	FGbChainReceipt R; R.Status = TEXT("failed"); Story->HandleBattleReceipt(Turn.BattleId, R);
	TestFalse(TEXT("A failed mint does not advance Arrival"), Story->Data->Progress.bFirstBind);
	Turn.BattleId = TEXT("b2"); Story->HandleBattle(Turn); R.Status = TEXT("done"); R.TokenId = TEXT("tal-2"); Story->HandleBattleReceipt(Turn.BattleId, R);
	TestTrue(TEXT("A confirmed owned binding advances Arrival"), Story->Data->Progress.bFirstBind);
	auto& P = Story->Data->Progress; P.bVisitedBroker = true; P.bDebtAcknowledged = true; P.bBankruptcy = true; P.bCalamity = true;
	FGbRuleVerdict V; V.JobId = TEXT("rejected"); V.bAccepted = false; Story->HandleVerdict(V); Story->HandleRuleReceipt(V.JobId, R);
	TestFalse(TEXT("A rejected proposal does not answer the Calamity"), P.bResponded);
	V.JobId = TEXT("failed"); V.bAccepted = true; Story->HandleVerdict(V); R.Status = TEXT("failed"); Story->HandleRuleReceipt(V.JobId, R);
	TestFalse(TEXT("A failed ledger write does not answer the Calamity"), P.bResponded);
	Turn.Outcome = EGbOutcome::Fled; Turn.Points = 0; Story->HandleBattle(Turn);
	TestFalse(TEXT("Fleeing does not complete the response"), P.bResponded);
	V.JobId = TEXT("accepted"); Story->HandleVerdict(V); R.Status = TEXT("done"); Story->HandleRuleReceipt(V.JobId, R);
	TestTrue(TEXT("An accepted and recorded rule completes the response"), P.bResponded);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbStoryIntroTest, "Grudgebound.StoryVisual.IntroPanels", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbStoryIntroTest::RunTest(const FString&)
{
	UGbIntroWidget* Intro = NewObject<UGbIntroWidget>();
	Intro->AddToRoot(); Intro->Initialize();
	// Offscreen widgets have no local player; Initialize builds the same native tree.
	if (!TestNotNull(TEXT("Intro tree built"), Intro->WidgetTree->RootWidget.Get())) { Intro->RemoveFromRoot(); return false; }
	Intro->Open();
	const TSharedRef<SWidget> SlateWidget = Intro->TakeWidget();
	FWidgetRenderer Renderer(false);
	const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("StoryPreview"));
	IFileManager::Get().MakeDirectory(*Folder, true);
	int32 Finished = 0; Intro->OnFinished.AddLambda([&Finished]() { ++Finished; });
	for (int32 I = 0; I < 4; ++I)
	{
		TestEqual(TEXT("Panel order follows the story script"), Intro->PanelIndex, I);
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateWidget, FVector2D(1920, 1080));
		if (!TestNotNull(TEXT("Native intro renders to a target"), Target)) { Intro->RemoveFromRoot(); return false; }
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags; ReadFlags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Rendered intro pixels are readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) { Intro->RemoveFromRoot(); return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(1920, 1080, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const FString File = FPaths::Combine(Folder, FString::Printf(TEXT("Intro-%d.png"), I + 1));
		TestTrue(TEXT("Native preview saved"), FFileHelper::SaveArrayToFile(Png, *File));
		AddInfo(File);
		TestEqual(TEXT("The ribbon names the page"), Intro->Title->GetText().ToString(), GbStory::IntroPanels()[I].Title);
		if (I == 3) TestEqual(TEXT("Final page enters the Ward"), Intro->NextLabel->GetText().ToString(), FString(TEXT("Enter Lantern Ward")));
		Intro->HandleNext();
	}
	TestEqual(TEXT("Four Next presses close the intro once"), Finished, 1);
	Intro->Open(); TestEqual(TEXT("Replay starts at the first panel"), Intro->PanelIndex, 0);
	Intro->HandleSkip(); TestEqual(TEXT("Skipping closes the intro"), Finished, 2);
	Intro->OnFinished.Clear(); Intro->RemoveFromRoot();
	return true;
}
#endif
#endif
