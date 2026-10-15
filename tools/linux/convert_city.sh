#!/usr/bin/env bash
# Converts a game city (dynamicslice "*_complete") into a clean glb + terrain, without Unreal.
#
#     tools/linux/convert_city.sh <slice>        e.g. tools/linux/convert_city.sh settlement_reekwater_complete
#
# Steps (each one skips if its output already exists; safe to run again):
#   1. nwbt models slices  → extract/cities/slices/structures/settlements/v2_settlements/<slice>.glb
#   2. glb_strip_nodraw.py → extract/cities/limpa/<slice>.glb
#   3. terrain data (capitals, heightmaps, roads_splat) in extract/terrain, extracted only if missing
#   4. make_terrain.py     → extract/cities/<short>_terrain.glb + .json
#      (<short> = slice without "_complete", "settlement_" and "_settlement"; e.g. southwindsward)
#
# Safeguards: nwbt runs with cwd = repo root (cache in .nwbt/, never in /tmp); everything heavy with
# nice/ionice; waits for MemAvailable >= 12 GB (up to 20 min) before starting and kills the conversion if
# MemAvailable drops below 8 GB; stops if any disk involved has less than 30 GB free.
# Log in extract/cities/logs/<slice>.log. Outputs are written under a temporary name and renamed at the end.
#
# Exit codes: 0 ok (terrain may have been skipped, see log), 2 usage, 3 memory/disk, 4 conversion failure.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck disable=SC1091
source tools/linux/env.sh

GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
LEVEL=newworld_vitaeeterna
TDIR=extract/terrain

