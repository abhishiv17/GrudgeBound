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

## Implementation checkpoint · 10 Nov

Local source implements the replicated mission state, owned interaction/attack/vote RPCs, procedural
district and character proxies, HUD, shared clues, ritual, boss openings/tolls, revives, retry and endings.
The Development Editor target builds. Three native co-op logic checks pass: unique shared clues,
party-size anchor requirements and ending majority/ties. Together with the five story/intro checks,
`Grudgebound` reports **8 passed, zero warnings, failures or skipped tests** in
`unreal/GrudgeBound/Saved/Automation/T1/index.json`.

That was the 7 Nov source baseline. T-4 now generates `/Game/Grudgebound/NinthBell` with `GbCoopMode`,
the district, ten objects at the existing `GbBell::Position` coordinates and four starts. Regeneration
updates existing actors, validates counts and checks save success. Lantern Ward remains the default map.
`unreal/tools/play-ninth-bell.ps1 -Host` and `-Join 127.0.0.1` select listen/join; `-Mode` remains compatible.
The Host alias uses a different parameter name to avoid PowerShell's automatic variable.

Verified in real local D3D12 game processes:

- Ending A: both bots logged `GB_COOP_RESULT Complete Ending=1 Party=2`
  (`Saved/Logs/T4-host-B.log`, `T4-client-B.log`, 9 Nov).
- An idle third player held Decision open. Disconnecting it lowered the votes needed; both remaining
  players completed Ending A (`T4-host-C.log`, `T4-client-C.log`).
- A third process joined mid-Search and received the current two-clue snapshot:
  `GB_COOP_CLIENT Phase=1 Clues=3` in `T4-late-E.log` (10 Nov).
- Final two-process native check: authority damage wiped both pawns; both observed replicated Defeat,
  R through the normal input binding returned to Ritual with all clues, and key 2 through the normal
  binding completed Ending B. Each report has **1 passed, zero warnings/failures/skips**:
  `Saved/Automation/T4-network-host-final` and `T4-network-client-final`, `2026.11.10-09.56.57` UTC.
  These are automated input checks; physical manual voting remains an owner play check.
- During the final check both pawns walked from about 704 units from the Collector with 1.3 seconds
  left to about 950 units after the toll, retaining 100 health. The anchor positions, 800-unit radius,
  walking speed and two-second warning are unchanged.

Development-only `-GbCoopBotManualKeys` yields R and 1/2 to normal bindings; default bots still retry/vote 1.
Diagnostics now report manual clients' phase/clues/party changes too. A play-test error with extra local
players exposed BeginPlay preceding ULocalPlayer attachment; HUD/input setup now waits for attachment
and uses that player's screen. Its extra-local-player follow-up remains constrained by the resource gap below.

Two simultaneous 1920x1080 offscreen instances on the RTX 3050, about 41 seconds each:

| Capture | Mean FPS | 99th-percentile frame | Worst frame | Mean GPU |
| --- | ---: | ---: | ---: | ---: |
| Host, 4,941 frames | 120.5 | 18.84 ms | 48.17 ms | 4.22 ms |
| Client, 4,926 frames | 119.8 | 19.85 ms | 129.14 ms | 4.23 ms |

CSVs: `Saved/Profiling/CSV/T4-host-23376.csv` and `T4-client-33264.csv`. GLOBAL/Basic capture replaced an
earlier full-category capture that stalled its profiler. Three host and seven client frames exceeded
33.3 ms; no guarantee of a 30 FPS floor or foreground Play performance is claimed. No gameplay/rendering
tunables changed. The runtime logs confirm D3D12 adapter 0 is the RTX 3050.

Final Development Editor build succeeded. Map regeneration: zero errors/warnings (`T4-map-final.log`).
Full editor `Grudgebound` suite with `-RenderOffscreen`: **24 passed, zero warnings/failures/skips**,
`Saved/Automation/T4-final/index.json`, `2026.11.10-09.58.00` UTC. Game-context process tests run separately.

Remaining acceptance: an external fifth-player connection is unverified. E stopped before launching
its probe when commit headroom fell below 1 GB. After packaging was idle, H's third editor still had
not finished startup within the 90-second orchestration window; its own processes were stopped.
The game-only party helper remains available with sufficient headroom (a 360-second test timeout);
`-GbCoopLimitHookOnly` explicitly does not prove an external connection. No OS/page-file changes made.
Owner phase-end checks: actual LAN/firewall play, fifth-player refusal, extra-local-player HUD,
physical 1/2 voting, manual revive/wipe/R, camera/collision/readability and foreground 1080p performance.
Final art/audio and packaged acceptance remain open. T-13's current package lacks this uncommitted map
and has a separately recorded ServerUrl config blocker. The older draft-helper caveat below predates this checkpoint.

## Multiplayer references

Uses Unreal's [client/server networking model](https://dev.epicgames.com/documentation/unreal-engine/networking-overview-for-unreal-engine)
and [multiplayer test setup](https://dev.epicgames.com/documentation/unreal-engine/testing-and-debugging-networked-games-in-unreal-engine?application_version=5.8).
The controls above describe the source prototype. Working multiplayer run commands require the pending
map and host/client validation; do not treat the draft helpers as a verified playable build.
