# Grudgebound: The Rite - Art Prompts

Prompts for generating concept art and game assets in a consistent style.
They work with any image generator. Copy a prompt, paste it after the style block, and fill in the `[BRACKETS]`.

---

## 0. Art decisions (read first)

**The style guide is the paper set** (`docs/art/concepts/paper-1-town-map.jpeg` to `paper-4-battle.jpeg`).
Attach `paper-2-explore.jpeg` (and `paper-4-battle.jpeg` for battle work) as reference images to every prompt and say "match this style exactly".

| Decision | Rule |
|---|---|
| Overall look | Paper, ink, coins, wax seals, stamps, lanterns. Warm, cozy, but uneasy. A grimy debt town, not a fairytale. |
| What curses are | Made of **objects** from the town (receipts, purses, candles, ledgers, bells), never animals or elemental beasts. |
| Each Ward's curses | Made from that Ward's materials: market = paper and ledgers, docks = sea creatures with coin purses, foundry = iron and embers, temples = stamps, rule books and stone. |
| Emotions | Exactly three: **Rage** (red), **Sorrow** (blue), **Dread** (violet). |
| Kessa | The paper-lantern referee spirit. **Not a Ward and not an emotion.** |
| Wards | Lantern Ward (night market), Ash Ward (foundry), Tide Ward (docks), Bone Ward (temples and archives). |
| Hub map | Follow `paper-1-town-map.jpeg`: four Wards around a central plaza (Rite Spire, Vow Shrine, Binding Hall, Exchange, Observatory), in the harbor-town look. No floating islands or castles. |
| Player character | Must not look like any famous hero. **Avoid green tunic + pointed cap + satchel** (reads as a well-known Nintendo character). |
| Camera in game | Top-down 3/4 view on a 32x32 grid. **Not isometric.** |
| Text | No text inside generated images. Titles and labels are added in the game or in an editor. |

### How the existing images were judged

| Image | Verdict |
|---|---|
Each set has four screens: `1-town-map`, `2-explore`, `3-market`, `4-battle`.

| Set | Verdict |
|---|---|
| `paper-*` | **Style guide.** Paper, ink and lantern look; curses made of town objects; Ward names correct. Base all game art on this. |
| `tide-*` | Good reference for the **Tide Ward** only (docks, sea-creature curses with coin purses). Too bright and Mediterranean for the whole game. |
| `porcelain-*` | Use for UI layout ideas only (market and battle panels). **Do not use the player character**: the green tunic, pointed cap and satchel look too much like a famous Nintendo hero. |

The first five drafts (`01-` to `05-`) were replaced by these sets and are no longer in the repo.

---

## 1. Seven rules for better prompts

1. **Attach the Paper Market image as a reference every time.** This does more for consistency than anything else.
2. **Paste the style block (section 2) at the start of every prompt**, word for word.
3. **One subject per prompt.** One curse, one screen or one map. Collages drift.
4. **Ask for game assets, not illustrations.** Name the camera angle, the grid size and a plain background.
5. **Say what to avoid**: no animals, no elemental beasts, no castles, no cherry blossoms, no isometric view.
6. **No text in images.** AI tools garble lettering.
7. **Never name an existing game, anime or artist.** It pulls the output toward copying, and this art will be public. Describe the look in your own words.

---

## 2. Style block (paste first, every time)

```
STYLE: detailed pixel art for a 2D game, warm limited palette, cream paper (#F3E9D2),
ink black (#1A1726), coin gold (#D9A532), crimson seal red (#B0153A).
Emotion accents: rage red (#C8402A), sorrow blue (#2B6CB0), dread violet (#6B46C1).
Setting: a cursed Japanese-folklore harbor town that ran on debt. Motifs: paper
talismans, receipts, ledgers, wax seals, ink stamps, coins with square holes, lanterns.
Mood: cozy but uneasy, grimy market, not fairytale. Clean outlines, crisp pixels,
no blur, no text, no letters, no watermark.
```

---

## 3. Prompts

### A. One curse (battle sprite) - one prompt per curse, 12 in total

```
Single creature sprite for a turn-based battle screen, front 3/4 view, centered,
plain flat cream background, full body visible, 128x128 pixel art.
The creature is a CURSE born from [EMOTION] in a debt-ridden town. It is made of
OBJECTS, not an animal: [DESCRIPTION]. Accent color: [EMOTION COLOR].
Expressive face, readable silhouette, slightly cute but unsettling.
Avoid: animals, elemental beasts, foxes, wolves, deer, dragons, text.
```

| Curse | Emotion | Omen | Description to paste |
|---|---|---|---|
| Cinder Grin | Rage | Whisper | a floating ember-hot coin with a wide jagged grin, sparks for teeth (Ash Ward foundry) |
| Knucklebrat | Rage | Whisper | a small dented iron coin-purse with a clenched fist, bandaged knuckles |
| Gutterjaw | Rage | Wail | a crab made of a snapping coin purse, claws are clasp locks, scam contracts in its mouth (Tide Ward) |
| Riot Bell | Rage | Calamity (boss) | a huge cracked temple bell with a furious face, chains and torn banners, shockwaves |
| Lantern Drowned | Sorrow | Whisper | a jellyfish made of a soggy paper lantern, tentacles of wet receipts dripping ink (Tide Ward) |
| Weepwick | Sorrow | Whisper | a melting candle with a crying face, unpaid wage slips stuck to the wax |
| Tallow Widow | Sorrow | Wail | a tall wax figure wearing a veil of IOU notes, wax tears, holds an empty purse |
| The Shuttered | Sorrow | Calamity (boss) | a shop's wooden shutter come alive, mouth full of receipts and coins, "closed" tags |
| Hushmoth | Dread | Whisper | a moth with paper wings stamped with red seals, silent, glowing eye on each wing (Bone Ward) |
| Stairmouth | Dread | Whisper | a crooked archive staircase with a mouth, ledgers as steps, dust and loose pages |
| Ninefold Shade | Dread | Wail | a shadow folded from nine sheets of black paper, origami edges, eyes between the folds |
| Curfew King | Dread | Calamity (boss) | a giant rule book walking on ink legs, covered in red "forbidden" stamps, paper crown |

