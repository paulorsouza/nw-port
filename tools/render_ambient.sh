#!/usr/bin/env bash
# Renderiza os sons de ambiente de um território (vida selvagem por bioma, assentamento) dos .txtp do jogo.
#
#     tools/render_ambient.sh windsward
#
# Precisa dos .txtp gerados por tools/extract_audio.py em extract/_raw/txtp/ambient. Saída
# extract/audio/ambient/<território>/<tipo>_<bioma>_<hora>.ogg (importados por tools/ue/import_audio.py).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
T="${1:?território, ex.: windsward}"
OUT="$ROOT/extract/audio/ambient/$T"
mkdir -p "$OUT"
cd "$ROOT/extract/_raw/txtp/ambient"
n=0
for f in *"mx_territory=$T"*.txtp; do
    [ -e "$f" ] || continue
    name=$(echo "$f" | sed -E 's/play_quadamb_//; s/\(ext_int=amb_ext\) //; s/\(mx_territory=[a-z]+\) //; s/[()]//g;
        s/ /_/g; s/tract_state=//; s/tod_state=//; s/poi_state=//; s/\.txtp$//')
    [ -f "$OUT/$name.ogg" ] && continue
    nice "$ROOT/bin/vgmstream-cli" -i -p "$f" 2>/dev/null | nice "$ROOT/bin/oggenc" -Q -q 4 -o "$OUT/$name.ogg" -
    n=$((n + 1))
done
echo "$n sons novos em $OUT ($(ls "$OUT" | wc -l) no total)"
