# Battle UI review — 11 November 2026

Implementation review, not a new gameplay or world-design plan. Builds on the existing paper UI,
`paper-4-battle.jpeg`, the first native layout and the earlier in-progress Unreal battle stage.
The playable target is Unreal for Windows. No browser gameplay changes.

## Goals and acceptance

| Goal | Acceptance evidence | Status |
|---|---|---|
| Understand both sides immediately | Partner left, wild right; corresponding portrait cards; name, emotion, omen and numeric HP | Implemented; capture pending |
| Keep all six commands visible | Attack, Ward, Salve, Seal, Gilded Seal, Flee in a stable 3-by-2 grid; counts/chances stay with their action | Implemented; capture pending |
| Make feedback legible | Lunge, sustained hit tint and damage number; Ward shield; heal number; seal stamp; delayed result overlay | Automated checks pending |
| Prevent overlapping turns | Commands locked during request and response animation; terminal outcomes never re-enable them | Automated checks pending |
| Support keyboard and mouse | Battle-local 1–6 shortcuts, arrow focus, Enter/Space native button activation, gold focus/hover surround | Implemented; interactive check pending |
| Preserve readability at 720p | Uniform scaling; minimum authored text size 22 Slate units; actual raster review at both sizes | Capture pending; font size alone is not glyph measurement |
| Readable contrast | Essential active text target >=4.5:1; ink text on paper/gold, cream text on crimson/ink | Palette analysis pending |
| Avoid motion dependence | Display > Reduce battle motion removes idle motion, lunges, shake, moving numbers and stamp scaling; text and symbols remain | Automated checks pending |
| Preserve the paper style | Shared notched frames, crimson corner studs, flat panel colours, Pixelify Sans and Courier Prime; existing pixel art | Implemented |

## Composition (1920 × 1080 reference canvas)

The entire canvas fits uniformly within the viewport. Non-16:9 displays use ink margins rather than
stretching controls. A 3px reference edge becomes 2px at 1280 × 720.

| Element | x | y | width | height |
|---|---:|---:|---:|---:|
| Clipped market stage | 0 | 0 | 1920 | 720 |
| Paper control strip | 0 | 720 | 1920 | 360 |
| Partner status card | 24 | 744 | 420 | 228 |
| Wild status card | 1476 | 744 | 420 | 228 |
| Partner sprite | 324 | 402 | 288 | 288 |
| Wild sprite | 1192 | 62 | 448 | 448 |
| Partner portrait frame | 48 | 804 | 96 | 96 |
| Wild portrait frame | 1500 | 804 | 96 | 96 |
| Partner / wild portrait art | 60 / 1512 | 816 | 72 | 72 |
| Partner / wild name | 48 / 1500 | 756 | 372 | 42 |
| Partner / wild emotion icon | 162 / 1614 | 810 | 32 | 32 |
| Partner / wild emotion and omen text | 206 / 1658 | 806 | 220 | 82 |
| Partner / wild HP bar | 48 / 1500 | 912 | 372 | 21 |
| Partner / wild HP numbers | 48 / 1500 | 938 | 372 | 30 |
| Attack / Ward / Salve | 468 / 804 / 1140 | 744 | 312 | 108 |
| Seal / Gilded Seal / Flee | 468 / 804 / 1140 | 864 | 312 | 108 |
| Last two log lines | 24 | 990 | 1872 | 72 |
| Error backing, only when needed | 384 | 620 | 1152 | 76 |
| Result panel, above scene and controls | 480 | 210 | 960 | 246 |

Within each button: icon plate (12,18,72,72), icon (16,22,64,64), label (96,18,204,40),
count/chance (96,62,162,32), keycap (267,66,30,30). Coordinates are button-relative.
Result content uses 30px horizontal / 24px vertical padding; its return button is 600 × 64, centred.
Long status names/log entries ellipsize; the full log entry remains available on hover.

## Type and colour

Sizes below are authored Slate font sizes, not claims about measured raster glyph height.

| Use | Font | Size | Colour |
|---|---|---:|---|
| Curse names / result | Pixelify Sans Bold | 28 | Ink / cream |
| Action names / return | Pixelify Sans | 26 | Cream on crimson; ink otherwise |
| Emotion, omen, counts, chance, keys | Pixelify Sans | 22 | Ink, except cream on crimson |
| HP numbers | Courier Prime | 24 | Ink |
| Log | Courier Prime | 22 | Cream |
| Error | Pixelify Sans | 24 | Cream |
| Damage / heal feedback | Pixelify Sans Bold | 36 | Cream / light green, with 3px ink outline |

