#include "GbApiSubsystem.h"
#include "GbAuthSubsystem.h"

#include "GbJson.h"
#include "GbSettings.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"
#include "GenericPlatform/GenericPlatformHttp.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrudgebound, Log, All);

void UGbApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UGbAuthSubsystem* Auth = Collection.InitializeDependency<UGbAuthSubsystem>();
	Auth->OnSessionChanged.AddDynamic(this, &UGbApiSubsystem::HandleSessionChanged);
	KessaLine = TEXT("Kessa: Welcome to Hollowmere. Mind the tall grass after dark.");
}

void UGbApiSubsystem::Deinitialize()
{
	StopPolling();
	if (UGbAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()) Auth->OnSessionChanged.RemoveDynamic(this, &UGbApiSubsystem::HandleSessionChanged);
	Super::Deinitialize();
}

FString UGbApiSubsystem::GetServerUrl() const
{
	return GetDefault<UGbSettings>()->GetServerUrl();
}

FString UGbApiSubsystem::GetPlayerAccount() const
{
	return GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->GetAccount();
}

FString UGbApiSubsystem::GetPlayerName() const
{
	return GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->GetPlayerName();
}

void UGbApiSubsystem::HandleSessionChanged()
{
	const FString NewToken = GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->GetToken();
	if (NewToken == SessionToken) return;
	SessionToken = NewToken; ++SessionSerial;
	SwearingVowJobs.Reset();
	AbandonBattle(); bBattleRequestInFlight = false; bStateInFlight = false; bEventsInFlight = false; bCursesInFlight = false;
}

// ---------------------------------------------------------------- HTTP

void UGbApiSubsystem::Send(const FString& Verb, const FString& Path, const FString& Body, FDone Done)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerUrl() + Path);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!SessionToken.IsEmpty()) Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
	if (!Body.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(Body);
	}
	Request->SetTimeout(GetDefault<UGbSettings>()->RequestTimeoutSeconds);
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Serial = SessionSerial, Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (Serial != SessionSerial) return; // Ignore requests from a signed-out player.
			if (Response.IsValid() && Response->GetResponseCode() == 401)
			{
				GetGameInstance()->GetSubsystem<UGbAuthSubsystem>()->SessionExpired(); return;
			}
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
		const FGbWorldState Updated = GbJson::WorldState(O);
		if (WorldState.bValid && Updated.Tick < WorldState.Tick)
		{
			// A restarted server has a new clock. Its early events must not be discarded as old.
			LastEventTick = -1; SeenAtLastTick.Reset(); RecentTownEvents.Reset();
			++WorldEpoch;
		}
		WorldState = Updated;
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
	Send(TEXT("GET"), FString::Printf(TEXT("/events?since=%d"), Since), FString(), [this, Epoch = WorldEpoch](int32 Status, const FString& Body)
	{
		bEventsInFlight = false;
		if (Epoch != WorldEpoch) return;
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
		// Keep narrative facts for identities that sign in after the initial public feed was fetched.
		for (const auto& E : Fresh) if (E.Type == TEXT("bankruptcy") || E.Type == TEXT("calamity")) RecentTownEvents.Add(E);
		if (RecentTownEvents.Num() > 500) RecentTownEvents.RemoveAt(0, RecentTownEvents.Num() - 500);

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
	const FString Body = GbJson::StartBody(WardId.IsEmpty() ? Settings->WardId : WardId, PlayerCurse, GetPlayerName(), GetPlayerAccount());
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
		if (Turn.bHasChain)
		{
			if (Turn.Chain.Status == TEXT("pending")) PollChain(TEXT("battle"), Turn.BattleId, 30);
			else OnChainReceipt.Broadcast(Turn.BattleId, Turn.Chain);
		}
	});
}

