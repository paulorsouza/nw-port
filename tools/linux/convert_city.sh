#!/usr/bin/env bash
# Converte uma cidade do jogo (dynamicslice "*_complete") em glb limpo + terreno, sem Unreal.
#
#     tools/linux/convert_city.sh <slice>        ex.: tools/linux/convert_city.sh settlement_reekwater_complete
#
# Etapas (cada uma pula se a saída já existe; dá para rodar de novo):
#   1. nwbt models slices  → extract/cities/slices/structures/settlements/v2_settlements/<slice>.glb
#   2. glb_strip_nodraw.py → extract/cities/limpa/<slice>.glb
#   3. dados de terreno (capitals, heightmaps, roads_splat) em extract/terrain, extraídos só se faltarem
#   4. make_terrain.py     → extract/cities/<curto>_terrain.glb + .json
#      (<curto> = slice sem "_complete", "settlement_" e "_settlement"; ex. southwindsward)
#
# Proteções: nwbt roda com cwd = raiz do repo (cache em .nwbt/, nunca em /tmp); tudo pesado com
# nice/ionice; espera MemAvailable >= 12 GB (até 20 min) antes de começar e mata a conversão se
# MemAvailable cair abaixo de 8 GB; para se algum disco envolvido tiver menos de 30 GB livres.
# Log em extract/cities/logs/<slice>.log. Saídas são gravadas com nome temporário e renomeadas no fim.
#
# Códigos de saída: 0 ok (terreno pode ter sido pulado, ver log), 2 uso, 3 memória/disco, 4 falha na conversão.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck disable=SC1091
source tools/linux/env.sh

GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
LEVEL=newworld_vitaeeterna
TDIR=extract/terrain

[ $# -eq 1 ] || { echo "uso: $0 <slice>  (ex.: settlement_reekwater_complete)" >&2; exit 2; }
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
        echo "memória: $(( $(mem_avail_kb) / 1024 )) MB livres, esperando 60 s ($((i + 1))/20)"
        sleep 60
    done
    echo "ERRO: MemAvailable abaixo de 12 GB por 20 min"; exit 3
}

check_disk() {
    local d free
    for d in "$ROOT" "$GAME"; do
        free=$(df -Pk "$d" | awk 'NR==2 {print $4}')
        if (( free < DISK_MIN_KB )); then
            echo "ERRO: menos de 30 GB livres em $d ($((free / 1024 / 1024)) GB)"; exit 3
        fi
    done
}

