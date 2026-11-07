# Hollowmere · playable story draft

P4-U1/U2/U3. Draft for the lead's review; these lines are implemented locally in Unreal. No publication or
submission is implied. The delivery is the Unreal Windows game, with the existing game backend.

## Premise

You arrive in Hollowmere with a debt notice and no trade. Kessa, a paper-lantern referee spirit, gives you a
starter talisman and a way to earn your place: bind the curses that grow from the town's greed. Binding can
pay your way, but the economy that feeds you also feeds the Grudge. The Rite lets people write rules; it
cannot choose what kind of town they want.

The debt is narrative context in this milestone. It is not a new balance, fee, countdown or payment system.

## Arrival · four paper panels

1. **A town that keeps receipts.** “You reach Hollowmere with a debt notice folded inside your coat.
   Beyond the harbour wall, a thousand lanterns burn over a market that never quite closes.”
2. **What the market leaves behind.** “A stolen wage. A rigged bargain. A shop shuttered for the last time.
   Rage, Sorrow and Dread gather into Grudge. And Grudge gives the town's discarded things teeth.”
3. **Kessa's offer.** Kessa: “Keep the notice. You may want to remember what brought you here. Take this
   talisman instead. Bind what haunts our streets, earn your place, and mind what your bargains leave behind.”
4. **A place in the ledger.** Kessa: “The Rite remembers who holds a talisman and what the town has sworn.
   Earn points, write a rule, or stake them on a promise. The lanterns are lit. What happens next is partly yours.”

Each panel has an original procedural ink illustration drawn by Unreal's native UI. **Next** advances,
**Skip intro / Escape** enters the Ward. **I** replays the intro from the Ward without resetting progress.
Intro completion is local to the signed-in player and game-server address.

## Faces, wants and voices

| Character | Face / silhouette | Want | First line |
|---|---|---|---|
| Kessa | Warm paper lantern, a referee's red seal, a torn receipt for a sleeve | A town that can live with its own bargains | “A rule is a promise made loud enough for everyone to hear.” |
| Old Mother Fen | Hunched, a sorrow-blue shawl, a big basket of hoarded talismans | Enough stock that she never has to ask anyone for help; buys cheap and loves taxing trades she doesn't make | “Cheap today. Necessary tomorrow. You see the difference?” |
| Brother Vane | Stern, monk-like exorcist in a white robe, prayer beads wound round one fist | Order: hunt the strongest curses and ban PvP, even when it costs other people freedom | “Drive the curse out. Then ask who fed it.” |
| Silk | A violet coat, an easy smile, a ledger under one arm hiding a ragged contract | A profitable exception in every rule | “I only charged what the town agreed I could.” |
| Rook & Ash | Two mercenaries in rage-red sashes, one tall with a staff, one short with knuckle wraps, sharing one ledger | A contract that pays enough to stop taking contracts | “We don't need a good town. We need the next job to pay.” |

These are script/visual briefs; this milestone does not add NPC character models or dialogue interactions.
Silhouettes follow `ART_PROMPTS.md` and wants follow the agent personalities in `agents/src/runtime.ts`, so
the art, the AI agents' behaviour and the script describe the same people. Pell (the gambler agent who invests in
bankrupt shops) also appears in the town feed but has no brief yet.

## Playable chapters

| Chapter | Trigger | Objective / consequence |
|---|---|---|
| 1 · Arrival | First sign-in; no first binding recorded for this story profile | Bind a curse in Lantern Ward. After a successful seal, open the Broker (B) to learn what it is worth. |
| 2 · Debts Come Due | First binding plus a real bankruptcy event, or a merchant currently closed in the world snapshot | A merchant goes under and The Shuttered claws out of the shop. Kessa connects it to the Grudge. Visiting the Broker acknowledges this beat. No day number creates a bankruptcy. |
| 3 · The Calamity | First binding, the debt beat acknowledged, and an actual Calamity event | Respond through a completed fight, an accepted rule, or a newly sworn vow. A successful ledger receipt confirms an online action. |

The objective card stays in the HUD. World events may arrive before the first bind; they are remembered and
revealed in chapter order. A declined rule, a released vow, a failed ledger write, fleeing or losing does not
complete the Calamity response. Responding does not claim the Calamity was defeated: the world continues.
A Broker visit before the bankruptcy does not acknowledge that later event: the objective asks the player
to return after the shop fails before Chapter 3 can be revealed.

Default demo seed 2 produces a bankruptcy on day 3 (the Fence, in Tide Ward, not the Broker; hence the
chapter's name is about debt in general, not the Broker's) and a Calamity on day 4 in the same Ward. The chapter
reacts to those facts, including events in another Ward; it does not send the player through unfinished/locked district gates.

## Save and world boundaries

Story flags live in a separate Unreal SaveGame slot per account and server URL. Existing points, talismans
and saves are untouched. Signing out clears the active story; another identity loads its own flags. Seen
story beats persist as history when the world server restarts. The town event cursor resets if its clock
rolls back, so new events from the restarted server can be observed.

## Native verification

Verified 7 Nov: the Unreal Development Editor target compiles and **5/5 native checks pass** (zero failed
or skipped). All four generated intro panels were visually inspected; the opening panel's first-frame
text overlap was corrected before the final run. The `Grudgebound.Story` automation suite covers chapter
ordering, typed world facts, per-account/server save keys and in-memory serialization, confirmed own actions,
and rendering/navigation of all four intro panels. Tests serialize temporary objects in memory and do not
write to any player's real SaveGame slots. The visual check also saves native PNG previews under
`unreal/GrudgeBound/Saved/StoryPreview/Intro-1.png` through `Intro-4.png` (git-ignored).

After compiling, run from the repository root in PowerShell:

```powershell
$storyProject = (Resolve-Path 'unreal/GrudgeBound/GrudgeBound.uproject').Path
$storyReport = Join-Path (Split-Path $storyProject) 'Saved/Automation/Story'
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' $storyProject -unattended -nop4 -nosplash -nosound -RenderOffscreen '-ExecCmds=Automation RunTests Grudgebound.Story' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$storyReport"
```

The report is `Saved/Automation/Story/index.json`. The full Play-mode acceptance and lead's script review
below remain separate from the automated checks.

## Acceptance

- [ ] First sign-in shows all four panels; Next, Enter/Space and Skip/Escape work; walking stays blocked until closing.
- [ ] Returning to Play resumes the profile without repeating the intro; I replays it without changing story flags.
- [ ] A successful bind advances the Arrival objective; a failed mint does not.
- [ ] Opening the Broker acknowledges its objective without spending Ember.
- [ ] Real bankruptcy and Calamity events reveal Chapters 2 and 3 in order; elapsed days alone do not.
- [ ] A successful battle/rule/vow response completes the chapter objective; unrelated/rejected/failed actions do not.
- [ ] Two identities keep separate intro/progress flags; a server clock reset allows fresh town events.
- [ ] Review the script with the lead and inspect the layout in Unreal before treating P4-U1/U2/U3 as accepted.
