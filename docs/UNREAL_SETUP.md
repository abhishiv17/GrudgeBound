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

## 3. Open the project after cloning

The project is in the repo (since 5 Nov): `unreal/GrudgeBound` holds the `.uproject`, `Source`, `Config` and our own
content (`Content/Grudgebound`, the Lantern Ward map). The Top Down template's content (characters, blueprints,
~130 MB) is Epic content, so it is **not** in the public repo: you copy it from your own engine install once.

1. Make a throwaway project: Unreal > *Games* > **Top Down**, **C++**, any folder outside the repo, name `GrudgeBound`.
   Close Unreal.
2. In the repo folder:
   `powershell -ExecutionPolicy Bypass -File unreal\tools\setup-project.ps1 -TemplateContent <throwaway>\Content`
   It copies the template content in (everything except `Content/Grudgebound`, which is ours), turns on our plugin
   (`unreal/Plugins/GrudgeboundClient`, see section 4) and sets the 4 GB GPU render settings.
3. Right-click `unreal/GrudgeBound/GrudgeBound.uproject` > *Generate Visual Studio project files*, then open it:
   Unreal asks to build the missing modules, say **Yes** (first build takes a few minutes).
4. `git status` stays clean: the template content, `Binaries`, `Intermediate`, `Saved` and `DerivedDataCache` are
   git-ignored. Only `Content/Grudgebound` is committed, through Git LFS (`.gitattributes`).
   Put every asset we make or add under `Content/Grudgebound`, and only CC0 or our own assets (the repo is public;
   Fab/Megascans assets can't be redistributed). List each pack in the README (Q-005).

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
| `UGbSettings` | *Project Settings > Game > Grudgebound*: server URL, poll rate, encounter chance; identity comes from sign-in | |
| `UGbAuthSubsystem` | Browser email sign-in, session resume, sign-out, Windows DPAPI session storage | `docs/SIGN_IN.md` |

**Make the Lantern Ward level (once):**

Quick way: close the editor and run `unreal/tools/make-level.ps1`. It runs `make-lantern-ward.py` headless and saves
`/Game/Grudgebound/LanternWard` (Basic level + greybox + nav volume). Then set it in *Maps & Modes* (step 5).
The project's `.uproject` needs the `PythonScriptPlugin` and `EditorScriptingUtilities` plugins enabled for this.
By hand:

1. *File > New Level > Basic*, save it as `Content/Grudgebound/LanternWard`.
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

Also run `npm run dev` for the browser sign-in page. Configure email and signing using [`SIGN_IN.md`](SIGN_IN.md),
or use its console-email/off-chain development mode. Press **Play** in Unreal, select **Sign in with email**,
complete the browser page, and return to the game. Sessions resume automatically; F10 signs out in the Ward.
Click to walk; walk through the dark-green tall grass; a curse appears; **Fight!**.
Back in the Ward the points and the talisman show up (press **T**). The HUD says *Hollowmere is unreachable* if the server isn't running.
**V** opens the native Vow Shrine (stake points, keep or release a promise; needs testnet signing).
Controls and the remaining acceptance checks: [`VOWS.md`](VOWS.md). **B** opens the Broker, **R** the Rite,
**L** the latest ledger proof and **F10** signs out while in the Ward.
First entry now shows a four-panel arrival story. **Enter/Space** turns a page, **Escape** skips it, and
**I** replays it from the Ward. The HUD keeps the current chapter and objective visible; story flags are
saved separately per account and server. Script and chapter triggers: [`STORY.md`](STORY.md).
The Ninth Bell is a separate co-op source prototype, not the current entry map. Its map has not yet been
generated, and the draft generation/launch helpers have not been validated. Build and native logic checks
pass; host/client play-through is still open. See [`THE_NINTH_BELL.md`](THE_NINTH_BELL.md).
For faster world ticks while testing: `TICK_MS=3000 npm run server` (PowerShell: `$env:TICK_MS=3000; npm run server`).

**Before the engine is installed:** `unreal/tools/greybox-preview.html` is a browser preview of the same greybox
(same blocks, colours, camera angle and screens, drawn with three.js, not Unreal). With `npm run server` running,
double-click it for the visual preview. Its older standalone HTTP client does not implement sign-in; use the
Unreal client or the web game for authenticated battles.

The plugin's screens are plain paper-and-ink panels. For the art pass, turn off *Create Default Ui* in the settings
and build Blueprint widgets on the same subsystem events.

## 5. Server API the Unreal client uses

All JSON. Base URL `http://localhost:8787` (make it a setting so we can point it at the hosted server later).
Player actions and `/player` require the session's Bearer token; `UGbApiSubsystem` adds it automatically.
See [`SIGN_IN.md`](SIGN_IN.md) for the authentication routes and browser handoff.

| Call | What it gives |
|---|---|
| `GET /health` | `{ ok, tick }`: is the server up |
| `GET /state` | The whole town: Wards with Grudge per emotion, merchants, rules in force, points table |
| `GET /events?since=<tick>` | Log lines from ticks after `<tick>` (bankruptcies, Calamities, rules, battles) for Kessa's banner; `since=-1` gives all |
| `GET /shop?merchant=broker` | Shop items with price and a one-line reason |
| `GET /curses` | Every curse's `id`, `name`, `emotion`, `omen`, `home`, `bornFrom`, `signatureMove` (no stats): names for the Talisman Case |
| `POST /battle/start` | **Unreal uses this.** The player walked into tall grass. Body `{ ward, playerCurse?, by? }`. The server picks the curse from the Ward's Grudge and keeps the seed secret. Answer: `battleId`, `wild`, `matchup`, `view`, `lines` |
| `POST /battle/turn` | **Unreal uses this.** Body `{ battleId, action }`. Answer: this turn's `events` and ready-made log `lines`, the new `view`, `outcome`. When the battle ends: `points`, `seed`, `actions` |
| `POST /battle` | Removed in P2-C7: every client (web and Unreal) fights through `/battle/start` + `/battle/turn` |

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
| 401 | Sign in, or the saved session expired |
| 403 | Account mismatch, another player's battle, or a partner you do not own |
| 404 | No such battle: it ended, or nobody touched it for 15 minutes |
| 409 | That move isn't available now (e.g. no salves left); the turn is not spent |
| 503 | Too many battles open on the server; try again |

`outcome` is `won` (exorcised), `bound` (sealed into a talisman), `fled` or `lost`. On `bound`, add a talisman for
`view.wild.speciesId` to the Talisman Case. The server has already written the town log line and paid the points.

Try it without Unreal:

```sh
curl -X POST localhost:8787/battle/start -H "Authorization: Bearer <session-token>" -H "Content-Type: application/json" -d "{\"ward\":\"lantern\"}"
curl -X POST localhost:8787/battle/turn  -H "Authorization: Bearer <session-token>" -H "Content-Type: application/json" -d "{\"battleId\":\"<id>\",\"action\":{\"kind\":\"attack\"}}"
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
| Unreal Engine | **5.8.4** (Epic launcher; Windows target only, ~11.5 GB download) | 3 Nov, build laptop |
| Visual Studio | **Community 2026** (18.x), workloads in section 2 | 2 Nov, build laptop |

A packaged build can point at a hosted server without rebuilding: `Grudgebound.exe -GbServer=https://<host>`.
