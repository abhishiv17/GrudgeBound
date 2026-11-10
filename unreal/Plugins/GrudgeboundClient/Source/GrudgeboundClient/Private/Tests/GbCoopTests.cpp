#if WITH_DEV_AUTOMATION_TESTS
#include "GbCoopMission.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/GameSession.h"
#include "InputCoreTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CsvProfiler.h"

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

// Runs only in actual -game host/client processes, never as a fake editor-world network test.
// Launch both with -GbCoopBot -GbCoopBotManualKeys. The normal R/2 input bindings send the real RPCs.
namespace
{
 class FGbCoopNetworkRun : public IAutomationLatentCommand
 {
 public:
  explicit FGbCoopNetworkRun(FAutomationTestBase* InTest):Test(InTest),Deadline(FPlatformTime::Seconds()+360) {}
  virtual bool Update() override
  {
   if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Co-op recovery/Ending B timed out")); return true; }
   UWorld* World=nullptr;
   for (const auto& Context:GEngine->GetWorldContexts()) if (Context.World() && Context.World()->IsGameWorld() && Context.World()->GetGameState<AGbCoopState>()) { World=Context.World(); break; }
   if (!World) return false;
   auto* State=World->GetGameState<AGbCoopState>(); auto* PC=Cast<AGbCoopController>(World->GetFirstPlayerController());
   auto* Pawn=PC?Cast<AGbCoopCharacter>(PC->GetPawn()):nullptr; if (!Pawn) return false;
   const auto& M=State->Mission; const double Now=State->GetServerWorldTimeSeconds();
   // Editor processes can start at different speeds under paging. Keep the real world paused until
   // both participants are connected, rather than accidentally testing a solo boss before the join.
   if (World->GetAuthGameMode<AGbCoopMode>() && !bPartyReady)
   {
    if (State->PlayerArray.Num()<2) { if (!bWaitingForParty) { PC->SetPause(true); bWaitingForParty=true; } return false; }
    if (bWaitingForParty) PC->SetPause(false);
    bPartyReady=true;
   }
   if (!bCapture)
   {
    const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Profiling/CSV")));
    IFileManager::Get().MakeDirectory(*Folder,true);
    FCsvProfiler::Get()->BeginCapture(-1,Folder,FString::Printf(TEXT("T4-%s-%u"),World->GetNetMode()==NM_Client?TEXT("client"):TEXT("host"),FPlatformProcess::GetCurrentProcessId()));
    bCapture=true;
   }
   if (ReleaseKey.IsValid()) { PC->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,ReleaseKey,IE_Released,0.f,false,FPlatformTime::Cycles64())); ReleaseKey=FKey(); }
   if (!bWiped && World->GetAuthGameMode<AGbCoopMode>() && M.Phase==EGbBellPhase::Boss)
   {
    int32 Count=0; for (TActorIterator<AGbCoopCharacter> It(World);It;++It) if (It->GetController()) { It->Hurt(100); ++Count; }
    Test->TestEqual(TEXT("Real host wipes both connected pawns"),Count,2); bWiped=true;
    UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST Wipe Party=%d"),State->PlayerArray.Num());
   }
   if (M.Phase==EGbBellPhase::Defeat)
   {
    if (!bSawDefeat) { DefeatAt=Now; bSawDefeat=true; UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST DefeatObserved Clues=%d"),M.Clues); }
    if (!bRetryKey && Now-DefeatAt>=1) { Press(PC,EKeys::R); bRetryKey=true; }
   }
   if (bSawDefeat && M.Phase==EGbBellPhase::Ritual && !bSawRetry)
   {
    bSawRetry=true; Test->TestEqual(TEXT("R retry retains all shared clues"),int32(M.Clues),7);
    UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST RetryObserved Phase=2 Clues=%d"),M.Clues);
   }
   if (bSawRetry && M.Phase==EGbBellPhase::Boss)
   {
    const double Until=M.PulseAt-Now;
    if (!bTrackedWarning && Until>0 && Until<=1.3)
    { bTrackedWarning=true; PulseAt=M.PulseAt; HealthBefore=Pawn->Health; UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST Warning Distance=%.1f Seconds=%.2f"),FVector::Dist2D(Pawn->GetActorLocation(),GbBell::Position(EGbBellObject::Collector)),Until); }
    if (bTrackedWarning && !bEscaped && Now>=PulseAt+.2)
    {
     bEscaped=true; Test->TestEqual(TEXT("Walking out during warning avoids toll damage"),Pawn->Health,HealthBefore);
     UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST TollEscaped Distance=%.1f Health=%.0f"),FVector::Dist2D(Pawn->GetActorLocation(),GbBell::Position(EGbBellObject::Collector)),Pawn->Health);
    }
   }
   if (M.Phase==EGbBellPhase::Decision && !bVoteKey) { Press(PC,EKeys::Two); bVoteKey=true; UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST InputKey=2")); }
   if (M.Phase==EGbBellPhase::Complete)
   {
    if (!bFinished)
    {
     Test->TestTrue(TEXT("Defeat replicated before R"),bSawDefeat);
     Test->TestTrue(TEXT("Retry returned to Ritual"),bSawRetry);
     Test->TestTrue(TEXT("A real toll warning/escape was observed"),bEscaped);
     Test->TestEqual(TEXT("Key 2 through normal input chooses Ending B"),M.Ending,EGbBellEnding::BreakBell);
     Test->TestEqual(TEXT("Two actual players finish"),State->PlayerArray.Num(),2);
     UE_LOG(LogTemp,Display,TEXT("GB_COOP_RESULT Complete Ending=%d Party=%d"),int32(M.Ending),State->PlayerArray.Num());
     CsvResult=FCsvProfiler::Get()->EndCapture(); bFinished=true;
    }
    if (CsvResult.IsReady()) { Test->AddInfo(CsvResult.Get()); return true; }
   }
   return false;
  }
 private:
  void Press(AGbCoopController* PC,FKey Key) { PC->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,Key,IE_Pressed,1.f,false,FPlatformTime::Cycles64())); ReleaseKey=Key; }
  FAutomationTestBase* Test;
  double Deadline,DefeatAt=0,PulseAt=0;
  float HealthBefore=0;
  bool bCapture=false,bWiped=false,bSawDefeat=false,bRetryKey=false,bSawRetry=false,bTrackedWarning=false,bEscaped=false,bVoteKey=false,bFinished=false;
  bool bWaitingForParty=false,bPartyReady=false;
  FKey ReleaseKey;
  TSharedFuture<FString> CsvResult;
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbCoopNetworkTest,"Grudgebound.Coop.NetworkRecoveryAndEndingB",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGbCoopNetworkTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FGbCoopNetworkRun(this));
 return true;
}

