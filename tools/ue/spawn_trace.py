"""Puts the open map's PlayerStart on a free street spot near the center (capsule sweep, editor open).

    python3 tools/ue/remote.py tools/ue/spawn_trace.py

Spiraling out from (0, 0): a capsule slightly larger than the character's (34 × 86 cm) drops from above and must
stop on the same floor a thin ray hits (no ledge, railing or roof in between) with the normal pointing up. The
PlayerStart is placed with the capsule 10 cm above that floor.

Replaces fix_spawn.py for hillside cities: that one only moves horizontally and keeps the terrain height at the center,
which in Everfall was below the plaza floor. Spawning against a mesh with per-triangle collision makes the
character fall through everything. Traces hit nothing in the commandlet: works only with the editor open (outside Play).
"""
import math

import unreal

RADIUS, HALF = 45.0, 100.0  # test capsule (the character's is 34 × 86)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if world is None:
    raise RuntimeError("stop Play first: the PlayerStart belongs to the editor map")
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
    bottom = c[4].z - HALF   # capsule center when stopped - half height
    if abs(bottom - lz) > 15 or c[7].z < 0.9:  # stopped on something else or on a steep slope
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
unreal.log(f"SPAWN free floor: {found}")
print("FOUND", found)
if found:
    ps = next(a for a in eas.get_all_level_actors() if isinstance(a, unreal.PlayerStart))
    ps.set_actor_location(unreal.Vector(found[0], found[1], found[2] + 86 + 10), False, False)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("SAVED", ps.get_actor_location())
