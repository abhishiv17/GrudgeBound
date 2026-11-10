"""Create the separate co-op map without overwriting Lantern Ward or rebuilding unrelated content."""
import unreal

LEVEL = "/Game/Grudgebound/NinthBell"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    if not levels.load_level(LEVEL):
        raise RuntimeError("Could not load existing NinthBell")
else:
    if not levels.new_level_from_template(LEVEL, "/Engine/Maps/Templates/Template_Default"):
        raise RuntimeError("Could not create NinthBell")
    # Only the Basic template's floor is replaced by this district's floor.
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.StaticMeshActor):
            actors.destroy_actor(actor)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.GbCoopMode)

existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
def spawn(label, cls, position):
    if label in existing:
        a = existing[label]
        a.set_actor_location(unreal.Vector(*position), False, False)
        return a
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*position), unreal.Rotator())
    if not a:
        raise RuntimeError("Could not spawn " + label)
    a.set_actor_label(label)
    return a

spawn("GbCoop_District", unreal.GbCoopDistrict, (0,0,0))
objects = [
    ("Mira", unreal.GbBellObject.MIRA, (-2200,0,0)),
    ("Nara", unreal.GbBellObject.NARA, (400,2050,0)),
    ("Silk", unreal.GbBellObject.SILK, (-1300,-2050,0)),
    ("Eiden", unreal.GbBellObject.EIDEN, (650,0,0)),
    ("Notice", unreal.GbBellObject.CLUE_NOTICE, (-1000,-1850,0)),
    ("Contract", unreal.GbBellObject.CLUE_CONTRACT, (400,1700,0)),
    ("Seal", unreal.GbBellObject.CLUE_SEAL, (1900,-1800,0)),
    ("AnchorA", unreal.GbBellObject.ANCHOR_A, (1200,-600,0)),
    ("AnchorB", unreal.GbBellObject.ANCHOR_B, (1200,600,0)),
    ("Collector", unreal.GbBellObject.COLLECTOR, (1600,0,0)),
]
for label, kind, position in objects:
    a = spawn("GbCoop_" + label, unreal.GbCoopObject, position)
    a.set_editor_property("kind", kind)

# Reuse the template start as binder one; add three distinct, non-overlapping starts.
starts = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
first = next((a for a in starts if not a.get_actor_label().startswith("GbCoop_Start")), None)
if first:
    first.set_actor_location(unreal.Vector(-2700,-240,100), False, False)
else:
    spawn("GbCoop_Start0", unreal.PlayerStart, (-2700,-240,100))
for i in range(1,4):
    spawn("GbCoop_Start%d" % i, unreal.PlayerStart, (-2700,-240+i*160,100))
all_actors = actors.get_all_level_actors()
if len([a for a in all_actors if isinstance(a, unreal.PlayerStart)]) != 4:
    raise RuntimeError("Ninth Bell must have exactly four player starts; existing extras need owner review")
if len([a for a in all_actors if isinstance(a, unreal.GbCoopObject)]) != 10:
    raise RuntimeError("Ninth Bell must have exactly ten co-op objects")
if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL):
    raise RuntimeError("Could not save NinthBell (check write access / LFS read-only attribute)")
unreal.log("GB_COOP_MAP_READY " + LEVEL)
