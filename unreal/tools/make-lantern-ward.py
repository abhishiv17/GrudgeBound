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

# A fresh Ward every time: its parts change between versions, and an old saved one would keep stale pieces.
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.GbWardGreybox):
        actors.destroy_actor(a)
ward = actors.spawn_actor_from_class(unreal.GbWardGreybox, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
ward.set_actor_label("LanternWard")
ward.rebuild_ward()

# Warm paper daylight; existing runtime clock still controls sun/sky and the night grade.
fog = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.ExponentialHeightFog)), None)
fog = fog or actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -200))
fog.set_actor_label("WardHarbourHaze")
fog.component.set_editor_property("fog_density", 0.014)
fog.component.set_editor_property("fog_height_falloff", 0.3)
fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.62, 0.52, 0.40, 1))

grade = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == "WardPaperGrade"), None)
grade = grade or actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
grade.set_actor_label("WardPaperGrade")
grade.set_editor_property("unbound", True)
settings = grade.get_editor_property("settings")
settings.set_editor_property("override_color_saturation", True)
settings.set_editor_property("color_saturation", unreal.Vector4(1.06, 1.06, 1.06, 1))
settings.set_editor_property("override_color_contrast", True)
settings.set_editor_property("color_contrast", unreal.Vector4(1.06, 1.06, 1.06, 1))
settings.set_editor_property("override_vignette_intensity", True)
settings.set_editor_property("vignette_intensity", 0.45)
settings.set_editor_property("override_bloom_intensity", True)
settings.set_editor_property("bloom_intensity", 0.9)
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
    # Each door stands in front of the door piece of its house (GbWardGreybox.cpp, Dress: Houses).
    (unreal.GbBuilding.COUNTING_HOUSE, "Counting House", tile(27, 18.3), unreal.GbBuildingUse.BROKER, "Broker"),
    (unreal.GbBuilding.MENDER, "Mender's House", tile(17.25, 8.2), unreal.GbBuildingUse.MENDER, "Mender"),
    (unreal.GbBuilding.RITE_HALL, "Rite Hall", tile(27.25, 7.7), unreal.GbBuildingUse.RULES, "Kessa"),
    (unreal.GbBuilding.ROOM, "Your room", tile(18, 18.3), unreal.GbBuildingUse.ROOM, "Bed"),
]
for index, (kind, name, outside, counter_use, counter_name) in enumerate(buildings, 1):
    origin = unreal.Vector(20000+index*1600, 0, 0)
    room = named_actor("Interior " + name, unreal.GbBuildingRoom, origin)
    room.set_editor_property("building", kind)
    entrance = named_actor("Door " + name, unreal.GbBuildingDoor, outside)
    entrance.set_editor_property("building", kind)
    entrance.set_editor_property("use", unreal.GbBuildingUse.ENTER)
    entrance.set_editor_property("label", name)
    entrance.set_actor_location(outside, False, False)
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

# Tide Ward (P4-U11): a second district in the same level, 160 m south across the water, reached by ferry.
# The door boards a runtime boat; the UI director carries the player across, then lands on the other pier.
TIDE = unreal.Vector(-16000, 0, 0)
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.GbTideWard):
        actors.destroy_actor(a)
tide = actors.spawn_actor_from_class(unreal.GbTideWard, TIDE, unreal.Rotator(0, 0, 0))
tide.set_actor_label("TideWard")
tide.rebuild_ward()
tide_nav = named_actor("TideNavBounds", unreal.NavMeshBoundsVolume, TIDE + unreal.Vector(0, 0, 100))
tide_nav.set_actor_location(TIDE + unreal.Vector(0, 0, 100), False, False)
tide_nav.set_actor_scale3d(unreal.Vector(16, 23, 4))
ferries = [
    ("Ferry to Tide Ward", tile(21.5, 27.4), tide.get_landing_location(), "tide"),
    ("Ferry to Lantern Ward", tide.get_ferry_location(), tile(21.5, 25.4), "lantern"),
]
for label, at, target, ward_id in ferries:
    ferry = named_actor(label, unreal.GbBuildingDoor, at)
    ferry.set_actor_location(at, False, False)
    ferry.set_editor_property("building", unreal.GbBuilding.OUTSIDE)
    ferry.set_editor_property("use", unreal.GbBuildingUse.TRAVEL)
    ferry.set_editor_property("label", label)
    ferry.set_editor_property("travel_ward", ward_id)
    ferry.set_editor_property("target_feet", target)

# Personal discoveries share the existing proximity/click interaction. Their contents are server-owned.
finds = [
    ("alley-parcel", "Inspect parcel", tile(12, 14.8)),
    ("lamp-kit", "Inspect lamp kit", tile(7, 12)),
    ("delivery-slip", "Read delivery slip", tile(8.8, 12)),
    ("reed-bundle", "Recover netting", TIDE + tile(33, 17.2)),
    ("dock-tin", "Inspect supply tin", TIDE + tile(18.5, 23)),
]
for find_id, label, at in finds:
    find = named_actor("Discovery " + find_id, unreal.GbBuildingDoor, at)
    find.set_actor_location(at, False, False)
    find.set_editor_property("building", unreal.GbBuilding.OUTSIDE)
    find.set_editor_property("use", unreal.GbBuildingUse.DISCOVER)
    find.set_editor_property("discovery_id", find_id)
    find.set_editor_property("label", label)

store_origin = unreal.Vector(28000, 0, 0)
store_outside = TIDE + tile(12.75, 13.7)
store = named_actor("Interior Sealed Storehouse", unreal.GbBuildingRoom, store_origin)
store.set_editor_property("building", unreal.GbBuilding.STOREHOUSE)
for label, at, use, target in [
    ("Sealed Storehouse", store_outside, unreal.GbBuildingUse.ENTER, store_origin + unreal.Vector(-50, 0, 0)),
    ("Storehouse exit", store_origin + unreal.Vector(-300, 0, 0), unreal.GbBuildingUse.EXIT, store_outside),
    ("Storehouse chest", store_origin + unreal.Vector(65, 0, 0), unreal.GbBuildingUse.DISCOVER, unreal.Vector()),
]:
    door = named_actor(label, unreal.GbBuildingDoor, at)
    door.set_editor_property("building", unreal.GbBuilding.STOREHOUSE)
    door.set_editor_property("use", use)
    door.set_editor_property("target_feet", target)
    door.set_editor_property("label", label)
    door.set_editor_property("discovery_id", "storehouse-chest" if use == unreal.GbBuildingUse.DISCOVER else "storehouse-door" if use == unreal.GbBuildingUse.ENTER else "")
store_nav = named_actor("Nav Sealed Storehouse", unreal.NavMeshBoundsVolume, store_origin + unreal.Vector(0, 0, 100))
store_nav.set_actor_scale3d(unreal.Vector(4.5, 4, 3))

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL):
    raise RuntimeError("Could not save " + LEVEL)
unreal.log("Grudgebound: saved " + LEVEL)