Attack and Seal use crimson; Gilded Seal and return use gold; other commands use paper dark.
Names and emotion words use ink so red/blue/purple aren't relied on for small-text contrast.
Emotion icons retain rage/sorrow/dread colours. HP fill is green above 50%, gold above 20%,
crimson otherwise; its empty track is ink and exact HP is always printed below.

Hover and keyboard focus add a 3px gold surround. The shared button hover slightly brightens its
face. Press moves content down 3px and darkens the face. Disabled commands retain their positions
and use the shared disabled treatment; both mouse and shortcut routes obey availability and
request/animation locks. Selection means focus, not an additional gameplay state.

## Feedback timings (project choices, not published accessibility thresholds)

- Lunge: 320ms, 72px maximum travel; idle bob: 4px; local hit shake: at most 6px. No camera shake.
- Hit: one sustained tint, not repeated blinking; numbers remain for about 950ms.
- HP target changes at impact; the bar eases toward the server value.
- Ward shield: 1100ms; seal sequence: about 1450ms. Result appears after the sequence.
- Turn input remains disabled until the sequence completes. Reduced motion preserves that timing.

## References and what they informed

- [XAG 101 — Text display](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/101):
  judge actual on-screen glyphs, not just a font setting; the PC guidance differs from console viewing distances.
- [XAG 102 — Contrast](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/102):
  readable foreground/background pairs, opaque text backings and distinguishable bar tracks.
- [XAG 117 — Motion](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/117):
  reduced-motion control, stable text and no unnecessary repeated flashing.
- Project reference: `docs/art/concepts/paper-4-battle.jpeg` for scene/control hierarchy;
  `Saved/GbShots/title.png` and `menu.png` for the implemented paper style.

These are design references, not a claim of full XAG compliance. Font resizing, screen-reader support,
and a comprehensive accessibility audit are outside this battle pass.

## Validation record

11 Nov evening: editor build OK (the presentation check needed `RenderCore` in GrudgeboundClient.Build.cs);
full `Grudgebound` automation suite 25/25 including `Ui.BattlePresentation`; in-game shots
`Saved/GbShots/battle.png`, `battle-hit.png` (1600 × 900, real game window) and review renders in
`Saved/BattleReview/{1080,720}`.

Changes after the first capture: damage/heal numbers sit on the curse's body (48, 4px ink outline) instead of
up in the lantern strings; the result panel covers the locked command grid (468,744,984,228) so the seal,
fade or fallen partner stays in view; points go on their own line.

Measured contrast (WCAG formula, flat colours as drawn):

| Pair | Ratio | Target |
|---|---:|---|
| Ink on paper panel / paper-dark card | 13.4 / 11.8 | 4.5 ✔ |
| Ink on paper-dark button / gold button | 10.5 / 6.5 | 4.5 ✔ |
| Cream on crimson button | 7.3 | 4.5 ✔ |
| Cream log text on ink | 16.2 | 4.5 ✔ |
| HP fills on ink track: green / gold / rage | 4.2 / 7.9 / 3.5 | 3.0 ✔ |
| Emotion icon colours on paper-hi plate | 4.6–5.9 | 3.0 ✔ |
| Disabled commands (estimated from the washed face) | ~2.4–3.4 | inactive controls are exempt in WCAG 1.4.3 |

The screenshot fixtures use staged server data and placeholder log lines: they do not demonstrate a live
testnet battle. Existing backend tests passed earlier in this session; no backend rules change here.

## Encounter prompt (11 Nov)

The card shown before a fight now uses the battle screen's look: Ward dimmed with ink at 68%; crimson title
ribbon; paper card 1080 wide; the wild curse at a whole-number scale on a cream plate (48 px ×6, 56 ×5, 64 ×4);
name 44, emotion icon + emotion/omen 22, intro lines 20, "Born from" 18; matchup row with the partner's portrait,
the existing matchup sentence and a verdict chip (Advantage = ink on gold 6.5:1, Resists = cream on crimson
7.3:1, Even = ink on paper-dark); Fight! (crimson, Enter keycap, 72 high) and Slip away (paper-dark, 72 high).
Idle bob on the curse follows "Reduce battle motion". The signature move is not shown: battles don't use it yet
(server/src/battles.ts only passes it through), so showing its effect would promise something that doesn't happen.
New UI labels needing owner review: "Advantage", "Resists", "Even".

Checks: `Grudgebound.Ui.EncounterPresentation` (chip per matchup, art imported and whole-number scaled, Enter
starts the fight, review renders `Saved/BattleReview/{1080,720}/encounter-*.png`); in-game shot
`Saved/GbShots/encounter.png`. Full suite 26/26.
