#!/usr/bin/env bash
# Põe em bin/ as ferramentas da extração (fora do git): baixa texconv.exe e vgmstream-cli das releases oficiais e
# compila nwbt, dbgwater (Go + vendor/nw-buddy) e libooz.so (g++ + powzix/ooz). Pula o que já existe;
# --force refaz tudo. oggenc vem do apt (vorbis-tools).
#
#     tools/linux/get_tools.sh [--force]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
mkdir -p bin
FORCE=0; [ "${1:-}" = --force ] && FORCE=1
need() { [ "$FORCE" = 1 ] || [ ! -e "$1" ]; }

TEXCONV=may2026     # microsoft/DirectXTex
VGMSTREAM=r2117     # vgmstream/vgmstream

if need bin/texconv.exe; then
    curl -fsSL -o bin/texconv.exe "https://github.com/microsoft/DirectXTex/releases/download/$TEXCONV/texconv.exe"
fi
ln -sfn ../tools/linux/texconv bin/texconv

if need bin/vgmstream-cli; then
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
    curl -fsSL -o "$tmp/vgm.zip" "https://github.com/vgmstream/vgmstream/releases/download/$VGMSTREAM/vgmstream-linux.zip"
    unzip -qo "$tmp/vgm.zip" vgmstream-cli -d bin
    chmod +x bin/vgmstream-cli
fi

if need bin/libooz.so; then
    tools/linux/build_ooz.sh
fi

if need bin/nwbt || need bin/dbgwater || ! python3 tools/build_nwbt.py --conferir >/dev/null 2>&1; then
    [ -d vendor/nw-buddy/.git ] || git clone https://github.com/paulorsouza/nw-buddy.git vendor/nw-buddy
    PATH="$HOME/.local/go/bin:$PATH" tools/linux/build_nwbt.sh --test
fi

[ -x bin/oggenc ] || command -v oggenc >/dev/null || echo "falta oggenc: sudo apt install vorbis-tools"
echo "ferramentas em bin/: $(ls bin | tr '\n' ' ')"
