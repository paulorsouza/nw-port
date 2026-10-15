"""Builds /Game/NW/Maps/L_Arena: combat test arena with game pieces (walls and stands of the Legion arena, duel
ring, Bow targets, dummies, braziers, banners, rocks outside).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/make_test_map.py"

Needs the pieces in /Game/NW/Arena (tools/arena/meshes.txt → tools/linux/convert_gatherables.sh tools/arena/meshes.txt
extract/ue/arena → tools/ue/import_gatherables.py extract/ue/arena/limpa /Game/NW/Arena), the compiled C++
(NWCharacter, NWLost) and the player set. A missing piece is skipped with a warning.

Zones (long axis of the arena = X):
- center: start point;
- south side: shooting range, firing line on the east and targets at 10, 20, 30, 40 and 50 m;
- north side: dummies (idle, shield, facing away, attacker) and traversal course along the wall;
- west: duel ring with a Lost duelist inside;
- enemy waves: F10 menu > Arena.
Light to render: few Nanite pieces, floor from one plane with a world-coordinate material, 4 point lights
without shadows.
"""
import os

import unreal

MAP = "/Game/NW/Maps/L_Arena"
PIECES = "/Game/NW/Arena"
P_MAT = "/Game/NW/Materials"
SOIL = "/Game/NW/Cities/SouthWindsward/Terrain/T_jav_fst_forestsoil_a_diff"
GRASS = "/Game/NW/Cities/SouthWindsward/Terrain/T_jav_fst_feildgrass_a_diff"
MUSIC = os.path.join(os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", "..")),
                     "extract/audio/music/gameplay/territory/brightwoods_ambient.ogg")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# The map is the project's startup map (loaded at startup, cannot be deleted): reuse it and clear the actors.
if eal.does_asset_exist(MAP):
    if not les.load_level(MAP):
        raise RuntimeError(f"could not open {MAP}")
    eas.destroy_actors(eas.get_all_level_actors())
elif not les.new_level(MAP):
    raise RuntimeError(f"could not create {MAP}")


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None, folder=None):
    """rot = (pitch, yaw, roll). Positional unreal.Rotator is (roll, pitch, yaw): use names."""
    # spawn_actor_from_class distorts the given rotation (yaw 180 becomes pitch 180): spawn unrotated and rotate after.
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator())
    actor.set_actor_rotation(unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]), False)
    if label:
        actor.set_actor_label(label)
    if folder:
        actor.set_folder_path(folder)
    return actor


def piece(name):
    m = eal.load_asset(f"{PIECES}/{name}/{name}")
    if not m:
        unreal.log_warning(f"arena: missing piece {name}")
    return m


def bounds(mesh):
    b = mesh.get_bounding_box()
    return b.min, b.max


def place(name, loc, yaw=0.0, scale=1.0, label=None, folder="Arena/Cenario", ground=True):
    """Game piece with its base on the ground (point z = z of the mesh base)."""
    m = piece(name)
    if not m:
        return None
    lo, _ = bounds(m)
    z = loc[2] - lo.z * scale if ground else loc[2]
    a = spawn(unreal.StaticMeshActor, (loc[0], loc[1], z), (0, yaw, 0), label or name, folder)
    a.static_mesh_component.set_static_mesh(m)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    return a


def sign(text, loc, yaw=180.0, size=60):
    s = spawn(unreal.TextRenderActor, loc, (0, yaw, 0), f"Placa {text}", "Arena/Placas")
    s.text_render.set_text(text)
    s.text_render.set_editor_property("world_size", size)
    s.text_render.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    s.text_render.set_text_render_color(unreal.Color(255, 230, 160, 255))
    return s


def floor_material():
    """The game's soil in world coordinates (4 m tile) with a wide variation (37 m) blending in grass."""
    path = f"{P_MAT}/M_NW_ArenaChao"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    soil, grass = eal.load_asset(SOIL), eal.load_asset(GRASS)
    if not soil:
        return None
    m = tools.create_asset("M_NW_ArenaChao", P_MAT, unreal.Material, unreal.MaterialFactoryNew())
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1200, 0)
    xy = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1000, 0)
    xy.set_editor_property("r", True)
    xy.set_editor_property("g", True)
    mel.connect_material_expressions(wp, "", xy, "")

    def scaled(cm, y):
        d = mel.create_material_expression(m, unreal.MaterialExpressionDivide, -800, y)
        d.set_editor_property("const_b", cm)
        mel.connect_material_expressions(xy, "", d, "A")
        return d

    def sample(tex, uv, y):
        t = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -550, y)
        t.texture = tex
        mel.connect_material_expressions(uv, "", t, "UVs")
        return t

    fine = sample(soil, scaled(400.0, 0), 0)
    wide = sample(grass or soil, scaled(3700.0, 300), 300)
    patch = sample(soil, scaled(1300.0, 600), 600)
    mask = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -300, 600)
    mask.set_editor_property("g", True)
    mel.connect_material_expressions(patch, "", mask, "")
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -150, 150)
    mel.connect_material_expressions(fine, "", lerp, "A")
    mel.connect_material_expressions(wide, "", lerp, "B")
    mel.connect_material_expressions(mask, "", lerp, "Alpha")
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -150, 400)
    rough.set_editor_property("r", 0.92)
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


