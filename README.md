# Grudgebound: The Rite

A Windows game (Unreal Engine 5) set in Hollowmere, a harbour town where **greed creates monsters** and
**players vote in the rules**. Every scam, greedy trade and bankruptcy in the town's simulated economy
raises a Grudge; the Grudge spawns curses; players fight and bind those curses into talismans they own,
then spend the points they earn to add rules to the game for everyone.

Built at [NEAR HACKATHON](https://nearhackathon.com) (1-14 November 2026), track **Games With Real Economies**.

<!-- FILL before submission (P4-C1, P4-U8, P4-G1): replace each "added at release" with the real link. -->
| | |
|---|---|
| Demo video | added at release |
| Windows build (GitHub Release) | added at release |
| Hosted game server / sign-in page | added at release |
| Contracts (NEAR testnet) | [`items.grudgebound.testnet`](https://testnet.nearblocks.io/address/items.grudgebound.testnet) · [`rite.grudgebound.testnet`](https://testnet.nearblocks.io/address/rite.grudgebound.testnet) |

> **Build status (9 Nov):** steps marked ⏳ below are being finished for the final build (11 Nov).
> Everything else works today. Testnet only.

## The problem

Game economies are usually patched by developers: prices, rules and drop rates change when the studio decides.
Grudgebound tries the opposite: the economy runs by itself, AI merchants and rivals trade in it with their own
NEAR accounts, and players change the rules through a contract nobody (including us) can quietly override.
Players never see a wallet, a seed phrase or a gas prompt.

## Play it (Windows)

1. Download `Grudgebound-Win64-*.zip` from the Release (link above), unzip, run `GrudgeBound.exe`. ⏳
2. Click **Sign in**: your browser opens the sign-in page. Enter your email, type the code you receive,
   return to the game. A NEAR testnet account is created for you in the background.

| Do this in the game | You'll see |
|---|---|
| First entry | A four-panel paper intro: you arrive in Hollowmere in debt, and Kessa, the Rite's referee, offers a way out. The HUD then shows your chapter and objective (**I** replays the intro). |
| Walk (click to move) into the dark tall grass | A curse appears, chosen by the Ward's live Grudge (Rage, Sorrow or Dread). |
| **Fight!** → attack / ward → **Seal** when it's weak | A server-run battle. A successful seal mints the talisman to your NEAR account; the HUD shows your new points. Press **L** to open the transaction on the explorer. |
| **B** (Satchel) | Your carried talismans with their ink (HP), supplies (blank talismans, Ink Salves), keepsakes and your ledger (Ember, points, debt). Binding uses up a blank talisman. Ink stays low after a fight. |
| Enter the **Counting House** ⏳ | The AI Broker's prices, each with a one-line reason (scarcity, demand, fear). Buy and sell talismans and supplies for Ember. Agents appear at the counter when they trade. |
| Enter the **Mender's House** / your **room** ⏳ | Restore ink for Ember (price rises with the Ward's Sorrow), or rest for free until morning. |
| Enter the **Rite Hall**, propose a rule ⏳ (today: **R**) | Kessa checks the proposal; a contradiction is rejected with her reason, a valid rule is written to the `rite` contract and applies to every player and agent (prices or curfews change). |
| **V** at the Rite Hall's shrine | Swear a vow and stake points on it. The `rite` contract returns the stake with a bonus if you keep it, or burns it if you break it. |
| Open the **Observatory** ⏳ | Grudge, prices and rules over time, including rules added by AI agents, plus a fast-forward preview of the next days. |
| **The Ninth Bell** (2-4 players, LAN) ⏳ | A co-op story level: shared clues, two players holding the seal anchors together, a boss fight with revives, and a party vote between two endings. Not connected to the economy. |

Run against your own server instead of the hosted one: start the game with `-GbServer=http://localhost:8787`.

## Check it without Windows or Unreal

Requirements: Node 20+ and npm. `npm install` first.

| Do this | You'll see |
|---|---|
| `npm test` | Unit tests: type triangle, Grudge, pricing, rules, the Arbiter, engine determinism, battles, replay, sign-in, vows, Satchel. |
| `npm run sim` | A 7-day fast-forward of the town in about 0.1 s: Grudge per Ward, merchant Ember and prices, points, and every rule added by AI rivals. Same seed, same result (`--seed 7 --events` for another town). |
| `npm run server`, then open `http://localhost:8787/state` | The live world as JSON, one in-game hour per real minute. |
| `http://localhost:8787/shop?merchant=broker` | The Broker's prices with reasons. With a NEAR AI key in `.env`, the merchant agent writes the reason; the number always comes from the formula in `shared/src/pricing.ts`. |
| `npm run replay <seed>` | Replays a recorded battle from its seed and checks the result against what was written on NEAR. |
| `npm run agent:demo` | Each AI rival chooses its next move from the legal options; the Broker explains a price. |
| `cd contracts && cargo test` | Contract unit tests (Linux, macOS, WSL2, or Docker on Windows; see `contracts/README.md`). |

Full local setup (server, sign-in page, Unreal editor): [`docs/UNREAL_SETUP.md`](docs/UNREAL_SETUP.md) and
[`docs/SIGN_IN.md`](docs/SIGN_IN.md). Hosting: [`docs/HOSTING.md`](docs/HOSTING.md) ⏳.

## How NEAR is used

| NEAR feature | What it does here | Check it |
|---|---|---|
| Our own contract `items` (NEP-245 multi-token) | One contract holds the Ember coin and every talisman. Each talisman keeps a purse of Ember it has earned and its win record; transfers move the purse with it. | `talismans_of`, `mt_balance_of` on `items.grudgebound.testnet` |
| Our own contract `rite` | Points, rules (cost, Kessa's decision, repeal) and vows (stakes kept or burned). A rule in force can only be repealed through the contract. | `get_active_rules`, `get_points`, `get_vows` on `rite.grudgebound.testnet` |
| Named sub-accounts for players | Email sign-in creates a `<name>.grudgebound.testnet` account per player. | the account named in your ledger (**L**) |
| Meta transactions (NEP-366) + relayer | Players never pay gas or approve a popup; the relayer pays. | `relayer.grudgebound.testnet` transactions |
| Sub-accounts + function-call keys for AI agents ⏳ | Each merchant and rival has its own account holding only function-call keys for `items` and `rite`, with daily caps on actions and Ember. | the agent accounts listed in `docs/HOSTING.md` |
| NEAR AI Cloud | Agents' decisions and the merchants' explanations. The AI only chooses among legal, formula-priced options; the guardrail rejects anything else. | `agents/src` |

Battles are resolved on the game server for speed; each result is written with its seed and a log hash so
anyone can replay it (`npm run replay`). Ownership, points, rules and vows are decided by the contracts.

## Honest limits

- **Testnet only.** Nothing here has monetary value.
- **Server-held player keys.** For this demo, each player's key is stored encrypted on the game server and
  the server signs for them (chosen for a wallet-free first session). This is custodial and would not be
  used on mainnet.
- **Supplies are server-side.** Blank talismans and Ink Salves are kept by the server; the Ember spent on them
  is on chain.
- The web client in `client/` is an earlier browser version; the game is the Unreal client. The web part that
  is still used is the email sign-in page.

## Repository layout

```
unreal/      Unreal Engine 5.8 game (GrudgeboundClient plugin: API, Ward, battles, panels, story, co-op)
server/      game server: world ticks, HTTP API, sign-in, battles, Satchel, vows, chain writes
shared/      game data, formulas (pricing.ts), tick engine, battle resolver
agents/      AI agents: runtime, guardrail, NEAR AI client, merchant explanations
contracts/   Rust contracts: items (NEP-245), rite (points, rules, vows)
sim/         fast-forward simulator (CLI)
client/      Next.js: email sign-in page (and the earlier browser version of the game)
docs/        design, story, setup, art direction
```

## Configuration

Copy `.env.example` to `.env`. **Never commit `.env` or any key.** Sign-in and testnet signing:
[`docs/SIGN_IN.md`](docs/SIGN_IN.md).

## Disclosures (hackathon rule Q-005)

<!-- OWNER: confirm the AI-code wording and the Q-015 line before submission. -->
- **Libraries and engines:** Unreal Engine 5.8, Next.js, React, Phaser 3, TypeScript, tsx, near-api-js,
  near-sdk (Rust), Resend (email delivery), NEAR AI Cloud. Tests use Node's built-in test runner and
  Unreal's automation framework.
- **AI-written code:** a large part of the code was written with AI coding assistants and reviewed, tested
  and directed by the team.
- **AI-generated art:** the concept art in `docs/art/concepts/` was made with AI image tools. In-game
  intro illustrations are drawn procedurally in code.
- **Third-party assets:** CC0 models and textures and OFL fonts, each listed with author and link in
  [`docs/ASSETS.md`](docs/ASSETS.md) ⏳.
- **Inspiration:** creature-collecting RPGs and Japanese curse folklore. All names, creatures, characters,
  story and art direction are original.
- **Pre-hackathon code:** none.

## Team

<!-- OWNER: names are yours to fill in. -->
| Seat | Owns | Who |
|---|---|---|
| Chain & Server | contracts, relayer, server, deploy | _TBD_ |
| Game Client | Unreal client, battles, UI, art, demo video | _TBD_ |
| AI & Economy | pricing, Grudge tuning, agents, simulator, Observatory | _TBD_ |

## License

[MIT](LICENSE)
