# Grudgebound: The Rite

A creature-binding game where **greed creates monsters** and **players rewrite the rules**. Unreal is the main client; the browser client remains available.
Built for [NEAR HACKATHON](https://nearhackathon.com) (1-14 November 2026), track: Games With Real Economies.

> **Status: Phase 3 of 5 (accounts, agents, live world).** Phase 2's testnet gate passed on 5 Nov.
> Email sign-in and Unreal sessions use encrypted server-held signing on testnet (selected 7 Nov).
> A new identity's bind, sale and session recovery passed on testnet; real email delivery and remaining
> editor acceptance are open. Setup: [`docs/SIGN_IN.md`](docs/SIGN_IN.md).
> **7 Nov checkpoint:** 108 tests, type checks and the Unreal editor build pass; native Vow Shrine stakes,
> kept return and early-release burn verified on testnet. Final game delivery is Unreal Windows; the Node
> backend and browser email handoff support it. New web-game feature work is stopped.
> Remaining work and the exact restart point are in [`docs/BUILD_PLAN.md`](docs/BUILD_PLAN.md#recovery-checkpoint--7-nov).

## The idea

- Every greedy trade, scam and bankruptcy in the harbor town of Hollowmere feeds a **Grudge** (Rage, Sorrow, Dread).
- Grudge spawns **curses**. Players exorcise them or **bind** them into talismans they own.
- Points earned that way can be spent to **add a new rule to the game for everyone**, from 12 rule templates.
  An AI referee (Kessa) checks every proposal; the Rite contract on NEAR enforces it.
- Merchants, guild masters and five rival participants are **AI agents** with their own NEAR wallets.
- Players sign in with email: no seed phrases, no wallet popups, no gas prompts.

Build plan and progress: [`docs/BUILD_PLAN.md`](docs/BUILD_PLAN.md). Art direction: [`docs/ART_PROMPTS.md`](docs/ART_PROMPTS.md). Rules and judging criteria: [`docs/HACKATHON.md`](docs/HACKATHON.md).

## Try it

Requirements: Node 20+ and npm.

```sh
npm install
```

| Do this | You'll see |
|---|---|
| `npm run sim` | A 7-day fast-forward of the town printed in ~0.1 s: Grudge per Ward, merchant Ember and prices, the points table, and every rule added by AI rivals and simulated players. |
| `npm run sim -- --days 3 --seed 7 --events` | A different town (seed 7) with every notable event listed. Same seed always gives the same result. |
| `npm run server` then open `http://localhost:8787/state` | The live world as JSON, advancing one in-game hour per real minute (set `TICK_MS=3000` for faster ticks). |
| `npm run dev` then open `http://localhost:3000` | Lantern Ward. Walk with arrow keys / WASD. Walk through tall grass: a curse appears, chosen from the Ward's live Grudge. Press **Fight!**: attack, ward, then **Seal** a weakened curse to bind it. Back in the Ward, Kessa announces the result, your points go up and the talisman appears in your case (**T**), where you can make it your partner. Works without the server too (the town then runs in the browser). |
| Sign in from Unreal or `http://localhost:3000/sign-in`, then fight and bind a curse | A server-run battle credited to your authenticated player. Testnet signing needs the configuration in `docs/SIGN_IN.md`. |
| Call `/battle/start` and `/battle/turn` with your sign-in Bearer token | A battle run entirely by the server, turn by turn. Missing sessions and turns for another player's battle are refused. |
| `http://localhost:8787/shop?merchant=broker` (server running) | The Talisman Broker's prices, each with a one-line reason. With a NEAR AI key in `.env`, the reason is written by the merchant agent; the number always comes from the formula. |
| `npm run agent:demo` | Each AI rival decides its next move and the Talisman Broker explains a price. Uses NEAR AI Cloud if `.env` has a key, otherwise the scripted fallback. |
| `npm test` | Unit tests for the type triangle, Grudge, pricing, rules, the Arbiter, engine determinism, battles and replay. |
| `cd contracts && cargo test` | Unit tests for both contracts (Linux, macOS, WSL2, or Docker on Windows; see `contracts/README.md`). |
| `npm run build` | Production build of the Next.js client. |

## What works today

| Part | Status |
|---|---|
| Shared game data: 12 curses, type triangle, 12 rule templates, 6 vow templates | Done |
| `shared/src/pricing.ts`: every price formula in one file, with plain-English reasons | Done |
| Tick engine: Grudge, spawns, merchants, bankruptcies, AI rival policies, rules with costs and effects | First version, being tuned |
| Fast-forward simulator (`npm run sim`) | Done (text output; Observatory charts planned) |
| Game server: `/state`, `/events`, `/shop`, `/curses`, `/battle/start` + `/battle/turn` | Working; online player actions require a session. Legacy `POST /battle` removed. |
| Agent runtime: observe, legal options, LLM choice, guardrail, scripted fallback | Done; not yet acting through NEAR wallets |
| Game client (Next.js + Phaser): walkable Ward, tall-grass encounters, Grudge HUD, Kessa banner, Rules panel, Vow Shrine | Working; first art pass |
| Battles and binding: encounter, turn-based battle, seal, talisman case, partner choice, points | Working; Unreal reads ownership and points from the chain. Local saves are separated per signed-in player. |
| Unreal Engine 5.8 client (`unreal/Plugins/GrudgeboundClient`): server API, Lantern Ward greybox, tall-grass encounters, HUD, battle screen, Talisman Case | Builds and plays on UE 5.8.4: walk, encounter, battle, bind (`docs/UNREAL_SETUP.md`) |
| Contracts: `items` (NEP-245 shape) and `rite` (points, rules, vows) | Deployed to testnet; binding, trading and rules verified in Phase 2 |
| Testnet deploy, relayer (meta transactions), binding mints a token | G2 passed 5 Nov; remaining Rite-panel editor check recorded in the build plan |
| Email login and Unreal sessions | Encrypted testnet signing selected; new identity provisioning/bind/sale/restart/logout verified through the native HTTP flow. Real-mail and editor acceptance remain (`docs/SIGN_IN.md`). |
| Native Unreal Vow Shrine (V) | Built; real testnet stake/return/burn verified. Native play-through and power modifiers remain (`docs/VOWS.md`). |
| Native arrival story and chapter objective (I to replay) | Local Phase 4 implementation: four illustrated panels, per-player story history and real-event chapters. Editor build and 5 native checks pass; previews inspected. Script review and Play-mode acceptance remain (`docs/STORY.md`). |
| The Ninth Bell, 2–4 player co-op | Local source prototype: shared clues, ritual, boss, revives and ending vote. Editor build and 3 co-op logic checks pass; map generation and host/client play-through remain open (`docs/THE_NINTH_BELL.md`). |
| Satchel, supplies and ink carry-over (server) | Built: five carried talismans, Broker supplies shared with agents, ink that stays low after a fight, the Mender (Ember) and free rest. New players start with Kessa's kit (3 blank talismans, 1 Ink Salve). Native Satchel panel and live testnet checks remain |
| Agent accounts, native Unreal Observatory, native Satchel panel | Remaining Phase 3 work; feature freeze 8 Nov |

## How NEAR is used (planned, being built)

| NEAR feature | Use in Grudgebound |
|---|---|
| NEP-245 multi token | One contract for the Ember coin and every talisman. Talismans carry a purse of earned Ember. |
| Rite contract | Points, rules (cost, Arbiter decision, repeal), vows (stakes kept or burned). Nobody can change a rule in force. |
| Sub-accounts + function-call access keys | Each AI agent has its own wallet, limited to our contracts and a spend cap. |
| Meta transactions (NEP-366) + relayer | Players never pay gas or see a wallet popup. |
| NEAR AI Cloud | Agent decisions and dialogue. Numbers always come from formulas; the AI picks between legal options. |

Battles are resolved by the game server for speed; each result carries its seed and a log hash so it can be replayed.
Everything that decides fairness (ownership, points, rules, vows) is enforced by the contracts.

## Repository layout

```
shared/      game data + formulas + tick engine + battle resolver (used by everything below)
sim/         fast-forward simulator (CLI)
server/      game server (real-time ticks, HTTP API)
agents/      AI agents: runtime, NEAR AI client, merchant tooltips
unreal/      Unreal Engine 5 game client (from 2 Nov; setup in docs/UNREAL_SETUP.md)
client/      Next.js + Phaser 3 web client (working fallback; app/ pages, components/, game/ scenes)
contracts/   Rust contracts: items (NEP-245), rite (points, rules, vows)
docs/        hackathon rules, art direction and concept art
```

## Configuration

Copy `.env.example` to `.env` for the game server's email delivery, testnet signing and NEAR AI configuration.
For console-code development and Unreal sign-in, follow [`docs/SIGN_IN.md`](docs/SIGN_IN.md).
**Never commit `.env` or any key.**

## Disclosures (hackathon rule Q-005)

- **Libraries:** Next.js, React, Phaser 3, TypeScript, tsx, near-sdk (Rust). Tests use Node's built-in test runner.
- **AI-generated code:** the initial skeleton was generated with Claude Code (Anthropic) and reviewed by the team.
- **AI-generated art:** concept art in `docs/art/concepts/` was made with AI image tools. In-game art: to be listed here.
- **Inspiration:** classic creature-collecting RPGs and Japanese curse folklore. All names, creatures, characters and art are original.
- **Pre-hackathon code:** none. All code was written during the hackathon window.

## Team

| Seat | Owns | Who |
|---|---|---|
| Chain & Server | contracts, relayer, server, deploy | _TBD_ |
| Game Client | Phaser world, battles, UI, art, demo video | _TBD_ |
| AI & Economy | pricing, Grudge tuning, agents, simulator, Observatory, README | _TBD_ |

## License

[MIT](LICENSE)
