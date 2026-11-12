#include "GbTalk.h"

#include "GbApiSubsystem.h"
#include "GbAudio.h"
#include "GbBuildings.h"
#include "GbGreyboxMaterials.h"
#include "GbPlayerSubsystem.h"
#include "GbStorySubsystem.h"
#include "GbTownsfolk.h"
#include "GbUiDirector.h"
#include "GbUiStyle.h"
#include "GbUserSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FVoice { const TCHAR* Role; const TCHAR* Lines[2]; };

	// Draft lines for the owner's review. One voice per street route, in the order the Ward lists its routes.
	const FVoice Lantern[] = {
		{ TEXT("Lamplighter"), { TEXT("A thousand lanterns, and I trim every wick. Count them if you doubt me."),
			TEXT("Keep to the lit streets. What lives in the tall grass does not like my lamps.") } },
		{ TEXT("Noodle seller"), { TEXT("Broth's hot. Ember first, then the bowl. That is how we stay friends."),
			TEXT("The room above my stall is let to an exorcist now. A quiet lodger, so far.") } },
		{ TEXT("Porter"), { TEXT("Everything in this market came in on somebody's back. Usually mine."),
			TEXT("The Counting House weighs what I carry and pays me for less.") } },
		{ TEXT("Receipt clerk"), { TEXT("Keep your receipts. Hollowmere does."),
			TEXT("The Broker's prices move with the mood of the street. I only write them down.") } },
		{ TEXT("Dice player"), { TEXT("I was up forty Ember at dusk. Don't ask about now."),
			TEXT("Never stake what you can't lose. I tell everyone that, then I sit down again.") } },
		{ TEXT("Washerwoman"), { TEXT("Soot and broth scrub out. A grudge stays in the cloth."),
			TEXT("The Mender will ink a tired talisman back to strength. It costs more on sad nights.") } },
		{ TEXT("Courier"), { TEXT("A message for the Rite Hall. Kessa reads every one, the fair and the foolish."),
			TEXT("I carry each new rule from the Rite board to every door. People read the ones that cost them.") } },
		{ TEXT("Night watch"), { TEXT("Curfew or no curfew, I walk until the lanterns gutter."),
			TEXT("The gates to Ash and Bone stay shut. Ask Kessa why, not me.") } },
	};
	const FVoice Tide[] = {
		{ TEXT("Dockhand"), { TEXT("Mind the ropes. The tide takes whatever isn't tied down."),
			TEXT("Crates in, crates out. Nobody asks what is in the ones stamped twice.") } },
		{ TEXT("Net mender"), { TEXT("A net is all holes, held together by knots. So is a town."),
			TEXT("The reeds past the quay rustle on still nights. I mend with my back to the wall.") } },
		{ TEXT("Deckhand"), { TEXT("The ferry runs to Lantern Ward and back. Same water, different prices."),
			TEXT("It is a short crossing, and from the middle you see every lantern in town.") } },
		{ TEXT("Tally clerk"), { TEXT("Every crate is counted twice here. Once for the ledger, once for the truth."),
			TEXT("The Fence buys what the Counting House won't. You did not hear it from me.") } },
		{ TEXT("Fishwife"), { TEXT("Fresh at dawn, cheap by dusk. Same fish, different courage."),
			TEXT("When the docks go quiet the curses get loud. Buy your salve before dark.") } },
	};

	const TCHAR* MoodLines[4][3] = {
		{ TEXT("A quiet night. The grass is only grass, mostly."),
		  TEXT("The street feels light tonight. Enjoy it while it lasts."),
		  TEXT("No shouting, no weeping. I almost don't trust it.") },
		{ TEXT("Tempers are short tonight. Somebody was cheated and everybody knows it."),
		  TEXT("You can feel the anger in the cobbles. The curses will come out swinging."),
		  TEXT("Two stalls came to blows at dusk. Nights like this feed the Grudge.") },
		{ TEXT("Too many shutters are down. The street is grieving."),
		  TEXT("People are counting what they lost. Sorrow like this pools in the low places."),
		  TEXT("Hear how quiet it is? That is not peace. That is mourning.") },
		{ TEXT("Everyone is hurrying home. Something is owed and nobody knows who pays."),
		  TEXT("I keep looking over my shoulder tonight. So does everyone else."),
		  TEXT("The lanterns burn, but the dark feels closer. Stay out of the tall grass.") },
	};
	const TCHAR* ChapterLines[3][3] = {
		{ TEXT("New here, aren't you? Bind a curse or two and people will learn your face."),
		  TEXT("Kessa gave you that talisman? Then the tall grass is your work now."),
		  TEXT("An exorcist with a debt notice. You will fit right in.") },
		{ TEXT("A shop went under. They say something clawed its way out through the shutters."),
		  TEXT("Debts do not die with the shop. They grow teeth."),
		  TEXT("The Broker has been uneasy since the bankruptcy. Go and look at the prices.") },
		{ TEXT("A Calamity, they are saying. I have lived here all my life and never wanted to see one."),
		  TEXT("If you can fight, fight. If you can write a rule, write it. The town needs an answer."),
		  TEXT("Swear a vow at the shrine if you mean to stand. The Rite remembers.") },
	};

	const FVoice& Voice(const FString& WardId, int32 Index)
	{
		const int32 Safe = FMath::Abs(Index);
		if (WardId == TEXT("tide")) return Tide[Safe % UE_ARRAY_COUNT(Tide)];
		return Lantern[Safe % UE_ARRAY_COUNT(Lantern)];
	}
}

