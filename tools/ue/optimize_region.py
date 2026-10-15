"""Makes L_SouthWindsward lighter without moving anything.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/optimize_region.py"

- Instanced scenery components (Instancias_* actors in the Regiao/* folders): draw distance by mesh size (small
  ones disappear first) and, for the small ones, no shadow and out of global illumination (Lumen).
- Lights imported from the game files: no shadow (hundreds of lights with dynamic shadows tank the frame rate) and
  limited range.
- Logs how many actors there are per class.
"""
import collections
import time

import unreal

MAP = "/Game/NW/Maps/L_SouthWindsward"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
t0 = time.time()
les.load_level(MAP)
actors = eas.get_all_level_actors()
unreal.log_warning("OPTIMIZE classes: " + str(collections.Counter(type(a).__name__ for a in actors).most_common(12)))

# Draw distance (cm) by mesh radius.
def cull_for(radius):
    if radius < 60:
        return 6000
    if radius < 200:
        return 15000
    if radius < 600:
        return 40000
    return 0  # large ones (cliffs, ruins) always


ism = small = 0
for a in actors:
    if not a.get_actor_label().startswith("Instancias_"):
        continue
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        mesh = c.get_editor_property("static_mesh")
        if not mesh:
            continue
        radius = mesh.get_bounds().sphere_radius
        cull = cull_for(radius)
        if cull:
            c.set_editor_property("instance_start_cull_distance", int(cull * 0.8))
            c.set_editor_property("instance_end_cull_distance", cull)
        if radius < 200:
            c.set_editor_property("cast_shadow", False)
            c.set_editor_property("affect_distance_field_lighting", False)
            c.set_editor_property("affect_dynamic_indirect_lighting", False)
            small += 1
        ism += 1

lights = 0
for a in actors:
    for lc in a.get_components_by_class(unreal.LocalLightComponent):
        lc.set_editor_property("cast_shadows", False)
        lc.set_editor_property("attenuation_radius", min(lc.get_editor_property("attenuation_radius"), 1500.0))
        lights += 1

if not les.save_current_level():
    raise RuntimeError("could not save the map")
unreal.log_warning(f"OPTIMIZE {ism} instanced components ({small} small without shadow/Lumen), {lights} lights without shadow, "
                   f"{len(actors)} actors, {time.time() - t0:.0f}s")
