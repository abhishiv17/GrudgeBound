#include "GbJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString V;
		if (O.IsValid()) O->TryGetStringField(Key, V);
		return V;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default = 0.0)
	{
		double V = Default;
		if (O.IsValid() && !O->TryGetNumberField(Key, V)) V = Default;
		return V;
	}

	int32 Int(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32 Default = 0)
	{
		return FMath::RoundToInt32(Num(O, Key, Default));
	}

	bool Bool(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool Default = false)
	{
		bool V = Default;
		if (O.IsValid() && !O->TryGetBoolField(Key, V)) V = Default;
		return V;
	}

	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* V = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Key, V) && V) return *V;
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>& Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		static const TArray<TSharedPtr<FJsonValue>> Empty;
		const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, V) && V) return *V;
		return Empty;
	}

	TSharedPtr<FJsonObject> AsObj(const TSharedPtr<FJsonValue>& V)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (V.IsValid() && V->TryGetObject(O) && O) return *O;
		return nullptr;
	}

	TArray<FString> Strings(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FString> Out;
		for (const TSharedPtr<FJsonValue>& V : Arr(O, Key))
		{
			FString S;
			if (V.IsValid() && V->TryGetString(S)) Out.Add(S);
		}
		return Out;
	}

	FGbSide Side(const TSharedPtr<FJsonObject>& O)
	{
		FGbSide S;
		S.SpeciesId = Str(O, TEXT("speciesId"));
		S.Name = Str(O, TEXT("name"));
		S.Emotion = Str(O, TEXT("emotion"));
		S.Omen = Str(O, TEXT("omen"));
		S.Hp = Int(O, TEXT("hp"));
		S.MaxHp = FMath::Max(1, Int(O, TEXT("maxHp"), 1));
		S.Level = FMath::Clamp(Int(O, TEXT("level"), 1), 1, 10);
		return S;
	}

	FString Write(const TSharedRef<FJsonObject>& O)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(O, Writer);
		return Out;
	}
}

