#include "GbApiSubsystem.h"

#include "GbJson.h"
#include "GbSettings.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrudgebound, Log, All);

void UGbApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	KessaLine = TEXT("Kessa: Welcome to Hollowmere. Mind the tall grass after dark.");
}

void UGbApiSubsystem::Deinitialize()
{
	StopPolling();
	Super::Deinitialize();
}

FString UGbApiSubsystem::GetServerUrl() const
{
	return GetDefault<UGbSettings>()->GetServerUrl();
}

// ---------------------------------------------------------------- HTTP

void UGbApiSubsystem::Send(const FString& Verb, const FString& Path, const FString& Body, FDone Done)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerUrl() + Path);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!Body.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(Body);
	}
	Request->SetTimeout(GetDefault<UGbSettings>()->RequestTimeoutSeconds);
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				Done(0, FString());
				return;
			}
			Done(Response->GetResponseCode(), Response->GetContentAsString());
		});
	Request->ProcessRequest();
}

void UGbApiSubsystem::SetOnline(bool bNowOnline)
{
	if (bOnline == bNowOnline) return;
	bOnline = bNowOnline;
	UE_LOG(LogGrudgebound, Log, TEXT("Game server %s (%s)"), bOnline ? TEXT("online") : TEXT("offline"), *GetServerUrl());
	OnServerStatus.Broadcast(bOnline, WorldState.Tick);
}

// ---------------------------------------------------------------- town

void UGbApiSubsystem::StartPolling()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	const float Every = FMath::Max(0.5f, GetDefault<UGbSettings>()->PollSeconds);
	GI->GetTimerManager().SetTimer(PollTimer, this, &UGbApiSubsystem::Poll, Every, true, 0.f);
}

void UGbApiSubsystem::StopPolling()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PollTimer);
	}
}

void UGbApiSubsystem::RefreshNow()
{
	Poll();
}

void UGbApiSubsystem::Poll()
{
	FetchState();
	FetchEvents();
	if (!bCursesLoaded) FetchCurses();
}

void UGbApiSubsystem::FetchState()
{
	if (bStateInFlight) return;
	bStateInFlight = true;
	Send(TEXT("GET"), TEXT("/state"), FString(), [this](int32 Status, const FString& Body)
	{
		bStateInFlight = false;
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Body, O))
		{
			SetOnline(false);
			OnError.Broadcast(TEXT("state"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Body));
			return;
		}
		WorldState = GbJson::WorldState(O);
		SetOnline(true);
		OnWorldState.Broadcast(WorldState);
	});
}

void UGbApiSubsystem::FetchEvents()
{
	if (bEventsInFlight) return;
	bEventsInFlight = true;
	// Ask from one tick earlier so events added to the newest tick after the last fetch are not missed;
	// the ones already seen are skipped below.
	const int32 Since = LastEventTick < 0 ? -1 : LastEventTick - 1;
	Send(TEXT("GET"), FString::Printf(TEXT("/events?since=%d"), Since), FString(), [this](int32 Status, const FString& Body)
	{
		bEventsInFlight = false;
		TArray<TSharedPtr<FJsonValue>> Items;
		if (Status != 200 || !GbJson::ParseArray(Body, Items))
		{
			if (Status != 0) OnError.Broadcast(TEXT("events"), Status, GbJson::ErrorMessage(Body));
			return;
		}

		TArray<FGbTownEvent> Fresh;
		for (const TSharedPtr<FJsonValue>& V : Items)
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (!V.IsValid() || !V->TryGetObject(O) || !O) continue;
			const FGbTownEvent E = GbJson::TownEvent(*O);
			if (E.Tick < LastEventTick) continue;
			if (E.Tick == LastEventTick && SeenAtLastTick.Contains(E.Text)) continue;
			if (E.Tick > LastEventTick)
			{
				LastEventTick = E.Tick;
				SeenAtLastTick.Reset();
			}
			SeenAtLastTick.Add(E.Text);
			Fresh.Add(E);
		}
		if (Fresh.Num() == 0) return;

		OnTownEvents.Broadcast(Fresh);
		SayAsKessa(Fresh.Last().Text);
	});
}

void UGbApiSubsystem::FetchCurses()
{
	if (bCursesInFlight) return;
	bCursesInFlight = true;
	Send(TEXT("GET"), TEXT("/curses"), FString(), [this](int32 Status, const FString& Body)
	{
		bCursesInFlight = false;
		TArray<TSharedPtr<FJsonValue>> Items;
		if (Status != 200 || !GbJson::ParseArray(Body, Items))
		{
			if (Status != 0) OnError.Broadcast(TEXT("curses"), Status, GbJson::ErrorMessage(Body));
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : Items)
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (!V.IsValid() || !V->TryGetObject(O) || !O) continue;
			const FGbCurseInfo C = GbJson::Curse(*O);
			if (!C.Id.IsEmpty()) Curses.Add(C.Id, C);
		}
		bCursesLoaded = Curses.Num() > 0;
	});
}

bool UGbApiSubsystem::GetWard(const FString& WardId, FGbWardSummary& OutWard) const
{
	for (const FGbWardSummary& W : WorldState.Wards)
	{
		if (W.Id == WardId)
		{
			OutWard = W;
			return true;
		}
	}
	return false;
}

FGbCurseInfo UGbApiSubsystem::GetCurse(const FString& SpeciesId) const
{
	if (const FGbCurseInfo* C = Curses.Find(SpeciesId)) return *C;
	FGbCurseInfo Unknown;
	Unknown.Id = SpeciesId;
	Unknown.Name = SpeciesId;
	return Unknown;
}

void UGbApiSubsystem::SayAsKessa(const FString& Line)
{
	if (Line.IsEmpty()) return;
	KessaLine = Line;
	OnKessaLine.Broadcast(KessaLine);
}

// ---------------------------------------------------------------- battles

void UGbApiSubsystem::StartBattle(const FString& WardId, const FString& PlayerCurse)
{
	if (IsBattleOpen() || bBattleRequestInFlight) return;
	bBattleRequestInFlight = true;
	const UGbSettings* Settings = GetDefault<UGbSettings>();
	const FString Body = GbJson::StartBody(WardId.IsEmpty() ? Settings->WardId : WardId, PlayerCurse, Settings->PlayerName);
	Send(TEXT("POST"), TEXT("/battle/start"), Body, [this](int32 Status, const FString& Text)
	{
		bBattleRequestInFlight = false;
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(TEXT("battle/start"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		SetOnline(true);
		const FGbBattleStart Start = GbJson::BattleStart(O);
		BattleId = Start.BattleId;
		BattleView = Start.View;
		OnBattleStarted.Broadcast(Start);
	});
}

void UGbApiSubsystem::SendAction(EGbAction Action)
{
	if (!IsBattleOpen() || bBattleRequestInFlight) return;
	bBattleRequestInFlight = true;
	Send(TEXT("POST"), TEXT("/battle/turn"), GbJson::TurnBody(BattleId, Action), [this](int32 Status, const FString& Text)
	{
		bBattleRequestInFlight = false;
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			// 404: the battle ended or expired on the server, so it can't continue here either.
			if (Status == 404) BattleId.Reset();
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(TEXT("battle/turn"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		const FGbBattleTurn Turn = GbJson::BattleTurn(O);
		BattleView = Turn.View;
		if (Turn.bEnded) BattleId.Reset();
		OnBattleTurn.Broadcast(Turn);
	});
}

void UGbApiSubsystem::AbandonBattle()
{
	BattleId.Reset();
	BattleView = FGbBattleView();
}