void UGbApiSubsystem::PollChain(const FString& Kind, const FString& Id, int32 TriesLeft)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	if (TriesLeft <= 0)
	{
		if (Kind == TEXT("vow")) OnError.Broadcast(TEXT("vows"), 0, TEXT("The ledger is taking longer than expected. Check the Shrine again shortly."));
		return;
	}
	FTimerHandle Unused;
	GI->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateWeakLambda(this, [this, Serial = SessionSerial, Kind, Id, TriesLeft]()
	{
		if (Serial != SessionSerial) return;
		const FString Path = FString::Printf(TEXT("/chain?%s=%s"), *Kind, *Id);
		Send(TEXT("GET"), Path, FString(), [this, Kind, Id, TriesLeft](int32 Status, const FString& Text)
		{
			TSharedPtr<FJsonObject> O;
			if (Status != 200 || !GbJson::ParseObject(Text, O))
			{
				// 0: server briefly unreachable, try again; anything else (404: server restarted) is final.
				if (Status == 0) PollChain(Kind, Id, TriesLeft - 1);
				else if (Kind == TEXT("vow")) OnError.Broadcast(TEXT("vows"), Status, TEXT("The receipt is unavailable. Reopen the Shrine to read the ledger."));
				return;
			}
			const FGbChainReceipt R = GbJson::ChainReceipt(O);
			if (R.Status == TEXT("pending")) PollChain(Kind, Id, TriesLeft - 1);
			else if (Kind == TEXT("rule")) OnRuleReceipt.Broadcast(Id, R);
			else if (Kind == TEXT("shop")) OnTradeReceipt.Broadcast(Id, R);
		else if (Kind == TEXT("vow")) FinishVow(Id, R);
			else OnChainReceipt.Broadcast(Id, R);
		});
	}), 1.0f, false);
}

void UGbApiSubsystem::FetchVows()
{
	Send(TEXT("GET"), TEXT("/vows"), FString(), [this](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status == 200 && GbJson::ParseObject(Text, O)) OnVows.Broadcast(GbJson::VowsOverview(O));
		else OnError.Broadcast(TEXT("vows"), Status, Status == 0 ? TEXT("Hollowmere is unreachable. Try again shortly.") : GbJson::ErrorMessage(Text));
	});
}

void UGbApiSubsystem::SwearVow(const FString& TemplateId, int32 Days, const TMap<FString, FString>& Params)
{
	SendVow(TEXT("/vows/swear"), GbJson::VowBody(TemplateId, Days, Params));
}

void UGbApiSubsystem::ReleaseVow(int32 VowId)
{
	SendVow(TEXT("/vows/release"), FString::Printf(TEXT("{\"vowId\":%d}"), VowId));
}

void UGbApiSubsystem::SendVow(const FString& Path, const FString& Body)
{
	Send(TEXT("POST"), Path, Body, [this, bSwear = Path == TEXT("/vows/swear")](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			OnError.Broadcast(TEXT("vows"), Status, Status == 0 ? TEXT("The Shrine cannot be reached. Reopen it to check your vows.") : GbJson::ErrorMessage(Text)); return;
		}
		const FString JobId = O->GetStringField(TEXT("jobId"));
		if (bSwear) SwearingVowJobs.Add(JobId);
		const FGbChainReceipt R = GbJson::ChainReceipt(O->GetObjectField(TEXT("chain")));
		if (R.Status == TEXT("pending")) PollChain(TEXT("vow"), JobId, 120);
		else FinishVow(JobId, R);
	});
}

void UGbApiSubsystem::FinishVow(const FString& JobId, const FGbChainReceipt& Receipt)
{
	const bool bSworn = SwearingVowJobs.Remove(JobId) > 0 && Receipt.Status == TEXT("done");
	OnVowReceipt.Broadcast(JobId, Receipt);
	if (bSworn) OnVowSworn.Broadcast(JobId, Receipt);
	FetchPlayer(); FetchVows();
}

