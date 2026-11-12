#include "GbPanelFixtures.h"

#if !UE_BUILD_SHIPPING
#include "GbWidgets.h"
#include "GbSatchelWidget.h"
#include "GbVowWidget.h"
#include "GbObservatoryWidget.h"
#include "GbArrivalWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"

namespace GbPanelFixture
{
	FGbTalisman Talisman(const TCHAR* Id, const TCHAR* Species, const TCHAR* Omen, int32 Day, int32 Wins, const TCHAR* Token = TEXT(""), int32 Purse = 0)
	{
		FGbTalisman T; T.Id = Id; T.SpeciesId = Species; T.Omen = Omen; T.BoundDay = Day; T.Wins = Wins; T.ChainTokenId = Token; T.Purse = Purse;
		return T;
	}
	FGbCurseInfo Curse(const TCHAR* Name, const TCHAR* Emotion) { FGbCurseInfo C; C.Name = Name; C.Emotion = Emotion; return C; }

	// ---- Talisman Case: six talismans, the starter as partner.
	void CaseData(TArray<FGbTalisman>& OutTs, TMap<FString, FGbCurseInfo>& OutCs)
	{
		const TArray<FGbTalisman> Ts = {
			Talisman(TEXT("tal-starter"), TEXT("cinder-grin"), TEXT("whisper"), -1, 7),
			Talisman(TEXT("tal-1"), TEXT("hushmoth"), TEXT("wail"), 2, 3, TEXT("tal-12"), 5),
			Talisman(TEXT("tal-2"), TEXT("weepwick"), TEXT("whisper"), 3, 1, TEXT("tal-15")),
			Talisman(TEXT("tal-3"), TEXT("riot-bell"), TEXT("calamity"), 4, 0),
			Talisman(TEXT("tal-4"), TEXT("tallow-widow"), TEXT("wail"), 6, 2, TEXT("tal-21")),
			Talisman(TEXT("tal-5"), TEXT("lantern-drowned"), TEXT("whisper"), 7, 0) };
		const TMap<FString, FGbCurseInfo> Cs = {
			{ TEXT("cinder-grin"), Curse(TEXT("Cinder Grin"), TEXT("rage")) }, { TEXT("hushmoth"), Curse(TEXT("Hushmoth"), TEXT("dread")) },
			{ TEXT("weepwick"), Curse(TEXT("Weepwick"), TEXT("sorrow")) }, { TEXT("riot-bell"), Curse(TEXT("Riot Bell"), TEXT("rage")) },
			{ TEXT("tallow-widow"), Curse(TEXT("Tallow Widow"), TEXT("sorrow")) }, { TEXT("lantern-drowned"), Curse(TEXT("Lantern Drowned"), TEXT("dread")) } };
		OutTs = Ts; OutCs = Cs;
	}
}

const TArray<FString>& FGbPanelFixtures::Shots()
{
	static const TArray<FString> All = { TEXT("TalismanCase"), TEXT("Mender"), TEXT("Satchel-Talismans"), TEXT("Satchel-Supplies"),
		TEXT("Satchel-Keepsakes"), TEXT("Satchel-Ledger"), TEXT("Satchel-Gear"), TEXT("Satchel-Journal"), TEXT("Broker"), TEXT("Rite"), TEXT("Vows"), TEXT("Observatory-Grudge"),
		TEXT("Observatory-Prices"), TEXT("Observatory-Rules"), TEXT("Room") };
	return All;
}

