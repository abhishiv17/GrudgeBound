# Unreal client: setup and first steps

From 2 Nov the game client is built in **Unreal Engine 5**. The parts that are not the game screen stay as they are:
`server/` (world, ticks, battle checks), `shared/` (formulas, rules), `agents/` and `contracts/`.
The Unreal game talks to the game server over HTTP, the same way the web client does.

The web client (`client/`) stays in the repo and keeps working. It is the fallback demo until the
Unreal client can do the same loop (see the Phase 1 gate in [`BUILD_PLAN.md`](BUILD_PLAN.md)).

## 1. What the laptop needs

| Need | Notes |
|---|---|
| GPU | RTX 3050 (laptop) is fine for a stylized game. Its **4 GB of video memory** is the tightest limit: Lumen, Nanite and virtual shadow maps are turned off by `unreal/tools/setup-project.ps1`. Use *Settings > Engine Scalability* at Medium while editing. Keep the NVIDIA driver current (NVIDIA app). |
| RAM | 16 GB works for a small project; 32 GB is more comfortable. Close the browser while compiling. |
| Disk | About 100 GB free on the SSD (engine with only the Windows target, Visual Studio, project cache). |
| Power | Plug in, and set the laptop to performance mode (HP: OMEN Gaming Hub; or Windows *Best performance*): laptops throttle the GPU on battery. |

The build laptop (HP Victus 15, i5-12450H, 16 GB, RTX 3050 4 GB, 512 GB NVMe) was checked against this on 2 Nov.

## 2. Install (once, about 2-3 hours, mostly downloads)

1. **Epic Games Launcher** from epicgames.com, sign in with the laptop owner's Epic account
   (each person signs in themselves; never share passwords).
2. Launcher > *Unreal Engine* > *Library* > **+** > install **5.8** (newest hotfix, see the table at the bottom).
   Before *Install*, open **Options** and keep only **Windows** under target platforms; untick Android, iOS, Linux
   and *Editor symbols for debugging* (saves 30-60 GB). **All three of us use that same version.**
   Opening a project in a different version upgrades it and the others can't open it.
3. **Visual Studio Community 2026** (the version Epic recommends for 5.8). Workloads: **Game development with C++**
   (with **Unreal Engine installer** and **IDE support for Unreal Engine**), **Desktop development with C++**,
   **.NET desktop development**; component **Windows 11 SDK (10.0.26100)**. The *.NET MAUI* workload in Epic's list
   is for mobile targets; we build for Windows only, so skip it.
   One command does all of it (admin prompt once):
   `winget install --id Microsoft.VisualStudio.Community --override "--wait --passive --add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Workload.ManagedDesktop --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.Unreal --add Microsoft.VisualStudio.Component.Unreal.Ide --includeRecommended"`
4. **Node.js 20+** (nodejs.org) and **Git** with **Git LFS** (`git lfs install` once).
5. Clone the repo, then in the repo folder: `npm install`.

## 3. Create the project (only one person does this, once)

1. Open Unreal > *Games* > **Top Down** template (it already has click-to-walk and a camera that suits a Ward map).
2. Project type **C++** (not Blueprint): the HTTP code for the server is C++. Target *Desktop*, quality *Scalable*,
   Starter Content off (keeps the repo small).
3. **Location:** the repo's `unreal` folder. **Name:** `Grudgebound`.
   Result: `unreal/Grudgebound/Grudgebound.uproject`.
4. Close Unreal and run, in the repo folder:
   `powershell -ExecutionPolicy Bypass -File unreal\tools\setup-project.ps1`
   It turns on our plugin (`unreal/Plugins/GrudgeboundClient`, see section 4) and sets the 4 GB GPU render settings.
   Then right-click `Grudgebound.uproject` > *Generate Visual Studio project files*, and open the project again:
   Unreal asks to build the missing modules, say **Yes** (first build takes a few minutes).
5. Check `git status`: you should see `unreal/Grudgebound/Source`, `Config`, `Content`
   and the `.uproject`, but **not** `Binaries`, `Intermediate`, `Saved` or `DerivedDataCache` (git-ignored).
6. Big files (`.uasset`, `.umap`, audio, textures) go through Git LFS automatically (`.gitattributes`).
   Run `git lfs ls-files` after adding content to check.

**Git LFS quota:** GitHub's free plan has a limited LFS storage and download allowance (check yours at
*Settings > Billing*). Keep `Content/` lean: no Starter Content, no unused marketplace packs.
Only commit at milestones, as for the rest of the repo.

