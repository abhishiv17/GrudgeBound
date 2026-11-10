# Builds /Game/Grudgebound/LanternWard (docs/UNREAL_SETUP.md, section 4): Unreal's Basic level + the Ward greybox
# + a nav mesh volume for click-to-walk. Run with the editor closed:  unreal/tools/make-level.ps1
# Safe to run again: it rebuilds the level each time.
import unreal

TEMPLATE = "/Engine/Maps/Templates/Template_Default"  # the "Basic" level: sun, sky, fog, floor, player start
LEVEL = "/Game/Grudgebound/LanternWard"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
existing = unreal.EditorAssetLibrary.does_asset_exist(LEVEL)
if not (levels.load_level(LEVEL) if existing else levels.new_level_from_template(LEVEL, TEMPLATE)):
    raise RuntimeError("Could not create " + LEVEL)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# The greybox brings its own ground, so the template's floor goes.
for a in actors.get_all_level_actors():
    label = a.get_actor_label()
    if not existing and isinstance(a, unreal.StaticMeshActor):
        unreal.log("Grudgebound: remove %s" % label)
        actors.destroy_actor(a)
    else:
        unreal.log("Grudgebound: keep %s (%s)" % (label, a.get_class().get_name()))

ward = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.GbWardGreybox)), None)
ward = ward or actors.spawn_actor_from_class(unreal.GbWardGreybox, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
ward.set_actor_label("LanternWard")
ward.rebuild_ward()

# Warm paper daylight; existing runtime clock still controls sun/sky and the night grade.
fog = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.ExponentialHeightFog)), None)
fog = fog or actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -200))
fog.set_actor_label("WardHarbourHaze")
fog.component.set_editor_property("fog_density", 0.008)
fog.component.set_editor_property("fog_height_falloff", 0.3)
fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.62, 0.52, 0.40, 1))

grade = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == "WardPaperGrade"), None)
grade = grade or actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
grade.set_actor_label("WardPaperGrade")
grade.set_editor_property("unbound", True)
settings = grade.get_editor_property("settings")
settings.set_editor_property("override_color_saturation", True)
settings.set_editor_property("color_saturation", unreal.Vector4(0.85, 0.85, 0.85, 1))
grade.set_editor_property("settings", settings)

# 44 x 30 tiles of 1 m around the origin: X +-15 m, Y +-22 m. The editor spawns volumes as a 2 m cube.
nav = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.NavMeshBoundsVolume)), None)
nav = nav or actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
nav.set_actor_scale3d(unreal.Vector(16, 23, 4))
nav.set_actor_label("WardNavBounds")

# T-10: four small offsite rooms, with one local return door per entrance. No streaming/server movement.
def tile(col, row):
    return unreal.Vector((15-row)*100, (col-22)*100, 0)

def named_actor(label, cls, location):
    actor = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)
    actor = actor or actors.spawn_actor_from_class(cls, location, unreal.Rotator(0, 0, 0))
    actor.set_actor_label(label)
    return actor

buildings = [
    (unreal.GbBuilding.COUNTING_HOUSE, "Counting House", tile(8, 12.5), unreal.GbBuildingUse.BROKER, "Broker"),
    (unreal.GbBuilding.MENDER, "Mender's House", tile(15.5, 7.8), unreal.GbBuildingUse.MENDER, "Mender"),
    (unreal.GbBuilding.RITE_HALL, "Rite Hall", tile(27, 7.8), unreal.GbBuildingUse.RULES, "Kessa"),
    (unreal.GbBuilding.ROOM, "Your room", tile(17, 18.2), unreal.GbBuildingUse.ROOM, "Bed"),
]
for index, (kind, name, outside, counter_use, counter_name) in enumerate(buildings, 1):
    origin = unreal.Vector(20000+index*1600, 0, 0)
    room = named_actor("Interior " + name, unreal.GbBuildingRoom, origin)
    room.set_editor_property("building", kind)
    entrance = named_actor("Door " + name, unreal.GbBuildingDoor, outside)
    entrance.set_editor_property("building", kind)
    entrance.set_editor_property("use", unreal.GbBuildingUse.ENTER)
    entrance.set_editor_property("label", name)
    entrance.set_editor_property("target_feet", origin + unreal.Vector(-50,0,0))
    exit_door = named_actor("Exit " + name, unreal.GbBuildingDoor, origin + unreal.Vector(-300,0,0))
    exit_door.set_editor_property("building", kind)
    exit_door.set_editor_property("use", unreal.GbBuildingUse.EXIT)
    exit_door.set_editor_property("label", "Exit")
    exit_door.set_editor_property("target_feet", outside)
    counter = named_actor("Counter " + name, unreal.GbBuildingDoor, origin + unreal.Vector(65,0,0))
    counter.set_editor_property("building", kind)
    counter.set_editor_property("use", counter_use)
    counter.set_editor_property("label", counter_name)
    room_nav = named_actor("Nav " + name, unreal.NavMeshBoundsVolume, origin + unreal.Vector(0,0,100))
    room_nav.set_actor_scale3d(unreal.Vector(4.5,4,3))
    if kind == unreal.GbBuilding.RITE_HALL:
        shrine = named_actor("Vow Shrine", unreal.GbBuildingDoor, origin + unreal.Vector(80,-230,0))
        shrine.set_editor_property("building", kind)
        shrine.set_editor_property("use", unreal.GbBuildingUse.VOWS)
        shrine.set_editor_property("label", "Vow Shrine")
    if kind == unreal.GbBuilding.ROOM:
        chest = named_actor("Room chest", unreal.GbBuildingDoor, origin + unreal.Vector(65,-230,0))
        chest.set_editor_property("building", kind)
        chest.set_editor_property("use", unreal.GbBuildingUse.CASE)
        chest.set_editor_property("label", "Talisman Case")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL):
    raise RuntimeError("Could not save " + LEVEL)
unreal.log("Grudgebound: saved " + LEVEL)
