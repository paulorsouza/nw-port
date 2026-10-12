#!/usr/bin/env bash
# Monta o projeto Unreal a partir de extract/ (depois de tools/build_all.sh). Com o editor FECHADO.
#
#     tools/ue_import_all.sh          # todas as etapas que faltam
#     tools/ue_import_all.sh 05       # só a etapa 05
#
# Cada etapa grava extract/.etapas/ue_<NN>.ok. Tempo na primeira vez: ~1 h 30 (as 435 malhas dos coletáveis levam
# ~25 min, o cenário da região ~3 min por arquivo). Os commandlets devolvem código de erro por avisos de material do
# glTF: por isso cada etapa confere a própria linha de resultado no log em vez do código de saída.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
UE_DIR="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}"
UE="$UE_DIR/Engine/Binaries/Linux/UnrealEditor-Cmd"
P="$ROOT/unreal/NWPort/NWPort.uproject"
LOGS="$ROOT/unreal/NWPort/Saved/ImportLogs"
DONE=extract/.etapas
mkdir -p "$LOGS" "$DONE"
ONLY="${1:-}"

py() {  # py <log> <script e argumentos>
    local log="$LOGS/$1.log"; shift
    nice "$UE" "$P" -run=pythonscript -script="$*" -unattended -nosplash -nullrhi > "$log" 2>&1
    grep -aE "LogPython: Error|Traceback" "$log" | head -5
    grep -aq "Python script executed successfully" "$log" || { echo "falhou, ver $log" >&2; exit 1; }
}

step() {
    local n="$1" what="$2"; shift 2
    if [ -n "$ONLY" ] && [ "$ONLY" != "$n" ]; then return; fi
    if [ -z "$ONLY" ] && [ -f "$DONE/ue_$n.ok" ]; then echo "[ue $n] $what: já feito"; return; fi
    if pgrep -f "Binaries/Linux/UnrealEditor .*NWPort.uproject" >/dev/null; then
        echo "O editor está aberto: feche antes." >&2; exit 3
    fi
    echo "[ue $n] $what ($(date +%T))"
    local t0=$SECONDS
    "$@"
    touch "$DONE/ue_$n.ok"
    echo "[ue $n] ok em $(( (SECONDS - t0) / 60 )) min"
}

build() {
    (cd "$UE_DIR" && nice Engine/Build/BatchFiles/Linux/Build.sh NWPortEditor Linux Development -Project="$P" -WaitMutex \
        -MaxParallelActions=6) > "$LOGS/build.log" 2>&1 || { grep -E "error" "$LOGS/build.log" | head; exit 1; }
}
sets() {
    py import_sword "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/sword_shield/manifest.json"
    py import_lost "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/lost_sword/manifest.json"
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
    # Camada já importada (pasta em Content/NW/Region) não é refeita; para refazer, apague a pasta.
    [ -d "$ROOT/unreal/NWPort/Content/NW/Region/Capitals" ] \
        || py region_capitals "$ROOT/tools/ue/import_region_scene.py" "$ROOT/extract/cities/chunks/windsward_capitals.glb" Capitals
    # Uma camada por vez (arquivos menores; o pacote único esgotava a memória do import).
    local f name
    for f in "$ROOT"/extract/cities/chunks/windsward_*.glb; do
        name=$(basename "$f" .glb); name=${name#windsward_}
        [ "$name" = capitals ] && continue
        [ -d "$ROOT/unreal/NWPort/Content/NW/Region/$name" ] && continue
        py "region_$name" "$ROOT/tools/ue/import_region_scene.py" "$f" "$name"
    done
}

step 01 "compilar o C++ do editor" build
step 02 "personagens, armas e animações" sets
step 03 "mapa de treino (L_Arena)" py test_map "$ROOT/tools/ue/make_test_map.py"
step 04 "cidade de Guardaventos" city
step 05 "terreno e água da região" terrain
step 06 "malhas dos coletáveis" py gatherables "$ROOT/tools/ue/import_gatherables.py" "$ROOT/extract/ue/gatherables/limpa"
step 07 "cenário da região" scenery
step 08 "materiais (Nanite e instâncias)" py materials "$ROOT/tools/ue/fix_nanite_materials.py" \
    /Game/NW/Cities /Game/NW/Gatherables /Game/NW/Region /Game/NW/Materials /Game/NW/Terrain
step 11 "otimização do mapa (distâncias, sombras, luzes)" py optimize "$ROOT/tools/ue/optimize_region.py"
step 09 "texturas da interface" py ui "$ROOT/tools/ue/import_ui.py"
step 10 "sons de fundo" py audio "$ROOT/tools/ue/import_audio.py"
echo "Pronto. Abra o editor: $UE_DIR/Engine/Binaries/Linux/UnrealEditor $P /Game/NW/Maps/L_SouthWindsward"
