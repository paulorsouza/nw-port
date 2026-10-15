#!/usr/bin/env bash
# Builds the Unreal project from extract/ (after tools/linux/build_all.sh). With the editor CLOSED.
#
#     tools/linux/ue_import_all.sh          # all missing steps
#     tools/linux/ue_import_all.sh 05       # only step 05
#
# Each step writes extract/.etapas/ue_<NN>.ok. Time on the first run: ~1 h 30 (the 435 gatherable meshes take
# ~25 min, the region scenery ~3 min per file). The commandlets return an error code for glTF material
# warnings: so each step checks its own result line in the log instead of the exit code.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Heavy-job queue (several sessions on the same machine; docs/workflow.md).
# NW_PESADO: old name, still accepted.
[ -n "${NW_HEAVY:-}${NW_PESADO:-}" ] || exec "$ROOT/tools/linux/heavy.sh" "$ROOT/tools/linux/$(basename "$0")" "$@"
cd "$ROOT"
UE_DIR="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}"
UE="$UE_DIR/Engine/Binaries/Linux/UnrealEditor-Cmd"
P="$ROOT/unreal/NWPort/NWPort.uproject"
LOGS="$ROOT/unreal/NWPort/Saved/ImportLogs"
DONE=extract/.etapas
mkdir -p "$LOGS" "$DONE"
ONLY="${1:-}"

py() {  # py <log> <script and arguments>
    local log="$LOGS/$1.log"; shift
    nice "$UE" "$P" -run=pythonscript -script="$*" -unattended -nosplash -nullrhi > "$log" 2>&1
    grep -aE "LogPython: Error|Traceback" "$log" | head -5
    grep -aq "Python script executed successfully" "$log" || { echo "failed, see $log" >&2; exit 1; }
}

step() {
    local n="$1" what="$2"; shift 2
    if [ -n "$ONLY" ] && [ "$ONLY" != "$n" ]; then return; fi
    if [ -z "$ONLY" ] && [ -f "$DONE/ue_$n.ok" ]; then echo "[ue $n] $what: already done"; return; fi
    if pgrep -f "Binaries/Linux/UnrealEditor .*NWPort.uproject" >/dev/null; then
        echo "The editor is open: close it first." >&2; exit 3
    fi
    echo "[ue $n] $what ($(date +%T))"
    local t0=$SECONDS
    "$@"
    touch "$DONE/ue_$n.ok"
    echo "[ue $n] ok in $(( (SECONDS - t0) / 60 )) min"
}

build() {
    (cd "$UE_DIR" && nice Engine/Build/BatchFiles/Linux/Build.sh NWPortEditor Linux Development -Project="$P" -WaitMutex \
        -MaxParallelActions=6) > "$LOGS/build.log" 2>&1 || { grep -E "error" "$LOGS/build.log" | head; exit 1; }
}
sets() {
    py import_sword "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/sword_shield/manifest.json"
    py import_lost "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/lost_sword/manifest.json"
    # Effects material (arrow trail, explosion, cloud) and short transitions for the Bow montages.
    py fx "$ROOT/tools/ue/make_fx.py"
}
arena() {
    # Game pieces (tools/arena/meshes.txt, converted by tools/linux/convert_gatherables.sh) and the map.
    if [ -d "$ROOT/extract/ue/arena/limpa" ]; then
        py arena_meshes "$ROOT/tools/ue/import_gatherables.py" "$ROOT/extract/ue/arena/limpa" /Game/NW/Arena
        py arena_materials "$ROOT/tools/ue/fix_nanite_materials.py" /Game/NW/Arena
    else
        echo "no extract/ue/arena/limpa: the arena comes out without the game pieces (tools/linux/convert_gatherables.sh tools/arena/meshes.txt extract/ue/arena)"
    fi
    py test_map "$ROOT/tools/ue/make_test_map.py"
}
creatures() {
    # G3: boar and wolf (tools/sets/*.json) and the creatures' template ABP (the graph is built by hand once).
    py import_boar "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/boar/manifest.json"
    py import_wolf "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/wolf/manifest.json"
    py creature_abp "$ROOT/tools/ue/make_abp.py" creature
}
city() {
    py import_city "$ROOT/tools/ue/import_city.py" "$ROOT/extract/cities/limpa/settlement_southwindsward_complete.glb" SouthWindsward
    py fix_city "$ROOT/tools/ue/fix_city.py" SouthWindsward
    py nanite_city "$ROOT/tools/ue/nanite_on.py" /Game/NW/Cities
}
terrain() {
    local T="$ROOT/extract/terrain/textures/terrain/forest"
    py terrain "$ROOT/tools/ue/import_terrain.py" "$ROOT/extract/cities/guardaventos_terrain.glb" SouthWindsward \
        "$T/jav_fst_feildgrass_a_diff.png" "$T/jav_fst_forestsoil_a_diff.png"
    py fix_spawn "$ROOT/tools/ue/fix_spawn.py" SouthWindsward
    py water "$ROOT/tools/ue/import_water.py" "$ROOT/extract/cities/guardaventos_water.glb"
}
scenery() {
    # A layer already imported (folder in Content/NW/Region) is not redone; to redo it, delete the folder.
    [ -d "$ROOT/unreal/NWPort/Content/NW/Region/Capitals" ] \
        || py region_capitals "$ROOT/tools/ue/import_region_scene.py" "$ROOT/extract/cities/chunks/windsward_capitals.glb" Capitals
    # One layer at a time (smaller files; the single package exhausted the import's memory).
    local f name
    for f in "$ROOT"/extract/cities/chunks/windsward_*.glb; do
        name=$(basename "$f" .glb); name=${name#windsward_}
        [ "$name" = capitals ] && continue
        [ -d "$ROOT/unreal/NWPort/Content/NW/Region/$name" ] && continue
        py "region_$name" "$ROOT/tools/ue/import_region_scene.py" "$f" "$name"
    done
}

step 01 "compile the editor C++" build
step 02 "characters, weapons and animations" sets
step 03 "test arena (L_Arena)" arena
step 04 "Windsward city" city
step 05 "region terrain and water" terrain
step 06 "gatherable meshes" py gatherables "$ROOT/tools/ue/import_gatherables.py" "$ROOT/extract/ue/gatherables/limpa"
step 07 "region scenery" scenery
step 08 "materials (Nanite and instances)" py materials "$ROOT/tools/ue/fix_nanite_materials.py" \
    /Game/NW/Cities /Game/NW/Gatherables /Game/NW/Region /Game/NW/Materials /Game/NW/Terrain
step 11 "map optimization (distances, shadows, lights)" py optimize "$ROOT/tools/ue/optimize_region.py"
step 09 "interface textures" py ui "$ROOT/tools/ue/import_ui.py"
step 10 "background sounds" py audio "$ROOT/tools/ue/import_audio.py"
step 12 "creatures (boar, wolf) and creature ABP" creatures
# Area steps (docs/workflow.md): tools/steps/<area>.ue.sh calls `step <area>-NN "description" command`.
for f in "$ROOT"/tools/steps/*.ue.sh; do
    # shellcheck disable=SC1090
    [ -e "$f" ] && source "$f"
done
echo "Done. Open the editor: $UE_DIR/Engine/Binaries/Linux/UnrealEditor $P /Game/NW/Maps/L_SouthWindsward"
