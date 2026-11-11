# Local asset inventory

The owner supplied these packs in `%USERPROFILE%/GrudgeboundCC0` on 9 November. No other packs are used.
Original pack License.txt files were checked locally; each explicitly identifies CC0. Model imports live
under Content/Grudgebound/Env and are covered by the repository's existing Unreal Git LFS patterns.

| Pack | Author | Source | Licence | Ward meshes |
|---|---|---|---|---|
| Fantasy Town Kit (2.0) | Kenney | https://kenney.nl/assets/fantasy-town-kit | CC0 1.0 | stall-red, wall-wood-window-shutters, roof-high, wall, lantern, fountain-round, fence |
| Nature Kit | Kenney | https://kenney.nl/assets/nature-kit | CC0 1.0 | plant_bush |
| Pirate Kit | Kenney | https://kenney.nl/assets/pirate-kit | CC0 1.0 | crate, barrel, platform-planks |

Licence: https://creativecommons.org/publicdomain/zero/1.0/ . Kenney attribution is retained here even
though CC0 does not require it. import-cc0.py defaults to all FBX meshes (GLB fallback) from these local
packs; `--used-only` imports the listed mesh slots and their referenced materials/textures. Filenames
use underscores in Unreal asset names. Missing packs retain the procedural geometry.

## Fonts and generated panel textures (T-12)

| Font | Author / upstream | Licence | Local subset |
|---|---|---|---|
| Yuji Syuku Regular (titles) | The Yuji Project Authors; https://github.com/Kinutafontfactory/Yuji | SIL OFL 1.1 | YujiSyuku-Regular.ttf, 109,476 bytes |
| Shippori Mincho Regular / Bold (body) | The Shippori Mincho Project Authors; https://github.com/fontdasu/ShipporiMincho | SIL OFL 1.1 | 73,544 / 73,620 bytes |
| Pixelify Sans SemiBold / Bold (headings, buttons) | The Pixelify Sans Project Authors; https://github.com/eifetx/Pixelify-Sans | SIL OFL 1.1 | 51,664 / 51,468 bytes (static cuts at wght 600 and 700 of the variable font) |
| Courier Prime Regular (body text) | The Courier Prime Project Authors; https://github.com/quoteunquoteapps/CourierPrime | SIL OFL 1.1 | 71,188 bytes (unmodified) |

The owner supplied the original TTFs under GrudgeboundCC0/Fonts. fontTools subsets retain the font names
and name tables, covering U+0000–00FF, U+2000–206F and U+2190–21FF. The supplied OFL texts declare no
Reserved Font Name and are copied beside the subsets under Content/Grudgebound/UI/Fonts. No new fonts
were downloaded. Three inline UFontFace assets carry their bytes; the runtime SourceFilename is cleared.
import-fonts.py runs with the full offscreen editor's -ExecutePythonScript flag (the Python commandlet
does not initialize Slate, which Unreal's font importer requires), then exits the editor.

11 Nov: the UI moved to the web client's pixel-paper look, so Pixelify Sans and Courier Prime (the web
client's two fonts, from Google Fonts) are now the fonts in use; the Mincho faces stay as the fallback when the
new faces are not imported. Their OFL texts are beside the TTFs. Panel frames, button faces and the title
screen's sky are generated in code (GbWidgets.cpp), no image files.

Paper grain and the uneven brush frame are generated in GbWidgets.cpp as one deterministic 64x64 texture,
shared by all panel/button helpers; no downloaded texture or AI asset. The palette remains in
GbGreyboxMaterials.h. All panels use the shared type, frame, button and visibility-animation helpers.

Battle art in Content/Grudgebound/UI/Battle is exported from the web client's own painters
(unreal/tools/export-battle-art.ts); no downloaded or AI-generated asset.

Player character and townsfolk (11 Nov): built in code from the engine basic shapes (GbExorcistLook.cpp),
following the web client's own player sprite (client/game/playerArt.ts: palette and design). No downloaded or
AI-generated asset.
