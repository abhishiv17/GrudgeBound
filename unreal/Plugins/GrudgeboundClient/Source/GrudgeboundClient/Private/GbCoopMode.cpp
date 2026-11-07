#include "GbCoopMission.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

namespace
{
	AGbCoopCharacter* Binder(AGbCoopController* PC) { return PC ? Cast<AGbCoopCharacter>(PC->GetPawn()) : nullptr; }
	bool Alive(AGbCoopController* PC) { const auto* P = Binder(PC); return P && !P->IsDown(); }
	bool Near(const AActor* A, FVector Position, float Range = 235.f) { return A && FVector::DistSquared2D(A->GetActorLocation(),Position) <= FMath::Square(Range); }
	bool Anchor(EGbBellObject O) { return O == EGbBellObject::AnchorA || O == EGbBellObject::AnchorB; }
}

AGbCoopMode::AGbCoopMode()
{
	DefaultPawnClass = AGbCoopCharacter::StaticClass(); PlayerControllerClass = AGbCoopController::StaticClass(); GameStateClass = AGbCoopState::StaticClass();
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .1f;
}
AGbCoopState* AGbCoopMode::State() const { return GetGameState<AGbCoopState>(); }
void AGbCoopMode::BeginPlay()
{
	Super::BeginPlay();
	Phase(EGbBellPhase::Arrival, TEXT("Mira: The notice has my route, my coat, tomorrow's date. Why won't anybody read me the name?"));
}
void AGbCoopMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error)
{
	Super::PreLogin(Options,Address,Id,Error);
	if (Error.IsEmpty() && GetNumPlayers() >= 4) Error = TEXT("This story party is full (four binders).");
}
void AGbCoopMode::PostLogin(APlayerController* Player)
{
	Super::PostLogin(Player);
	if (GetNumPlayers()>4) { Player->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("This party is full."))); Player->Destroy(); return; }
	if (Player->PlayerState) Player->PlayerState->SetPlayerName(FString::Printf(TEXT("Binder %d"), ++NextBinder));
	UE_LOG(LogTemp,Display,TEXT("GB_COOP_JOIN Party=%d"),GetNumPlayers());
}
void AGbCoopMode::Logout(AController* Player)
{
	if (auto* PC = ::Cast<AGbCoopController>(Player)) { Channels.Remove(PC); Revives.Remove(PC); ReviveTime.Remove(PC); Votes.Remove(PC); }
	Super::Logout(Player);
}
void AGbCoopMode::Phase(EGbBellPhase NewPhase, const FString& Line)
{
	if (!State()) return;
	State()->Mission.Phase = NewPhase; State()->Mission.Channel = 0; State()->Line = Line;
	Channels.Reset(); State()->ForceNetUpdate();
	UE_LOG(LogTemp,Display,TEXT("GB_COOP_SERVER Phase=%d Clues=%d Party=%d"),static_cast<int32>(NewPhase),State()->Mission.Clues,GetNumPlayers());
}
void AGbCoopMode::Use(AGbCoopController* PC, bool bHeld)
{
	if (!bHeld) { Channels.Remove(PC); Revives.Remove(PC); ReviveTime.Remove(PC); return; }
	if (!Alive(PC) || !State()) return;
	const auto PhaseNow = State()->Mission.Phase;
	if (PhaseNow == EGbBellPhase::Defeat || PhaseNow == EGbBellPhase::Complete) return;
	for (TActorIterator<AGbCoopCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != Binder(PC) && It->IsDown() && Near(Binder(PC),It->GetActorLocation()))
		{
			Channels.Remove(PC);
			if (Revives.FindRef(PC) != *It) { Revives.Add(PC,*It); ReviveTime.Add(PC,0); }
			return;
		}
	}
	AGbCoopObject* Closest = nullptr; float Best = FMath::Square(235.f);
	for (TActorIterator<AGbCoopObject> It(GetWorld()); It; ++It)
	{
		if (It->Kind == EGbBellObject::Collector) continue;
		const float D = FVector::DistSquared2D(Binder(PC)->GetActorLocation(),It->GetActorLocation());
		if (D < Best) { Closest = *It; Best = D; }
	}
	if (!Closest) { PC->ClientLine(TEXT("Move closer to a person, clue or seal before holding E.")); return; }
	const auto O = Closest->Kind;
	if (Anchor(O))
	{
		if (PhaseNow == EGbBellPhase::Ritual || PhaseNow == EGbBellPhase::Boss) { Revives.Remove(PC); Channels.Add(PC,O); }
		else PC->ClientLine(TEXT("The seal needs the three unfinished lines first."));
		return;
	}
	if (O == EGbBellObject::Mira)
	{
		PC->ClientLine(TEXT("Mira: Every door knows me. Every person forgets. Find what the archive, market and landing remember."));
		if (PhaseNow == EGbBellPhase::Arrival) Phase(EGbBellPhase::Search,TEXT("Kessa: Don't open that notice alone. Find the three unfinished lines. Stay where you can hear each other."));
	}
	else if (O == EGbBellObject::Nara) PC->ClientLine(TEXT("Nara: I recorded it as debt. If I wrote death, there would be nothing left to appeal. Yes. That is my signature."));
	else if (O == EGbBellObject::Silk) PC->ClientLine(TEXT("Silk: I sold a debt, not a person. ...Don't look at me like that. The contract is still here. Take it."));
	else if (O == EGbBellObject::Eiden)
	{
		PC->ClientLine(TEXT("Eiden: I was not breaking the seals. I was keeping the thing behind them from learning another name."));
		if (PhaseNow == EGbBellPhase::Revelation)
		{
			auto& M = State()->Mission; M.BossMaxHp = M.BossHp = 400 + 180 * FMath::Max(0,GetNumPlayers()-1); M.OpenUntil = 0; M.PulseAt = GetWorld()->GetTimeSeconds()+9;
			Phase(EGbBellPhase::Boss,TEXT("Kessa: The ninth seal is my name. I was a binder once. Hold the witness seals together. Don't let it finish the toll."));
		}
	}
	else if (GbBell::Collect(State()->Mission,O))
	{
		const TCHAR* Clue = O == EGbBellObject::ClueNotice ? TEXT("The notice names Mira. The payment was made nine years before she was born.")
			: O == EGbBellObject::ClueContract ? TEXT("The contract bought nine years for the Bell Quarter. The price was its residents' names.")
			: TEXT("The ninth seal is a paper lantern's signature. Kessa recognizes the handwriting before she recognizes the name.");
		State()->Line = Clue; State()->ForceNetUpdate();
		UE_LOG(LogTemp,Display,TEXT("GB_COOP_CLUE Mask=%d"),State()->Mission.Clues);
		if (State()->Mission.Phase == EGbBellPhase::Ritual) Phase(EGbBellPhase::Ritual,TEXT("Nara: The bargain cannot be opened by one witness. Hold the two seals. Make it remember you together."));
	}
	else PC->ClientLine(TEXT("This line is already witnessed, or the party is not ready to read it yet."));
}

