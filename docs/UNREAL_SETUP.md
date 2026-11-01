# Unreal client: setup and first steps

From 2 Nov the game client is built in **Unreal Engine 5**. The parts that are not the game screen stay as they are:
`server/` (world, ticks, battle checks), `shared/` (formulas, rules), `agents/` and `contracts/`.
The Unreal game talks to the game server over HTTP, the same way the web client does.

The web client (`client/`) stays in the repo and keeps working. It is the fallback demo until the
Unreal client can do the same loop (see the Phase 1 gate in [`BUILD_PLAN.md`](BUILD_PLAN.md)).

## 1. What the laptop needs

| Need | Notes |
|---|---|
| GPU | RTX 3050 (laptop) is fine for a stylized game. Use *Settings > Engine Scalability* at Medium while editing if the editor is slow. |
| RAM | 16 GB works for a small project; 32 GB is more comfortable. Close the browser while compiling. |
| Disk | About 150 GB free on the SSD (engine, Visual Studio, project cache). |
| Power | Plug in, and set Windows power mode to *Best performance*: laptops throttle the GPU on battery. |

## 2. Install (once, about 2-3 hours, mostly downloads)

1. **Epic Games Launcher** from epicgames.com, sign in with the laptop owner's Epic account
   (each person signs in themselves; never share passwords).
2. Launcher > *Unreal Engine* > *Library* > **+** > install the **newest 5.x release**.
   Write the exact version (e.g. `5.x.y`) in the table at the bottom of this file. **All three of us use that same version.**
   Opening a project in a different version upgrades it and the others can't open it.
3. **Visual Studio Community**: install the version the Unreal docs list for that engine version
   (docs page *Setting Up Visual Studio for Unreal Engine*). Tick the workload **Game development with C++**
   and, in its options, the **Unreal Engine installer** and the **Windows SDK** it suggests.
4. **Node.js 20+** (nodejs.org) and **Git** with **Git LFS** (`git lfs install` once).
5. Clone the repo, then in the repo folder: `npm install`.

## 3. Create the project (only one person does this, once)

1. Open Unreal > *Games* > **Top Down** template (it already has click-to-walk and a camera that suits a Ward map).
2. Project type **C++** (not Blueprint): the HTTP code for the server is C++. Target *Desktop*, quality *Scalable*,
   Starter Content off (keeps the repo small).
3. **Location:** the repo's `unreal` folder. **Name:** `Grudgebound`.
   Result: `unreal/Grudgebound/Grudgebound.uproject`.
4. Close Unreal, then check `git status`: you should see `unreal/Grudgebound/Source`, `Config`, `Content`
   and the `.uproject`, but **not** `Binaries`, `Intermediate`, `Saved` or `DerivedDataCache` (git-ignored).
5. Big files (`.uasset`, `.umap`, audio, textures) go through Git LFS automatically (`.gitattributes`).
   Run `git lfs ls-files` after adding content to check.

**Git LFS quota:** GitHub's free plan has a limited LFS storage and download allowance (check yours at
*Settings > Billing*). Keep `Content/` lean: no Starter Content, no unused marketplace packs.
Only commit at milestones, as for the rest of the repo.

**Unreal assets cannot be merged.** Two people editing the same map or Blueprint at once means one person's work
is lost. Before editing a `.umap` or Blueprint, say so in the team chat (or use `git lfs lock <file>`).

## 4. Run it with the server

```sh
npm run server            # in the repo folder; http://localhost:8787
```

Then press **Play** in Unreal. For faster world ticks while testing: `TICK_MS=3000 npm run server`.

## 5. Server API the Unreal client uses

All JSON. Base URL `http://localhost:8787` (make it a setting so we can point it at the hosted server later).

| Call | What it gives |
|---|---|
| `GET /health` | `{ ok, tick }`: is the server up |
| `GET /state` | The whole town: Wards with Grudge per emotion, merchants, rules in force, points table |
| `GET /events?since=<id>` | New log lines (bankruptcies, Calamities, rules, battles) for Kessa's banner; `since=-1` gives all |
| `GET /shop?merchant=broker` | Shop items with price and a one-line reason |
| `POST /battle` | Report a finished battle: `{ wild, omen, playerCurse, seed, actions, ward, by, outcome }`. The server replays it and answers `{ points }`, or 409 (already counted) / 422 (replay doesn't match) |

**Battles:** today the browser fights the battle itself and the server replays it. To keep one copy of the battle
rules, the Unreal client will **not** get a C++ copy of the resolver. Instead the server will run the battle turn by turn
(task P1-C5: `POST /battle/start` gives the curse and a server-made seed, `POST /battle/turn` takes your move and returns
what happened). The Unreal battle screen only shows the result of each turn.

In C++, use Unreal's `HTTP`, `Json` and `JsonUtilities` modules (add them to `Grudgebound.Build.cs`).

## 6. Order of work (matches BUILD_PLAN Phase 1 "U" tasks)

1. **U1** Install, create the project, first commit of the empty project (on the lead's yes).
2. **U2** Lantern Ward greybox: ground, houses as blocks, a tall-grass area, the player walks.
3. **U3** `GrudgeboundApi` C++ class: `/health`, `/state`, `/events`. Show the Ward's Grudge in a simple UMG HUD.
4. **U4** Walking through tall grass rolls an encounter (curse picked from the Ward's Grudge, as `WardScene` does).
5. **U5** Battle screen on top of the server's turn API (P1-C5).
6. **U6** Talisman Case and points in the HUD.
7. Art pass after the loop works: the paper art set in `docs/art/` is the style guide.

The web client's code is the reference for behaviour: `client/game/scenes/WardScene.ts`,
`client/game/scenes/BattleScene.ts`, `client/components/`.

## 7. Builds for judges

Judges can't build Unreal projects themselves, so each milestone also gets a packaged build:
*Platforms > Windows > Package Project* (Shipping), zip it and attach it to a **GitHub Release**
(not committed to the repo). The README links the newest one, plus the demo video.

## Team versions

| Thing | Version | Set by |
|---|---|---|
| Unreal Engine | _fill in on install_ | |
| Visual Studio | _fill in on install_ | |