**Unreal assets cannot be merged.** Two people editing the same map or Blueprint at once means one person's work
is lost. Before editing a `.umap` or Blueprint, say so in the team chat (or use `git lfs lock <file>`).

## 4. The GrudgeboundClient plugin (Phase 1 loop, ready-made)

`unreal/Plugins/GrudgeboundClient` holds the C++ for tasks U2-U6, so the template's own files stay untouched.
Everything is in `Source/GrudgeboundClient/`:

| Class | What it does | Task |
|---|---|---|
| `UGbApiSubsystem` | **GrudgeboundApi**: every server call (`/health`, `/state`, `/events`, `/curses`, `/battle/start`, `/battle/turn`), polled every 2 s; Blueprint events `OnWorldState`, `OnKessaLine`, `OnBattleStarted`, `OnBattleTurn`, `OnError` | U3, U5 |
| `UGbPlayerSubsystem` | Points, Ember, talismans, partner, battle record; saved on this PC (SaveGame slot `Grudgebound`), same shape as the web client's `playerStore` | U6 |
| `AGbWardGreybox` | Lantern Ward from blocks, tile for tile from the web map (`client/game/tiles.ts`), 1 tile = 1 m: wall, harbour and pier, streets, plaza with the Rite board, stalls and lanterns, 4 houses, 4 tall-grass patches | U2 |
| `UGbTallGrassComponent` / `AGbTallGrass` | Walking in tall grass rolls 12% per metre (3 safe metres after a battle), then calls `/battle/start` | U4 |
| `UGbHudWidget` and friends | HUD (Ward, Grudge per emotion, points, Ember, partner, server status), Kessa's banner, encounter dialog (Fight / Slip away), battle screen, Talisman Case (**T**). Built in C++, no widget assets | U3-U6 |
| `UGbSettings` | *Project Settings > Game > Grudgebound*: server URL, poll rate, player name, encounter chance | |

**Make the Lantern Ward level (once):**

1. *File > New Level > Basic*, save it as `Content/Maps/LanternWard`.
2. Delete the floor the Basic level comes with. *Place Actors* panel: search **Gb Ward Greybox**, drag it in,
   then set its location to `0, 0, 0` in *Details*.
3. Add a **Nav Mesh Bounds Volume**, location `0, 0, 0`, scale `16, 23, 4` (it must cover the 44 x 30 m map).
   Press **P** to see the walkable area in green.
4. *Window > World Settings > GameMode Override*: the template's game mode (`BP_TopDownGameMode` or `GrudgeboundGameMode`).
5. *Project Settings > Maps & Modes*: *Editor Startup Map* and *Game Default Map* = `LanternWard`.

**Run it:**

```sh
npm run server            # in the repo folder; http://localhost:8787
```

Then press **Play** in Unreal. Click to walk; walk through the dark-green tall grass; a curse appears; **Fight!**.
Back in the Ward the points and the talisman show up (press **T**). The HUD says *Server offline* if the server isn't running.
For faster world ticks while testing: `TICK_MS=3000 npm run server` (PowerShell: `$env:TICK_MS=3000; npm run server`).

The plugin's screens are plain paper-and-ink panels. For the art pass, turn off *Create Default Ui* in the settings
and build Blueprint widgets on the same subsystem events.

## 5. Server API the Unreal client uses

All JSON. Base URL `http://localhost:8787` (make it a setting so we can point it at the hosted server later).

| Call | What it gives |
|---|---|
| `GET /health` | `{ ok, tick }`: is the server up |
| `GET /state` | The whole town: Wards with Grudge per emotion, merchants, rules in force, points table |
| `GET /events?since=<tick>` | Log lines from ticks after `<tick>` (bankruptcies, Calamities, rules, battles) for Kessa's banner; `since=-1` gives all |
| `GET /shop?merchant=broker` | Shop items with price and a one-line reason |
| `GET /curses` | Every curse's `id`, `name`, `emotion`, `omen`, `home`, `bornFrom`, `signatureMove` (no stats): names for the Talisman Case |
| `POST /battle/start` | **Unreal uses this.** The player walked into tall grass. Body `{ ward, playerCurse?, by? }`. The server picks the curse from the Ward's Grudge and keeps the seed secret. Answer: `battleId`, `wild`, `matchup`, `view`, `lines` |
| `POST /battle/turn` | **Unreal uses this.** Body `{ battleId, action }`. Answer: this turn's `events` and ready-made log `lines`, the new `view`, `outcome`. When the battle ends: `points`, `seed`, `actions` |
| `POST /battle` | Web client only: reports a battle it fought itself; the server replays it |

