# The Ninth Bell · a co-op story level

New level direction from the lead: 2–4 players, shared exploration, NPC agendas, suspense and a
coordinated climax. This is a new Unreal mission beside Lantern Ward (build plan task **P4-U13, Must**).

**Where:** the Bell Quarter, a corner of **Bone Ward** (temples and archives) that was sealed nine years ago.
The rest of Bone Ward still runs in the live simulation; only the Bell Quarter fell silent. This is why the
Bone gate in Lantern Ward stays locked: Kessa will not say why, and now the players find out.

## The hook

At midnight, a bell rings from the Bell Quarter, silent for nine years. The canals turn black and
the exits fold back into the same street. Mira, a young courier, asks the party to deliver a sealed notice.
Every player can read the recipient's name. Mira cannot.

It is hers. The notice says she disappeared tomorrow.

Kessa hears the bell and, for the first time, stops speaking halfway through a sentence.

## Tone and visual direction

An original supernatural ensemble mystery with anime-like emotional turns, quiet pauses before spectacle,
strong character silhouettes and a costly final choice. No borrowed characters, powers or plotlines.
Hollowmere's paper, ink, coins, seals and debt remain the source of its monsters. Rain hangs in the air;
ink-dark canals reflect lanterns that have already gone out. Crimson ritual circles cut through blue mist.

The first implementation uses procedural district geometry and readable proxy characters/boss. Final
character models, animation, rain, voice acting, score and directed cinematics are later art/audio work.

## Cast

| NPC | Public role | Hidden want / truth | Voice |
|---|---|---|---|
| Mira | Courier who knows every canal shortcut | Keep the delivery route her mother taught her. The town is forgetting her because her name was sold to keep the district alive. | Brave until someone asks her surname. “If I finish the route, somebody has to remember I was here.” |
| Eiden | Bellkeeper; apparent saboteur | He broke the outer seals to stop the bell consuming more names. He has kept watch alone, trading his own memories for time. | Abrupt, exhausted. “You think a locked door means something dangerous is outside.” |
| Nara | Keeper of the Counting Archive | Expose the original bargain, including her own signature. She recorded it as a debt because recording it as a death would make it final. | Exact, restrained. “I did not falsify the ledger. I left one line unfinished.” |
| Silk | Merchant offering an easy way out | Recover the contract that bought his survival; he knows its profit came from people, but did not know Mira's name was next. | Deflection that cracks into guilt. “I sold a debt. I never asked what it was tied to.” |
| Kessa | Referee, familiar guide | The bell's missing ninth seal is her original human name. She was once a binder who accepted the same bargain. **Canon** for the main game. | Warmth gives way to fear. “I remember this street. I don't remember why that hurts.” |
| The Unwritten | The bell's Collector | A Dread Calamity made from nine years of names entered as unpaid debts; it enforces the bargain without understanding mercy. Ninefold Shades (Bone Ward's Dread curse, “born from curfews and tolls”) are its lesser tolls. | Short collective fragments. “A place was purchased. A name is due.” |

## Mission structure

| Beat | Play | Story turn |
|---|---|---|
| 1 · The Wrong Recipient | Meet Mira at the canal entrance. Party members share the briefing and objective. | The address on the notice is the party's current location, not the destination. |
| 2 · Three Unfinished Lines | Explore the archive, the market and the old landing. Collect three unique shared clues in any order; talk to Nara and Silk. | A name was sold, the “sabotage” delayed collection, and the ninth seal has Kessa's handwriting. |
| 3 · A Promise Needs Two Voices | Different players hold the two seal anchors together. A solo rehearsal needs one. | The binding works by witnessing each other, so an isolated binder cannot keep the town's bargain. |
| 4 · The Bellkeeper's Truth | Confront Eiden after the ritual. | Eiden is a jailer, not the attacker. The lock was keeping the Collector in. Kessa recognizes the missing seal as her name. |
| 5 · Ninth Toll | Fight The Unwritten in the bell court. Channel separate anchors to expose it, then attack during the short opening. Dodge the announced toll and revive fallen allies. | Each exposed layer is made of residents' names. Breaking the body alone cannot undo their bargain. |
| 6 · Who Gets Remembered? | The party votes after victory. Every connected player gets a say; a deterministic tie rule avoids a deadlock. | Return the names, or break the bell. Both save the party; neither erases the cost. |
| 7 · The Voice After the Bell | Shared ending and an unresolved last line. | A second bell answers from below the canal, and a voice speaks one line that depends on the ending (below). |