void AGbCoopMode::Cast(AGbCoopController* PC, const FVector& Target)
{
	if (!Alive(PC) || !State() || State()->Mission.Phase != EGbBellPhase::Boss || Target.ContainsNaN()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector Boss = GbBell::Position(EGbBellObject::Collector);
	if (Now-PC->LastCast < .65f || !Near(Binder(PC),Boss,950) || FVector::DistSquared2D(Target,Boss) > FMath::Square(260.f)) return;
	PC->LastCast = Now;
	if (Now >= State()->Mission.OpenUntil) { PC->ClientLine(TEXT("Its pages are shut. Channel the witness seals together to expose it.")); return; }
	State()->Mission.BossHp = FMath::Max(0.f,State()->Mission.BossHp-22);
	if (State()->Mission.BossHp <= 0) Phase(EGbBellPhase::Decision,TEXT("Kessa: I can return their names if I give it mine. Or we break the bell and go looking for Mira's name ourselves. I won't choose for you."));
}

void AGbCoopMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); if (!State()) return;
	auto& M = State()->Mission; const float Now = GetWorld()->GetTimeSeconds();
	for (auto It=Revives.CreateIterator(); It; ++It)
	{
		auto* PC = It.Key().Get(); auto* Target = It.Value().Get();
		if (!Alive(PC) || !Target || !Target->IsDown() || !Near(Binder(PC),Target->GetActorLocation())) { ReviveTime.Remove(It.Key()); It.RemoveCurrent(); continue; }
		float& Time = ReviveTime.FindOrAdd(It.Key()); Time += DeltaSeconds;
		if (Time >= 2) { Target->Restore(45); ReviveTime.Remove(It.Key()); It.RemoveCurrent(); }
	}
	TSet<EGbBellObject> Occupied;
	for (auto It=Channels.CreateIterator(); It; ++It)
	{
		auto* PC = It.Key().Get();
		if (!Alive(PC) || !Near(Binder(PC),GbBell::Position(It.Value()))) { It.RemoveCurrent(); continue; }
		Occupied.Add(It.Value());
	}
	if (M.Phase == EGbBellPhase::Ritual || M.Phase == EGbBellPhase::Boss)
	{
		if (Occupied.Num() >= GbBell::RequiredAnchors(GetNumPlayers())) M.Channel = FMath::Min(1.f,M.Channel+DeltaSeconds/3.f); else M.Channel = 0;
		if (M.Channel >= 1)
		{
			if (M.Phase == EGbBellPhase::Ritual) Phase(EGbBellPhase::Revelation,TEXT("Eiden: It was never a lock on the Bell Quarter. It was a lock on the Collector. Kessa... why is your name on the last seal?"));
			else { M.OpenUntil = Now+6; M.Channel = 0; Channels.Reset(); State()->Line = TEXT("Mira: We are still here. Its pages are open. Cast now!"); }
		}
	}
	if (M.Phase == EGbBellPhase::Boss)
	{
		if (M.OpenUntil > 0 && Now > M.OpenUntil) { M.OpenUntil = 0; State()->Line = TEXT("Eiden: Its pages are closing. Split to the witness seals again."); }
		if (Now >= M.PulseAt)
		{
			for (TActorIterator<AGbCoopCharacter> It(GetWorld()); It; ++It) if (Near(*It,GbBell::Position(EGbBellObject::Collector),800)) It->Hurt(28);
			M.PulseAt = Now+9; State()->Line = TEXT("The ninth toll tears across the court. Hold your ground for each other, not for the bell.");
		}
		int32 Up=0, Total=0;
		for (TActorIterator<AGbCoopCharacter> It(GetWorld()); It; ++It) if (It->GetController()) { ++Total; Up += !It->IsDown(); }
		if (Total && !Up) Phase(EGbBellPhase::Defeat,TEXT("Mira: Please. If you can still hear me, try again."));
	}
	if (M.Phase == EGbBellPhase::Decision) FinishVote();
}