Tip: for bosses (Calamity), add "larger, more detailed, menacing, 192x192".

### B. Kessa, the referee spirit

```
Character sheet of KESSA, a small friendly paper-lantern spirit who announces the
rules of a cursed contest. Round paper lantern body, simple happy face, red tassel,
floating strips of talisman paper like ribbons. Show 4 poses: idle, announcing
(raised paper strip), stern (rejecting a rule), celebrating. Plain cream background.
```

### C. Walking sprites (player and NPCs)

```
Pixel art sprite sheet, top-down 3/4 RPG view, 32x32 pixel character on a strict grid.
[CHARACTER DESCRIPTION].
4 rows (down, left, right, up) x 4 walking frames. Transparent or flat background,
no shadows outside the grid, consistent proportions in every frame.
```

| Character | Description to paste |
|---|---|
| Player | A young exorcist with a red scarf, dark hair tied up, satchel of paper talismans |
| Merchant (AI) | A shrewd shopkeeper with a straw hat and an abacus, apron with coin pouches |
| Guild master (AI) | A hooded elder holding a bounty scroll, lantern on a staff |
| Rival: Old Mother Fen | A hunched old woman hoarding talismans in a big basket, sorrow-blue shawl |
| Rival: Brother Vane | A stern monk-like exorcist with prayer beads and a white robe |
| Rival: Silk | A smiling schemer in a violet coat, ledger under one arm |
| Rival: Rook and Ash | Two mercenaries, one tall with a staff, one short with knuckle wraps, rage-red sashes |
| Rival: Pell | A gambler with dice charms, mismatched clothes, lucky coin on a string |

### D. Tilesets (one Ward per prompt)

```
Top-down 3/4 RPG tileset, strict 32x32 pixel grid, NOT isometric, tiles laid out
on a sheet with no gaps. [WARD DESCRIPTION]. Include: paths, walls, doors, a tall
grass patch (where curses appear), water edge, props. Seamless tiles.
```

| Ward | Description to paste |
|---|---|
| Lantern Ward | Night market: stone paths, stalls with striped awnings, paper lanterns, notice boards, crates of ledgers |
| Tide Ward | Docks: wooden piers, fishing nets, warehouses, moored boats, barrels, rope |
| Ash Ward | Foundry district: coal heaps, iron grates, chimneys, a sunken fighting pit, anvils |
| Bone Ward | Old temples: stone steps, stone lanterns, a graveyard, archive shelves, hanging prayer plaques |

### E. Town hub map

```
Top-down map of a cursed harbor town, not floating islands, no castle.
Central plaza with a tall wooden notice tower (the Rite Spire, where rules are posted).
Four districts around it: night market with lanterns (north-west), docks and piers
(south-east), foundry with chimneys (north-east), old temples and graveyard
(south-west). Small landmarks: vow shrine, binding hall, exchange house, observatory.
Readable layout for a game hub, muted dusk lighting.
```

### F. UI screens (one screen per prompt)

```
Game UI mockup, pixel art, paper-and-ink style: the [SCREEN] screen. [SCREEN DETAILS].
Leave text areas blank (no lettering).
```

| Screen | Details to paste |
|---|---|
| Rule board | A wooden notice board with pinned paper rules, one new rule glowing as it's added, a red wax seal button, point cost shown as a coin icon |
| Battle | Player's curse bottom-left, wild curse top-right, two health bars, four action buttons (attack, ward, seal, item) on a paper panel |
| Grudge meter | A tall paper lantern filling with three colored inks (red, blue, violet), cracks appearing as it fills |
| Vow | An unrolled scroll with a blank vow line, a red wax seal being pressed, a small stake of coins beside it |
| Merchant shop | Shop counter with item slots, a small tooltip bubble next to one price, merchant portrait on the side |
| Observatory | A star-chart room where the stars are line charts, a big brass telescope, a fast-forward lever |
| Title screen | The harbor town at dusk seen from a hill, the Rite Spire glowing, the player and Kessa in the foreground, empty space at the top for the logo |

---

## 4. Workflow

1. **Generate 4 variations, pick the best**, then use it as an extra reference for the next asset in the same family.
2. **Clean sprites up in a pixel editor** (Aseprite, or the free Pixelorama). AI sprites are never perfectly grid-aligned.
3. **Scale with "nearest neighbor"** so pixels stay crisp.
4. **Use AI art for curses, Kessa, characters and key screens; use free tilesets for maps.** This saves days.
5. **Save every chosen image in `docs/art/`**, with the exact prompt in a text file next to it.
6. **In the README, say the art is AI-generated** and name the tools used. If you use free tilesets, credit them according to their licenses.