[ $# -eq 1 ] || { echo "usage: $0 <slice>  (e.g. settlement_reekwater_complete)" >&2; exit 2; }
SLICE="${1##*/}"; SLICE="${SLICE%.dynamicslice}"
SRC="slices/structures/settlements/v2_settlements/$SLICE.dynamicslice"
RAW="extract/cities/slices/structures/settlements/v2_settlements/$SLICE.glb"
CLEAN="extract/cities/limpa/$SLICE.glb"
SHORT="${SLICE%_complete}"; SHORT="${SHORT#settlement_}"; SHORT="${SHORT%_settlement}"
TERRAIN="extract/cities/${SHORT}_terrain.glb"
mkdir -p extract/cities/logs extract/cities/limpa "$(dirname "$RAW")"
LOG="extract/cities/logs/$SLICE.log"
exec > >(tee -a "$LOG") 2>&1
echo "=== $SLICE $(date '+%F %T')"

NICE=(nice -n 19 ionice -c3)
MEM_START_KB=$((12 * 1024 * 1024))
MEM_MIN_KB=$((8 * 1024 * 1024))
DISK_MIN_KB=$((30 * 1024 * 1024))

mem_avail_kb() { awk '/^MemAvailable:/ {print $2}' /proc/meminfo; }

wait_mem() {
    local i
    for i in $(seq 0 20); do
        (( $(mem_avail_kb) >= MEM_START_KB )) && return 0
        (( i == 20 )) && break
        echo "memory: $(( $(mem_avail_kb) / 1024 )) MB free, waiting 60 s ($((i + 1))/20)"
        sleep 60
    done
    echo "ERROR: MemAvailable below 12 GB for 20 min"; exit 3
}

check_disk() {
    local d free
    for d in "$ROOT" "$GAME"; do
        free=$(df -Pk "$d" | awk 'NR==2 {print $4}')
        if (( free < DISK_MIN_KB )); then
            echo "ERROR: less than 30 GB free on $d ($((free / 1024 / 1024)) GB)"; exit 3
        fi
    done
}

# Runs "$@" with nice/ionice in its own process group; kills the group (and wine's texconv)
# if MemAvailable drops below 8 GB.
run_guarded() {
    setsid "${NICE[@]}" "$@" &
    local pid=$! peak=0 low
    while kill -0 "$pid" 2>/dev/null; do
        low=$(mem_avail_kb)
        if (( low < MEM_MIN_KB )); then
            echo "ERROR: MemAvailable $((low / 1024)) MB < 8 GB; killing $1"
            kill -TERM -- "-$pid" 2>/dev/null || true
            pkill -f "texconv.*$ROOT/.nwbt" 2>/dev/null || true
            sleep 5
            kill -KILL -- "-$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            return 99
        fi
        sleep 5
    done
    wait "$pid"
}

wait_mem
check_disk

# 1. slice → glb
if [ -s "$RAW" ]; then
    echo "[1] already exists: $RAW"
else
    echo "[1] nwbt models slices $SRC"
    TMPOUT="extract/cities/.tmp_$SLICE"
    mkdir -p "$TMPOUT"
    start=$(date +%s)
    rc=0
    run_guarded nwbt models slices -g "$GAME" -o "$TMPOUT" --binary --embed --ior 1.5 "$SRC" || rc=$?
    got=$(find "$TMPOUT" -name "$SLICE.glb" -size +0 | head -1)
    if (( rc != 0 )) || [ -z "$got" ]; then
        echo "ERROR: nwbt failed (rc=$rc); partial output in $TMPOUT"
        (( rc == 99 )) && exit 3
        exit 4
    fi
    mv "$got" "$RAW"
    # nwbt also writes animations/ (mannequin adb files); merge into extract/cities without overwriting
    [ -d "$TMPOUT/animations" ] && cp -rn "$TMPOUT/animations" extract/cities/
    rm -rf "$TMPOUT"
    echo "[1] ok in $(( $(date +%s) - start )) s: $(du -h "$RAW" | cut -f1)"
fi

# 2. remove NoDraw/effects
if [ -s "$CLEAN" ]; then
    echo "[2] already exists: $CLEAN"
else
    echo "[2] glb_strip_nodraw"
    run_guarded python3 tools/glb_strip_nodraw.py "$RAW" "$CLEAN.part" || { echo "ERROR: strip failed"; exit 4; }
    mv "$CLEAN.part" "$CLEAN"
fi

python3 - "$CLEAN" <<'EOF'
import json, struct, sys
with open(sys.argv[1], "rb") as f:
    f.read(12); n, _ = struct.unpack("<II", f.read(8)); j = json.loads(f.read(n))
nodes = j.get("nodes", [])
print(f"[2] {sys.argv[1]}: nodes {len(nodes)}, nodes with mesh {sum('mesh' in x for x in nodes)}, "
      f"meshes {len(j.get('meshes', []))}, materials {len(j.get('materials', []))}, images {len(j.get('images', []))}")
EOF

# 3. terrain data
if [ -s "$TERRAIN" ]; then
    echo "[3/4] already exists: $TERRAIN"
    exit 0
fi
REG="$TDIR/sharedassets/coatlicue/$LEVEL/regions"
if ! compgen -G "$REG/r_*/capitals/*/*.capitals.json" >/dev/null || ! compgen -G "$REG/r_*/region.heightmap" >/dev/null; then
    echo "[3] extracting capitals and heightmaps of $LEVEL"
    run_guarded nwbt unpack -g "$GAME" -o "$TDIR" -e \
        "sharedassets/coatlicue/$LEVEL/regions/r_[^/]+/(capitals/[^/]+/[^/]+\.capitals\.json|mapsettings\.json|region\.heightmap)$" \
        || { echo "ERROR: region unpack failed"; exit 4; }
fi

# capitals.json with exactly one capital whose sliceName ends in /<slice>; and the regions the square covers
info=$(python3 - "$REG" "$SLICE" <<'EOF'
import glob, json, math, sys
reg, sl = sys.argv[1], sys.argv[2].lower()
hits = []
for f in sorted(glob.glob(f"{reg}/r_*/capitals/*/*.capitals.json")):
    try:
        caps = json.load(open(f)).get("Capitals", [])
    except Exception:
        continue
    for c in caps:
        if c.get("sliceName", "").lower().rsplit("/", 1)[-1] == sl:
            hits.append((f, c))
if len(hits) != 1:
    print(f"NONE {len(hits)}"); sys.exit()
f, c = hits[0]
p = c["worldPosition"]
r = 768 / 2 * math.sqrt(2) + 2          # half-diagonal of make_terrain's default square
xs = range(int((p["x"] - r) // 2048), int((p["x"] + r) // 2048) + 1)
ys = range(int((p["y"] - r) // 2048), int((p["y"] + r) // 2048) + 1)
print(f, " ".join(f"{x}_{y}" for x in xs for y in ys))
EOF
)
CAPS="${info%% *}"
if [ "$CAPS" = NONE ]; then
    echo "[3] terrain skipped: $(echo "$info" | cut -d' ' -f2) capitals with sliceName .../$SLICE (expected 1)"
    exit 0
fi
REGIONS="${info#* }"
echo "[3] capitals: $CAPS; regions: $REGIONS"

need_hm=() need_sp=()
for xy in $REGIONS; do
    x=${xy%_*}; y=${xy#*_}
    [ -s "$(printf '%s/r_+%02d_+%02d/region.heightmap' "$REG" "$x" "$y")" ] || need_hm+=("$(printf 'r_\\+%02d_\\+%02d' "$x" "$y")")
    [ -s "$TDIR/materials/terrain/$LEVEL/jav_${LEVEL}_${x}_${y}/jav_${LEVEL}_${x}.${y}.roads_splat.png" ] || need_sp+=("${x}_${y}")
done
if (( ${#need_hm[@]} )); then
    pat=$(IFS='|'; echo "${need_hm[*]}")
    echo "[3] extracting heightmap: ${need_hm[*]}"
    run_guarded nwbt unpack -g "$GAME" -o "$TDIR" -e "sharedassets/coatlicue/$LEVEL/regions/($pat)/region\.heightmap$" \
        || { echo "ERROR: heightmap unpack failed"; exit 4; }
fi
if (( ${#need_sp[@]} )); then
    pat=$(IFS='|'; echo "${need_sp[*]}")
    echo "[3] extracting roads_splat: ${need_sp[*]}"
    run_guarded nwbt unpack -g "$GAME" -o "$TDIR" --x-dds png \
        -e "materials/terrain/$LEVEL/jav_${LEVEL}_($pat)/jav_${LEVEL}_[0-9]+\.[0-9]+\.roads_splat\.dds$" \
        || echo "warning: roads_splat unpack failed (terrain comes out without road weight)"
fi
for xy in $REGIONS; do
    x=${xy%_*}; y=${xy#*_}
    [ -s "$(printf '%s/r_+%02d_+%02d/region.heightmap' "$REG" "$x" "$y")" ] || echo "warning: region $xy has no heightmap (height 0 in that part)"
    [ -s "$TDIR/materials/terrain/$LEVEL/jav_${LEVEL}_${x}_${y}/jav_${LEVEL}_${x}.${y}.roads_splat.png" ] || echo "warning: region $xy has no roads_splat"
done

# 4. terrain mesh
echo "[4] make_terrain"
PART="extract/cities/${SHORT}_terrain.part.glb"
run_guarded python3 tools/make_terrain.py "$TDIR" "$CAPS" "$SLICE" "$PART" || { echo "ERROR: make_terrain failed"; exit 4; }
mv "${PART%.glb}.json" "${TERRAIN%.glb}.json"
mv "$PART" "$TERRAIN"
echo "[4] ok: $TERRAIN $(du -h "$TERRAIN" | cut -f1); $(cat "${TERRAIN%.glb}.json")"
