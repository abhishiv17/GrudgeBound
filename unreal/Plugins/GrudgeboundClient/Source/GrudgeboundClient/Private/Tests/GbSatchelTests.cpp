#if WITH_DEV_AUTOMATION_TESTS
#include "GbJson.h"
#include "GbSatchelWidget.h"
#include "GbPlayerSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#if WITH_EDITOR
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"
#include "UnrealClient.h"
#endif

namespace
{
	FGbSatchel Pocket()
	{
		FGbSatchel S; S.bValid = true; S.Carried = { TEXT("starter"), TEXT("tal-1"), TEXT("tal-2"), TEXT("tal-3"), TEXT("tal-4") };
		S.Case = { TEXT("tal-5") }; S.Blank = 3; S.Salves = 1; S.Ember = 60; S.Points = 5;
		for (const FString& Id : S.Carried) { FGbInk I; I.Hp = I.MaxHp = 44; S.Ink.Add(Id, I); }
		FGbInk I; I.Hp = 12; I.MaxHp = 44; S.Ink.Add(TEXT("tal-5"), I);
		return S;
	}
	FGbTalisman Token(const FString& Local, const FString& Server)
	{
		FGbTalisman T; T.Id = Local; T.ChainTokenId = Server; T.SpeciesId = TEXT("cinder-grin"); T.Omen = TEXT("whisper"); return T;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbSatchelJsonTest, "Grudgebound.Satchel.JsonContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbSatchelJsonTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> O;
	TestTrue(TEXT("Final server JSON parses"), GbJson::ParseObject(TEXT(R"({"carried":["starter"],"case":["tal-2"],"supplies":{"blank":3,"gilded":0,"salve":1},"keepsakes":[],"ink":{"starter":{"hp":0,"maxHp":44},"tal-2":{"hp":22,"maxHp":44}},"ember":0,"points":null})"), O));
	const FGbSatchel S = GbJson::Satchel(O);
	TestTrue(TEXT("Satchel is ready only after a valid answer"), S.bValid);
	TestEqual(TEXT("Carried ids"), S.Carried[0], FString(TEXT("starter")));
	TestEqual(TEXT("Case ids"), S.Case[0], FString(TEXT("tal-2")));
	TestEqual(TEXT("Starter blanks come from the server"), S.Blank, 3);
	TestEqual(TEXT("No free gilded supply"), S.Gilded, 0); TestEqual(TEXT("Starter salve"), S.Salves, 1);
	TestEqual(TEXT("A zero balance is known"), S.Ember, 0); TestEqual(TEXT("A null balance stays unknown"), S.Points, -1);
	TestEqual(TEXT("Downed ink stays zero"), S.Ink.FindChecked(TEXT("starter")).Hp, 0);
	TestFalse(TEXT("Zero ink cannot fight"), S.CanFight(TEXT("starter")));
	TestFalse(TEXT("A Case talisman cannot fight"), S.CanFight(TEXT("tal-2")));
	TestTrue(TEXT("No keepsakes yet"), S.Keepsakes.IsEmpty());
	GbJson::ParseObject(TEXT("{}"), O); TestFalse(TEXT("Malformed pockets cannot enable actions"), GbJson::Satchel(O).bValid);
	TestEqual(TEXT("No client-side kit while waiting"), FGbSatchel().Blank, 0);
	GbJson::ParseObject(GbJson::CarryBody(TEXT("tal-2"), false), O);
	TestEqual(TEXT("Carry request keeps the token id"), O->GetStringField(TEXT("tokenId")), FString(TEXT("tal-2")));
	TestFalse(TEXT("Uncarry is a boolean"), O->GetBoolField(TEXT("carry")));
	TestFalse(TEXT("Pocket mutations do not supply an identity"), O->HasField(TEXT("account")));
	GbJson::ParseObject(GbJson::StartBody(TEXT("lantern"), TEXT("cinder-grin"), TEXT("Binder"), TEXT("a.testnet"), TEXT("tal-2")), O);
	TestEqual(TEXT("Two identical species select the exact token"), O->GetStringField(TEXT("tokenId")), FString(TEXT("tal-2")));
	GbJson::ParseObject(TEXT(R"({"player":{"speciesId":"cinder-grin","hp":23,"maxHp":44},"items":{"blank":3,"gilded":0,"salve":1},"canUse":{"seal":true}})"), O);
	const FGbBattleView V = GbJson::BattleView(O);
	TestEqual(TEXT("Battle draws carried-over ink"), V.Player.Hp, 23); TestEqual(TEXT("Battle blank count"), V.BlankSeals, 3);
	TestTrue(TEXT("Server button availability survives parsing"), V.bCanSeal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbSatchelCarryTest, "Grudgebound.Satchel.CarryLimitUi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbSatchelCarryTest::RunTest(const FString&)
{
	UGbSatchelWidget* W = NewObject<UGbSatchelWidget>(); W->AddToRoot(); W->Initialize(); W->BuildTree();
	W->Data = Pocket(); W->SelectedToken = TEXT("tal-5"); W->Redraw();
	TestFalse(TEXT("A sixth carry is disabled in the real native button"), W->CarryButton->GetIsEnabled());
	W->Data.Carried.RemoveAt(4); W->Redraw(); TestTrue(TEXT("A free carried slot enables carrying"), W->CarryButton->GetIsEnabled());
	W->SelectedToken = TEXT("starter"); W->Redraw(); TestTrue(TEXT("Leaving a carried talisman is always possible"), W->CarryButton->GetIsEnabled());
	W->bBusy = true; W->Redraw(); TestFalse(TEXT("Pending mutations cannot be double-submitted"), W->CarryButton->GetIsEnabled());
	W->bBusy = false; W->Data.bValid = false; W->Redraw(); TestFalse(TEXT("An unavailable Satchel cannot carry"), W->CarryButton->GetIsEnabled());
	W->RemoveFromRoot(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbSatchelSalveTest, "Grudgebound.Satchel.SalveButtonUi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbSatchelSalveTest::RunTest(const FString&)
{
	UGbSatchelWidget* W = NewObject<UGbSatchelWidget>(); W->AddToRoot(); W->Initialize(); W->BuildTree();
	W->Data = Pocket(); W->SelectedToken = TEXT("starter"); W->Redraw();
	TestFalse(TEXT("Full ink disables the actual salve button"), W->SalveButton->GetIsEnabled());
	W->Data.Ink[TEXT("starter")].Hp = 10; W->Redraw(); TestTrue(TEXT("A wounded selected talisman can use a salve"), W->SalveButton->GetIsEnabled());
	W->SelectedToken = TEXT("tal-5"); W->Redraw(); TestTrue(TEXT("A wounded Case talisman may be healed outside battle"), W->SalveButton->GetIsEnabled());
	W->Data.Salves = 0; W->Redraw(); TestFalse(TEXT("Zero supplies disable salve use"), W->SalveButton->GetIsEnabled());
	W->Data.Salves = 1; W->Data.Ink.Remove(TEXT("tal-5")); W->Redraw(); TestFalse(TEXT("Unknown ink never spends a salve"), W->SalveButton->GetIsEnabled());
	W->SelectedToken = TEXT("starter"); W->bBusy = true; W->Redraw(); TestFalse(TEXT("Pending actions disable salve use"), W->SalveButton->GetIsEnabled());
	W->RemoveFromRoot(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbSatchelPartnerTest, "Grudgebound.Satchel.PartnerInkGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbSatchelPartnerTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>(); UGbPlayerSubsystem* P = NewObject<UGbPlayerSubsystem>(GI);
	P->Data = NewObject<UGbSaveGame>(); P->SaveSlot.Reset(); // Memory only: never write any real player's save.
	const FGbTalisman Starter = Token(UGbPlayerSubsystem::StarterId, TEXT("")), A = Token(TEXT("local-a"), TEXT("tal-1")), B = Token(TEXT("local-b"), TEXT("tal-2"));
	P->Data->Talismans = { Starter, A, B }; P->Data->PartnerId = A.Id;
	P->Satchel = Pocket(); P->Satchel.Carried = { TEXT("starter"), TEXT("tal-1") }; P->Satchel.Case = { TEXT("tal-2") };
	P->Satchel.Ink[TEXT("starter")].Hp = 0;
	TestEqual(TEXT("Legacy starter maps to the server's reserved id"), UGbPlayerSubsystem::ServerTokenId(Starter), FString(TEXT("starter")));
	TestEqual(TEXT("Same-species partner keeps its specific token"), P->GetPartnerTokenId(), FString(TEXT("tal-1")));
	FString Hint; TestTrue(TEXT("A carried partner with ink may fight"), P->CanStartBattle(Hint));
	P->SetPartner(B.Id); TestEqual(TEXT("The old Case panel cannot bypass carry rules"), P->Data->PartnerId, A.Id);
	P->SetPartner(Starter.Id); TestFalse(TEXT("A downed partner cannot open a fight"), P->CanStartBattle(Hint));
	TestTrue(TEXT("The refusal points to Mender/rest"), Hint.Contains(TEXT("Mender")) && Hint.Contains(TEXT("rest")));
	P->Satchel.Carried.Remove(TEXT("starter")); P->Satchel.Case.Add(TEXT("starter"));
	P->HandleSatchel(P->Satchel); TestEqual(TEXT("Leaving a partner in the Case chooses a carried fallback"), P->GetPartnerTokenId(), FString(TEXT("tal-1")));
	P->Satchel.Carried.Reset(); P->HandleSatchel(P->Satchel);
	TestTrue(TEXT("An empty carried pocket has no hidden starter fallback"), P->GetPartnerTokenId().IsEmpty());
	TestFalse(TEXT("An empty pocket cannot fight"), P->CanStartBattle(Hint));
	P->Satchel = FGbSatchel(); TestFalse(TEXT("Unloaded or cleared profile data cannot fight"), P->CanStartBattle(Hint));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbSatchelSupplyTradeTest, "Grudgebound.Satchel.SupplyReceiptIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbSatchelSupplyTradeTest::RunTest(const FString&)
{
	UGbPlayerSubsystem* P = NewObject<UGbPlayerSubsystem>(NewObject<UGameInstance>());
	P->Data = NewObject<UGbSaveGame>(); P->SaveSlot.Reset(); P->Data->Talismans = { Token(UGbPlayerSubsystem::StarterId, TEXT("")) };
	FGbTrade T; T.bBuy = true; T.JobId = TEXT("supply"); T.ItemId = TEXT("ink-salve"); T.Price = 10;
	P->HandleTrade(T); FGbChainReceipt R; R.Status = TEXT("done"); P->HandleTradeReceipt(T.JobId, R);
	TestEqual(TEXT("Buying a supply creates no empty talisman"), P->Data->Talismans.Num(), 1);
	TestEqual(TEXT("Buying a supply does not consume a local talisman id"), P->Data->NextTalisman, 1);
	TestEqual(TEXT("Supply payment updates the purse until refreshed"), P->Data->Ember, 50);
	T.JobId = TEXT("failed"); P->HandleTrade(T); R.Status = TEXT("failed"); P->HandleTradeReceipt(T.JobId, R);
	TestEqual(TEXT("Failed supply payment does not spend local Ember"), P->Data->Ember, 50);
	T.JobId = TEXT("real-talisman"); T.ItemId = TEXT("talisman-dread"); T.SpeciesId = TEXT("hushmoth"); T.Omen = TEXT("whisper");
	P->HandleTrade(T); R.Status = TEXT("done"); R.TokenId = TEXT("tal-7"); P->HandleTradeReceipt(T.JobId, R);
	TestEqual(TEXT("Actual talisman purchases still enter the Case"), P->Data->Talismans.Num(), 2);
	TestEqual(TEXT("Minted id is retained"), P->Data->Talismans.Last().ChainTokenId, FString(TEXT("tal-7")));
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbSatchelVisualTest, "Grudgebound.Satchel.FourPocketsRender", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbSatchelVisualTest::RunTest(const FString&)
{
	UGbSatchelWidget* W = NewObject<UGbSatchelWidget>(); W->AddToRoot(); W->Initialize(); W->BuildTree();
	W->Data = Pocket(); W->PartnerToken = TEXT("starter"); W->SelectedToken = TEXT("tal-5"); W->MenderPrice = 12;
	W->Names = { {TEXT("starter"),TEXT("Cinder Grin")}, {TEXT("tal-1"),TEXT("Hushmoth")}, {TEXT("tal-2"),TEXT("Weepwick")}, {TEXT("tal-3"),TEXT("Porcelain Widow")}, {TEXT("tal-4"),TEXT("Debt Hound")}, {TEXT("tal-5"),TEXT("Lantern Drowned")} };
	W->Redraw();
	const TSharedRef<SWidget> Slate = W->TakeWidget(); FWidgetRenderer Renderer(false);
	const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SatchelPreview")); IFileManager::Get().MakeDirectory(*Folder, true);
	for (int32 I = 0; I < 4; ++I)
	{
		W->Pockets->SetActiveWidgetIndex(I);
		W->Redraw();
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(Slate, FVector2D(1040, 720));
		if (!TestNotNull(TEXT("Native Satchel pocket renders"), Target)) { W->RemoveFromRoot(); return false; }
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Pocket pixels can be read"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) { W->RemoveFromRoot(); return false; }
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1040, 720, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const FString File = FPaths::Combine(Folder, FString::Printf(TEXT("Pocket-%d.png"), I + 1));
		TestTrue(TEXT("Pocket preview saved"), FFileHelper::SaveArrayToFile(Png, *File)); AddInfo(File);
	}
	W->RemoveFromRoot(); return true;
}
#endif
#endif
