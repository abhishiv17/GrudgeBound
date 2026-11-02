# Builds /Game/Grudgebound/LanternWard (docs/UNREAL_SETUP.md, section 4): Unreal's Basic level + the Ward greybox
# + a nav mesh volume for click-to-walk. Run with the editor closed:  unreal/tools/make-level.ps1
# Safe to run again: it rebuilds the level each time.
import unreal

TEMPLATE = "/Engine/Maps/Templates/Template_Default"  # the "Basic" level: sun, sky, fog, floor, player start
LEVEL = "/Game/Grudgebound/LanternWard"

if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    unreal.EditorAssetLibrary.delete_asset(LEVEL)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.new_level_from_template(LEVEL, TEMPLATE):
    raise RuntimeError("Could not create " + LEVEL)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# The greybox brings its own ground, so the template's floor goes.
for a in actors.get_all_level_actors():
    label = a.get_actor_label()
    if isinstance(a, unreal.StaticMeshActor):
        unreal.log("Grudgebound: remove %s" % label)
        actors.destroy_actor(a)
    else:
        unreal.log("Grudgebound: keep %s (%s)" % (label, a.get_class().get_name()))

ward = actors.spawn_actor_from_class(unreal.GbWardGreybox, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
ward.set_actor_label("LanternWard")

# 44 x 30 tiles of 1 m around the origin: X +-15 m, Y +-22 m. The editor spawns volumes as a 2 m cube.
nav = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
nav.set_actor_scale3d(unreal.Vector(16, 23, 4))
nav.set_actor_label("WardNavBounds")

levels.save_current_level()
unreal.log("Grudgebound: saved " + LEVEL)
