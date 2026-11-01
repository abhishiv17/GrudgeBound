# Grudgebound: The Rite

A browser creature-binding game where **greed creates monsters** and **players rewrite the rules**.
Built for [NEAR HACKATHON](https://nearhackathon.com) (1-14 November 2026), track: Games With Real Economies.

> **Status: Day 1 of 14.** This is the project skeleton. The economy engine, simulator, AI-agent loop
> and a walkable test Ward work today. Battles, binding, on-chain items and email login are scheduled
> for Days 3-10. The table under [What works today](#what-works-today) is kept up to date.

## The idea

- Every greedy trade, scam and bankruptcy in the harbor town of Hollowmere feeds a **Grudge** (Rage, Sorrow, Dread).
- Grudge spawns **curses**. Players exorcise them or **bind** them into talismans they own.
- Points earned that way can be spent to **add a new rule to the game for everyone**, from 12 rule templates.
  An AI referee (Kessa) checks every proposal; the Rite contract on NEAR enforces it.
- Merchants, guild masters and five rival participants are **AI agents** with their own NEAR wallets.
- Players sign in with email: no seed phrases, no wallet popups, no gas prompts.

Full design: [`docs/Grudgebound_Plan.pdf`](docs/Grudgebound_Plan.pdf). Art direction: [`docs/ART_PROMPTS.md`](docs/ART_PROMPTS.md).

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
| `npm run dev` then open `http://localhost:3000` | Lantern Ward. Walk with arrow keys / WASD. Walk through tall grass: a curse appears, chosen from the Ward's live Grudge. Kessa announces new rules and bankruptcies at the bottom. Works without the server too (the town then runs in the browser). |
| `npm run agent:demo` | Each AI rival decides its next move and the Talisman Broker explains a price. Uses NEAR AI Cloud if `.env` has a key, otherwise the scripted fallback. |
| `npm test` | Unit tests for the type triangle, Grudge, pricing, rules, the Arbiter and engine determinism. |
| `npm run build` | Production build of the Next.js client. |

## What works today

| Part | Status |
|---|---|
| Shared game data: 12 curses, type triangle, 12 rule templates, 6 vow templates | Done |
| `shared/src/pricing.ts`: every price formula in one file, with plain-English reasons | Done |
| Tick engine: Grudge, spawns, merchants, bankruptcies, AI rival policies, rules with costs and effects | First version, being tuned |
| Fast-forward simulator (`npm run sim`) | Done (text output; Observatory charts planned) |
| Game server with `/state` and `/events` | Skeleton |
| Agent runtime: observe, legal options, LLM choice, guardrail, scripted fallback | Done; not yet acting through NEAR wallets |
| Game client (Next.js + Phaser): walkable Ward, tall-grass encounters, Grudge HUD, Kessa banner | Placeholder art |
| Battles and binding | Planned (Days 3-6) |
| Contracts: `items` (NEP-245 shape) and `rite` (points, rules, vows) | Written, **not yet compiled or deployed** |
| Email login, relayer (meta transactions), testnet deploy | Planned (Days 5-6) |
| Observatory page | Planned (Day 11) |

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
shared/      game data + formulas + tick engine (used by everything below)
sim/         fast-forward simulator (CLI)
server/      game server (real-time ticks, HTTP API)
agents/      AI agents: runtime, NEAR AI client, merchant tooltips
client/      Next.js app hosting the Phaser 3 game (app/ pages, components/, game/ scenes)
contracts/   Rust contracts: items (NEP-245), rite (points, rules, vows)
docs/        plan PDF, hackathon rules, art direction and concept art
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
