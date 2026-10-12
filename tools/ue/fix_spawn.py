"""Move o PlayerStart de uma cidade para o ponto livre mais perto de onde ele está.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/fix_spawn.py <Nome>"

O ponto do terreno (rua mais perto do centro) caía dentro da estátua da praça da South Windsward: o personagem
nascia dentro dela e a câmera mostrava a estátua por dentro. Busca em espiral um ponto em que nenhuma malha da
cidade ocupe a altura do corpo (do joelho à cabeça) num raio de 1,2 m. Usa as caixas das malhas: raios de
colisão não acertam nada no commandlet.
"""
import math
import sys

import unreal

NAME = sys.argv[1]
MAP = f"/Game/NW/Maps/L_{NAME}"
RADIUS = 120.0       # cm, folga em volta do corpo
BODY = (40.0, 190.0)  # cm acima do chão: do joelho à cabeça
STEP = 100.0
MAX_DIST = 4000.0

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(MAP)

actors = eas.get_all_level_actors()
start = next(a for a in actors if isinstance(a, unreal.PlayerStart))
origin = start.get_actor_location()
ground = origin.z - 150.0  # o ponto do terreno fica 1,5 m acima do chão

boxes = []
for a in actors:
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    label = a.get_actor_label()
    if label.startswith("terreno_") or label == "ChaoSeguranca":
        continue
    c, e = a.get_actor_bounds(False)
    if e.x > 3000 or e.y > 3000:
        continue  # peças enormes (chão/muralha inteira) não servem de obstáculo local
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
unreal.log(f"SPAWN atual {origin} -> {'bloqueado por ' + hit if hit else 'livre'}")
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
        raise RuntimeError("não consegui salvar o mapa")
    unreal.log(f"SPAWN movido para ({best[0]:.0f}, {best[1]:.0f}, {origin.z:.0f}), {r:.0f} cm do original")
elif not hit:
    unreal.log("SPAWN já estava livre")
else:
    unreal.log("SPAWN sem ponto livre no raio")
