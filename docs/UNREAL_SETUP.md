# Unreal client: setup and first steps

## Sound, battle staging and street talk (12 Nov)

Every sound is synthesised by `unreal/tools/make-audio.py` (Python 3 + numpy; sine waves and filtered noise, no
samples, so no third-party licence) and imported by `unreal/tools/import-audio.py` to `/Game/Grudgebound/Audio`.
Footsteps follow the ground (stone outdoors, boards indoors, a rustle in tall grass); doors, panels, buttons and
every battle move have an effect; a night-market loop plays in the Wards, quieter indoors, behind panels and in
battle. Menu > Sound has Master, Market and music, and Effects sliders.

Battles: the two curses step in from their sides as the camera settles; a blow knocks its target back with one
ring and a short camera punch; the seal falls tilted, lands with a gold ring and the camera closes on the curse,
then holds (bound) or is thrown off (broken). Game > Reduce battle motion turns all of it off.

Checks: `Grudgebound.Audio.Sounds`, `Grudgebound.Ward.Talk` and `Grudgebound.Ui.BattlePresentation`. Review
pictures: `Saved/BattleReview/<height>/` and `Saved/PanelPreview/<height>/talk.png`. The sounds themselves and the
townsfolk lines have not been judged by ear or reviewed by the lead yet.

## Exploration camera and townsfolk (12 Nov polish)

Hold **Middle Mouse** and drag sideways to orbit or vertically to tilt; **Q/C** rotate,
the **mouse wheel** zooms, and **Home** restores the default view. All bindings remain configurable.
The same controls work on the ferry while walking is locked. Opening a panel or the settings menu
blocks camera input. Rooms use the same orbit and zoom, with a higher tilt and bounded distance to see
over the walls. Camera > Invert mouse orbit / tilt reverses both drag axes.

There are eight ambient walkers in Lantern Ward and five on Tide's docks. They follow the existing
street routes, ease into stops, pause at either end, and yield when the player stands ahead of them.
They make no economic decisions. Walk up to one and press **E** (Interact): they stop, turn to you and say a line
on a paper panel above Kessa's banner; **E** again for the next. Lines follow their trade, the mood of the Ward's
Grudge (calm, Rage, Sorrow or Dread) and the story chapter. Words only: nothing is sent to the server. The named economic agents run on the server; a confirmed
agent purchase can show its visitor marker at the Counting House. Ambient movement is not proof of an
agent trade or a story event.

Validated for this camera/movement pass: Development Editor build succeeded; native automation
**34/34 passed**, zero test warnings/failures/skips (`Saved/Automation/ExplorationPolish/index.json`).
Actual game captures at 1920x1080 are in `Saved/GbShots/ExplorationPolish`: shot0 market, shot1-3 crossing,
shot4 arrival, shot5 Tide quay, shot6-9 interiors, shot10 character. These use the development capture
harness, not a signed-in end-to-end story or economy run. The game log confirms ferry arrival at Tide.
For a recording, zoom in during the crossing; the default pullback shows mostly open water mid-trip.
Manual mouse feel, Tide click-to-walk and two-player character review remain play-test checks.

## Local CC0 district assets (T-11)

Use only the owner-supplied Kenney packs in `%USERPROFILE%/GrudgeboundCC0`; inventory/licences:
[ASSETS.md](ASSETS.md). With the editor closed, run UnrealEditor-Cmd against this project with
`-run=pythonscript "-script=<repo>/unreal/tools/import-cc0.py --used-only" -RenderOffscreen -unattended`.
Omit --used-only to import all FBX/GLB meshes from the three packs, or pass a different local source folder.
Then run make-lantern-ward.py through the same commandlet. It updates an existing level in place, retains
the actor/player-start/nav locations and does not delete its map asset. The actor's Rebuild Ward button
reconstructs procedural instances after slot edits. Missing imports use the procedural fallback.

If Git LFS has made LanternWard.umap read-only, make a local backup and clear its read-only attribute
before running the authorized generator edit; the script fails if the map cannot be saved. T-11's original
is preserved under Saved/T11Backup. Source builds are not committed or pushed by this workflow.

