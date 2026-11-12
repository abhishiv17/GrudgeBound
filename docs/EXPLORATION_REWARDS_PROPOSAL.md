# Exploration, rewards and secrets — proposed first adventure

Status: proposal for the owner's review, 12 November 2026. The owner chose useful supplies, stronger
equipment, and secrets that unlock places and stories. The adventure below, item names, reward numbers,
story reveal and implementation breakdown are proposed details, not approved canon or completed features.
This document does not replace BUILD_PLAN.md or authorize a deployment. A first implementation of the whole
chain was built on 12 November (server + Unreal, section 9); its names, numbers, NPC lines and the storehouse
record are still drafts for the owner's review, and the hosted server does not run it until the owner deploys.

The intended experience is: notice something, investigate, help a person, earn something useful, and use
what you learned to reach a place you could see but could not enter. The payoff must be visible in play.

## 1. First playable scope

Build one connected adventure across the existing Lantern and Tide Wards, approximately 15–20 minutes
on a first visit. That duration is a play-test target, not a promise. Include:

- Three existing NPC roles with tasks and remembered completion states: Lamplighter, Net mender, Receipt clerk.
- Six hand-placed discoveries with different purposes: supplies, clues, equipment or a visible route unlock.
- One small secret interior behind an initially locked door in Tide, using the existing building system.
- Three equippable charms, one active at a time, with explicit battle-stat effects.
- A Gear pocket and a short task journal in the existing B-key Satchel.
- Persistent progress, rewards, equipment ownership and personal door access.

Keep the current main story's real bankruptcy and Calamity triggers intact. This side adventure can be
completed without waiting for either world event. It must not pretend that either event occurred.

## 2. The connected adventure

Working title: **The Sealed Storehouse**. All names and story details below are drafts.

| Beat | Player action | Reward and visible consequence |
|---|---|---|
| First discovery | Spot a loose parcel on a side alley route in Lantern. A nearby NPC or environmental clue can lead a player who misses it. | Once per player: 2 blank talismans and 1 salve. Open B and see the new counts. This parcel is separate from the existing starter kit. |
| Lamplighter's task | Hear about a missing lamp kit, inspect the indicated service corner, and return the kit. Finding it before accepting the task also counts. | Worn Focus charm; the kit appears at the NPC's work spot, the NPC acknowledges the help, and a marked delivery slip points toward Tide. |
| A locked promise | On reaching Tide, see a chained storehouse door with the same mark as the delivery slip. | A concrete reason to investigate the docks. Inspecting the door records the clue rather than displaying an unexplained failure. |
| Net mender's task | Recover a marked bundle near the reeds and finish one Tide encounter after accepting the task. Winning or binding counts; fleeing or losing does not. Return to the NPC. | Ward Clasp charm and a brass storehouse key. The recovered bundle appears beside the NPC. The encounter uses the existing curse system. |
| Receipt clerk's connection | Bring the delivery slip and key back to the Receipt clerk. The clerk identifies their common mark and records the storehouse lead. | The journal explains the destination. No currency or extra token is required to proceed. |
| Secret-room payoff | Return to the previously locked door, open it, and explore the small storehouse interior. | Etched Focus charm, 1 gilded talisman, and a story record connected to the player's debt notice. The doorway remains accessible for this player. |

The storehouse reveal needs an owner-approved narrative answer before implementation: what does its
record establish about the debt notice, and what question does it leave the player with? Do not invent
final dialogue or imply the existing debt is a payable balance. The record should give one concrete
answer and one next lead; an empty room with a generic lore paragraph is not an adequate payoff.

No task spends a unique key or destroys a needed clue. The Tide encounter accepts a win so running out
of binding supplies cannot block progress. Existing free rest remains the recovery path for low ink.

## 3. Six discoveries with distinct purposes

| Discovery | Purpose | Proposed contents / consequence |
|---|---|---|
| Loose alley parcel | Teach that checking the environment pays off | 2 blanks + 1 salve; once per player |
| Missing lamp kit | Give the first NPC request an observable object | Quest object, retained as completion history after return |
| Marked delivery slip | Connect two places | Clue description and journal entry; discoverable alongside the kit |
| Reed-edge bundle | Put exploration next to an existing encounter area | Quest object; does not start an unavoidable scripted fight |
| Dockside supply tin | Reward optional curiosity away from the direct route | 1 salve; once per player |
| Storehouse chest and record | Pay off the full chain | Etched Focus + 1 gilded talisman + story record; one atomic reward claim |