GbTalk::EMood GbTalk::Mood(const FGbWardSummary& Ward)
{
	// Out of 100 across the three; below a third the street is at ease.
	if (Ward.Total < 30.f) return EMood::Calm;
	const FGbGrudge& G = Ward.Grudge;
	if (G.Rage >= G.Sorrow && G.Rage >= G.Dread) return EMood::Rage;
	return G.Sorrow >= G.Dread ? EMood::Sorrow : EMood::Dread;
}

FString GbTalk::Role(const FString& WardId, int32 Index) { return Voice(WardId, Index).Role; }

FString GbTalk::Line(const FString& WardId, int32 Index, EMood Mood, int32 Chapter, int32 Turn, const TArray<FString>& Gossip)
{
	const int32 Safe = FMath::Max(0, Turn);
	const int32 Kinds = Gossip.Num() > 0 ? 4 : 3;
	const int32 Round = Safe / Kinds;
	switch (Safe % Kinds)
	{
	case 3: return Gossip[(FMath::Abs(Index) + Round) % Gossip.Num()];
	case 0: return Voice(WardId, Index).Lines[Round % 2];
	case 1: return MoodLines[static_cast<int32>(Mood)][(FMath::Abs(Index) + Round) % 3];
	default: return ChapterLines[FMath::Clamp(Chapter, 1, 3) - 1][(FMath::Abs(Index) + Round) % 3];
	}
}

float GbTalk::ReadingTime(const FString& Line) { return FMath::Clamp(2.5f + Line.Len() * 0.055f, 4.f, 9.f); }

// ================================================================ the speech panel

void UGbTalkWidget::BuildTree()
{
	using namespace GbGreybox;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
	UBorder* Tag = MakePanel(Color::Crimson, FMargin(14.f, 4.f));
	Speaker = MakeText(TEXT(""), 16, GbUi::PaperHi(), true); Speaker->SetAutoWrapText(false);
	Tag->SetContent(Speaker);
	if (UVerticalBoxSlot* TagSlot = Stack->AddChildToVerticalBox(Tag)) { TagSlot->SetHorizontalAlignment(HAlign_Left); TagSlot->SetPadding(FMargin(18.f, 0.f, 0.f, -4.f)); }

	UBorder* Paper = MakePanel(Color::Paper, FMargin(22.f, 14.f));
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Line = MakeText(TEXT(""), 19, Color::Ink);
	Col->AddChildToVerticalBox(Line);
	Hint = MakeText(TEXT(""), 13, GbUi::InkSoft(), true); Hint->SetAutoWrapText(false); Hint->SetJustification(ETextJustify::Right);
	if (UVerticalBoxSlot* HintSlot = Col->AddChildToVerticalBox(Hint)) HintSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	Paper->SetContent(Sized(Col, 720.f, 0.f));
	Stack->AddChildToVerticalBox(Paper);

	// Above Kessa's banner, which keeps the bottom edge.
	UCanvasPanelSlot* Place = Root->AddChildToCanvas(Stack);
	Place->SetAnchors(FAnchors(0.5f, 1.f));
	Place->SetAlignment(FVector2D(0.5f, 1.f));
	Place->SetPosition(FVector2D(0.f, -92.f));
	Place->SetAutoSize(true);
}

void UGbTalkWidget::Say(const FString& InSpeaker, const FString& InLine)
{
	if (!Line) return;
	Speaker->SetText(FText::FromString(InSpeaker));
	Line->SetText(FText::FromString(InLine));
	Hint->SetText(FText::FromString(UGbUserSettings::Get()->KeyLabel(TEXT("Interact")) + TEXT("  to hear more")));
}

// ================================================================ who the player is next to

