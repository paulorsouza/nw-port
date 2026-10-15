"""Moves a city's PlayerStart to the free spot closest to where it is.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/fix_spawn.py <Name>"

The terrain point (street closest to the center) fell inside the statue in the South Windsward plaza: the character
spawned inside it and the camera showed the statue from the inside. Searches in a spiral for a spot where no city mesh
occupies the body height (knee to head) within a 1.2 m radius. Uses the mesh bounding boxes: collision traces
hit nothing in the commandlet.
"""
import math
import sys

import unreal

NAME = sys.argv[1]
MAP = f"/Game/NW/Maps/L_{NAME}"
RADIUS = 120.0       # cm, clearance around the body
BODY = (40.0, 190.0)  # cm above the ground: knee to head
STEP = 100.0
MAX_DIST = 4000.0

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(MAP)

actors = eas.get_all_level_actors()
start = next(a for a in actors if isinstance(a, unreal.PlayerStart))
origin = start.get_actor_location()
ground = origin.z - 150.0  # the terrain point sits 1.5 m above the ground

boxes = []
for a in actors:
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    label = a.get_actor_label()
    if label.startswith("terreno_") or label == "ChaoSeguranca":
        continue
    c, e = a.get_actor_bounds(False)
    if e.x > 3000 or e.y > 3000:
        continue  # huge pieces (whole floor/wall) are no use as local obstacles
    boxes.append((c.x - e.x, c.x + e.x, c.y - e.y, c.y + e.y, c.z - e.z, c.z + e.z, label))


def blocked(x, y):
    lo, hi = ground + BODY[0], ground + BODY[1]
    for x0, x1, y0, y1, z0, z1, label in boxes:
        if z1 < lo or z0 > hi:
            continue
        dx = max(x0 - x, 0.0, x - x1)
        dy = max(y0 - y, 0.0, y - y1)
        if dx * dx + dy * dy < RADIUS * RADIUS:
            return label
    return None


hit = blocked(origin.x, origin.y)
unreal.log(f"SPAWN current {origin} -> {'blocked by ' + hit if hit else 'free'}")
best = None
r = 0.0
while best is None and r <= MAX_DIST:
    r += STEP
    n = max(8, int(2 * math.pi * r / STEP))
    for i in range(n):
        ang = 2 * math.pi * i / n
        x, y = origin.x + r * math.cos(ang), origin.y + r * math.sin(ang)
        if not blocked(x, y):
            best = (x, y)
            break
if hit and best:
    start.set_actor_location(unreal.Vector(best[0], best[1], origin.z), False, False)
    if not les.save_current_level():
        raise RuntimeError("could not save the map")
    unreal.log(f"SPAWN moved to ({best[0]:.0f}, {best[1]:.0f}, {origin.z:.0f}), {r:.0f} cm from the original")
elif not hit:
    unreal.log("SPAWN was already free")
else:
    unreal.log("SPAWN no free spot within the radius")