### Ending A · Return the names

The party completes the unfinished line. Mira remembers her mother singing her surname; lights return to
the archive windows. Kessa supplies the ninth seal: her name, and every memory of the life it belonged to.
She keeps her voice and goes on refereeing the Rite, but she no longer knows why this street hurts. Eiden
walks her back to the gate. He has lost enough of his own memories to know what she just gave.

Final line, from below the water: “That wasn't the only name you promised.”

### Ending B · Break the bell

The party refuses the bargain and shatters the mechanism. Kessa keeps her voice. Mira survives, but the
notice is blank and she cannot remember the name her mother gave her. The party promises to find it rather
than invent a convenient replacement. Silk hands over the contract he was trying to steal.

Final line, from below the water, in Mira's voice: “I've been waiting for you.”

**Fit with the main game:** in both endings Kessa keeps her voice, so she can still announce the Rite's
verdicts in Lantern Ward. Ending A takes her name and memories; Ending B leaves them buried. Either way,
the main game's Kessa never says her old name.

Neither ending deletes talismans, ownership, balances or real player accounts. This mission's name ledger
is a story object, separate from the Rite's existing ownership ledger.

## Co-op gameplay contract

- One host-authoritative mission state for the party: clues, ritual, boss health and ending replicate to all.
- Shared credit for clues; duplicate interactions cannot advance twice. Late joiners receive current state.
- Interactions use E near an NPC/object; hold E at an anchor or beside a fallen ally. Release E or leave range
  to stop. One player cannot claim both anchors. Required channels adapt to the connected party size.
- WASD moves, left mouse casts a ranged seal toward the cursor. Attack distance, target and cooldown are
  checked by the server. Boss damage is possible only during the shared vulnerability window.
- A visible toll warning gives time to leave the danger radius. Downed players cannot attack or channel;
  an ally can revive them. A party wipe allows a retry without replaying the investigation.
- Keys 1/2 cast the final vote. All connected participants must vote; ties favor returning the names.
  Disconnecting removes that player's channel and vote requirement. Host migration is not in this slice.

The first playable mission is an opt-in LAN/direct-IP prototype, with no wallet rewards and no connection
to the live economy. It does not replace Lantern Ward or enable anonymous callers to earn NEAR assets.
Account-authenticated mission servers, online lobbies, internet hosting, reconnect persistence and reward
settlement require a later integration. Existing deferred Phase 3 work stays deferred.

## Full production expansion

The intended finished level is a 20–30 minute episode, not a claim about the proxy slice's current length.
Add voiced conversations with optional party responses; weather and bell audio; three short Ninefold Shade
fights linked to the clues; unique curse silhouettes; directed reveal shots; a multi-stage animated Collector; readable
revive/channel effects; a safe lobby and ready check; and the ending's canal reveal. Add costs and rewards
only after mission identity, authority and duplicate-settlement rules are integrated with the backend.

## Implementation checkpoint · 7 Nov

Local source implements the replicated mission state, owned interaction/attack/vote RPCs, procedural
district and character proxies, HUD, shared clues, ritual, boss openings/tolls, revives, retry and endings.
The Development Editor target builds. Three native co-op logic checks pass: unique shared clues,
party-size anchor requirements and ending majority/ties. Together with the five story/intro checks,
`Grudgebound` reports **8 passed, zero warnings, failures or skipped tests** in
`unreal/GrudgeBound/Saved/Automation/T1/index.json`.

This verifies compilation and the tested logic, not network play. The Ninth Bell `.umap` has not been
generated; `unreal/tools/make-ninth-bell.py` and `play-ninth-bell.ps1` are unvalidated draft helpers.
Map creation, placement/collision/camera checks, real host/client replication, late join, disconnect,
revive/wipe/retry and both ending play-throughs remain open. Final art/audio and packaged acceptance
also remain open. Lantern Ward is still the existing entry map.

## Multiplayer references

Uses Unreal's [client/server networking model](https://dev.epicgames.com/documentation/unreal-engine/networking-overview-for-unreal-engine)
and [multiplayer test setup](https://dev.epicgames.com/documentation/unreal-engine/testing-and-debugging-networked-games-in-unreal-engine?application_version=5.8).
The controls above describe the source prototype. Working multiplayer run commands require the pending
map and host/client validation; do not treat the draft helpers as a verified playable build.
