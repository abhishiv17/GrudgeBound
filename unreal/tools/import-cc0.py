"""Editor Python: import local Kenney packs only; no downloads. --used-only imports the Ward's slots."""
import argparse
import os
from pathlib import Path
import unreal

parser = argparse.ArgumentParser()
parser.add_argument("folder", nargs="?", default=str(Path(os.environ["USERPROFILE"]) / "GrudgeboundCC0"))
parser.add_argument("--used-only", action="store_true")
args = parser.parse_args()
root = Path(args.folder).resolve()
used = {
    "FantasyTown": {"stall-red", "wall-wood-window-shutters", "roof-high", "wall", "lantern", "fountain-round", "fence"},
    "Nature": {"plant_bush"},
    "Pirate": {"crate", "barrel", "platform-planks"},
}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for pack, names in used.items():
    folder = root if root.name == pack else root / pack
    if not folder.is_dir():
        unreal.log_warning("CC0 pack missing: " + str(folder))
        continue
    license_file = folder / "License.txt"
    if not license_file.exists() or "CC0" not in license_file.read_text(encoding="utf-8-sig"):
        raise RuntimeError("Expected Kenney CC0 License.txt: " + str(folder))
    # Prefer FBX and avoid importing the OBJ/GLB duplicates of the same meshes.
    sources = sorted(folder.rglob("*.fbx"))
    if not sources:
        sources = sorted(folder.rglob("*.glb"))
    for source in sources:
        if args.used_only and source.stem not in names:
            continue
        name = source.stem.replace("-", "_")
        destination = "/Game/Grudgebound/Env/" + pack
        if unreal.EditorAssetLibrary.does_asset_exist(destination + "/" + name):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        if source.suffix.lower() == ".fbx":
            options = unreal.FbxImportUI()
            options.set_editor_property("import_as_skeletal", False)
            options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
            options.set_editor_property("automated_import_should_detect_type", False)
            options.set_editor_property("import_materials", True)
            options.set_editor_property("import_textures", True)
            data = options.static_mesh_import_data
            data.set_editor_property("combine_meshes", True)
            data.set_editor_property("auto_generate_collision", False)
            data.set_editor_property("build_nanite", False)
            task.set_editor_property("options", options)
            task.set_editor_property("factory", unreal.FbxFactory())
        asset_tools.import_asset_tasks([task])
        if not task.imported_object_paths:
            raise RuntimeError("No assets imported from " + str(source))
        unreal.log("Grudgebound CC0: " + pack + "/" + name)
# Persist ISM shader permutations: editor auto-fixes alone do not make packaged materials safe.
for asset_path in unreal.EditorAssetLibrary.list_assets("/Game/Grudgebound/Env", recursive=True, include_folder=False):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if isinstance(asset, unreal.Material):
        unreal.MaterialEditingLibrary.set_base_material_usage(asset, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
        unreal.MaterialEditingLibrary.recompile_material(asset)
unreal.EditorAssetLibrary.save_directory("/Game/Grudgebound/Env", only_if_is_dirty=True, recursive=True)
