#!/usr/bin/env bash
# Converte as malhas dos coletáveis (extract/ue/gatherables/meshes.txt, de tools/make_gatherables.py) em glb
# limpos, um por .cgf, prontos para tools/ue/import_gatherables.py.
#
#     tools/linux/convert_gatherables.sh [lista] [saida]
#
# Outra lista de .cgf (um caminho do pak por linha) e outra pasta de saída servem para qualquer conjunto de malhas
# estáticas (ex.: tools/linux/convert_gatherables.sh tools/arena/meshes.txt extract/ue/arena).
#
# Saída: extract/ue/gatherables/glb/<caminho do pak>.glb (bruto do nwbt) e extract/ue/gatherables/limpa/<nome>.glb
# (sem as primitivas NoDraw). Pula o que já existe; pode rodar de novo. nwbt roda com cwd = raiz do repo (cache em
# .nwbt/) e com nice/ionice, em lotes de 8 malhas com 2 workers; um vigia mata o lote se MemAvailable cair abaixo
# de 10 GB (2026-10-11: as 160 de uma vez, 7 workers, levaram o systemd-oomd a matar a sessão). Sem MemoryMax: o
# teto faz o próprio job gerar pressão de memória, e o oomd mata outro app. Um trabalho pesado por vez: não rodar junto com build da Unreal.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck disable=SC1091
source tools/linux/env.sh

GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
OUT="${2:-extract/ue/gatherables}"
LIST="${1:-$OUT/meshes.txt}"
[ -f "$LIST" ] || { echo "falta $LIST (rode tools/make_gatherables.py)" >&2; exit 2; }
mkdir -p "$OUT/glb" "$OUT/limpa"

todo=()
while read -r m; do
    [ -n "$m" ] || continue
    [ -f "$OUT/glb/$m.glb" ] || todo+=("$m")
done < "$LIST"
echo "malhas: $(grep -c . "$LIST"), a converter: ${#todo[@]}"
mem_avail_mb() { awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo; }
BATCH=8
for ((i = 0; i < ${#todo[@]}; i += BATCH)); do
    (( $(mem_avail_mb) >= 16000 )) || { echo "memória baixa ($(mem_avail_mb) MB livres), parando" >&2; exit 3; }
    lote=("${todo[@]:i:BATCH}")
    echo "lote $((i / BATCH + 1)): ${#lote[@]} malhas ($(mem_avail_mb) MB livres)"
    nice -n 19 ionice -c3 nwbt models files -g "$GAME" -o "$OUT/glb" --binary --embed --ior 1.5 \
        -w 2 --texture-size 2048 --skip "${lote[@]}" >> "$OUT/nwbt.log" 2>&1 &
    job=$!
    while kill -0 "$job" 2>/dev/null; do
        if (( $(mem_avail_mb) < 10000 )); then
            echo "MemAvailable $(mem_avail_mb) MB: matando o lote" >&2
            kill -TERM "$job"; pkill -TERM -f "texconv" 2>/dev/null || true
            wait "$job" 2>/dev/null || true; exit 3
        fi
        sleep 2
    done
    wait "$job" || { echo "nwbt falhou no lote, ver $OUT/nwbt.log" >&2; exit 4; }
done

n=0; missing=0
while read -r m; do
    [ -n "$m" ] || continue
    src="$OUT/glb/$m.glb"
    dst="$OUT/limpa/$(basename "$m" .cgf).glb"
    if [ ! -f "$src" ]; then
        echo "sem glb: $m"; missing=$((missing + 1)); continue
    fi
    [ -f "$dst" ] && [ "$dst" -nt "$src" ] && continue
    python3 tools/glb_strip_nodraw.py "$src" "$dst" --sem-atributos > /dev/null
    n=$((n + 1))
done < "$LIST"
echo "limpos: $n novos, $missing sem glb -> $OUT/limpa ($(ls "$OUT/limpa" | wc -l) arquivos)"