Use restrained visual tells: unusual wrapping, a recurring stamped symbol, a loose board or a light
visible through a gap. Important clues get an NPC hint and a journal hint as well. Interaction prompts
appear nearby; keyboard and click should reach the same action. Optional finds stay optional.
Collected objects need an opened/empty appearance and a useful inspection response on later visits.

## 4. Equipment that changes play

The exorcist equips one charm that supports the selected carried partner. A charm changes that partner's
effective battle stats; it does not change the species definition or the minted talisman's metadata.
Provisional effects for balance tests:

| Charm | Source | Proposed effect | Choice it creates |
|---|---|---|---|
| Worn Focus | Lamplighter | +1 Attack | First improvement over an empty slot |
| Ward Clasp | Net mender | +2 Guard | Defensive alternative when ink is scarce |
| Etched Focus | Secret storehouse | +2 Attack | Stronger offensive replacement for Worn Focus |

Show base stat, item contribution and resulting stat in Gear. These bonuses affect attack damage and
incoming damage through the existing battle formula. They do not directly raise seal chance, guarantee
captures, grant currency or bypass vows/curfews. Numbers remain provisional until tested against the
curse roster. The defensive choice should remain useful after finding the stronger offensive charm.

Equip changes are allowed only outside an active battle and while inventory mutations are safe.
Battle start freezes the equipment contribution for that fight. Changing partners retains the equipped
charm but recomputes its displayed contribution. This first scope needs no durability, crafting or gear
upgrade currency. Proposed gear is personal quest equipment; any trading/minting decision is separate.

## 5. The Satchel and presentation

Keep B as the bag key and extend the existing paper interface:

- Character strip: existing exorcist portrait, selected partner and equipped charm.
- Talismans: preserve carried five, Case, partner selection and ink behavior.
- Supplies: preserve blanks, gilded talismans and salves; show newly received quantities clearly.
- Gear: owned charms, stat comparison, Equip / Equipped, where the item was found.
- Keepsakes: readable clue and key descriptions; distinguish quest objects from usable supplies.
- Journal: current NPC requests, one tracked task, discovered leads and completed entries.
- Ledger: preserve its existing balances and debt notice.

Use enough space or a compact tab arrangement to keep all pockets usable at 1280x720. A tracked side
task must coexist with the main-story objective without obscuring both. Give vague exploration hints
first and allow a more explicit hint on request. Do not make the player memorize an NPC conversation.

Reward feedback should name the item and quantity, play one short sound, and offer an obvious route
to its Satchel pocket. An equipment reward should show what equipping it changes. Rewards appear only
after the server confirms the grant. Loading, failure and retry need honest feedback.

The important moments to polish are the first useful find, the first equipment comparison, an NPC
recognising the player's help, and the familiar locked door opening. Use existing sound/art where suitable;
new icons or room art must be listed as required assets before their implementation task starts.

## 6. Persistence and backend work needed for this Unreal feature

Current foundation: Satchels are server-owned with supplies, ink and a keepsakes array. Native main-story
flags are local narrative history and explicitly cannot grant gameplay rewards. Battles are server-run
and replayed from an archived starting state. Talkable NPCs presently cycle contextual lines.

New server work is necessary because NPC rewards and equipment change inventory and battle power:

1. Define stable quest, discovery, item and reward IDs. Every request uses the authenticated account;
   the client supplies an interaction ID, never its own reward amounts, stats or completion verdict.
2. Keep reward receipt, inventory additions, quest completion and unlock flags in one atomic per-account
   mutation with the Satchel state. Retrying a response lost in transit must return the same result.
   Avoid a separate quest file that can commit independently of its reward.
3. Preserve existing account data through a versioned migration. Scope progress to account/server,
   retain the existing starter-kit history, and check isolation on sign-out/profile switch.
4. Advance the encounter objective from accepted server battle outcomes in Tide after task acceptance.
   Do not infer success from a client message or from an unsuccessful chain receipt where settlement matters.
5. Persist ownership and equipped item ID. Derive bonuses from server definitions, snapshot them at
   battle start, and include the versioned effective setup in battle archives/proof hashing. Existing
   records without equipment must replay exactly as before.
6. Restore visible personal changes from confirmed progress on login: opened finds, returned props,
   NPC completion state, equipped charm and unlocked storehouse entry.

