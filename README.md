# Grudgebound: The Rite

A Windows game (Unreal Engine 5) set in Hollowmere, a harbour town where **greed creates monsters** and
**players vote in the rules**. Every scam, greedy trade and bankruptcy in the town's simulated economy
raises a Grudge; the Grudge spawns curses; players fight and bind those curses into talismans they own,
then spend the points they earn to add rules to the game for everyone.

Built at [NEAR HACKATHON](https://nearhackathon.com) (1-14 November 2026), track **Games With Real Economies**.

<!-- OWNER: add the demo video link, and update the build link at each release. -->
| | |
|---|---|
| Demo video | added with the demo post |
| Windows build (GitHub Release) | [Grudgebound for Windows (12 Nov)](https://github.com/abhishiv17/GrudgeBound/releases/tag/build-2026-11-12), 344 MB zip |
| Hosted game server | `https://grudgebound-server.onrender.com` ([health](https://grudgebound-server.onrender.com/health), [live town](https://grudgebound-server.onrender.com/state), [AI agents' accounts](https://grudgebound-server.onrender.com/agents)). Free hosting: the first request after a quiet spell can take up to a minute. |
| Contracts (NEAR testnet) | [`items.grudgebound.testnet`](https://testnet.nearblocks.io/address/items.grudgebound.testnet) · [`rite.grudgebound.testnet`](https://testnet.nearblocks.io/address/rite.grudgebound.testnet) |

> **Build status (13 Nov).** The published build is from 12 Nov. The rows marked **(next build)** below are in
> this repository and pass its tests, but are not in that zip yet. Testnet only.

## The problem

Game economies are usually patched by developers: prices, rules and drop rates change when the studio decides.
Grudgebound tries the opposite: the economy runs by itself, five AI agents trade in it with their own
NEAR accounts, and players change the rules through the `rite` contract, where every rule, its cost, its author
and its repeal is recorded for anyone to read. (What the game server still decides is listed under
[Honest limits](#honest-limits).) Players never see a wallet, a seed phrase or a gas prompt.

## Play it (Windows)

Needs Windows 10/11 64-bit and a DirectX 12 GPU with 4 GB of video memory.

1. Download `Grudgebound-Win64-*.zip` from the Release (link above), unzip, run `GrudgeBound.exe`. The exe is not
   code-signed, so Windows may show "Windows protected your PC": click **More info**, then **Run anyway**.
2. Click **Enter Lantern Ward** on the title screen: your browser opens the sign-in page. Enter your email, type the code you
   receive, return to the game. A NEAR testnet account is created for you in the background.

| Do this in the game | You'll see |
|---|---|
| First entry | A four-panel paper intro: you arrive in Hollowmere in debt, and Kessa, the Rite's referee, offers a way out. The HUD then shows your chapter and objective (**I** replays the intro). |
| Arrive on the pier **(next build)** | Kessa meets you: you choose a name, a look, how you are addressed, and one of three first partners. The choice is saved to your account and cannot be changed. |
| Walk (WASD, or click to move) into the dark tall grass | A curse appears, chosen by the Ward's live Grudge (Rage, Sorrow or Dread). |
| **Fight!** → attack / ward → **Seal** when it's weak | A server-run battle. A successful seal mints the talisman to your NEAR account; the HUD shows your new points. Press **L** to open the transaction on the explorer. |
| **B** (Satchel) | Your carried talismans with their ink (HP), supplies (blank talismans, Ink Salves), keepsakes and your ledger (Ember, points, debt). Binding uses up a blank talisman. Ink stays low after a fight. |
| Satchel > **Journal**, **Gear** **(next build)** | Tasks from townsfolk with their next step, three charms that change battle numbers, lore pages opened by your progress. A gold beacon in the Ward marks the tracked task. |
| Satchel > **Ledger** > **Open my page** **(next build)** | Your own NEAR testnet account on the explorer: the talismans sealed under your name and the wins written on them. |
| Win fights with one partner **(next build)** | The partner gains levels (1-10). For a minted talisman each win is also written on the token (`record_win` on `items`). |
| Meet **Dace Marrow** on the pier, at the ferry, on Tide's quay **(next build)** | A rival who arrived on your boat: three fights against a mirror of your own partner. The ferry to Tide Ward opens after the second. |
| Enter the **Counting House** | The AI Broker's prices, each with a one-line reason (scarcity, demand, fear). Buy and sell talismans and supplies for Ember. Agents appear at the counter when they trade. |
| Enter the **Mender's House** / your **room** | Restore ink for Ember (price rises with the Ward's Sorrow), or rest for free until morning. |
| Enter the **Rite Hall**, press **R** | Kessa checks your rule proposal; a contradiction is rejected with her reason, a valid rule is written to the `rite` contract and applies to every player and agent (prices or curfews change). |
| **V** at the Rite Hall's shrine | Swear a vow and stake points on it. The `rite` contract returns the stake with a bonus if you keep it, or burns it if you break it. |
| Take the ferry at the southern pier | Tide Ward: docks, its own curses and townsfolk. Walk up to anyone and press **E** to talk. |
| **O** (Observatory) | Grudge, prices and rules over time, including rules added by AI agents, plus a fast-forward preview of the next seven days. |
| Observatory > **Agents** **(next build)** | The five AI agents: each one's own NEAR account, its points and Ember as the contracts report them, and its latest actions with a button that opens the transaction. An agent's Ember usually reads 0: the game mints exactly what a purchase needs and the agent pays it in the same step. |
| **The Ninth Bell** (2-4 players, LAN) | A co-op story level: shared clues, two players holding the seal anchors together, a boss fight with revives, and a party vote between two endings. Not connected to the economy. How to host and join: [`docs/UNREAL_SETUP.md`](docs/UNREAL_SETUP.md), section 7. |

Run against your own server instead of the hosted one: start the game with `-GbServer=http://127.0.0.1:8787`.

## Check it without Windows or Unreal

Requirements: Node 20.9+ and npm (checked on Node 25). Clone, then `npm install` in the repository folder. No `.env` file, key or
NEAR account is needed for any row in this table.

| Do this | You'll see |
|---|---|
| `npm test` | 169 tests pass: type triangle, Grudge, pricing, rules, the Arbiter, engine determinism, battles, replay, sign-in, vows, Satchel, rival, levels, agents' accounts. |
| `npm run typecheck` | No errors in any workspace. |
| `npm run sim` | A 7-day fast-forward of the town, printed in a couple of seconds: Grudge per Ward, merchant Ember and prices, points, and every rule added by AI rivals. Same seed, same result (`npm run sim -- --seed 7 --events` for another town). |
| `npm run server`, then open `http://localhost:8787/state` | The live world as JSON, one in-game hour per real minute. Without NEAR keys the server says the chain is off and runs the same game off-chain. |
| `http://localhost:8787/shop?merchant=broker` | The Broker's prices, each with its reason. With a NEAR AI key in `.env`, the merchant agent writes the reason; the number always comes from the formula in `shared/src/pricing.ts`. |
| `http://localhost:8787/observatory?mode=preview` | The seven-day preview the Observatory panel draws. |
| `npm run agent:demo` | With a NEAR AI key, the LLM chooses each AI agent's next move from the legal options, and the Broker explains a price. Without a key the scripted policy chooses. |
| `cd contracts && cargo test` | 25 contract unit tests pass: 15 for `items`, 10 for `rite` (needs Rust; toolchain notes: `contracts/README.md`). |

Full local setup (server, sign-in page, Unreal editor): [`docs/UNREAL_SETUP.md`](docs/UNREAL_SETUP.md) and
[`docs/SIGN_IN.md`](docs/SIGN_IN.md). Hosting: [`docs/HOSTING.md`](docs/HOSTING.md).

## How NEAR is used

| NEAR feature | What it does here | Check it |
|---|---|---|
| Our own contract `items` (multi-token, shaped on NEP-245) | One contract holds the Ember coin and every talisman. Each talisman keeps a purse of Ember it has earned and its win record; transfers move the purse with it. Partial NEP-245: balances and transfers between accounts work; `mt_transfer_call`, batch transfers and approvals are not implemented. | `talisman`, `talismans_of`, `mt_balance_of` on `items.grudgebound.testnet` |
| Our own contract `rite` | Points, rules (cost, Kessa's decision, repeal) and vows (stakes kept or burned). A rule in force can only be repealed through the contract. | `get_active_rules`, `get_points`, `get_vows` on `rite.grudgebound.testnet` |
| Named sub-accounts for players | Email sign-in creates a `<name>.grudgebound.testnet` account per player. | the account your **L** key opens |
| Meta transactions (NEP-366) + relayer | Players and agents never pay gas or approve a popup; the relayer sends their signed actions and pays. | `relayer.grudgebound.testnet` transactions |
| Sub-accounts + function-call keys for AI agents | Each of the five agents has its own account holding two function-call keys and no full-access key: one may only call `pay_minter` on `items`, the other only `propose_rule` on `rite`. The server also caps each agent's Ember and actions per in-game day. | `agent-fen`, `agent-vane`, `agent-silk`, `agent-rook-ash`, `agent-pell` under `grudgebound.testnet` |
| NEAR AI Cloud | On the hosted server: the merchants' one-line price explanations (the price itself always comes from the formula). The LLM agent runtime (`npm run agent:demo`) picks the agents' moves among legal, formula-priced options, and the guardrail rejects anything else; the hosted world runs the same five agents on the scripted policy. | `agents/src`, `server/src/index.ts` |

Battles are resolved on the game server for speed; each result is written with its seed and a log hash so
anyone can replay it (`npm run replay`, on a server that holds the battle archive). Ownership, points, rules
and vows are decided by the contracts.

### Proof you can open (testnet, read on 13 November 2026)

Nothing to install for the links. The two commands use [near-cli-rs](https://github.com/near/near-cli-rs).

| Claim | Open or run | What it showed |
|---|---|---|
| The two contracts are deployed | [`items`](https://testnet.nearblocks.io/address/items.grudgebound.testnet) · [`rite`](https://testnet.nearblocks.io/address/rite.grudgebound.testnet) | Both accounts hold contract code. |
| A talisman is a token with an owner | [`tal-11` owner and metadata](https://testnet.nearblocks.io/address/items.grudgebound.testnet) via `near contract call-function as-read-only items.grudgebound.testnet talisman json-args '{"token_id":"tal-11"}' network-config testnet now` | Owner `guest.grudgebound.testnet`, species `cinder-grin`, emotion `rage`, with `level`, `wins` and a `purse`. |
| An AI agent has its own account and can do only two things with it | [`agent-fen.grudgebound.testnet`](https://testnet.nearblocks.io/address/agent-fen.grudgebound.testnet) or `near account list-keys agent-fen.grudgebound.testnet network-config testnet now` | Two keys: "only do `pay_minter` on `items.grudgebound.testnet`" and "only do `propose_rule` on `rite.grudgebound.testnet`". No full-access key. |
| An agent pays for what it buys, and the relayer pays the gas | [transaction `5LJLxF…`](https://testnet.nearblocks.io/txns/5LJLxFXVsPQDYNFg7gX2WXqAAq9ZHRaYETrtJQDLHDQG) | Old Mother Fen buying a Rage talisman from the Broker for 11 Ember: signed by `relayer.grudgebound.testnet` on behalf of `agent-fen`, successful. |
| Rules in force live in the contract, and agents write some of them | `near contract call-function as-read-only rite.grudgebound.testnet get_active_rules json-args '{}' network-config testnet now` | Eight active rules. Rule 0 is by a player account; rule 10 is by `agent-silk.grudgebound.testnet` and cost it 225 points. |
| The agents are trading now | [`/agents` on the hosted server](https://grudgebound-server.onrender.com/agents) | Five agents, each with its account, points, Ember and its latest actions with transaction links. |

## Honest limits

- **Testnet only.** Nothing here has monetary value.
- **Server-held player keys.** For this demo, each player's key is stored encrypted on the game server and
  the server signs for them (chosen for a wallet-free first session). This is custodial and would not be
  used on mainnet.
- **The game server is trusted for game results.** It reports battle points to `rite`, and its arbiter (Kessa)
  accepts or rejects rule proposals. The contracts record and enforce what follows: points balances, rule costs,
  rules in force and their repeal, vow stakes kept or burned, and talisman ownership. They do not re-run battles.
  The team holds the contract accounts' keys and can upgrade both contracts on testnet.
- **Supplies, gear and partner levels are server-side.** Blank talismans, Ink Salves, charms and a partner's
  level are kept by the server; the Ember spent on supplies is on chain, and a minted partner's wins are
  written on its token.
- **The first partner is not a token.** The partner Kessa gives you is kept by the server. The contract method
  that would mint it (`mint_starter`) is written and tested but not deployed.
- **The shared world restarts with the server.** Grudge, prices and the clock start again from day 1 of the
  seed when the game server restarts; accounts, talismans, points, rules and vows are on chain and stay.
- The web client in `client/` is an earlier browser version; the game is the Unreal client. The web part that
  is still used is the email sign-in page.

## Repository layout

```
unreal/      Unreal Engine 5.8 game (GrudgeboundClient plugin: API, Wards, battles, panels, story, co-op)
server/      game server: world ticks, HTTP API, sign-in, battles, Satchel, rival, vows, chain writes
shared/      game data, formulas (pricing.ts), tick engine, battle resolver, levels, equipment
agents/      AI agents: runtime, guardrail, NEAR AI client, merchant explanations
contracts/   Rust contracts: items (multi-token, NEP-245 shape), rite (points, rules, vows)
sim/         fast-forward simulator (CLI)
client/      Next.js: email sign-in page (and the earlier browser version of the game)
docs/        design, story, setup, hosting, art direction
```

## Configuration

Copy `.env.example` to `.env`. **Never commit `.env` or any key.** Sign-in and testnet signing:
[`docs/SIGN_IN.md`](docs/SIGN_IN.md).

## Disclosures (hackathon rule Q-005)

<!-- OWNER: confirm the AI-code wording and the Q-015 line before submission. -->
- **Libraries and engines:** Unreal Engine 5.8, Next.js, React, Phaser 3, TypeScript, tsx, near-api-js,
  near-sdk (Rust), Resend (email delivery), Upstash Redis (hosted data mirror), NEAR AI Cloud. Tests use
  Node's built-in test runner and Unreal's automation framework.
- **Templates:** Epic's Unreal Engine Top Down template (C++): the project shell, its character, game mode and
  click-to-move input. The template's content is Epic's and is not in this repository
  ([`docs/UNREAL_SETUP.md`](docs/UNREAL_SETUP.md) section 3 copies it from your engine). Our game is the
  `unreal/Plugins/GrudgeboundClient` plugin and `unreal/GrudgeBound/Content/Grudgebound`.
- **AI-written code:** a large part of the code was written with AI coding assistants and reviewed, tested
  and directed by the team.
- **AI-generated art:** the concept art in `docs/art/concepts/` was made with AI image tools. In-game
  intro illustrations are drawn procedurally in code.
- **Third-party assets:** CC0 models (Kenney) and OFL fonts, each listed with author and link in
  [`docs/ASSETS.md`](docs/ASSETS.md). Every sound is synthesised by `unreal/tools/make-audio.py`; no samples.
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
