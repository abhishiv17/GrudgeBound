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

## Draft: harbour arrival and first partner (12 Nov)

Implementation draft for owner review; the names, dialogue and personalities below are not final writing.
After the four opening panels, Kessa stands beside the player on the harbour pier. The player advances
five short spoken pages explaining how market harms leave Rage, Sorrow and Dread; how Grudge wakes curses;
how weakened curses can be bound with blank talismans; how the Rite changes town rules; and how independent
merchants make bargains with their own money. She introduces the Satchel and Journal before asking who arrived.

The player chooses a validated name, a masculine or feminine presentation, he/she/they, and one existing
whisper species: Cinder Grin (Rage, impatient and bright), Weepwick (Sorrow, quiet and stubborn), or Hushmoth
(Dread, watchful and listening). Displayed stats come from the server's species definitions. These choices
are stored per account. Existing players keep their partner and skip only the partner page. Later/Escape
returns to town; Kessa remains available on the pier. New arrivals finish this choice before starting combat.

The choice is permanent and identical retries return the same partner. The items contract has a tested,
minter-only `mint_starter` method, with a permanent recipient record surviving transfers. It has **not been
deployed**. `STARTER_MINT=off` keeps the chosen partner virtual on current deployments. Enabling minting
requires the owner-approved contract upgrade and server configuration; pending replies retry the same
choice. A previously selected virtual partner can then settle without healing or a second welcome kit.

Wider NPC pronoun/deed reactions and guidance are later polish items. All text still needs owner review and
an interactive play-through.

## Draft: the rival and partner levels (12 Nov)

Implementation draft for owner review. The name, lines and numbers are not final.

**Dace Marrow** arrived on the same boat and got his first partner from Kessa too. He wants to know who in
Hollowmere kept the original of the debt notices, and he thinks he will get there first. He fights three
times. Each fight is a **mirror match**: his partner is the same species as yours, so the result depends on
level, Charms and play rather than on the type triangle. An earlier counter-pick version won 83-100% of
simulated fights and was dropped.

| Stage | Where he stands | Opens when | His level | Result |
|---|---|---|---|---|
| 1 | Harbour pier, beside Kessa | Arrival is finished | 1 | Fought once, win or lose. A win gives 10 Rite points |
| 2 | By the Tide ferry | Stage 1 is done | 4 | Must be won. **The ferry to Tide opens only after this.** A win gives 20 points |
| 3 | Tide quay, where the pier meets the wharf | Stage 2 is won and the storehouse chest is opened | 7 | A win gives 30 points |

After stage 3 he waits in Lantern plaza. A rival fight has no sealing and no running. The pier fight counts
as lost from the moment it starts until it is won. If the player walks away, lets it expire or the server
restarts mid-fight, it is a loss rather than a free retry. His line before a
fight depends on the stage and on whether you lost on the pier. His line after a fight depends on the
result and on how close it was.

Partners have **levels 1-10**:

- Experience comes only from fights you win or binds you complete. Fleeing and losing give nothing.
- A wild win gives 10 + 3 × the foe's level. A rival win gives 25 + 15 × the stage.
- Each level adds ink (hp) and stats by emotion. Rage gains the most attack, Sorrow the most ink and guard, Dread the most speed.
- Wild curses come at the partner's level or one below, inside each Ward's range: Lantern 1-4, Tide 3-7, Ash 4-8, Bone 5-9.
- The level is fixed when a battle starts and is stored in the battle's replay record.

Players who already reached Tide before the rival existed keep the ferry.

**Townsfolk remember** (draft lines, 12 Nov):

- **What the quest NPCs say depends on how and when you did their task.**
  - The Lamplighter knows whether you brought the kit before being asked.
  - The Net mender knows whether you bound the Tide curse or only beat it.
  - Each of them changes their line again after later deeds, such as beating Dace at the ferry or opening the storehouse.
- **Street walkers work your own deeds into their talk.** One line in four is gossip about you, using your chosen name and pronouns. The server writes it from facts recorded for your account only.

**Optional finds** (draft, 12 Nov): three more rewards for players who look around. None of them is needed for a quest.
- A notice on Lantern's shut east gate: a keepsake and 1 blank talisman.
- A dropped purse in the south-east tall grass: 1 blank talisman and 1 salve.
- A bottle in Tide's west reeds: a keepsake hinting at the Rite Hall, and 1 salve.

**Hollowmere lore** (draft): six short Journal pages open as the player learns the town:
- Hollowmere
- Grudge
- Talismans and the Rite (the first quest)
- Dace Marrow (the pier fight)
- Tide Ward (the ferry)
- The original record (the storehouse)

They let the world explain itself in play.

On-chain, every won fight or bind with a minted talisman partner (`tal-…`) queues the deployed
`items.record_win`, which raises that talisman's `wins` and `level` in its NEP-245 metadata. Set
`RECORD_WINS=off` to keep this off-chain.

## Acceptance

- [ ] First sign-in shows all four panels; Next, Enter/Space and Skip/Escape work; walking stays blocked until closing.
- [ ] Returning to Play resumes the profile without repeating the intro; I replays it without changing story flags.
- [ ] A successful bind advances the Arrival objective; a failed mint does not.
- [ ] Opening the Broker acknowledges its objective without spending Ember.
- [ ] Real bankruptcy and Calamity events reveal Chapters 2 and 3 in order; elapsed days alone do not.
- [ ] A successful battle/rule/vow response completes the chapter objective; unrelated/rejected/failed actions do not.
- [ ] Two identities keep separate intro/progress flags; a server clock reset allows fresh town events.
- [ ] Review the script with the lead and inspect the layout in Unreal before treating P4-U1/U2/U3 as accepted.
