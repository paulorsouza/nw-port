"""Cria /Game/NW/Maps/L_Arena: chão, céu, luz, ponto de partida, bonecos de treino e música de fundo.

No Output Log (Python), depois do import_nw.py:
    py "<repo>/tools/ue/make_test_map.py"
"""
import os

import unreal

MAP = "/Game/NW/Maps/L_Arena"
MUSIC = os.path.expanduser("~/workspace/nw-port/extract/audio/music/gameplay/territory/brightwoods_ambient.ogg")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary

# O mapa é o de abertura do projeto (carregado ao iniciar, não dá para apagar): reaproveita e limpa os atores.
if eal.does_asset_exist(MAP):
    if not les.load_level(MAP):
        raise RuntimeError(f"não consegui abrir {MAP}")
    eas.destroy_actors(eas.get_all_level_actors())
elif not les.new_level(MAP):
    raise RuntimeError(f"não consegui criar {MAP}")


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
    """rot = (pitch, yaw, roll). unreal.Rotator posicional é (roll, pitch, yaw): usar nomes."""
    # spawn_actor_from_class distorce a rotação recebida (yaw 180 vira pitch 180): nasce sem rotação e gira depois.
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator())
    actor.set_actor_rotation(unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]), False)
    if label:
        actor.set_actor_label(label)
    return actor


# Chão: plano básico de 100 m.
floor = spawn(unreal.StaticMeshActor, label="Chao")
floor.static_mesh_component.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Plane"))
floor.set_actor_scale3d(unreal.Vector(100, 100, 1))
grid = eal.load_asset("/Engine/EngineMaterials/WorldGridMaterial")
if grid:
    floor.static_mesh_component.set_material(0, grid)

sun = spawn(unreal.DirectionalLight, (0, 0, 500), (-40, -35, 0), "Sol")
sun.light_component.set_intensity(8.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
spawn(unreal.SkyAtmosphere, label="Ceu")
sky = spawn(unreal.SkyLight, (0, 0, 300), label="LuzDoCeu")
sky.light_component.set_editor_property("real_time_capture", True)
spawn(unreal.ExponentialHeightFog, label="Neblina")
spawn(unreal.PlayerStart, (0, 0, 100), (0, 0, 0), "Inicio")

# Bonecos: parado, com escudo levantado, de costas (golpe por trás) e um que ataca quem chega perto.
char_cls = unreal.load_class(None, "/Script/NWPort.NWCharacter")
if char_cls:
    dummies = (("Boneco", 180, False, False), ("BonecoEscudo", 180, True, False),
               ("BonecoCostas", 0, False, False), ("BonecoAtacante", 180, False, True))
    for i, (label, yaw, blocks, attacks) in enumerate(dummies):
        d = spawn(char_cls, (500, (i - 1.5) * 300, 100), (0, yaw, 0), label)
        d.set_editor_property("dummy_blocks", blocks)
        d.set_editor_property("dummy_attacks", attacks)
else:
    unreal.log_warning("NWCharacter não encontrado: compile o projeto antes")

# Um Lost mais longe: persegue quem entrar a 15 m.
lost_cls = unreal.load_class(None, "/Script/NWPort.NWLost")
if lost_cls:
    spawn(lost_cls, (1800, 0, 100), (0, 180, 0), "Lost")

# Pista de travessia atrás do início (faixas do TraversalComponent do jogo): degraus, cercas finas
# para vault, muros para mantle e um alto demais. (profundidade, altura) em cm; frente dos blocos em x = -450.
cube = eal.load_asset("/Engine/BasicShapes/Cube")
course = (("degrau 50", 300, 50), ("degrau 100", 300, 100), ("cerca 60", 30, 60), ("cerca 100", 30, 100),
          ("muro 160", 300, 160), ("muro 250", 300, 250), ("muro 330 (pular)", 300, 330), ("muro 420", 300, 420))
for i, (label, depth, height) in enumerate(course):
    y = (i - (len(course) - 1) / 2) * 350
    block = spawn(unreal.StaticMeshActor, (-450 - depth / 2, y, height / 2), label=f"Pista {label}")
    block.static_mesh_component.set_static_mesh(cube)
    block.set_actor_scale3d(unreal.Vector(depth / 100, 2.5, height / 100))
    if grid:
        block.static_mesh_component.set_material(0, grid)
    sign = spawn(unreal.TextRenderActor, (-430, y, height + 60), (0, 0, 0), f"Placa {label}")
    sign.text_render.set_text(label)
    sign.text_render.set_editor_property("world_size", 36)
    sign.text_render.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    sign.text_render.set_text_render_color(unreal.Color(255, 230, 120, 255))

# Música de Brightwood ao fundo.
if os.path.exists(MUSIC):
    task = unreal.AssetImportTask()
    task.filename = MUSIC
    task.destination_path = "/Game/NW/Music"
    task.destination_name = "S_brightwoods_ambient"
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    music = eal.load_asset("/Game/NW/Music/S_brightwoods_ambient")
    if music:
        amb = spawn(unreal.AmbientSound, (0, 0, 200), label="Musica")
        amb.audio_component.set_sound(music)
        amb.audio_component.set_editor_property("is_ui_sound", True)  # sem atenuação: toca igual em todo o mapa
        amb.audio_component.set_editor_property("volume_multiplier", 0.5)

if not les.save_current_level():
    raise RuntimeError("não consegui salvar o mapa")
unreal.log(f"mapa criado: {MAP}")
