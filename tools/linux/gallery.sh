#!/usr/bin/env bash
# tools/linux/gallery.sh <saida.png> <nome>...  — galeria de armaduras masculinas (nomes sem o prefixo male_)
set -e
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
here="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$here"; source tools/linux/env.sh
GAME="${GAME:-$NW_GAME}"
PATHS="${PATHS:-/tmp/claude-1000/paths.txt}"
out="$1"; shift
files=(); 
for n in "$@"; do
  p=$(grep -E "^objects/characters/player/male/[^/]+/male_$n\.cdf$" "$PATHS" | head -1)
  if [ -n "$p" ]; then files+=("$p"); else echo "não achei: $n" >&2; fi
done
nwbt models files -g "$GAME" -o extract/gallery --binary --embed --ior 1.5 "${files[@]}" >/dev/null 2>&1
glbs=(); for p in "${files[@]}"; do glbs+=("extract/gallery/$p.glb"); done
char=extract/ue/sword_shield/glb/objects/characters/player/male/player_male.cdf.glb
blender -b --python-exit-code 1 -P tools/gallery.py -- "$char" cmbt_idle_1h "$out" "${glbs[@]}" 2>&1 | grep -E "^gallery|Error|Trace"