bool UGbTalkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGbTalkSubsystem::QuestReply(const FString& Message)
{
 if (!bAwaitingQuest) return;
 bAwaitingQuest=false;
 if (Panel && Speaking.IsValid()) {Panel->Say(GbTalk::Role(Speaking->GetWardId(),Speaking->GetVoice()),Message); Remaining=GbTalk::ReadingTime(Message); APlayerController* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr; Speaking->Hold(Remaining+.5f,PC?PC->GetPawn():nullptr);}
}
void UGbTalkSubsystem::QuestError(const FString& What,int32,const FString& Message)
{
 if (What==TEXT("exploration/interact") || What==TEXT("satchel")) QuestReply(Message);
}
void UGbTalkSubsystem::Close()
{
	if (AGbTownsfolk* Walker = Speaking.Get()) Walker->Release();
	Speaking.Reset();
	Remaining = 0.f; bAwaitingQuest=false;
	if (Panel) Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UGbTalkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Director.IsValid() && World) for (TActorIterator<AGbUiDirector> It(World); It; ++It) { Director = *It; break; }
	const bool bWalking = Pawn && Director.IsValid() && Director->IsWalking();

	AGbTownsfolk* Nearest = nullptr;
	if (bWalking)
	{
		float Best = FMath::Square(Reach);
		for (TActorIterator<AGbTownsfolk> It(World); It; ++It)
		{
			const FVector To = It->GetActorLocation() - Pawn->GetActorLocation();
			if (FMath::Abs(To.Z) < 220.f && To.SizeSquared2D() < Best) { Best = To.SizeSquared2D(); Nearest = *It; }
		}
	}
	if (Nearest != Focus.Get())
	{
		if (AGbTownsfolk* Old = Focus.Get()) Old->ShowPrompt(false);
		Focus = Nearest;
		if (Nearest) Nearest->ShowPrompt(true);
	}

	if (Speaking.IsValid() || Remaining > 0.f)
	{
		Remaining -= DeltaTime;
		const AGbTownsfolk* Walker = Speaking.Get();
		if (!bWalking || !Walker || Remaining <= 0.f || FVector::DistSquared2D(Walker->GetActorLocation(), Pawn->GetActorLocation()) > FMath::Square(Reach + 130.f)) Close();
	}

	if (!Nearest || !UGbUserSettings::Get()->WasPressed(PC, TEXT("Interact"))) return;
	// A door underfoot takes the key first.
	for (TActorIterator<AGbBuildingDoor> It(World); It; ++It)
		if (!It->IsClaimed() && FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FMath::Square(150.f)) return;

	GbTalk::EMood Mood = GbTalk::EMood::Calm;
	int32 Chapter = 1;
	if (const UGameInstance* Game = World->GetGameInstance())
	{
		if (const UGbApiSubsystem* Api = Game->GetSubsystem<UGbApiSubsystem>())
			for (const FGbWardSummary& Ward : Api->GetWorldState().Wards) if (Ward.Id == Nearest->GetWardId()) { Mood = GbTalk::Mood(Ward); break; }
		if (const UGbStorySubsystem* Story = Game->GetSubsystem<UGbStorySubsystem>()) Chapter = Story->GetChapter();
	}
	TArray<FString> Gossip;
	if (const UGbPlayerSubsystem* Player = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UGbPlayerSubsystem>() : nullptr) Gossip = Player->GetSatchel().Gossip;
	FString Line = GbTalk::Line(Nearest->GetWardId(), Nearest->GetVoice(), Mood, Chapter, Nearest->NextTurn(), Gossip);
 FString Quest;
 if (Nearest->GetWardId()==TEXT("lantern") && Nearest->GetVoice()==0) Quest=TEXT("lamplighter");
 if (Nearest->GetWardId()==TEXT("lantern") && Nearest->GetVoice()==3) Quest=TEXT("receipt-clerk");
 if (Nearest->GetWardId()==TEXT("tide") && Nearest->GetVoice()==1) Quest=TEXT("net-mender");
 UGbApiSubsystem* QuestApi=World->GetGameInstance()->GetSubsystem<UGbApiSubsystem>();
 if (QuestApi && !Quest.IsEmpty()) { if (QuestApi->IsSatchelBusy()) return; Line=TEXT("Let me check... "); }
	if (!Panel)
	{
		Panel = CreateWidget<UGbTalkWidget>(PC, UGbTalkWidget::StaticClass());
		if (Panel) Panel->AddToViewport(5);
	}
	if (!Panel) return;
	if (AGbTownsfolk* Old = Speaking.Get()) if (Old != Nearest) Old->Release();
	Panel->Say(GbTalk::Role(Nearest->GetWardId(), Nearest->GetVoice()), Line);
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	Speaking = Nearest;
	Remaining = GbTalk::ReadingTime(Line);
	Nearest->Hold(Remaining + 0.5f, Pawn);
 if (QuestApi && !Quest.IsEmpty())
 {
  bAwaitingQuest=true; Remaining=30.f; Nearest->Hold(30.f,Pawn);
  QuestApi->OnSatchelAction.AddUniqueDynamic(this,&UGbTalkSubsystem::QuestReply);
  QuestApi->OnError.AddUniqueDynamic(this,&UGbTalkSubsystem::QuestError);
  QuestApi->Explore(Quest);
 }
	UGbAudio::Play(World, TEXT("talk"), 0.7f, 0.9f + 0.05f * (Nearest->GetVoice() % 5));
}
