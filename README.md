# Grudgebound: The Rite

A browser creature-binding game where **greed creates monsters** and **players rewrite the rules**.
Built for [NEAR HACKATHON](https://nearhackathon.com) (1-14 November 2026), track: Games With Real Economies.

> **Status: Phase 1 of 5 (core loop).** You can walk Lantern Ward, fight curses, bind them into talismans
> and earn points; the server replays every reported battle before it counts. Testnet, email login and
> AI agents with wallets come next. The table under [What works today](#what-works-today) is kept up to date.

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
| With `npm run server` running, fight and bind a curse, then open `http://localhost:8787/events?since=-1` | The town log shows "A newcomer bound ... (+N points)": the server replayed your battle from its seed and actions before counting it. |
| With `npm run server` running: `curl -X POST localhost:8787/battle/start -H "Content-Type: application/json" -d "{\"ward\":\"lantern\"}"`, then `POST /battle/turn` with the `battleId` and `{"kind":"attack"}` | A battle run entirely by the server, turn by turn, with log lines; at the end the points and the seed to replay it. This is what the Unreal client uses (`docs/UNREAL_SETUP.md`). |
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
| Game server: `/state`, `/events`, `/shop` (merchant price reasons), `/curses`, `POST /battle` (replays a battle before counting it), `/battle/start` + `/battle/turn` (server-run battles) | Working, in memory |
| Agent runtime: observe, legal options, LLM choice, guardrail, scripted fallback | Done; not yet acting through NEAR wallets |
| Game client (Next.js + Phaser): walkable Ward, tall-grass encounters, Grudge HUD, Kessa banner, Rules panel, Vow Shrine | Working; first art pass |
| Battles and binding: encounter, turn-based battle, seal, talisman case, partner choice, points | Working; progress kept in the browser until sign-in |
| Unreal Engine 5.8 client (`unreal/Plugins/GrudgeboundClient`): server API, Lantern Ward greybox, tall-grass encounters, HUD, battle screen, Talisman Case | Code written; first build waits for the engine install (`docs/UNREAL_SETUP.md`) |
| Contracts: `items` (NEP-245 shape) and `rite` (points, rules, vows) | Compile to wasm, unit tests written; **not deployed yet** |
| Testnet deploy, relayer (meta transactions), binding mints a token | Planned (Phase 2, 5-7 Nov) |
| Email login, agents with their own accounts, Observatory page | Planned (Phase 3, 7-10 Nov) |

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

Copy `.env.example` to `.env`. Only needed for real AI agents (NEAR AI Cloud) and, later, testnet.
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
