#!/usr/bin/env bash
# Conferência antes de mesclar um branch (docs/esteiras.md, "Várias pessoas"). Leve, sem Unreal; com --cpp também
# compila o C++ (alvo do jogo, funciona com o editor aberto; entra na fila de trabalho pesado).
#
#     tools/linux/check.sh [--cpp]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
FAIL=0
ok() { echo "ok    $*"; }
bad() { echo "FALHA $*"; FAIL=1; }

# Marcas de conflito esquecidas.
if git grep -nE '^(<<<<<<<|>>>>>>>) ' -- . ':!*.uasset' ':!*.umap' >/dev/null 2>&1; then
    bad "marcas de conflito:"; git grep -nE '^(<<<<<<<|>>>>>>>) ' -- . ':!*.uasset' ':!*.umap' | head
else
    ok "sem marcas de conflito"
fi

# JSON escritos à mão.
bad_json=$(git ls-files '*.json' | while read -r f; do python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$f" 2>/dev/null || echo "$f"; done)
[ -z "$bad_json" ] && ok "JSON válidos" || bad "JSON inválido: $bad_json"

# Partes que se juntam: conjuntos, textos, valores da RE (chave repetida entre esteiras é erro).
python3 - <<'PY' && ok "conjuntos, textos e valores da RE sem chave repetida" || bad "chave repetida (acima)"
import glob, os, sys
sys.path.insert(0, "tools")
import setspec, make_rules
from build_strings import load_strings
for f in sorted(glob.glob("tools/sets/*.json")):
    setspec.load(f)
load_strings()
make_rules.load_values()
PY

# Python e shell.
py_err=$(python3 -m py_compile tools/*.py tools/ue/*.py tools/re/*.py 2>&1) && ok "Python compila" || bad "Python: $py_err"
sh_err=""; for f in tools/*.sh tools/linux/*.sh tools/etapas/*.sh; do [ -e "$f" ] && { bash -n "$f" 2>&1 || sh_err="$sh_err $f"; }; done
[ -z "$sh_err" ] && ok "shell sem erro de sintaxe" || bad "shell:$sh_err"

# nwbt compilado com o patch deste checkout (patches/nwbt.patch) e convertendo por "@lista" (cubo do jogo, ~20 s na
# fila de trabalho pesado; sem o jogo instalado vira aviso). Recompilar: tools/linux/pesado.sh python3 tools/build_nwbt.py
python3 tools/build_nwbt.py --conferir || FAIL=1
tools/linux/pesado.sh python3 tools/build_nwbt.py --teste-lista || FAIL=1

# Versão do jogo (só aviso: quem não tem o jogo instalado ainda pode revisar).
NW_IGNORA_VERSAO=1 tools/linux/jogo_versao.sh 2>&1 | sed 's/^/aviso /'

# O que da RE ainda não entrou no port (informativo).
python3 tools/re_check.py 2>/dev/null | grep -E "^== (Valores|Perguntas)" | sed 's/^== /info  /'

if [ "${1:-}" = "--cpp" ]; then
    UE_DIR="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}"
    LOG="$ROOT/unreal/NWPort/Saved/check_build.log"
    mkdir -p "$(dirname "$LOG")"
    if tools/linux/pesado.sh bash -c "cd '$UE_DIR' && nice Engine/Build/BatchFiles/Linux/Build.sh NWPort Linux Development \
            -Project='$ROOT/unreal/NWPort/NWPort.uproject' -WaitMutex -MaxParallelActions=6" > "$LOG" 2>&1; then
        ok "C++ compila (alvo do jogo)"
    else
        bad "C++ (ver $LOG):"; grep -E "error" "$LOG" | head
    fi
fi

[ "$FAIL" = 0 ] && echo "tudo certo" || echo "corrija as falhas antes de mesclar"
exit "$FAIL"
