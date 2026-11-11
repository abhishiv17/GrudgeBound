"""Unreal editor Python: import the battle art PNGs (written by export-battle-art.ts) as crisp UI textures.

Run with the editor closed:
  UnrealEditor-Cmd.exe <GrudgeBound.uproject> -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
"""
from pathlib import Path
import unreal

destination = "/Game/Grudgebound/UI/Battle"
folder = Path(unreal.Paths.project_content_dir()) / "Grudgebound" / "UI" / "Battle"
files = sorted(folder.glob("*.png"))
if not files:
    raise RuntimeError("No PNG files in " + str(folder) + "; run export-battle-art.ts first")

tasks = []
for png in files:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(png))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", png.stem)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for png in files:
    path = destination + "/" + png.stem
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not texture:
        raise RuntimeError("Texture import failed: " + png.stem)
    # Pixel art: no filtering, no mips, no block compression, never streamed.
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.log("Grudgebound battle art: " + png.stem)
