#!/usr/bin/env bash
# Extrai e converte tudo o que o projeto usa, da instalação do New World de quem roda (docs/SETUP.md).
# Não usa Unreal: no fim, rode tools/ue_import_all.sh com o editor fechado.
#
#     tools/build_all.sh            # todas as etapas que faltam
#     tools/build_all.sh 07         # só a etapa 07 (refaz mesmo se já feita)
#
# Cada etapa grava extract/.etapas/<NN>.ok ao terminar; rodar de novo pula o que já foi feito. Tempo total na
# primeira vez: 2 a 4 horas (máquina de 8 núcleos, SSD). Precisa de ~60 GB livres no disco do projeto.
#
# Cuidados (aprendidos na prática, ver docs/ESTADO.md):
# - Um trabalho pesado por vez. Não abra o New World original nem a Unreal enquanto isto roda: o systemd-oomd
#   do Ubuntu fecha programas por pressão de memória, mesmo com RAM "livre" no htop.
# - O nwbt carrega ~6 GB de índice dos paks a cada chamada; as conversões de malha vão em lotes pequenos.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
# shellcheck disable=SC1091
source tools/env.sh
GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
LEVEL=newworld_vitaeeterna
REGION='r_+04_+01'       # Guardaventos
RTTI=vendor/nw-buddy/tools/nwbt/rtti/nwt
DONE=extract/.etapas
mkdir -p "$DONE" extract/manifest
ONLY="${1:-}"