namespace
{
 // Host-only check: pause after real shared clues so a new process has time to join mid-Search.
 // A second local player fills slot four, leaving room in RAM for a separate fifth-player connection.
 class FGbCoopPartyRun : public IAutomationLatentCommand
 {
 public:
  explicit FGbCoopPartyRun(FAutomationTestBase* InTest):Test(InTest),Deadline(FPlatformTime::Seconds()+360)
  { FParse::Value(FCommandLine::Get(),TEXT("abslog="),LogPath); }
  virtual bool Update() override
  {
   const double Now=FPlatformTime::Seconds();
   if (Now>Deadline) { Test->AddError(TEXT("Late-join/party-limit process check timed out")); return true; }
   UWorld* World=nullptr;
   for (const auto& Context:GEngine->GetWorldContexts()) if (Context.World() && Context.World()->GetNetMode()==NM_ListenServer && Context.World()->GetGameState<AGbCoopState>()) { World=Context.World(); break; }
   if (!World) return false;
   auto* State=World->GetGameState<AGbCoopState>(); auto* PC=World->GetFirstPlayerController();
   if (!PC || !PC->GetPawn()) return false;
   if (Stage==0 && State->Mission.Phase==EGbBellPhase::Search && State->Mission.Clues>0 && State->Mission.Clues<7 && State->PlayerArray.Num()==2)
   {
    for (TActorIterator<AGbCoopController> It(World);It;++It) if (!It->IsLocalController()) FirstRemote=*It;
    if (!Test->TestTrue(TEXT("Pause the actual Search for a late join"),PC->SetPause(true))) return true;
    SnapshotClues=State->Mission.Clues;
    Stage=1; UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST SearchPaused Clues=%d Party=2"),SnapshotClues);
   }
   if (Stage==1 && State->PlayerArray.Num()==3)
   {
    Test->TestEqual(TEXT("Late join preserves the shared clue snapshot"),int32(State->Mission.Clues),SnapshotClues);
    Test->TestEqual(TEXT("Late join stays in Search"),State->Mission.Phase,EGbBellPhase::Search);
    PC->SetPause(false);
    FString Error; ULocalPlayer* Fourth=World->GetGameInstance()->CreateLocalPlayer(1,Error,true);
    if (!Test->TestNotNull(TEXT("Local slot four created without another full editor process"),Fourth)) { Test->AddError(Error); return true; }
    Stage=2;
   }
   if (Stage==2 && State->PlayerArray.Num()==4)
   {
    // Replace the baseline remote with another local slot before starting the probe process.
    // Four active players remain, with at most three full Unreal processes resident at any time.
    auto* Mode=World->GetAuthGameMode<AGbCoopMode>();
    if (!Mode || !Mode->GameSession || !FirstRemote.IsValid() || !Mode->GameSession->KickPlayer(FirstRemote.Get(),FText::GetEmpty()))
    { Test->AddError(TEXT("Could not release the baseline remote test client")); return true; }
    Stage=3;
   }
   if (Stage==3 && State->PlayerArray.Num()==3)
   {
    FString Error; ULocalPlayer* Replacement=World->GetGameInstance()->CreateLocalPlayer(2,Error,true);
    if (!Test->TestNotNull(TEXT("Replacement local slot keeps four active players"),Replacement)) { Test->AddError(Error); return true; }
    Stage=4; UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST PartyFull=%d"),State->PlayerArray.Num());
    if (FParse::Param(FCommandLine::Get(),TEXT("GbCoopLimitHookOnly")))
    {
     FString Refusal; World->GetAuthGameMode<AGbCoopMode>()->PreLogin(FString(),TEXT("127.0.0.1"),FUniqueNetIdRepl(),Refusal);
     Test->TestTrue(TEXT("Actual full-party PreLogin hook refuses another player"),Refusal.Contains(TEXT("four binders")));
     Test->TestEqual(TEXT("Refusal retains four active players"),State->PlayerArray.Num(),4);
     Test->AddInfo(TEXT("External fifth-process probe omitted due laptop commit headroom; hook-only coverage."));
     UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST LimitHookRefused Party=4 Clues=%d"),State->Mission.Clues);
     return true;
    }
   }
   if (Stage==4 && Now>=NextRead)
   {
    NextRead=Now+1; FString Log;
    if (FFileHelper::LoadFileToString(Log,*LogPath) && Log.Contains(TEXT("This story party is full (four binders).")))
    {
     Test->TestEqual(TEXT("Rejected connection never adds a fifth player"),State->PlayerArray.Num(),4);
     UE_LOG(LogTemp,Display,TEXT("GB_COOP_TEST FifthRefused Party=%d"),State->PlayerArray.Num());
     return true;
    }
   }
   return false;
  }
 private:
  FAutomationTestBase* Test;
  FString LogPath;
  TWeakObjectPtr<AGbCoopController> FirstRemote;
  double Deadline,NextRead=0;
  int32 Stage=0;
  int32 SnapshotClues=0;
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGbCoopPartyNetworkTest,"Grudgebound.Coop.NetworkLateJoinAndLimit",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGbCoopPartyNetworkTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FGbCoopPartyRun(this));
 return true;
}
#endif
