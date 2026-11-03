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

	FGbTrade Trade(const TSharedPtr<FJsonObject>& O, bool bBuy)
	{
		FGbTrade T;
		T.bBuy = bBuy;
		T.JobId = Str(O, TEXT("jobId"));
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

	FString StartBody(const FString& Ward, const FString& PlayerCurse, const FString& By, const FString& Account)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("ward"), Ward);
		if (!PlayerCurse.IsEmpty()) O->SetStringField(TEXT("playerCurse"), PlayerCurse);
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
