#!/usr/bin/env bash
# Decompiles the interface .luac files (Lua 5.1 with a 2-byte prefix) with unluac.
#   tools/re/luac_batch.sh [source] [dest]
# Default: extract/_raw/names/lyshineui -> extract/lua/lyshineui (same tree, .lua extension).
# Files already decompiled are skipped; failures go to <dest>/falhas.txt.
set -u
SRC=${1:-extract/_raw/names/lyshineui}
DST=${2:-extract/lua/lyshineui}
JAR=${UNLUAC:-$HOME/.local/opt/unluac/unluac_2025_12_23.jar}
JAVA=$(ls -d "$HOME"/.local/opt/jdk-21*)/bin/java
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$DST"; : > "$DST/falhas.txt"
n=0; ok=0
while IFS= read -r -d '' f; do
  rel=${f#"$SRC"/}; out="$DST/${rel%.luac}.lua"
  n=$((n+1))
  [ -s "$out" ] && { ok=$((ok+1)); continue; }
  mkdir -p "$(dirname "$out")"
  python3 -I -c "import sys;d=open(sys.argv[1],'rb').read();open(sys.argv[2],'wb').write(d[d.index(b'\x1bLua'):])" "$f" "$TMP/x.luac" \
    && nice -n 10 "$JAVA" -Xmx512m -jar "$JAR" "$TMP/x.luac" > "$out" 2> "$TMP/err" && [ -s "$out" ] \
    && ok=$((ok+1)) || { echo "$rel $(head -c 200 "$TMP/err")" >> "$DST/falhas.txt"; rm -f "$out"; }
done < <(find "$SRC" -name '*.luac' -print0 | sort -z)
echo "total $n, ok $ok, failures $(wc -l < "$DST/falhas.txt")"
