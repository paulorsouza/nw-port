#!/usr/bin/env bash
# Converts the gatherable meshes (extract/ue/gatherables/meshes.txt, from tools/make_gatherables.py) into clean
# glb files, one per .cgf, ready for tools/ue/import_gatherables.py.
#
#     tools/linux/convert_gatherables.sh [list] [out]
#
# Another .cgf list (one pak path per line) and another output folder work for any set of static meshes
# (e.g. tools/linux/convert_gatherables.sh tools/arena/meshes.txt extract/ue/arena).
#
# Output: extract/ue/gatherables/glb/<pak path>.glb (raw from nwbt) and extract/ue/gatherables/limpa/<name>.glb
# (without the NoDraw primitives). Skips what already exists; safe to run again. nwbt runs with cwd = repo root
# (cache in .nwbt/) and with nice/ionice, in batches of 8 meshes with 2 workers; a watchdog kills the batch if
# MemAvailable drops below 10 GB (2026-10-11: all 160 at once, 7 workers, made systemd-oomd kill the session). No
# MemoryMax: the cap makes the job itself create memory pressure, and oomd kills another app. One heavy job at a time: do not run alongside a Unreal build.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck disable=SC1091
source tools/linux/env.sh

GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
OUT="${2:-extract/ue/gatherables}"
LIST="${1:-$OUT/meshes.txt}"
[ -f "$LIST" ] || { echo "missing $LIST (run tools/make_gatherables.py)" >&2; exit 2; }
mkdir -p "$OUT/glb" "$OUT/limpa"

todo=()
while read -r m; do
    [ -n "$m" ] || continue
    [ -f "$OUT/glb/$m.glb" ] || todo+=("$m")
done < "$LIST"
echo "meshes: $(grep -c . "$LIST"), to convert: ${#todo[@]}"
mem_avail_mb() { awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo; }
BATCH=8
for ((i = 0; i < ${#todo[@]}; i += BATCH)); do
    (( $(mem_avail_mb) >= 16000 )) || { echo "low memory ($(mem_avail_mb) MB free), stopping" >&2; exit 3; }
    lote=("${todo[@]:i:BATCH}")
    echo "batch $((i / BATCH + 1)): ${#lote[@]} meshes ($(mem_avail_mb) MB free)"
    nice -n 19 ionice -c3 nwbt models files -g "$GAME" -o "$OUT/glb" --binary --embed --ior 1.5 \
        -w 2 --texture-size 2048 --skip "${lote[@]}" >> "$OUT/nwbt.log" 2>&1 &
    job=$!
    while kill -0 "$job" 2>/dev/null; do
        if (( $(mem_avail_mb) < 10000 )); then
            echo "MemAvailable $(mem_avail_mb) MB: killing the batch" >&2
            kill -TERM "$job"; pkill -TERM -f "texconv" 2>/dev/null || true
            wait "$job" 2>/dev/null || true; exit 3
        fi
        sleep 2
    done
    wait "$job" || { echo "nwbt failed on the batch, see $OUT/nwbt.log" >&2; exit 4; }
done

n=0; missing=0
while read -r m; do
    [ -n "$m" ] || continue
    src="$OUT/glb/$m.glb"
    dst="$OUT/limpa/$(basename "$m" .cgf).glb"
    if [ ! -f "$src" ]; then
        echo "no glb: $m"; missing=$((missing + 1)); continue
    fi
    [ -f "$dst" ] && [ "$dst" -nt "$src" ] && continue
    python3 tools/glb_strip_nodraw.py "$src" "$dst" --sem-atributos > /dev/null
    n=$((n + 1))
done < "$LIST"
echo "cleaned: $n new, $missing without glb -> $OUT/limpa ($(ls "$OUT/limpa" | wc -l) files)"