void AGbCoopMode::Vote(AGbCoopController* PC, EGbBellEnding Ending)
{
	if (!PC || !State() || State()->Mission.Phase != EGbBellPhase::Decision || (Ending != EGbBellEnding::ReturnNames && Ending != EGbBellEnding::BreakBell)) return;
	Votes.Add(PC,Ending); FinishVote();
}
void AGbCoopMode::FinishVote()
{
	TArray<EGbBellEnding> Choices; int32 Required = 0;
	for (TActorIterator<AGbCoopController> It(GetWorld()); It; ++It)
	{
		if (!It->PlayerState || It->IsActorBeingDestroyed()) continue;
		++Required; if (const auto* Choice=Votes.Find(*It)) Choices.Add(*Choice);
	}
	State()->Mission.Votes = Choices.Num();
	if (!Required || Choices.Num() != Required) return;
	State()->Mission.Ending = GbBell::ResolveVotes(Choices);
	const bool Names = State()->Mission.Ending == EGbBellEnding::ReturnNames;
	Phase(EGbBellPhase::Complete, Names
		? TEXT("Mira remembers her mother's song. Kessa gives up her name and memories, but keeps her voice. From below the water: That wasn't the only name you promised.")
		: TEXT("The bell breaks. Kessa stays. Mira's notice is blank. From below the water, in Mira's voice: I've been waiting for you."));
}
void AGbCoopMode::Retry()
{
	if (!State() || State()->Mission.Phase != EGbBellPhase::Defeat) return;
	Revives.Reset(); ReviveTime.Reset(); Votes.Reset();
	int32 Index=0;
	for (TActorIterator<AGbCoopCharacter> It(GetWorld()); It; ++It) if (It->GetController()) { It->Restore(100); It->SetActorLocation(FVector(50,(Index++-1)*140,100)); }
	Phase(EGbBellPhase::Ritual,TEXT("Eiden: The clues are still true. Hold the witness seals. This time, watch the toll."));
}