Known boundary: the existing HTTP server has no authoritative movement simulation. It can validate
quest order, IDs, ownership and battle evidence, but cannot prove the player physically stood beside
a discovery. Unreal checks proximity for normal play. Do not claim this prevents a modified client
from requesting a known discovery directly; stronger movement validation is separate scope.

Use the existing store and hosting model. No custody, contract, deployment or economic-policy change
is implied by this proposal. Actual reward amounts and personal-gear policy remain part of owner review.

## 7. Implementation order after design agreement

| Task | Bounded delivery | Completion evidence |
|---|---|---|
| E1 — progress and rewards | Definitions, versioned state, authenticated interactions, atomic grant receipts | Retry, concurrent requests, restart, migration and cross-account tests; no duplicate rewards |
| E2 — first playable task | Lamplighter request, pickup, return, remembered result, one supply discovery | In actual Unreal play: accept/find/return, reconnect, inspect claimed item, see correct Satchel contents |
| E3 — real equipment | Owned/equipped charms, battle modifiers, frozen setup and replay compatibility | Base vs equipped battle fixtures, old/new replay tests, equipment exploit rejection, native JSON checks |
| E4 — bag and journal | Gear/keepsake/task UI, comparisons, tracking and reward feedback | Mouse/keyboard/rebind checks, 1080p/720p previews, network retry/failure and profile-switch checks |
| E5 — connected secret | Remaining NPC steps, Tide encounter evidence, six finds and one secret room | Complete adventure from fresh profile; find-before-accept and reordered exploration; key/door persistence |
| E6 — finish and capture | Sound/visual timing, NPC/world state reactions, packaging and video evidence | Editor + full relevant tests, packaged Windows play-through, actual interaction capture |

E2 should prove the whole talk -> task -> return -> reward loop before expanding content. E3 establishes
that gear genuinely changes combat before presenting all three items as working progression.
No launch date is promised until the first loop's integration cost is known.

Before treating the expanded build as releasable, recheck the recorded 12 Nov baseline gaps (the
remoteFiles upload/retry test and the talk/menu input gating) rather than assuming either is resolved.

Never deploy or publish a release as part of an inferred handoff.

## 8. Acceptance and recording

- A first-time player finds or is led to a useful discovery early, without already knowing its location.
- Each NPC task explains what to do, supplies a recoverable hint, and recognises completion.
- Find-before-accept works; leaving a conversation or losing a battle cannot lose quest objects.
- Rewards survive restart and cannot be collected twice through reconnects, repeat clicks or retries.
- Equip/unequip changes the stated stats; its effect appears in an actual server-backed fight.
- The secret door is visible before access, gives a meaningful clue, opens with earned progress and
  remains open for that player after reconnect. Other profiles retain their own locked state.
- The secret room contains a usable reward and the agreed story payoff.
- Existing main-story ordering, inventories, free rest, vows, economy and historical battle replays pass regression checks.
- Owner reviews narrative text, reward balance and final feel in the packaged Windows build.

Proposed short video sequence: notice parcel -> receive supplies -> accept an NPC task -> return item ->
equip charm and show its effect -> cross to Tide -> revisit locked door -> enter secret room -> read the
story record. A montage may shorten travel and fights; every shown reward/unlock must come from actual
completed gameplay. The earlier screenshot fixtures are not evidence of this proposed adventure.

Decisions still open for the owner: approve/revise this first adventure, its story reveal, charm model
and starting reward quantities. Final dialogue remains owner-reviewed writing. Once those are settled,
turn E1–E6 into claimed implementation tasks; do not mark this proposal approved automatically.

## 9. What was built (12 November)

- Server: exploration progress inside the Satchel store (version 2, migrated from version 1), one atomic
  mutation per interaction, `/exploration/interact` and `/satchel/equip`; Tide objective from settled
  battle results; equipment snapshotted into the battle's starting state and its replay proof.
- Unreal: five outdoor finds and the storehouse chest, the marked door that opens once the clerk's lead is
  recorded, the storehouse room, NPC task replies through street talk (Lamplighter, Net mender, Receipt
  clerk), Gear and Journal pockets in the Satchel, and the tracked task under the HUD objective.
- Checks: server suite and typecheck; Unreal automation 36/36; Gear and Journal previews at 1080p and 720p;
  in-game shots of each find and the storehouse room. Not yet done: a full play-through against a server
  running this code, and the owner's review of the writing and reward numbers.
