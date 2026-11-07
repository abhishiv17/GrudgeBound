#if WITH_DEV_AUTOMATION_TESTS
#include "GbCoopMission.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbCoopClueTest,"Grudgebound.Coop.UniqueSharedClues",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbCoopClueTest::RunTest(const FString&)
{
	FGbBellMission M;
	TestFalse(TEXT("Unbriefed parties cannot skip Arrival"),GbBell::Collect(M,EGbBellObject::ClueNotice));
	M.Phase=EGbBellPhase::Search;
	TestFalse(TEXT("An NPC cannot be submitted as a clue"),GbBell::Collect(M,EGbBellObject::Mira));
	TestTrue(TEXT("Any-order investigation accepts the seal"),GbBell::Collect(M,EGbBellObject::ClueSeal));
	TestFalse(TEXT("Two players cannot claim the same clue twice"),GbBell::Collect(M,EGbBellObject::ClueSeal));
	TestEqual(TEXT("Unique clue is one shared bit"),static_cast<int32>(M.Clues),4);
	GbBell::Collect(M,EGbBellObject::ClueNotice);
	TestEqual(TEXT("Two clues do not open the ritual"),M.Phase,EGbBellPhase::Search);
	GbBell::Collect(M,EGbBellObject::ClueContract);
	TestEqual(TEXT("All three unlock the shared ritual"),M.Phase,EGbBellPhase::Ritual);
	TestFalse(TEXT("Later phases cannot recollect evidence"),GbBell::Collect(M,EGbBellObject::ClueNotice));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbCoopVoteTest,"Grudgebound.Coop.PartyEnding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbCoopVoteTest::RunTest(const FString&)
{
	using E=EGbBellEnding;
	TestEqual(TEXT("No votes cannot produce an ending"),GbBell::ResolveVotes({}),E::None);
	TestEqual(TEXT("Ties have a deterministic resolution"),GbBell::ResolveVotes({E::ReturnNames,E::BreakBell}),E::ReturnNames);
	TestEqual(TEXT("A majority can choose the other ending"),GbBell::ResolveVotes({E::BreakBell,E::BreakBell,E::ReturnNames}),E::BreakBell);
	TestEqual(TEXT("Missing choices are not votes"),GbBell::ResolveVotes({E::None}),E::None);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbCoopScaleTest,"Grudgebound.Coop.ChannelScaling",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGbCoopScaleTest::RunTest(const FString&)
{
	TestEqual(TEXT("Solo rehearsal needs one seal"),GbBell::RequiredAnchors(1),1);
	for (int32 Party=2;Party<=4;++Party) TestEqual(TEXT("Co-op needs different witness seals"),GbBell::RequiredAnchors(Party),2);
	TestEqual(TEXT("A disconnect to solo cannot softlock the ritual"),GbBell::RequiredAnchors(1),1);
	return true;
}
#endif