Mesh slots use soft asset paths; Nanite is disabled at import. Original collision hulls and all four grass
trigger bounds remain; decorative imports have no collision. The closed docks gate has a blocking fence.
Lanterns use bounded, shadow-free dynamic point lights; the existing server day/night clock remains active.
Owner play checks: all streets/grass patches, three locked-gate lines, den row/canal labels, night readability,
and stat unit/stat gpu at 1080p. Performance and navigation acceptance require the phase-end Play test.

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
Controls and the remaining acceptance checks: [`VOWS.md`](VOWS.md). **B** opens the Satchel, **R** the Rite
and **V** the shrine inside the Rite Hall, **L** the latest ledger proof and **F10** signs out while walking.
**O** opens/closes the native Observatory while walking. **This town** refreshes live history every ten
seconds while open; **Seven days ahead** uses the server's isolated, cached seven-day preview. Grudge
shows four Ward totals and the selected Ward's emotions; Prices shows the Broker and shop closures;
Rules lists newest verdicts first, including available reasons and clickable testnet ledger proofs.
The chart's Day 1-7 axis covers the displayed seven-day window; the header retains the world's actual day/hour.
Native tab previews are written to `Saved/ObservatoryPreview` by `Grudgebound.Observatory` automation.
The Satchel has four pockets: **Talismans**, **Supplies**, **Keepsakes**, and **Ledger**. Select a talisman
to carry/leave it in the Case (five carried slots), choose a carried partner, or check its ink. The Supplies
pocket uses a salve on that selection; full ink disables the button. **T** still opens the legacy Case.
Walk to a named door and press **E**, or click its marker while nearby. A 0.3-second fade moves the pawn
to a small offsite room in the same map; its Exit marker returns to the outside doorway. No server route
or level streaming is involved. Each room has its own bounded camera and navigation volume.
The **Counting House** counter opens the Broker (talismans, blanks, gilded talismans and Ink Salves).
The **Mender's House** counter restores selected ink for Ember; the pre-action estimate follows the existing
Lantern Ward Sorrow formula, and the server's response supplies the actual restoration reason/receipt.
The **Rite Hall** has Kessa and a separate Vow Shrine. R/V outside it show only the supplied OWNER TEXT line.
**Your room** has the existing debt notice, free **Rest until morning**, and the Talisman Case. Rest advances
only your personal clock. Broker/Mender/Rest shortcuts are removed from the Satchel. B/L/I/F10 still work
while walking inside buildings; panels return to the same interior. Agent trades received while inside
the Counting House show the named agent at the counter for five seconds.
The server supplies every count and ink value. New players receive **3 blank / 0 gilded / 1 salve** once;
existing accounts keep their supplies. A partner with zero ink cannot start a fight: visit the Mender,
use a salve, or rest. The battle view shows its saved ink. **Slip away** from an encounter now attempts
the server's flee move; if it fails, the battle continues, and its turn/ink costs still count.
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

Judges can't build Unreal projects themselves, so each milestone also gets a packaged build, zipped and
attached to a **GitHub Release** (never committed to the repo). The README links the newest one, plus the demo video.

**Package** (close every Unreal editor first, and don't run it while another Unreal build or cook runs: on a
16 GB laptop the cook needs the memory). From the repo root:

```powershell
# the build for judges: Shipping, pointed at the hosted HTTPS game server
powershell -ExecutionPolicy Bypass -File unreal\tools\package-windows.ps1 -ServerUrl https://<game-server> -Version 1.0.0
# a quick local test build against the server on this PC
powershell -ExecutionPolicy Bypass -File unreal\tools\package-windows.ps1 -ServerUrl http://127.0.0.1:8787 -Config Development
```

The script cooks LanternWard and NinthBell (plus everything under `/Game/Grudgebound`), builds, paks with IoStore,
stages and archives into `unreal\Packaged\<date>-<Config>\Windows\`, then zips it as
`Grudgebound-Win64-<version>.zip` next to it and prints the size and the time taken. It writes the server URL into
`Config/DefaultGame.ini` only while it runs and puts the committed file back exactly afterwards. It never uploads.
A Shipping build refuses a plain `http://` server URL. If BuildCookRun fails, the first `Error:` line in its output
is the cause; the full log is under `%APPDATA%\Unreal Engine\AutomationTool\Logs\`.

**Run the packaged game:** unzip anywhere and start `GrudgeBound.exe`. Sign-in opens the system browser at the
server's sign-in page; the session is remembered for that Windows user and server, and F10 signs out. To use
another server without repackaging: `GrudgeBound.exe -GbServer=https://<host>`. The Ninth Bell with two players:

