# Grudgebound Build Plan

The living plan for the 14-day build. **Update it in the same pull request as the work it describes.**

- Hackathon: NEAR HACKATHON, 1-14 November 2026. Deadline **14 Nov 23:59 UTC**. We submit at **14 Nov 12:00 UTC** (12 h buffer).
- Track: Games With Real Economies. "Judged on the game loop, not the token; bonus if players never notice the chain."
- Rules: [`HACKATHON.md`](HACKATHON.md). Art direction: [`ART_PROMPTS.md`](ART_PROMPTS.md).

## How to keep this file up to date

1. Every task has an ID (`P2-C3` = Phase 2, Chain & Server, task 3). Put the ID in your branch name and PR title, e.g. `chain/P2-C3-relayer`.
2. When you start a task, set its status to 🟡 and put your name in **Who**. When it merges, set it to ✅ and tick its checkbox.
3. A gate is ✅ only when every "done when" line was shown on a teammate's screen.
4. New idea or change of plan? Add the task to the right phase (next free ID), and add a line to the [Decision log](#decision-log) if it changes the plan.
5. Every merged PR adds one line to the [Changelog](#changelog) (newest first).
6. Keep the "What works today" table in the [README](../README.md) in line with this file.

**Status:** ⬜ not started · 🟡 in progress · ✅ done · ⛔ blocked · ✂️ cut  
**Priority:** **Must** = needed for the demo · **Should** = only after the phase's musts pass · **Cut** = not in this hackathon

## Team

| Seat | Owns | Folders | Who |
|---|---|---|---|
| Chain & Server (C) | Contracts, testnet deploy, relayer, login, agent sub-accounts, game server, hosting | `contracts/`, `server/` | _TBD_ |
| Game Client (G) | Map, encounters, battles, binding, shop, rules and vow screens, art, demo video. **Unreal Engine 5 from 2 Nov** (setup: [`UNREAL_SETUP.md`](UNREAL_SETUP.md)) | `unreal/`, `client/` (web fallback) | _TBD_ |
| AI & Economy (A) | NEAR AI agents, pricing and Grudge tuning, simulator, Observatory, README, X post, submission draft | `shared/`, `agents/`, `sim/`, `README.md` | _TBD_ |

The lead (Abhishek) decides every submission field and approves anything public: making the repo public, the X post, the submission.

## Status at a glance

| Phase | Dates | Goal | Gate | Status |
|---|---|---|---|---|
| [0. Foundation](#phase-0--foundation) | 1 Nov | Repo, engine, simulator, agent loop, first client | none | ✅ |
| [1. Core loop offline](#phase-1--core-loop-offline) | 1-4 Nov | Grass, battle, bind, talisman, points, all in the browser. Contracts compile. | G1 · 4 Nov | ✅ |
| [2. On testnet](#phase-2--on-testnet) | 5-7 Nov | Binding mints a real token; rules go through the rite contract; we pay the gas | G2 · 7 Nov | 🟡 started 4 Nov |
| [3. Accounts, agents, live world](#phase-3--accounts-agents-live-world) | 7-10 Nov | Email sign-in, agents with wallets, Observatory | G3 · 10 Nov (feature freeze) | ⬜ |
| [4. Polish, hosting, video](#phase-4--polish-hosting-video) | 11-12 Nov | Hosted demo, recorded video, fresh-clone test | G4 · 12 Nov | ⬜ |
| [5. Ship](#phase-5--ship) | 13-14 Nov | Repo public, X post, submission | G5 · 14 Nov 12:00 UTC | ⬜ |

## The demo we are building towards (3 minutes)

Everything until the feature freeze must show up in this video. If a task doesn't appear in it, it is a **Should** at most.

| Time | What the judge sees | What it proves | Tasks |
|---|---|---|---|
| 0:00 | One line: in Hollowmere, greed spawns monsters and players vote in the rules. | The problem | P4-A2 |
| 0:15 | Sign in with an email. No seed phrase, no wallet popup. | Social login, relayer pays gas | P3-C1, P3-G1 |
| 0:30 | Walk Lantern Ward. Grudge is high in Rage because a merchant scammed people. | The economy creates the world | P1-G4 |
| 0:50 | Tall grass, a curse picked by the live Grudge, a battle, bind it. Explorer link shows the talisman. | Player-owned items (NEP-245) | P1-G1, P1-G2, P2-C4 |
| 1:30 | The AI Talisman Broker quotes a price and says why. | NPCs with wallets and reasons | P1-A2, P2-G1 |
| 1:55 | Spend points on a rule. Kessa rejects a contradiction, then accepts a fixed one. Prices change for everyone. | Rules enforced by contract | P2-C5, P2-G2 |
| 2:25 | Swear a vow and stake on it. | Stakes kept or burned by contract | P3-G2, P3-C4 |
| 2:40 | Observatory: 7 days fast-forwarded. Rules added by agents, a bankruptcy, a Calamity. | An economy that evolves without a dev patch | P2-A1, P3-A2 |
| 2:55 | Repo link and hosted demo link. | Next step | P4-C1 |

---

## Phase 0 · Foundation

**1 Nov · ✅ Done.** The skeleton every seat builds on.

| ID | Task | Seat | Status | Where |
|---|---|---|---|---|
| P0-1 | npm workspaces monorepo: `shared`, `sim`, `server`, `agents`, `client` | all | ✅ | `package.json` |
| P0-2 | Game data: 12 curses, Rage/Sorrow/Dread type triangle, 4 Wards, 12 rule templates, 6 vow templates | A | ✅ | `shared/src/` |
| P0-3 | All price formulas in one file, with plain-English reasons | A | ✅ | `shared/src/pricing.ts` |
| P0-4 | Deterministic tick engine (1 tick = 1 in-game hour = 1 real minute), seeded RNG | A | ✅ | `shared/src/engine.ts` |
| P0-5 | Arbiter: rejects bad parameters, duplicates and contradictions | A | ✅ | `shared/src/rules.ts` |
| P0-6 | Fast-forward simulator `npm run sim` | A | ✅ | `sim/` |
| P0-7 | Agent loop: observe, legal options, LLM choice, guardrail, scripted fallback | A | ✅ | `agents/` |
| P0-8 | Game server skeleton: `/health`, `/state`, `/events` | C | ✅ | `server/` |
| P0-9 | Contracts written: `items` (NEP-245 shape), `rite` (points, rules, vows). **Not compiled.** | C | ✅ | `contracts/` |
| P0-10 | Next.js + Phaser client, walkable Lantern Ward, tall-grass encounters | G | ✅ | `client/` |
| P0-11 | Battle resolver, seeded and replayable, 11 tests | G | ✅ | `client/game/battle/resolve.ts` |
| P0-12 | Battle screen and procedural battle art | G | ✅ | `client/game/scenes/BattleScene.ts` |
| P0-13 | Paper-and-ink UI: title screen, React HUD, Rules panel (runs the Arbiter), Vow Shrine | G | ✅ | `client/components/` |
| P0-14 | Concept art: paper (style guide), tide, porcelain sets | G | ✅ | `docs/art/concepts/` |

---

## Phase 1 · Core loop offline

**1-4 Nov · Gate G1 on 4 Nov.** The full loop works in the browser before any chain work depends on it.

**G1 is done when:**
- [x] Walking into tall grass starts a battle, and the battle returns to the Ward.
- [x] Binding a curse puts a talisman in the inventory; exorcising gives points; both show in the HUD.
- [x] `items` and `rite` compile to wasm.
- [x] `npm test` (30 tests), `npm run typecheck` and `npm run build` pass. (Not committed yet: commit at the milestone.)

**G1 passed on 4 Nov.** Nothing left in Phase 1.

**Unreal client (from 2 Nov).** The web loop above is done and stays as the fallback demo. The same loop is now rebuilt in
Unreal Engine 5 on a teammate's RTX 3050 laptop; steps in [`UNREAL_SETUP.md`](UNREAL_SETUP.md). Target: U1-U4 by 4 Nov,
U5-U6 by 6 Nov. If the Unreal loop isn't working by **6 Nov**, the web client is the NEAR entry and Unreal work stops.

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P1-C1 | Must | Install Rust and the wasm target; write the steps in `contracts/README.md`. Done natively (Windows GNU toolchain, no admin, no WSL). cargo-near moves to Phase 2 | C | | ✅ |
| P1-C2 | Must | Compile `items` and `rite` to wasm (324 KB and 374 KB) | C | | ✅ |
| P1-C3 | Must | Contract unit tests: mint, transfer, purse, points, propose/resolve/repeal rule, vow keep/break. **17/17 pass** on Windows with the MSVC Rust toolchain (`contracts/README.md`) | C | | ✅ |
| P1-C4 | Should | Server: `POST /battle` replays the battle from seed + actions, refuses repeats and fakes, then drains Grudge and returns points | C | | ✅ |
| P1-G1 | Must | Encounter dialog (Fight / Slip away) starts `BattleScene`; the result returns to `WardScene` | G | | ✅ |
| P1-G2 | Must | Bind result becomes a talisman card in the Talisman Case (T); pick your partner there; exorcise gives points | G | | ✅ |
| P1-G3 | Must | Your points and Ember shown in the HUD and used by the Rules panel (`client/lib/playerStore.ts`) | G | | ✅ |
| P1-G4 | Should | Kessa banner and Grudge meter react to bankruptcies and scams from the feed (bankruptcies, Calamities, scams and battle results announced) | G | | ✅ |
| P1-A1 | Must | NEAR AI Cloud key (claim credits, Q-000); agents choose through the LLM, fallback still works. Done: `anthropic/claude-haiku-5-5` on `cloud-api.near.ai/v1`, 6/6 calls through the LLM in `npm run agent:demo` | A | | ✅ |
| P1-A2 | Must | Merchant price tooltips ready for the shop screen: `GET /shop?merchant=broker` on the server | A | | ✅ |
| P1-A3 | Should | Points for battles: bind or exorcise 5 / 20 / 80 by omen, times R1 bonus rules; fleeing or losing gives 0 (`capturePoints` in shared) | A | | ✅ |
| P1-C5 | Must | Server runs battles turn by turn for the Unreal client: `POST /battle/start` (server picks the curse and a secret seed), `POST /battle/turn` (move in; events, log lines and view out; points and seed at the end). `server/src/battles.ts`, 10 tests | C | | ✅ |
| P1-U1 | Must | Install Unreal 5 + Visual Studio on the laptop; create `unreal/Grudgebound` (C++ Top Down template); write the engine version in `UNREAL_SETUP.md`. **UE 5.8.4 + VS Community 2026 (MSVC 14.51)** on the HP Victus; project at `unreal/GrudgeBound` | G | | ✅ |
| P1-U2 | Must | Lantern Ward greybox: ground, houses as blocks, tall grass, the player walks. `AGbWardGreybox`; level `/Game/Grudgebound/LanternWard` made by `unreal/tools/make-level.ps1`; click-to-walk works | G | | ✅ |
| P1-U3 | Must | `GrudgeboundApi` C++ class (`/health`, `/state`, `/events`); HUD shows the Ward's Grudge and Kessa's latest line. `UGbApiSubsystem`, `UGbHudWidget`; seen live (day/hour, Grudge bars, town events) | G | | ✅ |
| P1-U4 | Must | Walking through tall grass calls `POST /battle/start` and shows the encounter (Fight / Slip away). Both buttons tested in Unreal | G | | ✅ |
| P1-U5 | Must | Battle screen on the server's turn API (P1-C5): attack, ward, seal, flee; result back to the Ward. Played to a bind in Unreal (+10 points) | G | | ✅ |
| P1-U6 | Must | Talisman Case, partner choice, points and Ember in the HUD. Points and Ember update after a bind; the Talisman Case (T) lists the new talisman and the partner (checked in the editor, 4 Nov) | G | | ✅ |
| P1-C6 | Should | `GET /curses`: names and looks of every curse for the Unreal Talisman Case (no stats), 1 test | C | | ✅ |

---

## Phase 2 · On testnet

**5-7 Nov · Gate G2 on 7 Nov.** The things that decide fairness move on chain.

**G2 is done when:**
- [ ] Binding mints a talisman on testnet, visible on the explorer.
- [ ] A rule proposal goes through the `rite` contract, and the game reads it back.
- [ ] The relayer pays the gas; the player signs nothing visible.

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P2-C1 | Must | Testnet accounts: game master, `items`, `rite`, relayer (funded from the faucet). Done 4 Nov: see `contracts/README.md` | C | | ✅ |
| P2-C2 | Must | Deploy `items` and `rite`; account names in `.env.example`. Done 4 Nov with cargo-near 0.22 (`--no-abi` on Windows); mint and battle smoke-tested | C | | ✅ |
| P2-C3 | Must | Relayer endpoint in the server: signed delegate action in, gas paid by us (NEP-366). Relayer works on testnet: `relayer.grudgebound.testnet` sends players' delegate actions and pays the gas (`chain.ts` relay). For now the server signs for its demo players; an endpoint taking client-signed delegates comes with player keys (Phase 3) | C | | 🟡 |
| P2-C4 | Must | Battle result → contract call: bind mints a talisman, exorcise adds points. Server done and checked on testnet (`server/src/chain.ts`); Unreal shows the token id, L opens the explorer. Left: see it in the editor | C | | 🟡 |
| P2-C5 | Must | Rule propose / resolve / repeal through `rite`; server reads rules in force from the contract. Done 4 Nov: `POST /rules/propose` (relayed for the player, resolved by Kessa), rules loaded on start (`server/src/rules.ts`). Seen on testnet: Afreen's R8 accepted, Akshay's contradicting R1 rejected and refunded. Repeal not wired yet | C | | ✅ |
| P2-C6 | Should | Server reads contract events (NEP-297) into `/events` | C | | ⬜ |
| P2-G1 | Must | Shop screen: merchant price, reason, buy and sell | G | | 🟡 server + Unreal (B) built, testnet buy/sell verified; in-editor check at end of P2 |
| P2-G2 | Must | Rules panel sends proposals to the server and shows Kessa's verdict and the explorer link. Built 4 Nov in Unreal: R opens the Rite (rules in force, cost, your points, rule builder); Kessa's verdict at once, then "Recorded on NEAR as rule N (L to see it)". `GET /rules` feeds it. Editor check at the end of P2 | G | | 🟡 |
| P2-G3 | Should | Talisman card shows its on-chain ID and purse | G | | 🟡 Unreal case shows NEAR tal-N and purse; in-editor check at end of P2 |
| P2-A1 | Must | Tune Grudge so a 7-day run shows a rule war, a bankruptcy and a Calamity; pick the demo seed | A | | ⬜ |
| P2-A2 | Should | `npm run replay <seed>` re-checks any battle from its seed | A | | ⬜ |
| P2-C7 | Must | Web client uses `/battle/start` + `/battle/turn` when the server is up (it still picks its own seed and curse for `POST /battle`, so a cheater could retry seeds or claim a rarer curse), then `POST /battle` is removed | C | | ⬜ |
| P2-G4 | Should | Player battles respect curfew rules (R2): no exorcising in a Ward when a curfew forbids it | G | | 🟡 server: both curfews block the fight, bind curfew refuses Seal, exorcising under curfew pays 0 and is logged; Unreal shows the warnings; in-editor check at end of P2 |
| P2-G5 | Must | Talisman Case and points read from the chain instead of browser storage | G | | 🟡 GET /player + items `talismans_of` view (upgraded on testnet 4 Nov); Unreal case reads it; in-editor check at end of P2 |

---

## Phase 3 · Accounts, agents, live world

**7-10 Nov · Gate G3 on 10 Nov = feature freeze.** After G3: only fixes, art and text.

**G3 is done when:**
- [ ] A new player signs in with an email and plays without seeing a wallet.
- [ ] AI agents trade and propose rules through their own NEAR accounts.
- [ ] The Observatory shows Grudge, prices and rules over time.
- [ ] Vows are sworn and settled by the `rite` contract.

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P3-C1 | Must | Email sign-in. Start with a 2-hour spike; fallback: guest sign-in where the server creates a testnet sub-account and the key stays in the browser | C | | ⬜ |
| P3-C2 | Must | One sub-account per agent, function-call key limited to our contracts, with a spend cap | C | | ⬜ |
| P3-C3 | Must | Server writes binds, rules and vows on chain and serves the live state from it | C | | ⬜ |
| P3-C4 | Must | Vow stake, keep and break through `rite` | C | | ⬜ |
| P3-G1 | Must | Sign-in screen with no wallet words on it | G | | ⬜ |
| P3-G2 | Must | Vow Shrine swears real vows and shows the stake | G | | ⬜ |
| P3-G3 | Should | Tide Ward map using the tide art set | G | | ⬜ |
| P3-G4 | Should | New player sprite (not the porcelain one, it looks like Link) | G | | ⬜ |
| P3-A1 | Must | Agents act through their own accounts, guardrail on every action | A | | ⬜ |
| P3-A2 | Must | Observatory page: Grudge, prices and rules over time, plus fast-forward | A | | ⬜ |
| P3-A3 | Should | Rival dialogue lines from NEAR AI, shown by Kessa | A | | ⬜ |

---

## Phase 4 · Polish, hosting, video

**11-12 Nov · Gate G4 on 12 Nov.**

**G4 is done when:**
- [ ] Client and server are hosted, and stay up until 25 Nov.
- [ ] The 3-minute video is recorded from the hosted build.
- [ ] Someone clones the repo on a second machine and runs it by following the README alone.

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P4-C1 | Must | Host the client and server; add the links to the README | C | | ⬜ |
| P4-C2 | Must | Fresh-clone test on a second machine | C | | ⬜ |
| P4-G1 | Must | Record the demo video (script above) | G | | ⬜ |
| P4-G2 | Should | Sound, phone controls, transitions | G | | ⬜ |
| P4-A1 | Must | README for judges: "do X, see Y" steps, Q-005 disclosures, NEAR features used | A | | ⬜ |
| P4-A2 | Must | Draft the X post: problem first, one full moment, NEAR's role, tags @NEARProtocol #NEARHackathon | A | | ⬜ |

---

## Phase 5 · Ship

**13-14 Nov · Gate G5 on 14 Nov 12:00 UTC.** Re-read the live [HACKATHON.md](https://nearhackathon.com/HACKATHON.md) on 13 Nov first.

**G5 is done when:**
- [ ] Repo is public with LICENSE and disclosures.
- [ ] X post is live with the video.
- [ ] Submission answered 201.

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P5-1 | Must | Bug fixes only; nothing new merges after 13 Nov 18:00 UTC | all | | ⬜ |
| P5-2 | Must | Make the repo public (lead's yes) | lead | | ⬜ |
| P5-3 | Must | Mainnet payout account created by the team | lead | | ⬜ |
| P5-4 | Must | Post on X (the lead posts it) | lead | | ⬜ |
| P5-5 | Must | Submission fields drafted: name, lead, email, size `2 - 4`, tracks, repo link, post link, description (20-2000 chars), consent | A | | ⬜ |
| P5-6 | Must | Lead reads the printed payload, says yes, it is sent once | lead | | ⬜ |

---

## Cut (not in this hackathon)

- Mainnet deploy, NEAR Intents, anything cross-chain
- Full Ash and Bone Wards (shown as locked on the town map)
- Trading between players, agents hiring other agents

## Risks

| Risk | Fallback | Decide by | Status |
|---|---|---|---|
| Contracts have never compiled; Rust on Windows is slow to set up | Build in WSL. If `rite` doesn't compile by 4 Nov: items, points and rules on chain; vows on the server, logged as events | 4 Nov | closed (17/17 tests pass on Windows, 3 Nov) |
| NEAR AI Cloud credits run out or are slow | Scripted fallback exists; limit calls per tick; Haiku 5.5 is ~$0.0003 a call (~1–2 s) | 2 Nov | mitigated |
| Calamities are rare, so the demo may show none | Tune Grudge gain; pick a demo seed that reaches one | 6 Nov | open |
| Email login takes longer than planned | Guest sign-in via server-made sub-accounts, still no wallet popup | 8 Nov | open |
| Too many Wards for 14 days | Lantern only, Tide second, the rest locked | 9 Nov | open |
| Judges' AI screening can't run the project (Q-017) | Fresh-clone test; every README step is "do X, see Y" | 12 Nov | open |
| Hosted demo down during judging | Local run steps in the README; check the host daily until 25 Nov | 11 Nov | open |
| Unreal client not at parity in time (new engine, one laptop) | Web client stays working and is the NEAR entry; decide on 6 Nov | 6 Nov | open |
| Judges can't build an Unreal project | Packaged Windows build on a GitHub Release + demo video; web client runs from the README | 12 Nov | open |
| Unreal assets fill the Git LFS quota, or two people edit one map | No Starter Content; commit at milestones only; announce or `git lfs lock` before editing a map/Blueprint | ongoing | open |

## Open questions

- [ ] Who takes which seat?
- [ ] Which email-login option? (decided after the P3-C1 spike)
- [ ] Where do we host the server (the Unreal build runs on the judge's PC and needs a reachable server URL)?
- [x] Which Unreal 5.x version do we pin? **5.8** (newest hotfix), with Visual Studio Community 2026.
- [ ] Tracks besides `games`?
- [ ] Daily check-in time?

## Working agreements

- One branch per task: `seat/ID-short-name` (e.g. `client/P1-G1-battle-from-ward`). Merge by PR; another seat reviews.
- Main always runs: `npm test`, `npm run typecheck`, `npm run build` pass before merging.
- 15-minute check-in once a day: what merged, what's blocked, is the next gate on track.
- Never commit `.env`, keys or the submission payload. PDFs in `docs/` are git-ignored on purpose.
- Testnet only. Nothing is posted, made public or submitted without the lead's yes.

## Decision log

Newest first. Add a line whenever the plan, the stack or the scope changes.

| Date | Decision | Why |
|---|---|---|
| 2 Nov | Unreal client code lives in a plugin (`unreal/Plugins/GrudgeboundClient`) with its UI built in C++ | Written before the engine was installed, without touching template files; works with no widget assets; Blueprint widgets can replace the screens in the art pass |
| 2 Nov | Pin Unreal Engine 5.8 and Visual Studio Community 2026; Lumen, Nanite and virtual shadow maps off | Newest 5.x, out since June with hotfixes; VS 2026 is Epic's pick for 5.8; the build laptop's GPU has 4 GB |
| 2 Nov | Game client moves to Unreal Engine 5 (built on a teammate's RTX 3050 laptop); server, shared, agents and contracts unchanged; web client kept as fallback until the Unreal loop works | Team choice for a richer game; the fallback protects the NEAR deadline |
| 2 Nov | Server runs battles turn by turn for the Unreal client (P1-C5) instead of a C++ copy of the resolver | One copy of the battle rules; the server makes the seed |
| 1 Nov | Battle resolver moved to `shared/` | The server replays every reported battle with the same code the client fights with |
| 1 Nov | Player progress kept in the browser (`playerStore`) until Phase 2 | Lets the loop work now; the same shape is filled from the chain later |
| 1 Nov | Rust installed natively (Windows GNU toolchain) instead of WSL | No Ubuntu in WSL on the lead's machine; builds need no admin rights. Contract tests run in Docker/WSL/Linux |
| 1 Nov | Battles resolved by the game server, with seed and log hash for replay; ownership, points, rules and vows enforced by contracts | Fast battles, fair economy |
| 1 Nov | Paper art set is the style guide; tide set for Tide Ward only; porcelain player not used | Consistent look; porcelain player looks like Link |
| 1 Nov | Plan PDFs stay out of the repo and its history | Internal documents |
| 1 Nov | Node's built-in test runner instead of Vitest | No Vite in the project |
| 1 Nov | npm workspaces and Next.js (App Router) hosting Phaser 3 | Team preference; replaced pnpm and Vite |

## Changelog

Newest first. One line per merged PR.

| Date | Change | Who | PR |
|---|---|---|---|
| 3 Nov | First Unreal build on UE 5.8.4: project created, plugin compiles (2 fixes), LanternWard level script; walk, encounter, battle and bind work in the editor | | branch `client/P1-U1-unreal-client` |
| 2 Nov | Unreal client plugin (API subsystem, Lantern Ward greybox, tall grass, HUD, encounter, battle screen, Talisman Case), `GET /curses`, Unreal contract test, `setup-project.ps1`, UE 5.8 + VS 2026 pinned; 44 tests | | branch `client/P1-U1-unreal-client` |
| 1 Nov | Server-run battles (`/battle/start`, `/battle/turn`) for the Unreal client; encounter roll and battle log text moved to `shared/`; 40 tests | | |
| 1 Nov | Phase 1 core loop: fight from the Ward, bind, Talisman Case, points and Ember in the HUD; server `POST /battle` replay check and `GET /shop`; contracts compile, 17 contract tests written; Unreal setup guide | | branch `phase-1/core-loop-and-unreal-setup` |
| 1 Nov | Paper-and-ink design system, title screen, React HUD, Rules panel, Vow Shrine; Lantern Ward lighting | | #2 |
| 1 Nov | Battle resolver with tests, BattleScene, battle art; rule template and params in the world summary | | #1 |
| 1 Nov | Concept art replaced by three themed sets | | none |
| 1 Nov | Switched to npm workspaces and Next.js | | none |
| 1 Nov | Project skeleton | | none |
