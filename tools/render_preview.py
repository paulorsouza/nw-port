# blender -b -P tools/render_preview.py -- <entrada.glb> <saida.png> [frame]
# Importa um glTF, enquadra a câmera de frente e renderiza com EEVEE.
import sys
import math
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
frame = int(args[2]) if len(args) > 2 else None

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
scene = bpy.context.scene
if frame is not None:
    scene.frame_set(frame)

meshes = [o for o in scene.objects if o.type == "MESH"]
deps = bpy.context.evaluated_depsgraph_get()
pts = []
for o in meshes:
    ev = o.evaluated_get(deps)
    pts += [ev.matrix_world @ Vector(c) for c in ev.bound_box]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center, size = (lo + hi) / 2, (hi - lo).z
print(f"bbox min={tuple(round(v, 3) for v in lo)} max={tuple(round(v, 3) for v in hi)} meshes={len(meshes)}")

cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
cam.data.lens = 50
dist = size * 1.9
# Personagens do jogo olham para -Y depois da importação glTF.
cam.location = center + Vector((dist * 0.35, -dist, size * 0.1))
cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()
scene.camera = cam

for rot, energy in (((math.radians(50), 0, math.radians(150)), 4.0), ((math.radians(60), 0, math.radians(-30)), 1.5)):
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = energy
    sun.rotation_euler = rot
    scene.collection.objects.link(sun)

world = bpy.data.worlds.new("w")
world.color = (0.08, 0.08, 0.1)
scene.world = world
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 720, 960
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