# Roda "$@" com nice/ionice num grupo de processos próprio; mata o grupo (e o texconv do wine)
# se MemAvailable cair abaixo de 8 GB.
run_guarded() {
    setsid "${NICE[@]}" "$@" &
    local pid=$! peak=0 low
    while kill -0 "$pid" 2>/dev/null; do
        low=$(mem_avail_kb)
        if (( low < MEM_MIN_KB )); then
            echo "ERRO: MemAvailable $((low / 1024)) MB < 8 GB; matando $1"
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
    echo "[1] já existe: $RAW"
else
    echo "[1] nwbt models slices $SRC"
    TMPOUT="extract/cities/.tmp_$SLICE"
    mkdir -p "$TMPOUT"
    start=$(date +%s)
    rc=0
    run_guarded nwbt models slices -g "$GAME" -o "$TMPOUT" --binary --embed --ior 1.5 "$SRC" || rc=$?
    got=$(find "$TMPOUT" -name "$SLICE.glb" -size +0 | head -1)
    if (( rc != 0 )) || [ -z "$got" ]; then
        echo "ERRO: nwbt falhou (rc=$rc); parcial em $TMPOUT"
        (( rc == 99 )) && exit 3
        exit 4
    fi
    mv "$got" "$RAW"
    # o nwbt também grava animations/ (adb dos mannequins); junta em extract/cities sem sobrescrever
    [ -d "$TMPOUT/animations" ] && cp -rn "$TMPOUT/animations" extract/cities/
    rm -rf "$TMPOUT"
    echo "[1] ok em $(( $(date +%s) - start )) s: $(du -h "$RAW" | cut -f1)"
fi

# 2. remove NoDraw/efeitos
if [ -s "$CLEAN" ]; then
    echo "[2] já existe: $CLEAN"
else
    echo "[2] glb_strip_nodraw"
    run_guarded python3 tools/glb_strip_nodraw.py "$RAW" "$CLEAN.part" || { echo "ERRO: strip falhou"; exit 4; }
    mv "$CLEAN.part" "$CLEAN"
fi

python3 - "$CLEAN" <<'EOF'
import json, struct, sys
with open(sys.argv[1], "rb") as f:
    f.read(12); n, _ = struct.unpack("<II", f.read(8)); j = json.loads(f.read(n))
nodes = j.get("nodes", [])
print(f"[2] {sys.argv[1]}: nós {len(nodes)}, nós com malha {sum('mesh' in x for x in nodes)}, "
      f"malhas {len(j.get('meshes', []))}, materiais {len(j.get('materials', []))}, imagens {len(j.get('images', []))}")
EOF

# 3. dados de terreno
if [ -s "$TERRAIN" ]; then
    echo "[3/4] já existe: $TERRAIN"
    exit 0
fi
REG="$TDIR/sharedassets/coatlicue/$LEVEL/regions"
if ! compgen -G "$REG/r_*/capitals/*/*.capitals.json" >/dev/null || ! compgen -G "$REG/r_*/region.heightmap" >/dev/null; then
    echo "[3] extraindo capitals e heightmaps de $LEVEL"
    run_guarded nwbt unpack -g "$GAME" -o "$TDIR" -e \
        "sharedassets/coatlicue/$LEVEL/regions/r_[^/]+/(capitals/[^/]+/[^/]+\.capitals\.json|mapsettings\.json|region\.heightmap)$" \
        || { echo "ERRO: unpack de regiões falhou"; exit 4; }
fi

# capitals.json com exatamente um capital cujo sliceName termina em /<slice>; e regiões que o quadrado cobre
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
r = 768 / 2 * math.sqrt(2) + 2          # meia-diagonal do quadrado padrão do make_terrain
xs = range(int((p["x"] - r) // 2048), int((p["x"] + r) // 2048) + 1)
ys = range(int((p["y"] - r) // 2048), int((p["y"] + r) // 2048) + 1)
print(f, " ".join(f"{x}_{y}" for x in xs for y in ys))
EOF
)
CAPS="${info%% *}"
if [ "$CAPS" = NONE ]; then
    echo "[3] terreno pulado: $(echo "$info" | cut -d' ' -f2) capitais com sliceName .../$SLICE (esperado 1)"
    exit 0
fi
REGIONS="${info#* }"
echo "[3] capitals: $CAPS; regiões: $REGIONS"

need_hm=() need_sp=()
for xy in $REGIONS; do
    x=${xy%_*}; y=${xy#*_}
    [ -s "$(printf '%s/r_+%02d_+%02d/region.heightmap' "$REG" "$x" "$y")" ] || need_hm+=("$(printf 'r_\\+%02d_\\+%02d' "$x" "$y")")
    [ -s "$TDIR/materials/terrain/$LEVEL/jav_${LEVEL}_${x}_${y}/jav_${LEVEL}_${x}.${y}.roads_splat.png" ] || need_sp+=("${x}_${y}")
done
if (( ${#need_hm[@]} )); then
    pat=$(IFS='|'; echo "${need_hm[*]}")
    echo "[3] extraindo heightmap: ${need_hm[*]}"
    run_guarded nwbt unpack -g "$GAME" -o "$TDIR" -e "sharedassets/coatlicue/$LEVEL/regions/($pat)/region\.heightmap$" \
        || { echo "ERRO: unpack de heightmap falhou"; exit 4; }
fi
if (( ${#need_sp[@]} )); then
    pat=$(IFS='|'; echo "${need_sp[*]}")
    echo "[3] extraindo roads_splat: ${need_sp[*]}"
    run_guarded nwbt unpack -g "$GAME" -o "$TDIR" --x-dds png \
        -e "materials/terrain/$LEVEL/jav_${LEVEL}_($pat)/jav_${LEVEL}_[0-9]+\.[0-9]+\.roads_splat\.dds$" \
        || echo "aviso: unpack de roads_splat falhou (terreno sai sem peso de estrada)"
fi
for xy in $REGIONS; do
    x=${xy%_*}; y=${xy#*_}
    [ -s "$(printf '%s/r_+%02d_+%02d/region.heightmap' "$REG" "$x" "$y")" ] || echo "aviso: região $xy sem heightmap (altura 0 nessa parte)"
    [ -s "$TDIR/materials/terrain/$LEVEL/jav_${LEVEL}_${x}_${y}/jav_${LEVEL}_${x}.${y}.roads_splat.png" ] || echo "aviso: região $xy sem roads_splat"
done

# 4. malha do terreno
echo "[4] make_terrain"
PART="extract/cities/${SHORT}_terrain.part.glb"
run_guarded python3 tools/make_terrain.py "$TDIR" "$CAPS" "$SLICE" "$PART" || { echo "ERRO: make_terrain falhou"; exit 4; }
mv "${PART%.glb}.json" "${TERRAIN%.glb}.json"
mv "$PART" "$TERRAIN"
echo "[4] ok: $TERRAIN $(du -h "$TERRAIN" | cut -f1); $(cat "${TERRAIN%.glb}.json")"