nwbt_() { nice -n 19 ionice -c3 nwbt "$@"; }
mem_avail_gb() { awk '/^MemAvailable:/ {print int($2 / 1048576)}' /proc/meminfo; }
# Roda um comando pesado e o interrompe se a memória livre cair abaixo de 10 GB.
guarded() {
    "$@" &
    local pid=$!
    while kill -0 "$pid" 2>/dev/null; do
        if (( $(mem_avail_gb) < 10 )); then
            echo "ERRO: menos de 10 GB de memória livre; interrompendo: $*" >&2
            kill -TERM "$pid"; pkill -TERM -f texconv 2>/dev/null || true; wait "$pid" || true; exit 3
        fi
        sleep 2
    done
    wait "$pid"
}
# O --x-objects json do nwbt procura os arquivos de tipos com exec.LookPath: cópias executáveis.
objects_args() {
    mkdir -p .nwbt/rtti
    cp -f "$RTTI/nwt-crc.json" "$RTTI/nwt-types.json" .nwbt/rtti/ && chmod +x .nwbt/rtti/*.json
    echo --x-objects json --crc-file "$ROOT/.nwbt/rtti/nwt-crc.json" --uuid-file "$ROOT/.nwbt/rtti/nwt-types.json"
}

step() {  # step NN "descrição" função
    local n="$1" what="$2" fn="$3"
    if [ -n "$ONLY" ] && [ "$ONLY" != "$n" ]; then return; fi
    if [ -z "$ONLY" ] && [ -f "$DONE/$n.ok" ]; then echo "[$n] $what: já feito"; return; fi
    if pgrep -x NewWorld.exe >/dev/null || pgrep -f "[N]ewWorldLauncher.exe" >/dev/null; then
        echo "O New World original está aberto: feche antes de continuar." >&2; exit 3
    fi
    echo "[$n] $what ($(date +%T))"
    local t0=$SECONDS
    "$fn"
    touch "$DONE/$n.ok"
    echo "[$n] ok em $(( (SECONDS - t0) / 60 )) min"
}

s00() {  # ferramentas
    local miss=0
    for b in bin/nwbt bin/libooz.so bin/texconv.exe bin/vgmstream-cli bin/oggenc; do
        [ -e "$b" ] || { echo "falta $b (docs/SETUP.md, ferramentas)"; miss=1; }
    done
    for c in blender wine python3; do command -v "$c" >/dev/null || { echo "falta $c no PATH"; miss=1; }; done
    python3 -c "import numpy, PIL" 2>/dev/null || { echo "falta python3-numpy / python3-pil"; miss=1; }
    [ -d "$GAME/assets" ] || { echo "não achei o jogo em $GAME (defina NW_GAME)"; miss=1; }
    (( miss == 0 )) || exit 2
}
s01() {  # lista de arquivos dos paks (e dos ícones de item)
    mkdir -p extract/ui
    guarded nwbt_ list -g "$GAME" 2>/dev/null | awk '{print $NF}' > extract/manifest/paths.txt
    grep -E '^lyshineui/images/icons/items/.*\.dds$' extract/manifest/paths.txt > extract/ui/item_icons.txt
    wc -l extract/manifest/paths.txt
}
s02() {  # tabelas do jogo e textos (pt-BR e en-US)
    guarded nwbt_ unpack -g "$GAME" -o extract/data -e --x-datasheet json -w 4 'sharedassets/springboardentitites/datatables/.*\.datasheet$'
    guarded nwbt_ unpack -g "$GAME" -o extract/loc -e -w 4 'localization/(pt-br|en-us)/.*\.loc\.xml$'
}
s03() {  # telas (LyShine), fontes e scripts Lua das telas
    # shellcheck disable=SC2046
    guarded nwbt_ unpack -g "$GAME" -o extract/ui -e -w 4 $(objects_args) 'lyshineui/.*\.dynamicuicanvas$'
    guarded nwbt_ unpack -g "$GAME" -o extract/ui/tex -e -w 2 'lyshineui/fonts/.*\.otf$'
    guarded nwbt_ unpack -g "$GAME" -o extract/ui/luac -e -w 2 'lyshineui/.*\.luac$'
}
s04() {  # receitas de animação (.adb do Mannequin)
    guarded nwbt_ unpack -g "$GAME" -o extract/models -e -w 4 'animations/mannequin/adb/.*\.(adb|xml)$'
}
s05() {  # áudio: música, golpes, passos (tools/extract_audio.py) e ambiente de Guardaventos
    guarded python3 tools/extract_audio.py
    tools/render_ambient.sh windsward
}
s06() {  # dados da região: capitals, heightmap, distribuição, água, texturas de terreno
    guarded nwbt_ unpack -g "$GAME" -o extract/terrain -e -w 4 \
        "sharedassets/coatlicue/$LEVEL/(regions/r_[^/]+/(capitals/[^/]+/[^/]+\.capitals\.json|mapsettings\.json|region\.(heightmap|distribution|waterqt|tractmap\.tif))|[a-z]+\.json)$"
    guarded nwbt_ unpack -g "$GAME" -o extract/terrain -e --x-dds png -w 2 'materials/terrain/.*forest/.*(feildgrass|forestsoil)_a_diff\.dds$'
}
s07() {  # cidade de Guardaventos (assentamento) + recorte de terreno
    tools/convert_city.sh settlement_southwindsward_complete
}
s08() {  # terreno da região inteira (2 km)
    local caps
    caps=$(ls extract/terrain/sharedassets/coatlicue/$LEVEL/regions/$REGION/capitals/06swindsward_persistence/*.json)
    python3 tools/make_terrain.py extract/terrain "$caps" Settlement_SouthWindsward_Complete extract/cities/guardaventos_terrain.glb --region "$REGION"
    # Água: region.waterqt → altura por metro (bin/dbgwater) → malha com rios, lagos e cachoeiras.
    bin/dbgwater "extract/terrain/sharedassets/coatlicue/$LEVEL/regions/$REGION/region.waterqt" "extract/terrain/water_$REGION.f32"
    python3 tools/make_water.py "extract/terrain/water_$REGION.f32" "$caps" Settlement_SouthWindsward_Complete extract/cities/guardaventos_water.glb --region "$REGION"
}
s09() {  # coletáveis: dados + 435 malhas
    python3 tools/make_gatherables.py
    tools/convert_gatherables.sh
}
s10() {  # cenário pré-gerado da região (penhascos, ruínas, props, estradas, água)
    mkdir -p extract/cities/chunks
    local common=(-g "$GAME" -o extract/cities/chunks --binary --embed --ior 1.5 -w 2 --texture-size 2048
        --crc-file "$RTTI/nwt-crc.json" --uuid-file "$RTTI/nwt-types.json" --level "$LEVEL" --region "$REGION")
    [ -f extract/cities/chunks/windsward_capitals.glb ] \
        || guarded nwbt_ models chunks "${common[@]}" --name 06swindsward_capitals -f windsward_capitals.glb
    # Um glb por camada: tudo junto deu 2,4 GB e 150 mil peças, e o import da Unreal esgotou a memória (2026-10-11).
    local layer
    for layer in roaddressing water 06sw01_furybay_props 06sw02_cascadeharbor_props 06sw03_noblereach_props \
        06sw04_stoutmarch_props 06sw05_greenhaven_props 06sw06_eldergate_props 06sw07_perilbrook_props \
        06sw08_grenville_props 06sw09_primrose_prop 06sw_heartforge 06sw_medea terrain_softup_hdeform_a \
        terrain_softup_all_hdeform_c 06swindsward_lore 06swindsward_persistence; do
        [ -f "extract/cities/chunks/windsward_$layer.glb" ] && continue
        guarded nwbt_ models chunks "${common[@]}" --name "$layer" -f "windsward_$layer.glb" || echo "aviso: $layer sem conteúdo"
    done
}
s11() {  # personagem, armas, ferramentas e animações (jogador e Lost)
    guarded python3 tools/build_set.py tools/sets/sword_shield.json
    guarded python3 tools/build_set.py tools/sets/lost_sword.json
}
s13() {  # peças do jogo da arena de testes (muros da arena da Legião, alvos, bonecos, braseiros, rochas)
    guarded tools/convert_gatherables.sh tools/arena/meshes.txt extract/ue/arena
}
s12() {  # Data/ do projeto (catálogo, saque, textos, telas, mapa) e as texturas que as telas usam
    # POIs de Guardaventos para o mapa (posição de cada território; tools/make_worldmap.py).
    [ -f extract/pois/slices/pois/territories/r_+04_+01_pois.dynamicslice.json ] ||
        guarded nwbt_ unpack -g "$GAME" -o extract/pois -e -w 1 $(objects_args) '^slices/pois/territories/r_\+04_\+01_pois\.dynamicslice$'
    tools/make_data.sh
    local pat
    pat=$(cat extract/ui/*_textures.txt | sort -u | while read -r t; do [ -f "extract/ui/tex/${t%.dds}.png" ] || echo "$t"; done \
        | sed 's/[.+]/\\&/g' | paste -sd'|')
    if [ -n "$pat" ]; then
        guarded nwbt_ unpack -g "$GAME" -o extract/ui/tex -e --x-dds png -w 2 "^($pat)$"
    fi
}

step 00 "ferramentas" s00
step 01 "lista dos paks" s01
step 02 "tabelas e textos" s02
step 03 "telas da interface" s03
step 04 "receitas de animação" s04
step 05 "áudio" s05
step 06 "dados da região" s06
step 07 "cidade" s07
step 08 "terreno e água da região" s08
step 09 "coletáveis" s09
step 10 "cenário da região" s10
step 11 "personagens e animações" s11
step 12 "dados do projeto e texturas das telas" s12
step 13 "peças da arena de testes" s13
echo "Pronto. Próximo: feche a Unreal e rode tools/ue_import_all.sh"