```powershell
GrudgeBound.exe /Game/Grudgebound/NinthBell?listen -windowed -ResX=1280 -ResY=720 -port=7777   # host
GrudgeBound.exe 127.0.0.1:7777 -windowed -ResX=1280 -ResY=720                                 # join (use the host's LAN IP on another PC)
```

Windows may ask to allow the game through the firewall for the host; allow it on private networks.

**Publish (owner only, after a play test of the zip):**
1. On github.com, open the repo, then *Releases* > *Draft a new release*.
2. *Choose a tag*: type `v1.0.0` (match `-Version`) and pick *Create new tag on publish*; target `main`.
3. Title: `Grudgebound 1.0.0 (Windows)`. Notes: what's in the build, the server it points at, the minimum
   hardware (Windows 10/11 64-bit, a DirectX 12 GPU with 4 GB), and "unzip, run GrudgeBound.exe".
4. Drag the zip into *Attach binaries*, then *Publish release*. Copy the zip's download link into the
   README's "Play it" section.

On a machine with the GitHub CLI the same thing is:
`gh release create v1.0.0 unreal/Packaged/<date>-Shipping/Grudgebound-Win64-1.0.0.zip --title "Grudgebound 1.0.0 (Windows)" --notes-file <notes.md>`

## Team versions

| Thing | Version | Set by |
|---|---|---|
| Unreal Engine | **5.8.4** (Epic launcher; Windows target only, ~11.5 GB download) | 3 Nov, build laptop |
| Visual Studio | **Community 2026** (18.x), workloads in section 2 | 2 Nov, build laptop |

A packaged build can point at a hosted server without rebuilding: `Grudgebound.exe -GbServer=https://<host>`.

The draft harbour opening uses the same server's versioned Satchel profile. An old server without a profile
keeps exploration available; it cannot save the new choices. Use a matching local server to test arrival.
`Grudgebound.Arrival` checks profile parsing, legacy partner-page skipping, both character silhouettes,
name retention and native panel previews at 1920x1080 and 1280x720 (`Saved/ArrivalPreview`).
The development screenshot harness accepts `-GbShot=<folder> -GbShotCams="arrival;arrival-name;arrival-partner"`
to capture Kessa and the player on the actual pier with fixture choices, without an account or live mint.

The rival, Dace Marrow, stands where the server's Satchel puts him: the pier, the ferry, the Tide quay or the
plaza. Walk up and press **E** to fight him, or to hear why he won't fight yet. The ferry to Tide stays shut
until his second fight is won. Partner levels show in the Satchel, in the encounter title and on the battle
screen. `Grudgebound.Satchel.LevelsAndRival` checks the JSON parsing. The shot harness accepts
`rival-pier`, `rival-ferry`, `rival-quay` and `rival-plaza` to pin him at a spot. Combine these with fixed
cameras.

**Guidance.** A gold beacon hovers over the next step of the Journal task you track. That step can be a find,
a door, a quest NPC or Dace. When the step is across the water or inside a building, the beacon points to
the ferry or the door first. The HUD shows the distance under the tracked objective, for example "Beacon 42 m
away" or "Take the ferry to Tide Ward first". You can turn the beacon off in Settings (**Waypoint beacon**).

Five first-time tips are shown once per account: the beacon, the first encounter, the first find, the first
level-up and arriving in Tide. Kessa's banner shows each tip, and the server records that it was seen through
`POST /exploration/tip`. That route takes no player lock and is allowed mid-battle. Tips you have seen stay
listed in Satchel > Journal. `Grudgebound.Satchel.Guide` checks the parsing and the tracked-target rules. The
shot harness accepts `waypoint-<discovery id>`, for example `waypoint-lamp-kit`.
