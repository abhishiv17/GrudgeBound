"""Editor Python: import local Kenney packs only; no downloads.

Imports the pieces the Lantern Ward dressing uses (GbWardGreybox.cpp, Dress) and writes every imported mesh's
bounds to Saved/cc0-bounds.json, which is what the dressing's piece sizes were measured from.
Run with the editor closed:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=unreal/tools/import-cc0.py -unattended -nosplash -nullrhi
"""
import json
import os
from pathlib import Path
import unreal

root = Path(os.environ.get("GB_CC0", str(Path(os.environ["USERPROFILE"]) / "GrudgeboundCC0"))).resolve()

NATURE = """
tree_default tree_default_dark tree_default_fall tree_oak tree_oak_dark tree_oak_fall tree_detailed tree_detailed_dark
tree_detailed_fall tree_fat tree_fat_fall tree_tall tree_tall_dark tree_thin tree_thin_fall tree_small tree_small_dark
tree_simple tree_simple_fall tree_cone tree_cone_dark tree_blocks tree_blocks_fall tree_plateau tree_pineRoundA
tree_pineRoundC tree_pineTallA tree_pineSmallA
plant_bush plant_bushDetailed plant_bushLarge plant_bushLargeTriangle plant_bushSmall plant_bushTriangle plant_flatShort
plant_flatTall flower_purpleA flower_purpleB flower_purpleC flower_redA flower_redB flower_redC flower_yellowA
flower_yellowB flower_yellowC grass grass_large grass_leafs grass_leafsLarge
rock_smallA rock_smallB rock_smallC rock_smallD rock_smallE rock_smallF rock_smallFlatA rock_smallFlatB rock_largeA
rock_largeB rock_largeC rock_tallA rock_tallB rock_tallC stone_smallA stone_smallB stone_smallC stone_smallFlatA
stone_largeA stone_largeB stone_tallA stone_tallB
mushroom_red mushroom_redGroup mushroom_tan mushroom_tanGroup log log_large log_stack log_stackLarge stump_old
stump_round stump_roundDetailed lily_large lily_small pot_large pot_small sign statue_obelisk statue_column
statue_columnDamaged statue_head statue_ring statue_block campfire_stones campfire_logs canoe canoe_paddle bed
tent_smallOpen tent_detailedOpen path_stone path_stoneCircle path_stoneCorner path_stoneEnd path_wood crop_pumpkin
crop_melon crop_carrot crop_turnip crops_cornStageD crops_wheatStageB crops_dirtRow crops_dirtDoubleRow fence_simple
fence_simpleLow fence_gate fence_corner fence_planks hanging_moss platform_stone bridge_wood bridge_stone
""".split()

# "*" = the whole pack (the town and harbour kits are small; the Ward uses most of them).
packs = {"FantasyTown": "*", "Pirate": "*", "Nature": set(NATURE)}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for pack, names in packs.items():
    folder = root if root.name == pack else root / pack
    if not folder.is_dir():
        unreal.log_warning("CC0 pack missing: " + str(folder))
        continue
    license_file = folder / "License.txt"
    if not license_file.exists() or "CC0" not in license_file.read_text(encoding="utf-8-sig"):
        raise RuntimeError("Expected Kenney CC0 License.txt: " + str(folder))
    # Prefer FBX and avoid importing the OBJ/GLB duplicates of the same meshes.
    sources = sorted(folder.rglob("*.fbx")) or sorted(folder.rglob("*.glb"))
    for source in sources:
        if names != "*" and source.stem not in names:
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
            unreal.log_warning("Grudgebound CC0: nothing imported from " + str(source))
            continue
        unreal.log("Grudgebound CC0: " + pack + "/" + name)

# Persist ISM shader permutations: editor auto-fixes alone do not make packaged materials safe.
bounds = {}
for asset_path in unreal.EditorAssetLibrary.list_assets("/Game/Grudgebound/Env", recursive=True, include_folder=False):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if isinstance(asset, unreal.Material):
        unreal.MaterialEditingLibrary.set_base_material_usage(asset, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
        unreal.MaterialEditingLibrary.recompile_material(asset)
    elif isinstance(asset, unreal.StaticMesh):
        box = asset.get_bounding_box()
        key = asset_path.split(".")[0].replace("/Game/Grudgebound/Env/", "")
        bounds[key] = [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)]
unreal.EditorAssetLibrary.save_directory("/Game/Grudgebound/Env", only_if_is_dirty=True, recursive=True)
out = Path(unreal.Paths.project_saved_dir()) / "cc0-bounds.json"
out.write_text(json.dumps(bounds, indent=0, sort_keys=True), encoding="utf-8")
unreal.log("Grudgebound CC0: %d meshes, bounds in %s" % (len(bounds), out))