void UGbApiSubsystem::FetchPlayer()
{
	const FString Account = GetPlayerAccount();
	if (Account.IsEmpty()) return; // No account: the local save is all there is.
	Send(TEXT("GET"), TEXT("/player?account=") + FGenericPlatformHttp::UrlEncode(Account), FString(), [this](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(TEXT("player"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		OnPlayerView.Broadcast(GbJson::Player(O));
	});
}

void UGbApiSubsystem::FetchShop(const FString& MerchantId)
{
	const FString Account = GetPlayerAccount();
	FString Path = TEXT("/shop?merchant=") + FGenericPlatformHttp::UrlEncode(MerchantId);
	if (!Account.IsEmpty()) Path += TEXT("&account=") + FGenericPlatformHttp::UrlEncode(Account);
	Send(TEXT("GET"), Path, FString(), [this](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(TEXT("shop"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		OnShop.Broadcast(GbJson::Shop(O));
	});
}

void UGbApiSubsystem::BuyItem(const FString& MerchantId, const FString& ItemId)
{
	SendTrade(true, MerchantId, TEXT("itemId"), ItemId);
}

void UGbApiSubsystem::SellTalisman(const FString& MerchantId, const FString& TokenId)
{
	SendTrade(false, MerchantId, TEXT("tokenId"), TokenId);
}

void UGbApiSubsystem::SendTrade(bool bBuy, const FString& MerchantId, const FString& Key, const FString& Value)
{
	const FString What = bBuy ? TEXT("shop/buy") : TEXT("shop/sell");
	const FString Account = GetPlayerAccount();
	if (Account.TrimStartAndEnd().IsEmpty())
	{
		OnError.Broadcast(What, 400, TEXT("Sign in to enter Hollowmere first."));
		return;
	}
	Send(TEXT("POST"), TEXT("/") + What, GbJson::TradeBody(Account, MerchantId, Key, Value), [this, What, bBuy, Value](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(What, Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		FGbTrade T = GbJson::Trade(O, bBuy);
		if (!bBuy) T.TokenId = Value;
		OnTrade.Broadcast(T);
		if (T.Chain.Status == TEXT("pending")) PollChain(TEXT("shop"), T.JobId, 40);
		else OnTradeReceipt.Broadcast(T.JobId, T.Chain);
	});
}

void UGbApiSubsystem::FetchRules()
{
	const FString Account = GetPlayerAccount();
	const FString Path = Account.IsEmpty() ? FString(TEXT("/rules")) : TEXT("/rules?account=") + FGenericPlatformHttp::UrlEncode(Account);
	Send(TEXT("GET"), Path, FString(), [this](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(TEXT("rules"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		OnRules.Broadcast(GbJson::RulesOverview(O));
	});
}

void UGbApiSubsystem::ProposeRule(const FString& TemplateId, const TMap<FString, FString>& Params)
{
	const FString Account = GetPlayerAccount();
	if (Account.TrimStartAndEnd().IsEmpty())
	{
		OnError.Broadcast(TEXT("rules/propose"), 400, TEXT("Sign in to enter Hollowmere first."));
		return;
	}
	Send(TEXT("POST"), TEXT("/rules/propose"), GbJson::ProposeBody(Account, TemplateId, Params), [this](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(TEXT("rules/propose"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		const FGbRuleVerdict V = GbJson::RuleVerdict(O);
		OnRuleVerdict.Broadcast(V);
		if (V.Chain.Status == TEXT("pending")) PollChain(TEXT("rule"), V.JobId, 40);
		else OnRuleReceipt.Broadcast(V.JobId, V.Chain);
	});
}

void UGbApiSubsystem::RepealRule(int32 ChainRuleId)
{
	const FString Account = GetPlayerAccount();
	if (Account.TrimStartAndEnd().IsEmpty())
	{
		OnError.Broadcast(TEXT("rules/repeal"), 400, TEXT("Sign in to enter Hollowmere first."));
		return;
	}
	Send(TEXT("POST"), TEXT("/rules/repeal"), GbJson::RepealBody(Account, ChainRuleId), [this](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			if (Status == 0) SetOnline(false);
			OnError.Broadcast(TEXT("rules/repeal"), Status, Status == 0 ? TEXT("server not reachable") : GbJson::ErrorMessage(Text));
			return;
		}
		const FGbRuleVerdict V = GbJson::Repeal(O);
		OnRuleRepeal.Broadcast(V);
		if (V.Chain.Status == TEXT("pending")) PollChain(TEXT("rule"), V.JobId, 40);
		else OnRuleReceipt.Broadcast(V.JobId, V.Chain);
	});
}

void UGbApiSubsystem::AbandonBattle()
{
	BattleId.Reset();
	BattleView = FGbBattleView();
}
