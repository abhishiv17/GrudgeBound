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

**Delivery confirmed 7 Nov:** the final player-facing game is the packaged **Unreal Windows client**.
The Node game server remains necessary for shared world state, authoritative battles, AI agents and NEAR
settlement. The existing browser sign-in page is a support service for Unreal's email handoff. No new web-game
features are planned. The existing web game remains in the repository; the Observatory will be a native Unreal panel.

| Phase | Dates | Goal | Gate | Status |
|---|---|---|---|---|
| [0. Foundation](#phase-0--foundation) | 1 Nov | Repo, engine, simulator, agent loop, first client | none | ✅ |
| [1. Core loop offline](#phase-1--core-loop-offline) | 1-4 Nov | Grass, battle, bind, talisman, points, all in the browser. Contracts compile. | G1 · 4 Nov | ✅ |
| [2. On testnet](#phase-2--on-testnet) | 4-5 Nov | Binding mints a real token; rules go through the rite contract; we pay the gas | G2 · 7 Nov | ✅ G2 passed 5 Nov (Rite panel check in Unreal left) |
| [3. Accounts, agents, live world](#phase-3--accounts-agents-live-world) | 5-8 Nov | Email sign-in (from Unreal too), agents with wallets, vows, Observatory | G3 · 8 Nov (feature freeze) | 🟡 started 5 Nov |
| [4. Look, story, hosting, video](#phase-4--look-story-hosting-video) | 6-12 Nov (starts alongside Phase 3) | Unreal looks like a real game and tells a story; hosted server, packaged build, video | G4 · 12 Nov | 🟡 story implementation started |
| [5. Ship](#phase-5--ship) | 13-14 Nov | Repo public, X post, submission | G5 · 14 Nov 12:00 UTC | ⬜ |

### Recovery checkpoint · 7 Nov

- **Latest local work:** Phase 4 story draft, four illustrated native intro panels (skippable, I to replay), per-account/server story saves, HUD objectives and real-event chapters implemented on `main`. The Ninth Bell co-op source prototype is also present. Unreal editor build and 8/8 native checks pass (5 story/intro, 3 co-op logic); all four intro previews reviewed. Lead/script review and full Play-mode acceptance remain (`STORY.md`); the co-op map has not been generated or tested with host/client (`THE_NINTH_BELL.md`). New story/co-op work is uncommitted and has not been pushed.
- **Phase 3 work since recovery:** Unreal Vow Shrine (V) and authenticated backend stake/settlement implemented; 108 tests and type checks pass, and the Unreal editor target compiles. Real testnet stake return and release/burn verified for the existing Unreal demo account. See [`VOWS.md`](VOWS.md) for proof, controls and acceptance checks. The gameplay power bonuses and Oathbreaker duration remain open; the Shrine advertises only the implemented stake bonus.
- **Where we stopped:** P3-C1 / P3-G1 / P3-U1 (email sign-in and Unreal sessions). The 6 Nov changes were still uncommitted on `client/P1-U1-unreal-client`; GitHub remained at `71ee3ea` when this audit started.
- **Verified today:** `npm test` **100/100**, `npm run typecheck`, the web production build, and the Unreal Development Editor target all pass. The Unreal target is up to date with the local C++ changes. Added a regression test and fix so simultaneous browser approvals cannot replace the player during native account provisioning. The restricted build environment crashed bundled .NET before compilation; the same Unreal build succeeded outside it.
- **Last unresolved acceptance check:** restart/resume and F10 logout with two email identities in Unreal. The signed-in save and the legacy shared save are separate; yesterday's inspection found 5 points / two talismans in the signed-in save and 70 points / six in the legacy save. Confirm the HUD and Case show the signed-in values after restarting; do not copy the shared save into another identity.
- **Sign-in progress:** user selected encrypted server-held keys for testnet on 7 Nov; enabled in the local configuration. A dedicated new identity passed native-format HTTP approval, provisioning, testnet binding and selling, server restart/session recovery and logout (`npm run native:smoke`). Verified Resend sender, a new player's rule proposal and the remaining actual Unreal checks are still open. Console codes do not verify a real mailbox. See [`SIGN_IN.md`](SIGN_IN.md).
- **Next implementation order:** finish sign-in acceptance and the remaining P2 Rite/curfew editor checks; agent accounts and live actions (P3-C2/A1), vows (P3-C3/C4/G2), Satchel/supplies/ink (P3-C5/C6/C8/U4), then Observatory (P3-A2). P3-C7 remains optional. Story and level work can proceed alongside these once assigned.
- **Schedule (9 Nov):** team target moved to **10 Nov, 11 Nov at the latest**. G3 remains incomplete against the plan's **8 Nov feature freeze**. Phase 4's story, interiors, art, hosting, packaging and video are still open; its planned start was 6 Nov. A passing code checkpoint does not mean these gates have passed.

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
| 2:40 | The Ninth Bell: two players hold the seal anchors together, then strike The Unwritten in the shared opening. | Co-op story level, host-authoritative multiplayer | P4-U13 |
| 2:55 | Observatory: 7 days fast-forwarded. Rules added by agents, a bankruptcy, a Calamity. | An economy that evolves without a dev patch | P2-A1, P3-A2 |
| 3:05 | Repo link and hosted demo link. | Next step | P4-C1 |

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
- [x] Binding mints a talisman on testnet, visible on the explorer.
- [x] A rule proposal goes through the `rite` contract, and the game reads it back.
- [x] The relayer pays the gas; the player signs nothing visible.

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P2-C1 | Must | Testnet accounts: game master, `items`, `rite`, relayer (funded from the faucet). Done 4 Nov: see `contracts/README.md` | C | | ✅ |
| P2-C2 | Must | Deploy `items` and `rite`; account names in `.env.example`. Done 4 Nov with cargo-near 0.22 (`--no-abi` on Windows); mint and battle smoke-tested | C | | ✅ |
| P2-C3 | Must | Relayer endpoint in the server: signed delegate action in, gas paid by us (NEP-366). Relayer works on testnet: `relayer.grudgebound.testnet` sends players' delegate actions and pays the gas (`chain.ts` relay). For now the server signs for its demo players; an endpoint taking client-signed delegates comes with player keys (Phase 3) | C | | 🟡 |
| P2-C4 | Must | Battle result → contract call: bind mints a talisman, exorcise adds points. Server done and checked on testnet (`server/src/chain.ts`); Unreal shows the token id, L opens the explorer. Left: see it in the editor Checked 5 Nov: bound in Unreal (tal-9) and on the web (tal-11), both minted and recorded on the rite. | C | | ✅ |
| P2-C5 | Must | Rule propose / resolve / repeal through `rite`; server reads rules in force from the contract. Done 4 Nov: `POST /rules/propose` (relayed for the player, resolved by Kessa), rules loaded on start (`server/src/rules.ts`). Seen on testnet: Afreen's R8 accepted, Akshay's contradicting R1 rejected and refunded. Repeal: `POST /rules/repeal` (relayed, twice the cost; a repealed cruel rule eases the Ward's dread), Strike row in the Rite (R); rules synced from the contract every minute. Editor check at the end of P2 | C | | ✅ |
| P2-C6 | Should | Server reads contract events (NEP-297) into `/events` | C | | ✅ verified 5 Nov (contract events show in the Unreal and web feeds); block indexer for outside writes: Phase 3 |
| P2-G1 | Must | Shop screen: merchant price, reason, buy and sell | G | | ✅ checked in Unreal 5 Nov: buy tal-10, sell tal-8 and tal-9, all settled on NEAR |
| P2-G2 | Must | Rules panel sends proposals to the server and shows Kessa's verdict and the explorer link. Built 4 Nov in Unreal: R opens the Rite (rules in force, cost, your points, rule builder); Kessa's verdict at once, then "Recorded on NEAR as rule N (L to see it)". `GET /rules` feeds it. Editor check at the end of P2 | G | | 🟡 |
| P2-G3 | Should | Talisman card shows its on-chain ID and purse | G | | ✅ checked in Unreal (NEAR tal-N and purse in the case) |
| P2-A1 | Must | Tune Grudge so a 7-day run shows a rule war, a bankruptcy and a Calamity; pick the demo seed | A | | ✅ Demo seed 2: bankruptcy d3, Calamity d4 in the same Ward, rule war d5; `npm run sim` checked 5 Nov |
| P2-A2 | Should | `npm run replay <seed>` re-checks any battle from its seed | A | | ✅ server archives every paid battle (data/battles.jsonl); `npm run replay <seed or id>` replays it and checks the hash the rite contract logged. VERIFIED on testnet: seed 1499753501 (4 Nov), 4251663236 (Unreal bind) and 4063853491 (web bind) on 5 Nov |
| P2-C7 | Must | Web client uses `/battle/start` + `/battle/turn` when the server is up (it still picks its own seed and curse for `POST /battle`, so a cheater could retry seeds or claim a rarer curse), then `POST /battle` is removed | C | | ✅ checked 5 Nov: a web battle ran on the server (battle eba5d60e, tal-11 minted for the guest account); POST /battle removed |
| P2-G4 | Should | Player battles respect curfew rules (R2): no exorcising in a Ward when a curfew forbids it | G | | 🟡 server: both curfews block the fight, bind curfew refuses Seal, exorcising under curfew pays 0 and is logged; Unreal shows the warnings; in-editor check at end of P2 |
| P2-G5 | Must | Talisman Case and points read from the chain instead of browser storage | G | | ✅ GET /player + items `talismans_of`; checked in Unreal; local-only talismans hidden once the chain answers |

---

## Phase 3 · Accounts, agents, live world

**5-8 Nov · Gate G3 on 8 Nov = feature freeze** (pulled forward from 10 Nov on 5 Nov). After G3: no new systems; the look, story text, sound and fixes (Phase 4) carry on.

**G3 is done when:**
- [ ] A new player signs in with an email and plays without seeing a wallet, in the Unreal client.
- [ ] AI agents trade and propose rules through their own NEAR accounts.
- [ ] The Observatory shows Grudge, prices and rules over time.
- [ ] Vows are sworn and settled by the `rite` contract.
- [ ] The Satchel holds talismans and supplies; binding uses up a blank talisman; supplies are bought from the Broker with Ember.
- [ ] A talisman's ink (HP) carries over between battles and is restored by the Mender (Ember) or by resting overnight in your room (free).

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P3-C1 | Must | Email sign-in: email codes, encrypted identity/session store, session-bound API actions and testnet account provisioning built 6 Nov. User chose encrypted server-held signing for testnet on 7 Nov (`AUTH_KEY_MODE=server`). Dedicated new identity passed native-format HTTP approval, provisioning, bind, sale, restart/resume and logout on testnet. Real sender and new-player rule/native acceptance remain (`SIGN_IN.md`) | C | | 🟡 |
| P3-C2 | Must | One sub-account per agent, function-call key limited to our contracts, with a spend cap | C | | ⬜ |
| P3-C3 | Must | Server writes binds, rules and vows on chain and serves the live state from it. Binds, points, ownership and rules already integrated in Phase 2; vows remain | C | | 🟡 |
| P3-C4 | Must | Vow stake, keep and break through `rite`: authenticated routes, fixed stakes, contract deadlines, automatic settlement/retry after restart, explicit release/burn, and battle/trade restrictions implemented 7 Nov. Testnet: 15 points locked, 18 returned, then a second 15-point stake burned. Native acceptance remains; combat/purse bonuses and Oathbreaker duration still open (`VOWS.md`) | C | | 🟡 |
| P3-G1 | Must | Sign-in screen with no wallet words on it. `/sign-in` email/code page and HttpOnly-cookie gateway built 6 Nov; web production build passes. Local console-code sign-in reported working by the user (6 Nov). Real-mail acceptance remains | G | | 🟡 |
| P3-G2 | Must | Native Unreal Vow Shrine (V): four vow templates, duration/parameter choices, stake/return preview, active/history list, release/burn and ledger proof (L). Editor target compiles 7 Nov; play-through acceptance remains (`VOWS.md`) | G | | 🟡 |
| P3-G3 | Should | Tide Ward map using the tide art set. Moved 5 Nov: becomes P4-U11 in Unreal | G | | ➡️ P4-U11 |
| P3-G4 | Cut | New web player sprite. Unreal character/art work belongs to Phase 4 | G | | ✂️ 7 Nov: Unreal delivery |
| P3-A1 | Must | Agents act through their own accounts, guardrail on every action | A | | ⬜ |
| P3-A2 | Must | Native Unreal Observatory panel: Grudge, prices and rules over time, plus an isolated fast-forward preview | A | | ⬜ |
| P3-A3 | Should | Rival dialogue lines from NEAR AI, shown by Kessa | A | | ⬜ |
| P3-U1 | Must | Sign-in from Unreal: opens the browser, collects an approved session by polling with a separate private device secret, then plays as that account. No player account in Project Settings; Windows DPAPI remembers the session, F10 signs out, saves are per account. Editor target compiles; local browser-to-Unreal handoff reported working by the user (6 Nov). Manual resume/logout/isolation and real-mail/testnet acceptance remain (`SIGN_IN.md`) | G | | 🟡 |
| P3-U2 | Must | Chain words out of the game UI: 7 Nov, native HUD/case/shop/rules/battle confirmations use the Rite's ledger, event feed translates the NEAR prefix, and L keeps proof available. Native Vow Shrine uses the same language. Seal icon and visual acceptance remain | G | | 🟡 |
| P3-U3 | Must | Unreal project in the repo: `.uproject`, `Source`, `Config`, `Content/Grudgebound` (Git LFS); template content from each developer's own engine (`setup-project.ps1 -TemplateContent`) | G | | ✅ 5 Nov |
| P3-C5 | Must | Satchel on the server, kept per player: carried talismans (up to 5; the rest wait in the Case in your room), supplies (blank talisman, gilded talisman, Ink Salve), keepsakes (story items). Binding uses up a blank talisman; `GET /satchel`. Server built 7 Nov: `GET /satchel`, `POST /satchel/carry` and `/satchel/salve`, stored per account in `data/satchels.json`; new players get Kessa's starter kit (3 blank talismans, 1 Ink Salve) once. Native panel (P3-U4) and play acceptance remain | C | | 🟡 |
| P3-C6 | Must | Broker sells supplies for Ember at prices from `pricing.ts` (scarcity, demand, fear), with the merchant's reasoning in a tooltip; agents buy from the same stock, so a run on blank talismans raises the price for everyone. Built 7 Nov: Broker stocks blank talismans (15), gilded talismans (30) and Ink Salves (10) shared with agents; Ember paid like a talisman buy, supply counted only after payment, rolled back on failure. Live testnet purchase not yet checked | C | | 🟡 |
| P3-C7 | Should | Supplies are NEP-245 fungible tokens in `items` beside Ember and the talismans (one contract holds all three kinds); fallback: the server keeps supplies and only the Ember spent goes on chain | C | | ⬜ |
| P3-C8 | Must | Ink carries over: a talisman's ink stays low after a fight. The Mender restores it for Ember (price rises with the Ward's Sorrow; the reason shows); resting in your room restores it for free but the clock moves on to morning. Built 7 Nov: ink saved every turn (leaving a fight doesn't heal), 0 ink blocks a fight, `POST /mender/restore` (8 Ember × Sorrow factor, with the reason) and `POST /rest` (personal clock only, never the world tick). Battle proofs include starting ink. Live testnet restore not yet checked | C | | 🟡 |
| P3-U4 | Must | Unreal Satchel panel (B): pockets for Talismans, Supplies, Keepsakes and the Ledger (Ember, points, debt); use a salve outside battle; choose your partner. Built and reviewed 8 Nov: B opens it (Broker button inside), ink bars, carry/uncarry, partner choice, salve, interim Mender/rest buttons; battle blocked at 0 ink; editor build and 14/14 native checks pass. Play acceptance remains | G | | 🟡 |

---

## Phase 4 · Look, story, hosting, video

**6-12 Nov · Gate G4 on 12 Nov.** The look and story start on 6 Nov, alongside Phase 3; hosting, the packaged build and the video take 11-12 Nov.

**G4 is done when:**
- [ ] Client and server are hosted, and stay up until 25 Nov.
- [ ] The 3-minute video is recorded from the hosted build.
- [ ] Someone clones the repo on a second machine and runs it by following the README alone.
- [ ] Unreal: a new player gets the intro, an objective on screen, and Chapters 1-3 on the demo seed.
- [ ] Unreal: the Ward, the panels and the battle look like the concept art (CC0 assets, listed in the README).
- [ ] Unreal: you walk through a door into the Counting House, the Mender's House, the Rite Hall and your room, and do that building's job inside.
- [ ] Unreal: two players on LAN finish The Ninth Bell together: clues, the two-anchor ritual, the boss and the ending vote (`THE_NINTH_BELL.md`).
- [ ] A packaged Windows build on a GitHub Release plays against the hosted server.

| ID | Pri | Task | Seat | Who | Status |
|---|---|---|---|---|---|
| P4-C1 | Must | Host the client and server; add the links to the README | C | | ⬜ |
| P4-C2 | Must | Fresh-clone test on a second machine | C | | ⬜ |
| P4-G1 | Must | Record the demo video (script above) | G | | ⬜ |
| P4-G2 | Should | Sound, phone controls, transitions | G | | ⬜ |
| P4-A1 | Must | README for judges: "do X, see Y" steps, Q-005 disclosures, NEAR features used. Rewritten 9 Nov for the Unreal build: release/hosted/video links and ⏳ steps are filled when T-4/T-7/T-8/T-9/T-10/T-13 land; owner confirms the disclosure wording and team names | A | | 🟡 |
| P4-A2 | Must | Draft the X post: problem first, one full moment, NEAR's role, tags @NEARProtocol #NEARHackathon. Four-part thread and the submission description drafted 9 Nov (kept locally, not in the repo); owner edits and posts | A | | 🟡 |
| P4-U1 | Must | Story script: newcomer in debt, Kessa's offer, the Rite/Grudge and briefs for Vane, Silk, Fen and Rook & Ash. Draft written in `STORY.md`, implemented as the intro; lead review remains | G | | 🟡 |
| P4-U2 | Must | Native skippable four-panel paper intro with procedural ink illustrations, Next / Enter / Space, Skip / Escape, I to replay. Completion is per account and server. Editor build and native rendering/navigation check pass; all four previews inspected. Full Play-mode acceptance remains (`STORY.md`) | G | | 🟡 |
| P4-U3 | Must | Native story subsystem and HUD objective: Arrival → real bankruptcy → actual Calamity; debt acknowledgement and own confirmed battle/rule/vow response. Separate per-player story save. Native checks pass for chapter order, actual facts, save serialization/isolation and confirmed actions; full Play-mode acceptance remains (`STORY.md`) | G | | 🟡 |
| P4-U4 | Must | Lantern Ward becomes a district, not a test level: market street, gambling-den row, canal edge with the Tide docks gate, tall-grass lots, locked gates to Ash and Bone (Kessa says why). CC0 packs (Quaternius, Kenney, Poly Pizza); lantern lighting, fog, colour grade; fits the 4 GB GPU | G | | ⬜ |
| P4-U5 | Must | Panels in the paper-and-ink style of the concept art: paper textures, ink frames, a pixel/brush font, open and close animations (HUD, Rite, shop, case, battle) | G | | ⬜ |
| P4-U6 | Should | Battle staging: camera framing, the two curses on screen, hit flashes, the seal animation. Built 12 Nov: curses step in, a blow knocks back with a ring and a camera punch, the seal falls and holds or is thrown off; off under Reduce battle motion. Native check passes; play acceptance remains | G | | 🟡 |
| P4-U7 | Should | Sound: footsteps, grass rustle, battle hits, seal, UI clicks, a night-market loop. Built 12 Nov: all 25 sounds are synthesised by `unreal/tools/make-audio.py` (no third-party samples); Market and Effects volume sliders. Not yet judged by ear | G | | 🟡 |
| P4-U8 | Must | Packaged Windows build (Shipping) on a GitHub Release, pointed at the hosted server | G | | ⬜ |
| P4-U9 | Cut | New web-client modes. Final delivery is Unreal; existing browser game stays as archived fallback code | G | | ✂️ 7 Nov: Unreal delivery |
| P4-U10 | Must | Buildings you can enter: a door fades to an interior and back. **Counting House** (the Broker: buy and sell talismans and supplies), **Mender's House** (restore ink), **Rite Hall** (Kessa: rules, vows, points; replaces opening the Rite with R alone), **your room** above the noodle stall (rest, the Talisman Case chest, the debt notice). Agents are seen inside when they trade | G | | ⬜ |
| P4-U11 | Should | Tide Ward in Unreal (docks, warehouses, smugglers), a smaller second level loaded from the Lantern canal gate; only after G3, and only if P4-U4 and P4-U10 are done | G | | ⬜ |
| P4-U12 | Should | Townsfolk you can talk to: a few CC0 characters with lines that change with the Ward's Grudge and the chapter. Built 12 Nov: the 13 street walkers stop and speak on Interact (trade, mood, chapter). Lines are a draft for the lead's review | G | | 🟡 |
| P4-U13 | Must | **The Ninth Bell**, a 2-4 player co-op story level in Bone Ward's sealed Bell Quarter: shared clues, a two-anchor ritual, The Unwritten boss fight with revives, and a party vote between two endings. LAN/direct-IP, host-authoritative, no wallet rewards and no link to the live economy. Proxy art first. Script: [`THE_NINTH_BELL.md`](THE_NINTH_BELL.md) | G | | 🟡 |

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

## Our own game, not a copy

We use genre ideas (turn-based fights, binding what you defeat, a shop, a place to heal, a bag, buildings you can enter). Ideas and mechanics are free to use; names, characters, art, music, sounds and screen layouts are not. So:

- **Our words only:** curse, talisman, bind, seal, ink, Ember, Grudge, the Rite, Ward, Satchel, Mender, Counting House. Never Pokémon terms or close copies (no "Poké-", "trainer", "Center", "Mart", "wild X appeared", "evolve", "party").
- **Our look:** the paper-and-ink style guide; no red-and-white capture ball, no red-roofed healing building, no nurse with a healing jingle, no copies of known battle or bag screen layouts. Talismans are paper charms, not balls.
- **Our systems differ on purpose:** healing costs Ember or time (an economy sink), the shop's prices move with the world, curses carry emotions (Rage, Sorrow, Dread) instead of elemental types, and rules made by players and agents change the town.
- **Assets:** CC0 or our own only, every pack listed in the README with its licence; sounds and music CC0 or synthesized by us.
- Check new text against this list before it goes in the game.

## Cut (not in this hackathon)

- Mainnet deploy, NEAR Intents, anything cross-chain
- Full Ash and Bone Wards (shown as locked on the town map)
- Trading between players, agents hiring other agents

## Risks

| Risk | Fallback | Decide by | Status |
|---|---|---|---|
| Contracts have never compiled; Rust on Windows is slow to set up | Build in WSL. If `rite` doesn't compile by 4 Nov: items, points and rules on chain; vows on the server, logged as events | 4 Nov | closed (17/17 tests pass on Windows, 3 Nov) |
| NEAR AI Cloud credits run out or are slow | Scripted fallback exists; limit calls per tick; Haiku 5.5 is ~$0.0003 a call (~1–2 s) | 2 Nov | mitigated |
| Calamities are rare, so the demo may show none | Demo seed 2 reaches bankruptcy, a Calamity and a rule war; regression test checks the sequence | 6 Nov | mitigated (P2-A1; rechecked 7 Nov) |
| Email login takes longer than planned | Guest sign-in via server-made sub-accounts, still no wallet popup | 8 Nov | open |
| Too many Wards for 14 days | Lantern only, Tide second, the rest locked | 9 Nov | open |
| Judges' AI screening can't run the project (Q-017) | Fresh-clone test; every README step is "do X, see Y" | 12 Nov | open |
| Hosted demo down during judging | Local run steps in the README; check the host daily until 25 Nov | 11 Nov | open |
| Unreal client not at parity in time (new engine, one laptop) | Decided 5 Nov: Unreal is the main client; the web client stays working as the fallback | 6 Nov | closed |
| Judges can't build an Unreal project (and the AI screening can't either, Q-017) | Packaged Windows build on a GitHub Release (P4-U8) + demo video; web client, `npm test`, `npm run sim` and `npm run replay` run from the README | 12 Nov | open |
| The Unreal build is online-only: if the hosted server is down, it can't play | Host until 25 Nov and check daily; the video shows the full loop; the web client's practice run works with no server | 11 Nov | open |
| Email sign-in from inside Unreal (browser hand-off) takes too long | Same fallback as the web: a guest account the server makes, kept on the PC; no wallet words | 7 Nov | open |
| Satchel, supplies and ink carry-over add three systems to a full Phase 3 (freeze 8 Nov) | Server first (P3-C5, C6, C8), Unreal panel next; P3-C7 on-chain supplies is the first to drop; interiors are level work and can go on after the freeze | 8 Nov | open |
| The game looks or reads like a copy of Pokémon | Follow "Our own game, not a copy"; review the intro, the buildings and the battle screen against it before the video | 12 Nov | open |
| An asset pack's licence doesn't allow a public repo | CC0 or our own assets only; every pack and its licence listed in the README | ongoing | open |
| Unreal assets fill the Git LFS quota, or two people edit one map | No Starter Content; commit at milestones only; announce or `git lfs lock` before editing a map/Blueprint | ongoing | open |

## Open questions

- [ ] Who takes which seat?
- [ ] Sign-in sender: Resend email codes implemented; configure a verified sender. Custody was decided 7 Nov: encrypted server-held keys for testnet. See `SIGN_IN.md`.
- [ ] Where do we host the server (the Unreal build runs on the judge's PC and needs a reachable server URL)?
- [x] Which Unreal 5.x version do we pin? **5.8** (newest hotfix), with Visual Studio Community 2026.
- [ ] Tracks besides `games`?
- [ ] Daily check-in time?

## Working agreements

- **User instruction, 7 Nov:** sole-developer workflow on local `main` only. Do not push changes unless the user later explicitly requests it. Existing Unreal/Phase 3 milestones were fast-forwarded into local main; origin/main is unchanged. New work remains local for review.
- Main always runs: `npm test`, `npm run typecheck`, `npm run build` pass before merging.
- 15-minute check-in once a day: what merged, what's blocked, is the next gate on track.
- Never commit `.env`, keys or the submission payload. PDFs in `docs/` are git-ignored on purpose.
- Testnet only. Nothing is posted, made public or submitted without the lead's yes.

## Decision log

Newest first. Add a line whenever the plan, the stack or the scope changes.

| Date | Decision | Why |
|---|---|---|
| 9 Nov | Team target: all work finished by 10 Nov, 11 Nov at the latest (submission deadline 14 Nov unchanged). The Ninth Bell (full T-4 scope) and the native Observatory (full T-8 spec, with fast-forward) stay Musts. Hosting: game server on Render (Railway if needed) with a persistent disk; sign-in site on Vercel (T-9) | Lead's choice after the 9 Nov scope check, which flagged high schedule risk and proposed cuts; the lead kept both features. Serverless can't host the server: the world tick runs in-process and data is file-based |
| 8 Nov | Agents' chain balances mirror the simulation: one batched `submit_battle` per tick carries every agent's points; the game mints Ember to an agent just before it pays, up to its simulated balance and at most 200 Ember a day. Agent accounts hold only two function-call keys (items, rite), no full-access key; 6 chain actions per agent per day | Agents need real points and Ember to propose rules and trade on chain (G3), and the chain should match the world players see. The limited keys and caps are the guardrail judges can check |
| 7 Nov | New players get Kessa's starter kit once (3 blank talismans, 1 Ink Salve, server-side); otherwise supplies start at zero and the starter partner (Cinder Grin) keeps saved ink | Binding now uses up a blank and blanks cost Ember, but new accounts have no Ember: without the kit a new player could never bind. Fits the intro, where Kessa hands you a talisman |
| 7 Nov | The Ninth Bell co-op level (P4-U13) is a Must and joins the demo video (now about 3:05). The video needs two clients on LAN | Lead's choice: a coordinated co-op climax shows the town's stakes better than solo play. Schedule risk: the remaining Musts (Satchel, district, interiors, packaging, hosting, video) are unchanged, so some Should work will slip |
| 7 Nov | Story canon: Kessa was once a human binder, and her name is the Ninth Bell's missing seal. In Ending A she gives up her name and memories but keeps her voice, so she stays the Rite's referee. The Ninth Bell takes place in Bone Ward's sealed Bell Quarter, not a new district; its minor curses are Ninefold Shades | Keeps exactly four Wards and keeps the referee who speaks in Lantern Ward consistent with both endings |
| 7 Nov | Continue on local main only; do not push. Defer the listed remaining Phase 3 systems and work on the parallel Phase 4 Unreal story flow | User is the sole developer and explicitly requested local main; the deferred items remain visible rather than being marked complete |
| 7 Nov | User selected encrypted server-held signing for player accounts on testnet; enabled locally and in the example configuration. Player-held signing is outside the chosen demo flow | Keeps Unreal's no-wallet experience; the implementation rejects this option on mainnet |
| 7 Nov | Final game delivery is Unreal Windows. Keep the required Node game backend and browser email handoff; stop new web-game feature work. Observatory becomes an Unreal panel; web sprite/mode tasks are cut | User confirmed that players should receive the Unreal version; backend services support that game rather than a second client |
| 6 Nov | Unreal/browser sign-in uses email codes and a device-secret polling handoff; a local callback listener is unnecessary. Testnet server custody is implemented behind an explicit option, not enabled; the key-custody decision remains open | Gives Unreal an authenticated session without putting it in a URL; isolates the remaining custody choice from the implemented authentication flow |
| 5 Nov | Lantern Ward grows into a district with buildings you can enter (Counting House, Mender's House, Rite Hall, your room), a Satchel, supplies bought with Ember, and ink that carries over between fights; Tide Ward moves to Unreal as a Should after G3; Ash and Bone stay locked | Lead wants the depth of the classic monster-binding games; each building is also an Ember sink or source, so the NEAR economy is what the player walks through |
| 5 Nov | Healing is not free: the Mender charges Ember (price follows Sorrow) or you rest and lose the night | Makes Ember matter and keeps our design apart from the free healing building of other games |
| 5 Nov | Phase 3 pulled forward: G3 and the feature freeze on 8 Nov (was 10 Nov); Phase 4 (look, story) starts 6 Nov alongside it | Leaves 4 days for the Unreal look, story, packaging and video instead of 2 |
| 5 Nov | Story: the player is a newcomer in debt whom Kessa takes in as a binder; three chapters trigger on the world's real events (demo seed 2: bankruptcy day 3, Calamity day 4, rule war day 5), with an objective line in the HUD | The loop worked but the game had no reason to play; the simulation already writes the drama, so the story frames it instead of adding systems |
| 5 Nov | Chain words stay out of the game UI ("Sealed in the Rite's ledger" + seal icon); the explorer link is one key away | Track bonus: "players never notice the chain"; the video still shows the proof |
| 5 Nov | Modes: the Unreal client is online-only, against the hosted server; the web client keeps its offline town as a labelled "Practice run" | Offline Unreal would need the battle engine in C++ or a bundled Node server; the web practice run covers judges with no server |
| 5 Nov | Art for the public repo: CC0 or our own assets only (Quaternius, Kenney, Poly Pizza), each listed in the README; no Fab/Megascans | The repo goes public on 14 Nov and Fab assets can't be redistributed; low-poly CC0 suits the paper-lantern style and the 4 GB GPU |
| 5 Nov | The Unreal project is committed: `.uproject`, `Source`, `Config`, `Content/Grudgebound` through Git LFS; the Top Down template's content comes from each developer's own engine (`setup-project.ps1 -TemplateContent`) | Judges and teammates can see the level and setup; the project no longer lives on one laptop; Epic template content stays out of the public repo |
| 5 Nov | Unreal is the main client (video and players), decided a day early; the web client stays working as the fallback and the path the AI screening can run, without visual polish | Lead's choice after reading the live rules: any engine is allowed, a local demo is fine with a video (Q-012), screening doesn't decide (Q-017) |
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
| 7 Nov | Native Unreal Vow Shrine, authenticated vow stakes/settlement and restrictions; ledger wording; 108 tests; real vow stake/return/burn and a new player's native-session bind/sale/restart/logout verified on testnet. Unreal-only feature delivery confirmed; encrypted testnet player custody selected | | branch `client/P1-U1-unreal-client` |
| 5 Nov | Buildings, Satchel, supplies and ink carry-over planned (P3-C5..C8, P3-U4, P4-U10..U12); Lantern Ward as a district; Tide Ward to Unreal; originality guardrails | | branch `client/P1-U1-unreal-client` |
| 5 Nov | Unreal project committed (Git LFS, template content kept out); decisions on Unreal, story, modes, assets; Phase 3 to 8 Nov and Phase 4 look-and-story tasks | | branch `client/P1-U1-unreal-client` |
| 3 Nov | First Unreal build on UE 5.8.4: project created, plugin compiles (2 fixes), LanternWard level script; walk, encounter, battle and bind work in the editor | | branch `client/P1-U1-unreal-client` |
| 2 Nov | Unreal client plugin (API subsystem, Lantern Ward greybox, tall grass, HUD, encounter, battle screen, Talisman Case), `GET /curses`, Unreal contract test, `setup-project.ps1`, UE 5.8 + VS 2026 pinned; 44 tests | | branch `client/P1-U1-unreal-client` |
| 1 Nov | Server-run battles (`/battle/start`, `/battle/turn`) for the Unreal client; encounter roll and battle log text moved to `shared/`; 40 tests | | |
| 1 Nov | Phase 1 core loop: fight from the Ward, bind, Talisman Case, points and Ember in the HUD; server `POST /battle` replay check and `GET /shop`; contracts compile, 17 contract tests written; Unreal setup guide | | branch `phase-1/core-loop-and-unreal-setup` |
| 1 Nov | Paper-and-ink design system, title screen, React HUD, Rules panel, Vow Shrine; Lantern Ward lighting | | #2 |
| 1 Nov | Battle resolver with tests, BattleScene, battle art; rule template and params in the world summary | | #1 |
| 1 Nov | Concept art replaced by three themed sets | | none |
| 1 Nov | Switched to npm workspaces and Next.js | | none |
| 1 Nov | Project skeleton | | none |
