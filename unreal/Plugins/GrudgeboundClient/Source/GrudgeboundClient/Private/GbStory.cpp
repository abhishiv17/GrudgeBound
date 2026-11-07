#include "GbStory.h"
#include "Misc/SecureHash.h"

namespace
{
	FString WardName(const FString& Id)
	{
		if (Id == TEXT("lantern")) return TEXT("Lantern Ward");
		if (Id == TEXT("tide")) return TEXT("Tide Ward");
		if (Id == TEXT("ash")) return TEXT("Ash Ward");
		if (Id == TEXT("bone")) return TEXT("Bone Ward");
		return TEXT("the town");
	}
}

namespace GbStory
{
	const TArray<FPanel>& IntroPanels()
	{
		static const TArray<FPanel> Panels = {
			{ TEXT("A town that keeps receipts"), TEXT("You reach Hollowmere with a debt notice folded inside your coat.\n\nBeyond the harbour wall, a thousand lanterns burn over a market that never quite closes.") },
			{ TEXT("What the market leaves behind"), TEXT("A stolen wage. A rigged bargain. A shop shuttered for the last time.\n\nRage, Sorrow and Dread gather into Grudge. And Grudge gives the town's discarded things teeth.") },
			{ TEXT("Kessa's offer"), TEXT("Kessa: Keep the notice. You may want to remember what brought you here.\n\nTake this talisman instead. Bind what haunts our streets, earn your place, and mind what your bargains leave behind.") },
			{ TEXT("A place in the ledger"), TEXT("Kessa: The Rite remembers who holds a talisman and what the town has sworn.\n\nEarn points, write a rule, or stake them on a promise. The lanterns are lit. What happens next is partly yours.") },
		};
		return Panels;
	}

	int32 Chapter(const FGbStoryProgress& P)
	{
		if (P.bFirstBind && P.bBankruptcy && P.bDebtAcknowledged && P.bCalamity) return 3;
		return P.bFirstBind && P.bBankruptcy ? 2 : 1;
	}

	FString ChapterTitle(const FGbStoryProgress& P)
	{
		switch (Chapter(P))
		{
		case 2: return TEXT("Chapter 2  /  Debts Come Due");
		case 3: return TEXT("Chapter 3  /  The Calamity");
		default: return TEXT("Chapter 1  /  Arrival");
		}
	}

	FString Objective(const FGbStoryProgress& P)
	{
		if (!P.bFirstBind) return TEXT("Bind your first curse in Lantern Ward's tall grass.");
		if (!P.bVisitedBroker) return TEXT("Open the Broker (B). See what your first seal is worth.");
		if (!P.bBankruptcy) return TEXT("Keep binding and watch the market. A failing shop feeds the Grudge.");
		if (!P.bDebtAcknowledged) return FString::Printf(TEXT("Return to the Broker (B). A shuttered shop in %s has left more than debt."), *WardName(P.DebtWard));
		if (!P.bCalamity) return FString::Printf(TEXT("A shop failed in %s. Watch what its debt leaves behind."), *WardName(P.DebtWard));
		if (!P.bResponded) return FString::Printf(TEXT("A Calamity rose in %s. Win a fight, pass a rule (R), or swear a vow (V)."), *WardName(P.CalamityWard));
		return TEXT("You answered the town. Keep binding, trade carefully, and shape the next rule.");
	}

	bool Observe(FGbStoryProgress& P, const FGbTownEvent& E)
	{
		if (E.Type == TEXT("bankruptcy") && !P.bBankruptcy)
		{
			P.bBankruptcy = true; P.DebtWard = E.Ward; return true;
		}
		if (E.Type == TEXT("calamity") && !P.bCalamity)
		{
			P.bCalamity = true; P.CalamityWard = E.Ward; return true;
		}
		return false;
	}

	bool Observe(FGbStoryProgress& P, const FGbWorldState& World)
	{
		if (!World.bValid || P.bBankruptcy) return false;
		for (const auto& Merchant : World.Merchants)
		{
			if (Merchant.bClosed) { P.bBankruptcy = true; P.DebtWard = Merchant.Ward; return true; }
		}
		return false;
	}

	bool Respond(FGbStoryProgress& P)
	{
		if (Chapter(P) != 3 || P.bResponded) return false;
		P.bResponded = true; return true;
	}

	FString SaveSlot(const FString& Account, const FString& ServerUrl)
	{
		// Hash both parts so server URLs and account punctuation never become paths.
		return TEXT("Grudgebound_Story_") + FMD5::HashAnsiString(*(ServerUrl + TEXT("\n") + Account));
	}
}