**Battles are run by the server.** The Unreal client has no copy of the battle rules: it sends a move and draws what
comes back. Code: `server/src/battles.ts` (tests in `battles.test.ts`).

`action` is one of `{"kind":"attack"}`, `{"kind":"ward"}`, `{"kind":"seal"}`, `{"kind":"seal","gilded":true}`,
`{"kind":"item","item":"salve"}`, `{"kind":"flee"}`. Grey out buttons using `view.canUse`; show `view.chances` on them.

`view` (after start and after every turn):

```json
{
  "turn": 0, "outcome": null,
  "player": { "speciesId": "cinder-grin", "name": "Cinder Grin", "emotion": "rage", "omen": "whisper", "hp": 38, "maxHp": 38 },
  "wild":   { "speciesId": "knucklebrat", "name": "Knucklebrat", "emotion": "rage", "omen": "whisper", "hp": 42, "maxHp": 42 },
  "items": { "salve": 2, "gilded": 1 },
  "chances": { "seal": 0, "gildedSeal": 0, "flee": 0.6 },
  "canUse": { "attack": true, "ward": true, "seal": true, "gildedSeal": true, "salve": true, "flee": true }
}
```

| Answer | Meaning |
|---|---|
| 200 | OK |
| 400 | Bad body: unknown ward, curse or action |
| 404 | No such battle: it ended, or nobody touched it for 15 minutes |
| 409 | That move isn't available now (e.g. no salves left); the turn is not spent |
| 503 | Too many battles open on the server; try again |

`outcome` is `won` (exorcised), `bound` (sealed into a talisman), `fled` or `lost`. On `bound`, add a talisman for
`view.wild.speciesId` to the Talisman Case. The server has already written the town log line and paid the points.

Try it without Unreal:

```sh
curl -X POST localhost:8787/battle/start -H "Content-Type: application/json" -d "{\"ward\":\"lantern\"}"
curl -X POST localhost:8787/battle/turn  -H "Content-Type: application/json" -d "{\"battleId\":\"<id>\",\"action\":{\"kind\":\"attack\"}}"
```

In C++, use Unreal's `HTTP`, `Json` and `JsonUtilities` modules (add them to `Grudgebound.Build.cs`).

## 6. Order of work (matches BUILD_PLAN Phase 1 "U" tasks)

1. **U1** Install, create the project, run `setup-project.ps1`, first commit of the empty project (on the lead's yes).
2. **U2-U6** The code is in the plugin (section 4); what's left is the level (section 4, "Make the Lantern Ward level"),
   building it, and playing the loop through once with the server running. Fix what the first run shows.
   - U2 Lantern Ward greybox: `AGbWardGreybox`.
   - U3 `GrudgeboundApi` = `UGbApiSubsystem`; HUD = `UGbHudWidget`.
   - U4 tall grass: `UGbTallGrassComponent`; encounter dialog: `UGbEncounterWidget`. Slip away needs no call: the battle expires on its own.
   - U5 battle screen: `UGbBattleWidget` on `/battle/start` + `/battle/turn`.
   - U6 Talisman Case (**T**) and points: `UGbPlayerSubsystem`, `UGbTalismanCaseWidget`.
3. Art pass after the loop works: the paper art set in `docs/art/` is the style guide.

The web client's code is the reference for behaviour: `client/game/scenes/WardScene.ts`,
`client/game/scenes/BattleScene.ts`, `client/components/`.

## 7. Builds for judges

Judges can't build Unreal projects themselves, so each milestone also gets a packaged build:
*Platforms > Windows > Package Project* (Shipping), zip it and attach it to a **GitHub Release**
(not committed to the repo). The README links the newest one, plus the demo video.

## Team versions

| Thing | Version | Set by |
|---|---|---|
| Unreal Engine | **5.8** (newest hotfix; 5.8.4 on 2 Nov). Write the exact `5.8.x` here once installed | 2 Nov, build laptop |
| Visual Studio | **Community 2026** (18.x), workloads in section 2 | 2 Nov, build laptop |

A packaged build can point at a hosted server without rebuilding: `Grudgebound.exe -GbServer=https://<host>`.