# ---------- floor, sky and light ----------
floor = spawn(unreal.StaticMeshActor, label="Chao", folder="Arena")
floor.static_mesh_component.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Plane"))
floor.set_actor_scale3d(unreal.Vector(400, 400, 1))   # 400 m: the horizon does not show the edge
mat = floor_material() or eal.load_asset("/Engine/EngineMaterials/WorldGridMaterial")
if mat:
    floor.static_mesh_component.set_material(0, mat)

sun = spawn(unreal.DirectionalLight, (0, 0, 800), (-38, -50, 0), "Sol", "Arena/Luz")
sun.light_component.set_intensity(8.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
spawn(unreal.SkyAtmosphere, label="Ceu", folder="Arena/Luz")
sky = spawn(unreal.SkyLight, (0, 0, 400), label="LuzDoCeu", folder="Arena/Luz")
sky.light_component.set_editor_property("real_time_capture", True)
fog = spawn(unreal.ExponentialHeightFog, label="Neblina", folder="Arena/Luz")
fog.component.set_editor_property("fog_density", 0.012)

# ---------- walls and stands (same frame: both pieces are centered on the origin) ----------
walls = piece("legion_arena_walls_a")
yaw = 0.0
half_long, half_short = 3800.0, 2300.0
if walls:
    lo, hi = bounds(walls)
    ex, ey = hi.x - lo.x, hi.y - lo.y
    yaw = 0.0 if ex >= ey else 90.0
    half_long, half_short = max(ex, ey) * 0.5 * 0.86, min(ex, ey) * 0.5 * 0.82
    muros = place("legion_arena_walls_a", (0, 0, 0), yaw, label="Muros")
    # The stands share the walls' frame (base 12.6 m below the ground): same height, not resting on the ground.
    # Resting on it, they rose 12.6 m and the awnings hung in the sky, casting shadow on the arena (2026-10-12).
    place("legion_arena_seating_01", (0, 0, muros.get_actor_location().z if muros else 0), yaw, label="Arquibancada", ground=False)
L, W = half_long, half_short
unreal.log(f"arena: half length {L / 100:.1f} m, half width {W / 100:.1f} m")

# ---------- start ----------
spawn(unreal.PlayerStart, (0, 0, 120), (0, 0, 0), "Inicio", "Arena")

# ---------- shooting range (south): firing line on the east, targets toward the west ----------
lane_y = -W * 0.62
fire_x = L * 0.85
sign("Linha de tiro", (fire_x, lane_y, 260), 180)
char_cls = unreal.load_class(None, "/Script/NWPort.NWCharacter")
for d in (10, 20, 30, 40, 50):
    x = fire_x - d * 100
    if x < -L:
        break
    place("jav_prp_targets_a_group" if d % 20 else "jav_prp_targets_b_group", (x, lane_y - 250, 0), 90, label=f"Alvo {d} m",
          folder="Arena/Tiro")
    sign(f"{d} m", (x, lane_y, 300), 0)
    if char_cls:
        dummy = spawn(char_cls, (x, lane_y + 150, 100), (0, 0, 0), f"Boneco {d} m", "Arena/Tiro")
        dummy.set_editor_property("dummy_blocks", False)
        dummy.set_editor_property("dummy_attacks", False)

# ---------- training dummies (north) ----------
row_y = W * 0.35
sign("Bonecos", (0, row_y - 250, 280), 180)
if char_cls:
    dummies = (("Boneco", 180, False, False), ("Boneco escudo", 180, True, False),
               ("Boneco de costas", 0, False, False), ("Boneco atacante", 180, False, True))
    for i, (label, dyaw, blocks, attacks) in enumerate(dummies):
        x = (i - 1.5) * 450
        d = spawn(char_cls, (x, row_y, 100), (0, dyaw - 270, 0), label, "Arena/Bonecos")  # 180 = facing the center (-Y)
        d.set_editor_property("dummy_blocks", blocks)
        d.set_editor_property("dummy_attacks", attacks)
        sign(label, (x, row_y - 90, 230), 180, 30)
for i, x in enumerate((-1100, 1100)):
    place("jav_weapon_rack", (x, row_y + 250, 0), 180, label=f"Armas {i}", folder="Arena/Bonecos")
    place("jav_prp_targetdummy", (x, row_y - 150, 0), 270, label=f"Boneco do jogo {i}", folder="Arena/Bonecos")
if not char_cls:
    unreal.log_warning("NWCharacter not found: compile the project first")

# ---------- traversal (along the north wall): steps, fences for vault, walls for mantle ----------
cube = eal.load_asset("/Engine/BasicShapes/Cube")
course_y = W * 0.82
course = (("degrau 50", 300, 50), ("degrau 100", 300, 100), ("cerca 60", 30, 60), ("cerca 100", 30, 100),
          ("muro 160", 300, 160), ("muro 250", 300, 250), ("muro 330 (pular)", 300, 330), ("muro 420", 300, 420))
for i, (label, depth, height) in enumerate(course):
    x = (i - (len(course) - 1) / 2) * 380
    block = spawn(unreal.StaticMeshActor, (x, course_y, height / 2), label=f"Pista {label}", folder="Arena/Travessia")
    block.static_mesh_component.set_static_mesh(cube)
    block.set_actor_scale3d(unreal.Vector(2.5, depth / 100, height / 100))
    if mat:
        block.static_mesh_component.set_material(0, mat)
    sign(label, (x, course_y - depth / 2 - 20, height + 60), 180, 32)

# ---------- duel ring (west) with a Lost ----------
ring_x = -L * 0.62
place("jav_struc_def_marauder_arena_ring", (ring_x, 0, 0), 0, label="Circulo de duelo", folder="Arena/Duelo")
sign("Duelo", (ring_x, 0, 420), 90)
lost_cls = unreal.load_class(None, "/Script/NWPort.NWLost")
if lost_cls:
    lost = spawn(lost_cls, (ring_x - 200, 0, 120), (0, 0, 0), "Lost duelista", "Arena/Duelo")
    lost.set_editor_property("aggro_range", 900.0)

# ---------- braziers with light, banners ----------
for i, (fx, fy) in enumerate(((0.55, 0.75), (-0.55, 0.75), (0.55, -0.88), (-0.55, -0.88))):
    x, y = fx * L, fy * W
    place("pre_ruins_brazier_a", (x, y, 0), 0, label=f"Braseiro {i}", folder="Arena/Luz")
    light = spawn(unreal.PointLight, (x, y, 260), label=f"Fogo {i}", folder="Arena/Luz")
    lc = light.point_light_component
    lc.set_intensity(4000.0)
    lc.set_light_color(unreal.LinearColor(1.0, 0.55, 0.25, 1.0))
    lc.set_editor_property("attenuation_radius", 1400.0)
    lc.set_editor_property("cast_shadows", False)
for i, (fx, fy) in enumerate(((0.95, 0.9), (-0.95, 0.9), (0.95, -0.9), (-0.95, -0.9))):
    place("jav_prp_outpost_banner_a", (fx * L, fy * W, 0), 0 if fx > 0 else 180, label=f"Estandarte {i}", folder="Arena/Cenario")

# ---------- outside the walls: rocks and cliffs (background) ----------
outer = [("jav_fst_cliff_10x20_a", 1.0), ("jav_fst_boulder_lg_a", 1.2), ("jav_fst_cliff_10x10_a", 1.0),
         ("jav_fst_boulder_lg_b", 1.4), ("jav_fst_boulder_md_a", 1.6), ("jav_fst_cliff_10x20_a", 1.2)]
import math
for i in range(18):
    ang = i / 18 * 2 * math.pi
    name, sc = outer[i % len(outer)]
    r = 1.0 + 0.12 * ((i * 7) % 3)
    x, y = math.cos(ang) * (L / 0.86 + 2500) * r, math.sin(ang) * (W / 0.82 + 2500) * r
    place(name, (x, y, -150), math.degrees(ang) + 90 + (i * 37) % 40, sc, f"Rocha {i}", "Arena/Fora")
for i in range(6):
    ang = (i + 0.5) / 6 * 2 * math.pi
    place(("jav_fst_boulder_md_b", "jav_fst_boulder_md_c", "jav_fst_boulder_lg_frag_a")[i % 3],
          (math.cos(ang) * L * 0.97, math.sin(ang) * W * 0.95, -30), i * 60, 0.8, f"Pedra {i}", "Arena/Cenario")

# ---------- music ----------
if os.path.exists(MUSIC):
    task = unreal.AssetImportTask()
    task.filename = MUSIC
    task.destination_path = "/Game/NW/Music"
    task.destination_name = "S_brightwoods_ambient"
    task.automated = True
    task.replace_existing = True
    task.save = True
    if not eal.does_asset_exist("/Game/NW/Music/S_brightwoods_ambient"):
        tools.import_asset_tasks([task])
    music = eal.load_asset("/Game/NW/Music/S_brightwoods_ambient")
    if music:
        amb = spawn(unreal.AmbientSound, (0, 0, 200), label="Musica", folder="Arena")
        amb.audio_component.set_sound(music)
        amb.audio_component.set_editor_property("is_ui_sound", True)  # no attenuation: plays the same across the map
        amb.audio_component.set_editor_property("volume_multiplier", 0.4)

if not les.save_current_level():
    raise RuntimeError("could not save the map")
unreal.log(f"map created: {MAP}")
