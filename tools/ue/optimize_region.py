"""Deixa o L_SouthWindsward mais leve sem tirar nada do lugar.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/optimize_region.py"

- Componentes instanciados do cenário (atores Instancias_* das pastas Regiao/*): distância de desenho pelo tamanho
  da malha (pequenos somem antes) e, nos pequenos, sem sombra e fora da iluminação global (Lumen).
- Luzes importadas dos arquivos do jogo: sem sombra (centenas de luzes com sombra dinâmica derrubam o quadro) e
  com alcance limitado.
- Registra no log quantos atores há por classe.
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
unreal.log_warning("OTIMIZAR classes: " + str(collections.Counter(type(a).__name__ for a in actors).most_common(12)))

# Distância de desenho (cm) pelo raio da malha.
def cull_for(radius):
    if radius < 60:
        return 6000
    if radius < 200:
        return 15000
    if radius < 600:
        return 40000
    return 0  # grandes (penhascos, ruínas) sempre


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
    raise RuntimeError("não consegui salvar o mapa")
unreal.log_warning(f"OTIMIZAR {ism} componentes instanciados ({small} pequenos sem sombra/Lumen), {lights} luzes sem sombra, "
                   f"{len(actors)} atores, {time.time() - t0:.0f}s")