namespace GbJson
{
	bool ParseObject(const FString& Text, TSharedPtr<FJsonObject>& Out)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	}

	bool ParseArray(const FString& Text, TArray<TSharedPtr<FJsonValue>>& Out)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Out);
	}

	FString ErrorMessage(const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (ParseObject(Text, O))
		{
			const FString E = Str(O, TEXT("error"));
			if (!E.IsEmpty()) return E;
		}
		return Text.Left(200);
	}

	FGbObservatoryData Observatory(const TSharedPtr<FJsonObject>& O)
	{
		FGbObservatoryData D;
		if (!O.IsValid()) return D;
		D.Mode = Str(O, TEXT("mode"));
		D.bValid = D.Mode == TEXT("live") || D.Mode == TEXT("preview");
		D.Seed = Int(O, TEXT("seed"));
		for (const auto& V : Arr(O, TEXT("samples")))
		{
			const auto S = AsObj(V); if (!S.IsValid()) continue;
			auto& Sample = D.Samples.AddDefaulted_GetRef();
			Sample.Tick=Int(S,TEXT("tick")); Sample.Day=Int(S,TEXT("day"),1); Sample.Hour=Int(S,TEXT("hour"));
			Sample.Closed=Strings(S,TEXT("closed"));
			for (const auto& W : Arr(S,TEXT("wards")))
			{
				const auto Ward=AsObj(W); if (!Ward.IsValid()) continue;
				auto& Out=Sample.Wards.AddDefaulted_GetRef(); Out.Id=Str(Ward,TEXT("id"));
				Out.Total=Num(Ward,TEXT("total")); Out.Grudge.Rage=Num(Ward,TEXT("rage"));
				Out.Grudge.Sorrow=Num(Ward,TEXT("sorrow")); Out.Grudge.Dread=Num(Ward,TEXT("dread"));
			}
			if (const auto Prices=Obj(S,TEXT("prices")); Prices.IsValid()) for (const auto& Merchant:Prices->Values)
			{
				const auto Items=AsObj(Merchant.Value); if (!Items.IsValid()) continue;
				auto& Out=Sample.Prices.Add(FString(Merchant.Key));
				for (const auto& Item:Items->Values) { double Price=0; if (Item.Value->TryGetNumber(Price) && FMath::IsFinite(Price)) Out.Items.Add(FString(Item.Key),Price); }
			}
		}
		for (const auto& V:Arr(O,TEXT("rules")))
		{
			const auto R=AsObj(V); if (!R.IsValid()) continue;
			auto& Out=D.Rules.AddDefaulted_GetRef(); Out.Id=Str(R,TEXT("id")); Out.Text=Str(R,TEXT("text"));
			Out.Author=Str(R,TEXT("author")); Out.AuthorKind=Str(R,TEXT("authorKind")); Out.Tick=Int(R,TEXT("tick"));
			Out.Status=Str(R,TEXT("status")); Out.Reason=Str(R,TEXT("reason")); Out.Url=Str(R,TEXT("url"));
		}
		D.Rules.StableSort([](const auto& A,const auto& B){return A.Tick>B.Tick;});
		for (const auto& V:Arr(O,TEXT("events")))
		{
			const auto E=AsObj(V); if (!E.IsValid()) continue;
			auto& Out=D.Events.AddDefaulted_GetRef(); Out.Tick=Int(E,TEXT("tick")); Out.Type=Str(E,TEXT("type")); Out.Ward=Str(E,TEXT("ward")); Out.Text=Str(E,TEXT("text"));
		}
		return D;
	}

	EGbOutcome Outcome(const FString& Text)
	{
		if (Text == TEXT("won")) return EGbOutcome::Won;
		if (Text == TEXT("bound")) return EGbOutcome::Bound;
		if (Text == TEXT("fled")) return EGbOutcome::Fled;
		if (Text == TEXT("lost")) return EGbOutcome::Lost;
		return EGbOutcome::None;
	}

	FGbWorldState WorldState(const TSharedPtr<FJsonObject>& O)
	{
		FGbWorldState S;
		if (!O.IsValid()) return S;
		S.bValid = true;
		S.Tick = Int(O, TEXT("tick"));
		S.Day = Int(O, TEXT("day"), 1);
		S.Hour = Int(O, TEXT("hour"));
		S.bNight = Bool(O, TEXT("night"), true);
		S.BurnedEmber = Int(O, TEXT("burnedEmber"));

		for (const TSharedPtr<FJsonValue>& V : Arr(O, TEXT("wards")))
		{
			const TSharedPtr<FJsonObject> W = AsObj(V);
			if (!W.IsValid()) continue;
			FGbWardSummary& Ward = S.Wards.AddDefaulted_GetRef();
			Ward.Id = Str(W, TEXT("id"));
			Ward.Name = Str(W, TEXT("name"));
			const TSharedPtr<FJsonObject> G = Obj(W, TEXT("grudge"));
			Ward.Grudge.Rage = Num(G, TEXT("rage"));
			Ward.Grudge.Sorrow = Num(G, TEXT("sorrow"));
			Ward.Grudge.Dread = Num(G, TEXT("dread"));
			Ward.Total = Num(W, TEXT("total"));
			Ward.Wild = Int(W, TEXT("wild"));
		}

		for (const TSharedPtr<FJsonValue>& V : Arr(O, TEXT("merchants")))
		{
			const TSharedPtr<FJsonObject> M = AsObj(V);
			if (!M.IsValid()) continue;
			FGbMerchantSummary& Merchant = S.Merchants.AddDefaulted_GetRef();
			Merchant.Id = Str(M, TEXT("id"));
			Merchant.Name = Str(M, TEXT("name"));
			Merchant.Ward = Str(M, TEXT("ward"));
			Merchant.Ember = Int(M, TEXT("ember"));
			Merchant.bClosed = Bool(M, TEXT("closed"));
			if (const TSharedPtr<FJsonObject> P = Obj(M, TEXT("prices")))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : P->Values)
				{
					double Price = 0.0;
					if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Price))
					{
						Merchant.Prices.Add(Pair.Key, FMath::RoundToInt32(Price));
					}
				}
			}
		}

		for (const TSharedPtr<FJsonValue>& V : Arr(O, TEXT("leaderboard")))
		{
			const TSharedPtr<FJsonObject> L = AsObj(V);
			if (!L.IsValid()) continue;
			FGbLeaderEntry& Entry = S.Leaderboard.AddDefaulted_GetRef();
			Entry.Id = Str(L, TEXT("id"));
			Entry.Name = Str(L, TEXT("name"));
			Entry.Kind = Str(L, TEXT("kind"));
			Entry.Points = Int(L, TEXT("points"));
			Entry.Ember = Int(L, TEXT("ember"));
			Entry.Talismans = Int(L, TEXT("talismans"));
		}

		for (const TSharedPtr<FJsonValue>& V : Arr(O, TEXT("rules")))
		{
			const TSharedPtr<FJsonObject> R = AsObj(V);
			if (!R.IsValid()) continue;
			FGbRule& Rule = S.Rules.AddDefaulted_GetRef();
			Rule.Id = Int(R, TEXT("id"));
			Rule.Text = Str(R, TEXT("text"));
			Rule.Author = Str(R, TEXT("author"));
			Rule.Cost = Int(R, TEXT("cost"));
			Rule.TemplateId = Str(R, TEXT("templateId"));
		}
		return S;
	}

	FGbTownEvent TownEvent(const TSharedPtr<FJsonObject>& O)
	{
		FGbTownEvent E;
		E.Tick = Int(O, TEXT("tick"));
		E.Day = Int(O, TEXT("day"));
		E.Hour = Int(O, TEXT("hour"));
		E.Type = Str(O, TEXT("type"));
		E.Ward = Str(O, TEXT("ward"));
		E.Actor = Str(O, TEXT("actor"));
		E.Text = Str(O, TEXT("text"));
		if (E.Type == TEXT("chain")) E.Text = E.Text.Replace(TEXT("NEAR:"), TEXT("The Rite:")).Replace(TEXT("minted to"), TEXT("entered for"));
		E.Url = Str(O, TEXT("url"));
		return E;
	}

	FGbCurseInfo Curse(const TSharedPtr<FJsonObject>& O)
	{
		FGbCurseInfo C;
		C.Id = Str(O, TEXT("id"));
		C.Name = Str(O, TEXT("name"));
		C.Emotion = Str(O, TEXT("emotion"));
		C.Omen = Str(O, TEXT("omen"));
		C.Home = Str(O, TEXT("home"));
		C.BornFrom = Str(O, TEXT("bornFrom"));
		const TSharedPtr<FJsonObject> Move = Obj(O, TEXT("signatureMove"));
		C.MoveName = Str(Move, TEXT("name"));
		C.MoveEffect = Str(Move, TEXT("effect"));
		return C;
	}

	FGbBattleView BattleView(const TSharedPtr<FJsonObject>& O)
	{
		FGbBattleView V;
		V.Turn = Int(O, TEXT("turn"));
		V.Outcome = Outcome(Str(O, TEXT("outcome")));
		V.Player = Side(Obj(O, TEXT("player")));
		V.Wild = Side(Obj(O, TEXT("wild")));

		const TSharedPtr<FJsonObject> Items = Obj(O, TEXT("items"));
		V.Salves = Int(Items, TEXT("salve"));
		V.GildedSeals = Int(Items, TEXT("gilded"));
		V.BlankSeals = Int(Items, TEXT("blank"));

		const TSharedPtr<FJsonObject> Chances = Obj(O, TEXT("chances"));
		V.SealChance = Num(Chances, TEXT("seal"));
		V.GildedSealChance = Num(Chances, TEXT("gildedSeal"));
		V.FleeChance = Num(Chances, TEXT("flee"));

		const TSharedPtr<FJsonObject> Can = Obj(O, TEXT("canUse"));
		V.bCanAttack = Bool(Can, TEXT("attack"));
		V.bCanWard = Bool(Can, TEXT("ward"));
		V.bCanSeal = Bool(Can, TEXT("seal"));
		V.bCanGildedSeal = Bool(Can, TEXT("gildedSeal"));
		V.bCanSalve = Bool(Can, TEXT("salve"));
		V.bCanFlee = Bool(Can, TEXT("flee"));
		return V;
	}

	FGbBattleStart BattleStart(const TSharedPtr<FJsonObject>& O)
	{
		FGbBattleStart B;
		B.BattleId = Str(O, TEXT("battleId"));
		B.Ward = Str(O, TEXT("ward"));
		B.Wild = Curse(Obj(O, TEXT("wild")));
		B.Matchup = Str(O, TEXT("matchup"));
		B.View = BattleView(Obj(O, TEXT("view")));
		B.Lines = Strings(O, TEXT("lines"));
		if (const TSharedPtr<FJsonObject> R = Obj(O, TEXT("rival"))) { B.RivalStage = Int(R, TEXT("stage")); B.RivalName = Str(R, TEXT("name")); }
		return B;
	}

	FGbBattleTurn BattleTurn(const TSharedPtr<FJsonObject>& O)
	{
		FGbBattleTurn T;
		T.BattleId = Str(O, TEXT("battleId"));
		T.Action = Str(O, TEXT("action"));
		T.Lines = Strings(O, TEXT("lines"));
		T.View = BattleView(Obj(O, TEXT("view")));
		// The top-level outcome can be set when the view's is not (the server ends a battle at the turn limit).
		T.Outcome = Outcome(Str(O, TEXT("outcome")));
		if (T.Outcome == EGbOutcome::None) T.Outcome = T.View.Outcome;
		T.bEnded = T.Outcome != EGbOutcome::None;
		T.Points = Int(O, TEXT("points"));
		if (const TSharedPtr<FJsonObject> P = Obj(O, TEXT("progress")))
		{
			T.XpGained = FMath::Max(0, Int(P, TEXT("gained"))); T.Level = Int(P, TEXT("level")); T.LevelBefore = Int(P, TEXT("before"));
		}
		double Seed = -1.0;
		if (O.IsValid() && O->TryGetNumberField(TEXT("seed"), Seed)) T.Seed = static_cast<int64>(Seed);
		if (const TSharedPtr<FJsonObject> C = Obj(O, TEXT("chain")))
		{
			T.bHasChain = true;
			T.Chain = ChainReceipt(C);
		}
		return T;
	}

	FGbChainReceipt ChainReceipt(const TSharedPtr<FJsonObject>& O)
	{
		FGbChainReceipt R;
		R.Status = Str(O, TEXT("status"));
		R.Network = Str(O, TEXT("network"));
		R.Account = Str(O, TEXT("account"));
		R.TokenId = Str(O, TEXT("tokenId"));
		R.Error = Str(O, TEXT("error"));
		R.RuleId = Int(O, TEXT("ruleId"), -1);
		const TArray<TSharedPtr<FJsonValue>>* Txs = nullptr;
		if (O.IsValid() && O->TryGetArrayField(TEXT("txs"), Txs))
		{
			for (const TSharedPtr<FJsonValue>& V : *Txs)
			{
				const TSharedPtr<FJsonObject> Tx = V.IsValid() ? V->AsObject() : nullptr;
				// The mint (or the rule's verdict) is the link worth showing; else the first one.
				const FString Kind = Str(Tx, TEXT("kind"));
				if (R.ExplorerUrl.IsEmpty() || Kind == TEXT("mint") || Kind == TEXT("resolve")) R.ExplorerUrl = Str(Tx, TEXT("url"));
			}
		}
		return R;
	}

	static TArray<TSharedPtr<FJsonObject>> Objects(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				if (V.IsValid() && V->Type == EJson::Object) Out.Add(V->AsObject());
			}
		}
		return Out;
	}

	FGbRulesOverview RulesOverview(const TSharedPtr<FJsonObject>& O)
	{
		FGbRulesOverview R;
		R.bChain = Bool(O, TEXT("chain"));
		R.Cost = Int(O, TEXT("cost"));
		R.Points = Int(O, TEXT("points"), -1);
		for (const TSharedPtr<FJsonObject>& T : Objects(O, TEXT("templates")))
		{
			FGbRuleTemplate Tpl;
			Tpl.Id = Str(T, TEXT("id"));
			Tpl.Text = Str(T, TEXT("text"));
			Tpl.bCruel = Bool(T, TEXT("cruel"));
			for (const TSharedPtr<FJsonObject>& P : Objects(T, TEXT("params")))
			{
				FGbRuleParam Param;
				Param.Key = Str(P, TEXT("key"));
				for (const TSharedPtr<FJsonObject>& Opt : Objects(P, TEXT("options")))
				{
					FGbRuleOption& Option = Param.Options.AddDefaulted_GetRef();
					Option.Value = Str(Opt, TEXT("value"));
					Option.Label = Str(Opt, TEXT("label"));
				}
				Tpl.Params.Add(Param);
			}
			R.Templates.Add(Tpl);
		}
		for (const TSharedPtr<FJsonObject>& A : Objects(O, TEXT("active")))
		{
			FGbActiveRule Rule;
			Rule.Id = Int(A, TEXT("id"));
			Rule.Text = Str(A, TEXT("text"));
			Rule.Author = Str(A, TEXT("author"));
			Rule.Cost = Int(A, TEXT("cost"));
			Rule.ChainId = Int(A, TEXT("chainId"), -1);
			Rule.RepealCost = Int(A, TEXT("repealCost"), Rule.Cost * 2);
			R.Active.Add(Rule);
		}
		return R;
	}

	FGbShop Shop(const TSharedPtr<FJsonObject>& O)
	{
		FGbShop S;
		const TSharedPtr<FJsonObject> M = Obj(O, TEXT("merchant"));
		S.MerchantId = Str(M, TEXT("id"));
		S.MerchantName = Str(M, TEXT("name"));
		S.bClosed = Bool(M, TEXT("closed"));
		S.Ember = Int(O, TEXT("ember"), -1);
		const TSharedPtr<FJsonObject> BuyBack = Obj(O, TEXT("buyBack"));
		for (const TSharedPtr<FJsonObject>& I : Objects(O, TEXT("items")))
		{
			FGbShopItem& It = S.Items.AddDefaulted_GetRef();
			It.Id = Str(I, TEXT("id"));
			It.Name = Str(I, TEXT("name"));
			It.Stock = Int(I, TEXT("stock"));
			It.Price = Int(I, TEXT("price"));
			It.Text = Str(I, TEXT("text"));
			It.BuyBack = Int(BuyBack, *It.Id);
		}
		return S;
	}

	FGbPlayerView Player(const TSharedPtr<FJsonObject>& O)
	{
		FGbPlayerView P;
		P.Account = Str(O, TEXT("account"));
		P.Name = Str(O, TEXT("name"));
		P.bChain = Bool(O, TEXT("chain"));
		P.Points = Int(O, TEXT("points"), -1);
		P.Ember = Int(O, TEXT("ember"), -1);
		P.ExplorerUrl = Str(O, TEXT("explorerUrl"));
		for (const TSharedPtr<FJsonObject>& T : Objects(O, TEXT("talismans")))
		{
			FGbChainTalisman& C = P.Talismans.AddDefaulted_GetRef();
			C.TokenId = Str(T, TEXT("tokenId"));
			C.SpeciesId = Str(T, TEXT("speciesId"));
			C.Emotion = Str(T, TEXT("emotion"));
			C.Omen = Str(T, TEXT("omen"));
			C.Level = Int(T, TEXT("level"), 1);
			C.Wins = Int(T, TEXT("wins"));
			C.Purse = Int(T, TEXT("purse"));
		}
		return P;
	}

	FGbSatchel Satchel(const TSharedPtr<FJsonObject>& O)
	{
		FGbSatchel S;
		const TSharedPtr<FJsonObject> Supplies = Obj(O, TEXT("supplies"));
		const TSharedPtr<FJsonObject> Ink = Obj(O, TEXT("ink"));
		// Fail closed on an error/malformed body rather than showing a usable empty pocket.
		S.bValid = O.IsValid() && O->HasTypedField<EJson::Array>(TEXT("carried")) && O->HasTypedField<EJson::Array>(TEXT("case")) && Supplies.IsValid() && Ink.IsValid();
		if (!S.bValid) return S;
		S.Carried = Strings(O, TEXT("carried")); S.Case = Strings(O, TEXT("case"));
		S.Keepsakes = Strings(O, TEXT("keepsakes"));
        const auto Profile=Obj(O,TEXT("profile"));
        if (Profile && Int(Profile,TEXT("version"))==1)
        {
            auto& P=S.Profile; P.bValid=true; P.bComplete=Bool(Profile,TEXT("complete")); P.bLegacyStarter=Bool(Profile,TEXT("legacyStarter"));
            P.Name=Str(Profile,TEXT("name")); P.Look=Str(Profile,TEXT("look")); P.Pronouns=Str(Profile,TEXT("pronouns"));
            P.Starter=Str(Profile,TEXT("starter")); P.TokenId=Str(Profile,TEXT("tokenId")); P.Mint=Str(Profile,TEXT("mint")); P.ProofUrl=Str(Profile,TEXT("proofUrl"));
            for (const auto& V:Arr(Profile,TEXT("choices"))) {const auto C=AsObj(V); if (!C) continue; const auto Stats=Obj(C,TEXT("stats")); FGbStarterChoice Choice;
                Choice.Id=Str(C,TEXT("id")); Choice.Name=Str(C,TEXT("name")); Choice.Emotion=Str(C,TEXT("emotion")); Choice.Personality=Str(C,TEXT("personality"));
                Choice.Hp=Int(Stats,TEXT("hp")); Choice.Attack=Int(Stats,TEXT("attack")); Choice.Guard=Int(Stats,TEXT("guard")); Choice.Speed=Int(Stats,TEXT("speed")); P.Choices.Add(Choice);}
        }
        if (const auto Levels = Obj(O, TEXT("levels")))
            for (const auto& Entry : Levels->Values)
                if (const auto L = AsObj(Entry.Value)) { FGbLevel V; V.Level = FMath::Clamp(Int(L, TEXT("level"), 1), 1, 10); V.Xp = Int(L, TEXT("xp")); V.Next = Int(L, TEXT("next"), -1); S.Levels.Add(FString(Entry.Key), V); }
        if (const auto R = Obj(O, TEXT("rival")))
        {
            S.Rival.bValid = true; S.Rival.Name = Str(R, TEXT("name")); S.Rival.Where = Str(R, TEXT("where")); S.Rival.Stage = Int(R, TEXT("stage"));
            S.Rival.bReady = Bool(R, TEXT("ready")); S.Rival.Level = Int(R, TEXT("level"), 1); S.Rival.SpeciesId = Str(R, TEXT("speciesId")); S.Rival.Line = Str(R, TEXT("line"));
        }
        if (O->HasTypedField<EJson::Boolean>(TEXT("tideOpen"))) S.bTideOpen = Bool(O, TEXT("tideOpen"));
        const auto E = Obj(O, TEXT("exploration"));
        S.bExplorationAvailable = E.IsValid() && Int(E, TEXT("version")) == 1;
        if (S.bExplorationAvailable)
        {
            S.DiscoveryFlags = Strings(E, TEXT("flags")); S.Equipped = Str(E, TEXT("equipped")); S.Tracked = Str(E, TEXT("tracked"));
            for (const auto& V : Arr(E, TEXT("gear"))) { const auto G=AsObj(V); if (!G) continue; FGbGear R;
                R.Id=Str(G,TEXT("id")); R.Name=Str(G,TEXT("name")); R.Source=Str(G,TEXT("source")); R.Attack=Int(G,TEXT("attack")); R.Guard=Int(G,TEXT("guard")); S.Gear.Add(R); }
            for (const auto& V : Arr(E, TEXT("journal"))) { const auto G=AsObj(V); if (!G) continue; FGbJournalEntry R;
                R.Id=Str(G,TEXT("id")); R.Title=Str(G,TEXT("title")); R.Status=Str(G,TEXT("status")); R.Objective=Str(G,TEXT("objective")); R.Hint=Str(G,TEXT("hint")); R.Target=Str(G,TEXT("target")); S.Journal.Add(R); }
            for (const auto& V : Arr(E, TEXT("notes"))) { const auto G=AsObj(V); if (!G) continue; FGbKeepsake R;
                R.Id=Str(G,TEXT("id")); R.Title=Str(G,TEXT("title")); R.Text=Str(G,TEXT("text")); S.Notes.Add(R); }
            S.Gossip = Strings(E, TEXT("gossip"));
            for (const auto& V : Arr(E, TEXT("lore"))) { const auto G=AsObj(V); if (!G) continue; FGbKeepsake R;
                R.Id=Str(G,TEXT("id")); R.Title=Str(G,TEXT("title")); R.Text=Str(G,TEXT("text")); if (!R.Title.IsEmpty()) S.Lore.Add(R); }
            for (const auto& V : Arr(E, TEXT("tips"))) { const auto G=AsObj(V); if (!G) continue; FGbTip R;
                R.Id=Str(G,TEXT("id")); R.Text=Str(G,TEXT("text")); G->TryGetBoolField(TEXT("seen"), R.bSeen); if (!R.Id.IsEmpty() && !R.Text.IsEmpty()) S.Tips.Add(R); }
        }
		S.Blank = FMath::Max(0, Int(Supplies, TEXT("blank")));
		S.Gilded = FMath::Max(0, Int(Supplies, TEXT("gilded")));
		S.Salves = FMath::Max(0, Int(Supplies, TEXT("salve")));
		S.Ember = Int(O, TEXT("ember"), -1); S.Points = Int(O, TEXT("points"), -1);
		for (const auto& Entry : Ink->Values)
		{
			const auto I = AsObj(Entry.Value);
			if (!I.IsValid()) continue;
			FGbInk Value; Value.MaxHp = FMath::Max(0, Int(I, TEXT("maxHp")));
			Value.Hp = FMath::Clamp(Int(I, TEXT("hp")), 0, Value.MaxHp);
			S.Ink.Add(FString(Entry.Key), Value);
		}
		return S;
	}

    FString NamedBody(const FString& Key, const FString& Value)
    {
        const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetStringField(Key, Value); return Write(O);
    }
	FString TokenBody(const FString& TokenId)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("tokenId"), TokenId); return Write(O);
	}

	FString CarryBody(const FString& TokenId, bool bCarry)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("tokenId"), TokenId); O->SetBoolField(TEXT("carry"), bCarry); return Write(O);
	}

	FGbTrade Trade(const TSharedPtr<FJsonObject>& O, bool bBuy)
	{
		FGbTrade T;
		T.bBuy = bBuy;
		T.JobId = Str(O, TEXT("jobId"));
		T.ItemId = Str(O, TEXT("itemId"));
		T.Price = Int(O, TEXT("price"));
		T.Text = Str(O, TEXT("text"));
		T.SpeciesId = Str(O, TEXT("speciesId"));
		T.Omen = Str(O, TEXT("omen"));
		T.Chain = ChainReceipt(Obj(O, TEXT("chain")));
		T.TokenId = T.Chain.TokenId;
		return T;
	}

	FString TradeBody(const FString& Account, const FString& Merchant, const FString& Key, const FString& Value)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("account"), Account.TrimStartAndEnd().ToLower());
		O->SetStringField(TEXT("merchant"), Merchant);
		O->SetStringField(Key, Value);
		return Write(O);
	}

	FGbRuleVerdict RuleVerdict(const TSharedPtr<FJsonObject>& O)
	{
		FGbRuleVerdict V;
		V.JobId = Str(O, TEXT("jobId"));
		V.bAccepted = Bool(O, TEXT("accepted"));
		V.Reason = Str(O, TEXT("reason"));
		V.Text = Str(O, TEXT("text"));
		V.Cost = Int(O, TEXT("cost"));
		V.Chain = ChainReceipt(Obj(O, TEXT("chain")));
		return V;
	}

	FGbRuleVerdict Repeal(const TSharedPtr<FJsonObject>& O)
	{
		FGbRuleVerdict V;
		V.JobId = Str(O, TEXT("jobId"));
		V.bAccepted = true;
		V.Text = Str(O, TEXT("text"));
		V.Cost = Int(O, TEXT("cost"));
		V.Chain = ChainReceipt(Obj(O, TEXT("chain")));
		V.Chain.RuleId = Int(O, TEXT("ruleId"), V.Chain.RuleId);
		return V;
	}

	FString RepealBody(const FString& Account, int32 ChainRuleId)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("account"), Account.TrimStartAndEnd().ToLower());
		O->SetNumberField(TEXT("ruleId"), ChainRuleId);
		return Write(O);
	}

	FString ProposeBody(const FString& Account, const FString& TemplateId, const TMap<FString, FString>& Params)
	{
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& KV : Params) P->SetStringField(KV.Key, KV.Value);
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("account"), Account.TrimStartAndEnd().ToLower());
		O->SetStringField(TEXT("templateId"), TemplateId);
		O->SetObjectField(TEXT("params"), P);
		return Write(O);
	}

	FGbVowsOverview VowsOverview(const TSharedPtr<FJsonObject>& O)
	{
		FGbVowsOverview V;
		V.bChain = Bool(O, TEXT("chain")); V.Points = Int(O, TEXT("points"), -1); V.DaySeconds = Num(O, TEXT("daySeconds"));
		for (const auto& T : Objects(O, TEXT("templates")))
		{
			FGbVowTemplate& R = V.Templates.AddDefaulted_GetRef();
			R.Id = Str(T, TEXT("id")); R.Text = Str(T, TEXT("text")); R.Stake = Int(T, TEXT("stake"));
			R.MinDays = Int(T, TEXT("minDays"), 1); R.MaxDays = Int(T, TEXT("maxDays"), 1);
		}
		for (const auto& T : Objects(O, TEXT("talismans")))
		{
			FGbRuleOption& R = V.Talismans.AddDefaulted_GetRef(); R.Value = Str(T, TEXT("id")); R.Label = Str(T, TEXT("name"));
		}
		for (const auto& T : Objects(O, TEXT("vows")))
		{
			FGbVow& R = V.Vows.AddDefaulted_GetRef();
			R.Id = Int(T, TEXT("id"), -1); R.Text = Str(T, TEXT("text")); R.Status = Str(T, TEXT("status"));
			R.Stake = Int(T, TEXT("stake")); R.Payout = Int(T, TEXT("payout")); R.EndsAt = Num(T, TEXT("endsAt"));
		}
		return V;
	}

	FString VowBody(const FString& TemplateId, int32 Days, const TMap<FString, FString>& Params)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		for (const auto& KV : Params) P->SetStringField(KV.Key, KV.Value);
		O->SetStringField(TEXT("templateId"), TemplateId); O->SetNumberField(TEXT("days"), Days); O->SetObjectField(TEXT("params"), P);
		return Write(O);
	}

	FString StartBody(const FString& Ward, const FString& PlayerCurse, const FString& By, const FString& Account, const FString& TokenId, int32 Rival)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("ward"), Ward);
		if (Rival > 0) O->SetNumberField(TEXT("rival"), Rival);
		if (!PlayerCurse.IsEmpty()) O->SetStringField(TEXT("playerCurse"), PlayerCurse);
		if (!TokenId.IsEmpty()) O->SetStringField(TEXT("tokenId"), TokenId);
		if (!By.IsEmpty()) O->SetStringField(TEXT("by"), By);
		const FString Acct = Account.TrimStartAndEnd().ToLower();
		if (!Acct.IsEmpty()) O->SetStringField(TEXT("account"), Acct);
		return Write(O);
	}

	FString TurnBody(const FString& BattleId, EGbAction Action)
	{
		const TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
		switch (Action)
		{
		case EGbAction::Attack: A->SetStringField(TEXT("kind"), TEXT("attack")); break;
		case EGbAction::Ward: A->SetStringField(TEXT("kind"), TEXT("ward")); break;
		case EGbAction::Seal: A->SetStringField(TEXT("kind"), TEXT("seal")); break;
		case EGbAction::GildedSeal:
			A->SetStringField(TEXT("kind"), TEXT("seal"));
			A->SetBoolField(TEXT("gilded"), true);
			break;
		case EGbAction::Salve:
			A->SetStringField(TEXT("kind"), TEXT("item"));
			A->SetStringField(TEXT("item"), TEXT("salve"));
			break;
		case EGbAction::Flee: A->SetStringField(TEXT("kind"), TEXT("flee")); break;
		}
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("battleId"), BattleId);
		O->SetObjectField(TEXT("action"), A);
		return Write(O);
	}
}
