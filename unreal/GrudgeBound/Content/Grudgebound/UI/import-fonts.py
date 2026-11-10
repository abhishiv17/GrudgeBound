"""Unreal editor Python: import only the already-subsetted local OFL font faces."""
from pathlib import Path
import unreal

folder = Path(__file__).parent / "Fonts"
names = {"YujiSyuku-Regular": "YujiSyukuRegular", "ShipporiMincho-Regular": "ShipporiMinchoRegular", "ShipporiMincho-Bold": "ShipporiMinchoBold"}
destination = "/Game/Grudgebound/UI/Fonts"
for filename, asset_name in names.items():
    path = destination + "/" + asset_name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(folder / (filename + ".ttf")))
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        factory = unreal.FontFileImportFactory()
        factory.set_editor_property("batch_create_font_asset", unreal.BatchCreateFontAsset.NO)
        task.set_editor_property("factory", factory)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    face = unreal.EditorAssetLibrary.load_asset(path)
    if not face:
        raise RuntimeError("Font face import failed: " + asset_name)
    face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
    # Data is inline; remove the workstation filename from the runtime object.
    face.set_editor_property("source_filename", "")
    unreal.EditorAssetLibrary.save_loaded_asset(face, only_if_is_dirty=False)
    unreal.log("Grudgebound font: " + asset_name)
unreal.SystemLibrary.quit_editor()
