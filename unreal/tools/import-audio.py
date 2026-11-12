"""Unreal editor Python: import the WAVs written by make-audio.py as sound waves.

Run with the editor closed:
  UnrealEditor-Cmd.exe <GrudgeBound.uproject> -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
"""
from pathlib import Path
import unreal

destination = "/Game/Grudgebound/Audio"
folder = Path(unreal.Paths.project_content_dir()) / "Grudgebound" / "Audio"
files = sorted(folder.glob("*.wav"))
if not files:
    raise RuntimeError("No WAV files in " + str(folder) + "; run make-audio.py first")

tasks = []
for wav in files:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(wav))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", wav.stem)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for wav in files:
    sound = unreal.EditorAssetLibrary.load_asset(destination + "/" + wav.stem)
    if not sound:
        raise RuntimeError("Sound import failed: " + wav.stem)
    sound.set_editor_property("looping", wav.stem.endswith("_loop"))
    unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False)
    unreal.log("Grudgebound audio: " + wav.stem)
