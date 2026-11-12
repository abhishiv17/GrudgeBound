// Levels and the rival as the client reads them: Satchel levels/rival/tideOpen, a rival battle start, and the experience
// line at the end of a fight. Older servers that send none of these keep level 1, no rival and an open ferry.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GbJson.h"
#include "GbTalk.h"
#include "GbGuide.h"
#include "GbTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbRivalJsonTest, "Grudgebound.Satchel.LevelsAndRival", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbRivalJsonTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> O;
	TestTrue(TEXT("Satchel parses"), GbJson::ParseObject(TEXT(R"({"carried":["starter"],"case":[],"supplies":{"blank":1,"gilded":0,"salve":1},"ink":{"starter":{"hp":30,"maxHp":44}},
		"levels":{"starter":{"level":3,"xp":52,"next":75}},"tideOpen":false,
		"rival":{"name":"Dace Marrow","where":"ferry","stage":2,"ready":true,"level":4,"speciesId":"weepwick","line":"Show me you have too."}})"), O));
	const FGbSatchel S = GbJson::Satchel(O);
	TestTrue(TEXT("Valid"), S.bValid);
	TestEqual(TEXT("Level read"), S.LevelOf(TEXT("starter")), 3);
	TestEqual(TEXT("Unknown talisman is level 1"), S.LevelOf(TEXT("tal-9")), 1);
	TestEqual(TEXT("Experience to next"), S.Levels.FindRef(TEXT("starter")).Next, 75);
	TestFalse(TEXT("Ferry closed"), S.bTideOpen);
	TestTrue(TEXT("Rival read"), S.Rival.bValid && S.Rival.bReady && S.Rival.Stage == 2 && S.Rival.Where == TEXT("ferry"));

	TestTrue(TEXT("Old Satchel parses"), GbJson::ParseObject(TEXT(R"({"carried":[],"case":[],"supplies":{},"ink":{}})"), O));
	const FGbSatchel Old = GbJson::Satchel(O);
	TestTrue(TEXT("Older server: ferry open"), Old.bTideOpen);
	TestFalse(TEXT("Older server: no rival"), Old.Rival.bValid);

	TestTrue(TEXT("Start parses"), GbJson::ParseObject(TEXT(R"({"battleId":"b1","ward":"lantern","wild":{"id":"weepwick","name":"Weepwick"},"matchup":"even",
		"rival":{"stage":2,"name":"Dace Marrow"},"lines":["Dace Marrow: \"Show me.\""],
		"view":{"turn":0,"player":{"speciesId":"weepwick","name":"Weepwick","hp":44,"maxHp":52,"level":3},"wild":{"speciesId":"weepwick","name":"Weepwick","hp":56,"maxHp":56,"level":4},
		"items":{},"chances":{},"canUse":{"attack":true,"seal":false,"flee":false}}})"), O));
	const FGbBattleStart B = GbJson::BattleStart(O);
	TestEqual(TEXT("Rival stage"), B.RivalStage, 2);
	TestEqual(TEXT("Foe level"), B.View.Wild.Level, 4);
	TestFalse(TEXT("No sealing a rival's partner"), B.View.bCanSeal);
	TestFalse(TEXT("No running from a rival"), B.View.bCanFlee);

	TestTrue(TEXT("Turn parses"), GbJson::ParseObject(TEXT(R"({"battleId":"b1","action":"attack","lines":[],"outcome":"won","points":20,
		"progress":{"gained":55,"level":5,"before":4,"next":150,"xp":110},
		"view":{"turn":5,"outcome":"won","player":{"name":"Weepwick","hp":10,"maxHp":52},"wild":{"name":"Weepwick","hp":0,"maxHp":56},"items":{},"chances":{},"canUse":{}}})"), O));
	const FGbBattleTurn T = GbJson::BattleTurn(O);
	TestTrue(TEXT("Ended"), T.bEnded);
	TestEqual(TEXT("Experience"), T.XpGained, 55);
	TestTrue(TEXT("Level up"), T.Level == 5 && T.LevelBefore == 4);
	TestEqual(TEXT("Missing level is 1"), T.View.Player.Level, 1);
	TestTrue(TEXT("Rival start body"), GbJson::StartBody(TEXT("lantern"), TEXT("weepwick"), TEXT("Ari"), FString(), TEXT("starter"), 2).Contains(TEXT("\"rival\":2")));
	TestFalse(TEXT("Wild start body has no rival"), GbJson::StartBody(TEXT("lantern"), TEXT("weepwick"), TEXT("Ari")).Contains(TEXT("rival")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbGossipTest, "Grudgebound.Talk.Gossip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbGossipTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> O;
	TestTrue(TEXT("Satchel parses"), GbJson::ParseObject(TEXT(R"({"carried":[],"case":[],"supplies":{},"ink":{},
		"exploration":{"version":1,"flags":[],"equipped":"","tracked":"rival","gear":[],"journal":[],"notes":[],"gossip":["Mira opened the storehouse.","Mira beat Dace."]}})"), O));
	const FGbSatchel S = GbJson::Satchel(O);
	TestEqual(TEXT("Gossip read"), S.Gossip.Num(), 2);
	// Without gossip the old three-line rotation is unchanged; with it every fourth line is about the player.
	TestEqual(TEXT("No gossip: same rotation"), GbTalk::Line(TEXT("lantern"), 2, GbTalk::EMood::Calm, 1, 3), GbTalk::Line(TEXT("lantern"), 2, GbTalk::EMood::Calm, 1, 3, TArray<FString>()));
	TestEqual(TEXT("Fourth line is gossip"), GbTalk::Line(TEXT("lantern"), 0, GbTalk::EMood::Calm, 1, 3, S.Gossip), S.Gossip[0]);
	TestEqual(TEXT("Next round, next deed"), GbTalk::Line(TEXT("lantern"), 0, GbTalk::EMood::Calm, 1, 7, S.Gossip), S.Gossip[1]);
	TestEqual(TEXT("Another walker starts elsewhere"), GbTalk::Line(TEXT("lantern"), 1, GbTalk::EMood::Calm, 1, 3, S.Gossip), S.Gossip[1]);
	TestNotEqual(TEXT("Other turns are not gossip"), GbTalk::Line(TEXT("lantern"), 0, GbTalk::EMood::Calm, 1, 0, S.Gossip), S.Gossip[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbGuideTest, "Grudgebound.Satchel.Guide", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGbGuideTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> O;
	TestTrue(TEXT("Satchel parses"), GbJson::ParseObject(TEXT(R"({"carried":[],"case":[],"supplies":{},"ink":{},"profile":{"version":1,"complete":true,"name":"Mira"},
		"exploration":{"version":1,"flags":[],"equipped":"","tracked":"net","gear":[],"notes":[],
		"journal":[{"id":"lamp","title":"A","status":"complete","objective":"x","hint":"y","target":""},{"id":"net","title":"B","status":"active","objective":"x","hint":"y","target":"find:reed-bundle"}],
		"tips":[{"id":"battle","text":"Attack, then seal.","seen":true},{"id":"tide","text":"Tide Ward.","seen":false},{"id":"","text":"dropped"}],
		"lore":[{"id":"grudge","title":"Grudge","text":"Rage, Sorrow and Dread."}]}})"), O));
	const FGbSatchel S = GbJson::Satchel(O);
	TestEqual(TEXT("Tips read, blank ones dropped"), S.Tips.Num(), 2);
	TestTrue(TEXT("Lore read"), S.Lore.Num() == 1 && S.Lore[0].Title == TEXT("Grudge"));
	TestTrue(TEXT("Seen flag read"), S.Tips[0].bSeen && !S.Tips[1].bSeen);
	const FGbJournalEntry* J = GbGuide::Tracked(S);
	TestTrue(TEXT("Tracked task has a target"), J && J->Target == TEXT("find:reed-bundle"));
	FGbSatchel Done = S; Done.Tracked = TEXT("lamp");
	TestNull(TEXT("A finished task points nowhere"), GbGuide::Tracked(Done));
	FGbSatchel NotArrived = S; NotArrived.Profile.bComplete = false; NotArrived.Profile.bLegacyStarter = false;
	TestNull(TEXT("No beacon before the arrival is finished"), GbGuide::Tracked(NotArrived));
	TestTrue(TEXT("Regions"), GbGuide::RegionOf(FVector(-15000, 0, 0)) == GbGuide::ERegion::Tide && GbGuide::RegionOf(FVector(0, 0, 0)) == GbGuide::ERegion::Lantern
		&& GbGuide::RegionOf(FVector(28000, 0, 0)) == GbGuide::ERegion::Room);
	FVector At; FString Via;
	TestFalse(TEXT("No world, no beacon"), GbGuide::Resolve(nullptr, TEXT("rival"), FVector::ZeroVector, At, Via));
	return true;
}
#endif