void FGbPanelFixtures::Fill(UGbWidgetBase* Widget, const FString& Shot)
{
	using namespace GbPanelFixture;
    if (UGbArrivalWidget* W=Cast<UGbArrivalWidget>(Widget))
    {
        W->Profile=FGbArrivalProfile(); W->Profile.bValid=true;
        const TCHAR* Ids[]={TEXT("cinder-grin"),TEXT("weepwick"),TEXT("hushmoth")};
        const TCHAR* Names[]={TEXT("Cinder Grin"),TEXT("Weepwick"),TEXT("Hushmoth")};
        const TCHAR* Emotions[]={TEXT("rage"),TEXT("sorrow"),TEXT("dread")};
        for (int32 I=0;I<3;++I) {FGbStarterChoice C; C.Id=Ids[I]; C.Name=Names[I]; C.Emotion=Emotions[I]; C.Personality=TEXT("A companion for the road ahead."); C.Hp=40; C.Attack=10; C.Guard=8; C.Speed=10; W->Profile.Choices.Add(C);}
        W->ChosenName=TEXT("Ari"); W->ChosenLook=TEXT("feminine"); W->ChosenPronouns=TEXT("they"); W->ChosenStarter=TEXT("hushmoth");
        W->Page=Shot==TEXT("arrival-name")?5:Shot==TEXT("arrival-partner")?6:0;
        W->Redraw(); W->PreviewLook(); return;
    }
	if (UGbTalismanCaseWidget* W = Cast<UGbTalismanCaseWidget>(Widget))
	{
		TArray<FGbTalisman> Ts; TMap<FString, FGbCurseInfo> Cs; CaseData(Ts, Cs);
		W->Show(Ts, Cs, TEXT("tal-starter"), 4, 5, 1, 0, 42, 60, true);
		return;
	}

	// ---- Mender: one talisman drained, the price known.
	if (UGbMenderWidget* W = Cast<UGbMenderWidget>(Widget))
	{
		FGbSatchel S; S.bValid = true; S.Carried = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15") }; S.Case = { TEXT("tal-21") }; S.Ember = 60;
		const TCHAR* Ids[] = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15"), TEXT("tal-21") };
		const int32 Hp[] = { 44, 12, 0, 40 };
		const TCHAR* Sp[] = { TEXT("cinder-grin"), TEXT("hushmoth"), TEXT("weepwick"), TEXT("tallow-widow") };
		const TCHAR* Nm[] = { TEXT("Cinder Grin"), TEXT("Hushmoth"), TEXT("Weepwick"), TEXT("Tallow Widow") };
		W->Names.Reset(); W->Species.Reset(); W->Selected.Reset();
		for (int32 I = 0; I < 4; ++I) { FGbInk Ink; Ink.Hp = Hp[I]; Ink.MaxHp = 44; S.Ink.Add(Ids[I], Ink); W->Names.Add(Ids[I], Nm[I]); W->Species.Add(Ids[I], Sp[I]); }
		W->Data = S; W->Price = 12; W->bBusy = false; W->Redraw();
		W->Status->SetText(FText::FromString(TEXT("Hushmoth's ink is restored.")));
		return;
	}

	// ---- Satchel: four carried, two in the Case, one drained; the pocket named by the shot.
	if (UGbSatchelWidget* W = Cast<UGbSatchelWidget>(Widget))
	{
		FGbSatchel S; S.bValid = true; S.Carried = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15"), TEXT("tal-21") }; S.Case = { TEXT("tal-30"), TEXT("tal-31") };
		S.Blank = 3; S.Gilded = 1; S.Salves = 2; S.Ember = 60; S.Points = 42; S.Keepsakes = { TEXT("Keepsake one"), TEXT("Keepsake two") };
		const TCHAR* Ids[] = { TEXT("starter"), TEXT("tal-12"), TEXT("tal-15"), TEXT("tal-21"), TEXT("tal-30"), TEXT("tal-31") };
		const TCHAR* Sp[] = { TEXT("cinder-grin"), TEXT("hushmoth"), TEXT("weepwick"), TEXT("tallow-widow"), TEXT("riot-bell"), TEXT("lantern-drowned") };
		const TCHAR* Nm[] = { TEXT("Cinder Grin"), TEXT("Hushmoth"), TEXT("Weepwick"), TEXT("Tallow Widow"), TEXT("Riot Bell"), TEXT("Lantern Drowned") };
		const int32 Hp[] = { 44, 30, 0, 44, 44, 18 };
		W->Names.Reset(); W->Talismans.Reset();
		for (int32 I = 0; I < 6; ++I)
		{
			FGbInk Ink; Ink.Hp = Hp[I]; Ink.MaxHp = 44; S.Ink.Add(Ids[I], Ink); W->Names.Add(Ids[I], Nm[I]);
			W->Talismans.Add(Talisman(I == 0 ? TEXT("tal-starter") : Ids[I], Sp[I], TEXT("whisper"), I, 0, I == 0 ? TEXT("") : Ids[I]));
		}
        S.bExplorationAvailable=true; S.Equipped=TEXT("worn-focus"); S.Tracked=TEXT("net");
        const TCHAR* GearIds[]={TEXT("worn-focus"),TEXT("ward-clasp"),TEXT("etched-focus")};
        const TCHAR* GearNames[]={TEXT("Worn Focus"),TEXT("Ward Clasp"),TEXT("Etched Focus")};
        const TCHAR* Sources[]={TEXT("Returned the Lamplighter's kit."),TEXT("Helped the Net mender on Tide's docks."),TEXT("Found in the sealed storehouse.")};
        for (int32 I=0;I<3;++I) {FGbGear G; G.Id=GearIds[I]; G.Name=GearNames[I]; G.Source=Sources[I]; G.Attack=I==0?1:I==2?2:0; G.Guard=I==1?2:0; S.Gear.Add(G);}
        FGbJournalEntry J; J.Id=TEXT("lamp"); J.Title=TEXT("A light returned"); J.Status=TEXT("complete"); J.Objective=TEXT("The Lamplighter's kit is back. Your Worn Focus is in Gear."); J.Hint=TEXT("Speak with the Lamplighter along the market lane."); S.Journal.Add(J);
        J.Id=TEXT("net"); J.Title=TEXT("Among the reeds"); J.Status=TEXT("active"); J.Objective=TEXT("Recover the bundle by Tide's eastern reeds."); J.Hint=TEXT("Take the ferry from Lantern Ward's southern pier."); S.Journal.Add(J);
        J.Id=TEXT("storehouse"); J.Title=TEXT("The sealed storehouse"); J.Status=TEXT("available"); J.Objective=TEXT("Bring the marked slip and brass key to the Receipt clerk."); J.Hint=TEXT("The marked door stands on Tide's northern quay."); S.Journal.Add(J);
        FGbKeepsake Note; Note.Id=TEXT("delivery-slip"); Note.Title=TEXT("Marked delivery slip"); Note.Text=TEXT("A dockside storehouse stamp marks this delivery. Ask the Receipt clerk after finding its key."); S.Notes.Add(Note); S.Keepsakes={Note.Id};
		W->Data = S; W->PartnerToken = TEXT("starter"); W->SelectedToken = TEXT("tal-15"); W->bBusy = false;
		const TCHAR* Pockets[] = { TEXT("Satchel-Talismans"), TEXT("Satchel-Supplies"), TEXT("Satchel-Keepsakes"), TEXT("Satchel-Ledger"), TEXT("Satchel-Gear"), TEXT("Satchel-Journal") };
		for (int32 I = 0; I < 6; ++I) if (Shot == Pockets[I]) W->Pockets->SetActiveWidgetIndex(I);
		W->Redraw();
		return;
	}

	// ---- The Broker: the Lantern Ward stock (shared/src/engine.ts), 60 Ember, a trade waiting on the ledger.
	if (UGbShopWidget* W = Cast<UGbShopWidget>(Widget))
	{
		FGbShop Shop; Shop.MerchantId = UGbShopWidget::MerchantId; Shop.MerchantName = TEXT("the Talisman Broker"); Shop.Ember = 60;
		const auto Item = [&Shop](const TCHAR* Id, const TCHAR* Name, int32 Price, int32 Stock, const TCHAR* Text)
		{
			FGbShopItem It; It.Id = Id; It.Name = Name; It.Price = Price; It.Stock = Stock; It.Text = Text; It.BuyBack = Price / 2; Shop.Items.Add(It);
		};
		Item(TEXT("talisman-rage"), TEXT("Rage talismans"), 34, 6, TEXT("Riot season; rage sells."));
		Item(TEXT("talisman-sorrow"), TEXT("Sorrow talismans"), 28, 5, TEXT("Quiet week by the canal."));
		Item(TEXT("talisman-dread"), TEXT("Dread talismans"), 30, 6, TEXT("Steady."));
		Item(TEXT("blank-talisman"), TEXT("Blank talismans"), 15, 10, TEXT("Paper and ink."));
		Item(TEXT("gilded-talisman"), TEXT("Gilded talismans"), 30, 0, TEXT("Sold out until the gilder returns."));
		Item(TEXT("ink-salve"), TEXT("Ink Salves"), 10, 12, TEXT("Not for players yet."));
		W->Data = Shop;
		W->Redraw();
		W->Status->SetText(FText::FromString(TEXT("You bought a blank talisman for 15 Ember. Sealing it in the ledger...")));
		return;
	}

	// ---- The Rite: two rules in force on the ledger, R1 being built (shared/src/rules.ts), Kessa answering.
	if (UGbRulesWidget* W = Cast<UGbRulesWidget>(Widget))
	{
		FGbRulesOverview Rules; Rules.bChain = true; Rules.Cost = 20; Rules.Points = 42;
		const auto Param = [](const TCHAR* Key, std::initializer_list<const TCHAR*> Labels)
		{
			FGbRuleParam P; P.Key = Key;
			for (const TCHAR* L : Labels) { FGbRuleOption O; O.Value = L; O.Label = L; P.Options.Add(O); }
			return P;
		};
		FGbRuleTemplate R1; R1.Id = TEXT("R1"); R1.Text = TEXT("{action} in {ward} gives x{multiplier} points");
		R1.Params = { Param(TEXT("action"), { TEXT("binding"), TEXT("exorcising") }), Param(TEXT("ward"), { TEXT("Lantern Ward"), TEXT("Tide Ward") }),
			Param(TEXT("multiplier"), { TEXT("1.5"), TEXT("2") }) };
		FGbRuleTemplate R8; R8.Id = TEXT("R8"); R8.Text = TEXT("No PvP in {ward}"); R8.Params = { Param(TEXT("ward"), { TEXT("Lantern Ward") }) };
		Rules.Templates = { R1, R8 };
		const auto Active = [&Rules](int32 Id, const TCHAR* Text, const TCHAR* Author, int32 Cost)
		{
			FGbActiveRule R; R.Id = Id; R.ChainId = Id - 1; R.Text = Text; R.Author = Author; R.Cost = Cost; R.RepealCost = Cost * 2; Rules.Active.Add(R);
		};
		Active(1, TEXT("No PvP in Lantern Ward"), TEXT("fen.grudgebound.testnet"), 20);
		Active(2, TEXT("Fleeing a battle costs 5 points"), TEXT("player.testnet"), 20);
		W->HandleRules(Rules);
		W->Status->SetText(FText::FromString(TEXT("Kessa is reading your rule...")));
		return;
	}

	// ---- The Vow Shrine: the four MVP vows (shared/src/vows.ts), one holding, one kept, one released; V3 being sworn.
	if (UGbVowWidget* W = Cast<UGbVowWidget>(Widget))
	{
		FGbVowsOverview Vows; Vows.bChain = true; Vows.Points = 42; Vows.DaySeconds = 600.;
		const auto Template = [&Vows](const TCHAR* Id, const TCHAR* Text, int32 Stake, int32 Min, int32 Max)
		{
			FGbVowTemplate T; T.Id = Id; T.Text = Text; T.Stake = Stake; T.MinDays = Min; T.MaxDays = Max; Vows.Templates.Add(T);
		};
		Template(TEXT("V1"), TEXT("I will not leave {ward}"), 20, 1, 3);
		Template(TEXT("V2"), TEXT("I will not use healing items"), 15, 1, 2);
		Template(TEXT("V3"), TEXT("I will not sell {talisman}"), 10, 3, 3);
		Template(TEXT("V4"), TEXT("I will fight only {omen} curses"), 25, 1, 1);
		for (const TCHAR* Species : { TEXT("weepwick"), TEXT("hushmoth") })
		{
			FGbRuleOption O; O.Value = FString::Printf(TEXT("%d"), Vows.Talismans.Num() + 12); O.Label = Species; Vows.Talismans.Add(O);
		}
		const auto Vow = [&Vows](int32 Id, const TCHAR* Text, const TCHAR* Status, int32 Stake, double EndsInSeconds)
		{
			FGbVow V; V.Id = Id; V.Text = Text; V.Status = Status; V.Stake = Stake; V.Payout = Stake + Stake / 5;
			V.EndsAt = (FDateTime::UtcNow().ToUnixTimestamp() + EndsInSeconds) * 1000.; Vows.Vows.Add(V);
		};
		Vow(3, TEXT("I will not leave lantern"), TEXT("Active"), 20, 754.);
		Vow(2, TEXT("I will fight only wail curses"), TEXT("Kept"), 25, -60.);
		Vow(1, TEXT("I will not use healing items"), TEXT("Released"), 15, -600.);
		W->TemplateIndex = 0;
		W->HandleVows(Vows);
		W->HandleNextTemplate(); W->HandleNextTemplate();
		W->Status->SetText(FText::FromString(TEXT("Sealing your promise in the Rite's ledger...")));
		return;
	}

	// ---- The Observatory: a week of four Wards and the Broker's prices, a bankruptcy, a Calamity, two rules.
	if (UGbObservatoryWidget* W = Cast<UGbObservatoryWidget>(Widget))
	{
		W->bOpen = true;
		FGbObservatoryData D; D.bValid = true; D.Mode = TEXT("live");
		for (int32 Tick = 1; Tick <= 168; ++Tick)
		{
			FGbObservatorySample& S = D.Samples.AddDefaulted_GetRef(); S.Tick = Tick; S.Day = Tick / 24 + 1; S.Hour = Tick % 24;
			int32 Shift = 0;
			for (const TCHAR* Id : { TEXT("lantern"), TEXT("tide"), TEXT("ash"), TEXT("bone") })
			{
				auto& Ward = S.Wards.AddDefaulted_GetRef(); Ward.Id = Id;
				Ward.Grudge.Rage = 12 + (Tick + Shift) % 30 / 2; Ward.Grudge.Sorrow = 18 + FMath::RoundToInt(6 * FMath::Sin(Tick / 9.f + Shift)); Ward.Grudge.Dread = 8 + Tick / 24;
				Ward.Total = Ward.Grudge.Rage + Ward.Grudge.Sorrow + Ward.Grudge.Dread; Shift += 7;
			}
			auto& Broker = S.Prices.Add(TEXT("broker"));
			Broker.Items = { { TEXT("blank-talisman"), 15.f }, { TEXT("talisman-rage"), float(34 + Tick % 24 / 3) }, { TEXT("talisman-sorrow"), 28.f + Tick / 40 },
				{ TEXT("talisman-dread"), 30.f }, { TEXT("gilded-talisman"), 30.f + Tick / 30 }, { TEXT("ink-salve"), 10.f } };
		}
		FGbObservatoryEvent E; E.Tick = 49; E.Type = TEXT("bankruptcy"); E.Ward = TEXT("tide"); D.Events.Add(E);
		E.Tick = 73; E.Type = TEXT("calamity"); E.Ward = TEXT("lantern"); D.Events.Add(E);
		FGbObservatoryRule R; R.Id = TEXT("1"); R.Tick = 24; R.Author = TEXT("Old Mother Fen"); R.AuthorKind = TEXT("agent"); R.Text = TEXT("No PvP in Lantern Ward"); R.Status = TEXT("active"); D.Rules.Add(R);
		R.Id = TEXT("2"); R.Tick = 72; R.Text = TEXT("Every talisman trade pays 10% Ember to author"); R.Status = TEXT("rejected");
		R.Reason = TEXT("A toll on every trade for its author alone."); R.Url = TEXT("https://testnet.nearblocks.io/txns/fixture"); D.Rules.Add(R);
		W->Receive(D);
		const TCHAR* Tabs[] = { TEXT("Observatory-Grudge"), TEXT("Observatory-Prices"), TEXT("Observatory-Rules") };
		for (int32 Tab = 0; Tab < 3; ++Tab) if (Shot == Tabs[Tab]) W->Tab = Tab;
		W->Redraw();
		return;
	}
	// ---- Your room needs no data.
}
#endif
