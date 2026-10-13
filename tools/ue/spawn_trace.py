"""Põe o PlayerStart do mapa aberto num ponto de rua livre perto do centro (varredura com cápsula, editor aberto).

    python3 tools/ue/remote.py tools/ue/spawn_trace.py

Em espiral a partir de (0, 0): uma cápsula um pouco maior que a do personagem (34 × 86 cm) desce de cima e precisa
parar no mesmo chão que um raio fino acerta (sem borda, grade ou telhado no meio) e com a normal para cima. O
PlayerStart fica com a cápsula 10 cm acima desse chão.

Substitui o fix_spawn.py em cidade em encosta: ele só desvia na horizontal e mantém a altura do terreno no centro,
que em Queda Eterna ficava abaixo do piso da praça. Nascer encostado numa malha com colisão por triângulo faz o
personagem atravessar tudo. Os raios não acertam nada no commandlet: só funciona com o editor aberto (fora do Play).
"""
import math

import unreal

RADIUS, HALF = 45.0, 100.0  # cápsula de teste (a do personagem é 34 × 86)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if world is None:
    raise RuntimeError("pare o Play antes: o PlayerStart é do mapa do editor")
VIS = unreal.TraceTypeQuery.ECC_VISIBILITY
NONE = unreal.DrawDebugTrace.NONE


def floor_at(x, y):
    line = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                  VIS, True, [], NONE, True)
    caps = unreal.SystemLibrary.capsule_trace_single(world, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     RADIUS, HALF, VIS, True, [], NONE, True)
    if not line or not caps:
        return None
    lz = line.to_tuple()[4].z
    c = caps.to_tuple()
    bottom = c[4].z - HALF   # centro da cápsula ao parar - meia altura
    if abs(bottom - lz) > 15 or c[7].z < 0.9:  # parou em outra coisa ou em rampa forte
        return None
    return lz


found = None
for r in range(0, 8000, 150):
    n = max(1, int(2 * math.pi * r / 150))
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = r * math.cos(a), r * math.sin(a)
        z = floor_at(x, y)
        if z is not None:
            found = (x, y, z)
            break
    if found:
        break
unreal.log(f"SPAWN chão livre: {found}")
print("ACHADO", found)
if found:
    ps = next(a for a in eas.get_all_level_actors() if isinstance(a, unreal.PlayerStart))
    ps.set_actor_location(unreal.Vector(found[0], found[1], found[2] + 86 + 10), False, False)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("SALVO", ps.get_actor_location())
