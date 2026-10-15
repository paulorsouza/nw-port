#!/usr/bin/env bash
# Extracts and converts everything the project uses, from the runner's own New World install (docs/setup.md).
# Does not use Unreal: at the end, run tools/linux/ue_import_all.sh with the editor closed.
#
#     tools/linux/build_all.sh            # all missing steps
#     tools/linux/build_all.sh 07         # only step 07 (redoes it even if already done)
#
# Each step writes extract/.etapas/<NN>.ok when it finishes; running again skips what is done. Total time on the
# first run: 2 to 4 hours (8-core machine, SSD). Needs ~60 GB free on the project disk.
#
# Caveats (learned in practice, see docs/status.md):
# - One heavy job at a time. Do not open the original New World or Unreal while this runs: Ubuntu's
#   systemd-oomd closes programs under memory pressure, even with "free" RAM in htop.
# - nwbt loads ~6 GB of pak index on every call; mesh conversions go in small batches.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Heavy-job queue (several sessions on the same machine; docs/workflow.md).
# NW_PESADO: old name, still accepted.
[ -n "${NW_HEAVY:-}${NW_PESADO:-}" ] || exec "$ROOT/tools/linux/heavy.sh" "$ROOT/tools/linux/$(basename "$0")" "$@"
cd "$ROOT"
# shellcheck disable=SC1091
source tools/linux/env.sh
GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
LEVEL=newworld_vitaeeterna
REGION='r_+04_+01'       # Windsward
RTTI=vendor/nw-buddy/tools/nwbt/rtti/nwt
DONE=extract/.etapas
mkdir -p "$DONE" extract/manifest
ONLY="${1:-}"

nwbt_() { nice -n 19 ionice -c3 nwbt "$@"; }
mem_avail_gb() { awk '/^MemAvailable:/ {print int($2 / 1048576)}' /proc/meminfo; }
# Runs a heavy command and interrupts it if free memory drops below 10 GB.
guarded() {
    "$@" &
    local pid=$!
    while kill -0 "$pid" 2>/dev/null; do
        if (( $(mem_avail_gb) < 10 )); then
            echo "ERROR: less than 10 GB of free memory; interrupting: $*" >&2
            kill -TERM "$pid"; pkill -TERM -f texconv 2>/dev/null || true; wait "$pid" || true; exit 3
        fi
        sleep 2
    done
    wait "$pid"
}
# nwbt's --x-objects json looks up the type files with exec.LookPath: executable copies.
objects_args() {
    mkdir -p .nwbt/rtti
    cp -f "$RTTI/nwt-crc.json" "$RTTI/nwt-types.json" .nwbt/rtti/ && chmod +x .nwbt/rtti/*.json
    echo --x-objects json --crc-file "$ROOT/.nwbt/rtti/nwt-crc.json" --uuid-file "$ROOT/.nwbt/rtti/nwt-types.json"
}

step() {  # step NN "description" function [args]
    local n="$1" what="$2"; shift 2
    if [ -n "$ONLY" ] && [ "$ONLY" != "$n" ]; then return; fi
    if [ -z "$ONLY" ] && [ -f "$DONE/$n.ok" ]; then echo "[$n] $what: already done"; return; fi
    if pgrep -x NewWorld.exe >/dev/null || pgrep -f "[N]ewWorldLauncher.exe" >/dev/null; then
        echo "The original New World is open: close it before continuing." >&2; exit 3
    fi
    echo "[$n] $what ($(date +%T))"
    local t0=$SECONDS
    "$@"
    touch "$DONE/$n.ok"
    echo "[$n] ok in $(( (SECONDS - t0) / 60 )) min"
}

s00() {  # tools
    local miss=0
    for b in bin/nwbt bin/libooz.so bin/texconv.exe bin/vgmstream-cli; do
        [ -e "$b" ] || { echo "missing $b (tools/linux/get_tools.sh)"; miss=1; }
    done
    [ -x bin/oggenc ] || command -v oggenc >/dev/null || { echo "missing oggenc (sudo apt install vorbis-tools)"; miss=1; }
    for c in blender wine python3; do command -v "$c" >/dev/null || { echo "missing $c on PATH"; miss=1; }; done
    python3 -c "import numpy, PIL" 2>/dev/null || { echo "missing python3-numpy / python3-pil"; miss=1; }
    [ -d "$GAME/assets" ] || { echo "game not found in $GAME (set NW_GAME)"; miss=1; }
    (( miss == 0 )) || exit 2
}
s01() {  # file list of the paks (and of the item icons)
    mkdir -p extract/ui
    guarded nwbt_ list -g "$GAME" 2>/dev/null | awk '{print $NF}' > extract/manifest/paths.txt
    grep -E '^lyshineui/images/icons/items/.*\.dds$' extract/manifest/paths.txt > extract/ui/item_icons.txt
    wc -l extract/manifest/paths.txt
}
s02() {  # game tables and texts (pt-BR and en-US)
    guarded nwbt_ unpack -g "$GAME" -o extract/data -e --x-datasheet json -w 4 'sharedassets/springboardentitites/datatables/.*\.datasheet$'
    guarded nwbt_ unpack -g "$GAME" -o extract/loc -e -w 4 'localization/(pt-br|en-us)/.*\.loc\.xml$'
}
s03() {  # screens (LyShine), fonts and the screens' Lua scripts
    # shellcheck disable=SC2046
    guarded nwbt_ unpack -g "$GAME" -o extract/ui -e -w 4 $(objects_args) 'lyshineui/.*\.dynamicuicanvas$'
    guarded nwbt_ unpack -g "$GAME" -o extract/ui/tex -e -w 2 'lyshineui/fonts/.*\.(otf|ttf)$'
    guarded nwbt_ unpack -g "$GAME" -o extract/ui/luac -e -w 2 'lyshineui/.*\.luac$'
}
s04() {  # animation recipes (Mannequin .adb)
    guarded nwbt_ unpack -g "$GAME" -o extract/models -e -w 4 'animations/mannequin/adb/.*\.(adb|xml)$'
}
s05() {  # audio: music, hits, footsteps (tools/extract_audio.py) and Windsward ambience
    guarded python3 tools/extract_audio.py
    tools/linux/render_ambient.sh windsward
}
s06() {  # region data: capitals, heightmap, distribution, water, terrain textures
    guarded nwbt_ unpack -g "$GAME" -o extract/terrain -e -w 4 \
        "sharedassets/coatlicue/$LEVEL/(regions/r_[^/]+/(capitals/[^/]+/[^/]+\.capitals\.json|mapsettings\.json|region\.(heightmap|distribution|waterqt|tractmap\.tif))|[a-z]+\.json)$"
    guarded nwbt_ unpack -g "$GAME" -o extract/terrain -e --x-dds png -w 2 'materials/terrain/.*forest/.*(feildgrass|forestsoil)_a_diff\.dds$'
}
s07() {  # Windsward city (settlement) + terrain cutout
    tools/linux/convert_city.sh settlement_southwindsward_complete
}
s08() {  # terrain of the whole region (2 km)
    local caps
    caps=$(ls extract/terrain/sharedassets/coatlicue/$LEVEL/regions/$REGION/capitals/06swindsward_persistence/*.json)
    python3 tools/make_terrain.py extract/terrain "$caps" Settlement_SouthWindsward_Complete extract/cities/guardaventos_terrain.glb --region "$REGION"
    # Water: region.waterqt → height per meter (bin/dbgwater) → mesh with rivers, lakes and waterfalls.
    bin/dbgwater "extract/terrain/sharedassets/coatlicue/$LEVEL/regions/$REGION/region.waterqt" "extract/terrain/water_$REGION.f32"
    python3 tools/make_water.py "extract/terrain/water_$REGION.f32" "$caps" Settlement_SouthWindsward_Complete extract/cities/guardaventos_water.glb --region "$REGION"
}
s09() {  # gatherables: data + 435 meshes
    python3 tools/make_gatherables.py
    tools/linux/convert_gatherables.sh
}
s10() {  # pre-generated region scenery (cliffs, ruins, props, roads, water)
    mkdir -p extract/cities/chunks
    local common=(-g "$GAME" -o extract/cities/chunks --binary --embed --ior 1.5 -w 2 --texture-size 2048
        --crc-file "$RTTI/nwt-crc.json" --uuid-file "$RTTI/nwt-types.json" --level "$LEVEL" --region "$REGION")
    [ -f extract/cities/chunks/windsward_capitals.glb ] \
        || guarded nwbt_ models chunks "${common[@]}" --name 06swindsward_capitals -f windsward_capitals.glb
    # One glb per layer: all together came to 2.4 GB and 150k pieces, and the Unreal import ran out of memory (2026-10-11).
    local layer
    for layer in roaddressing water 06sw01_furybay_props 06sw02_cascadeharbor_props 06sw03_noblereach_props \
        06sw04_stoutmarch_props 06sw05_greenhaven_props 06sw06_eldergate_props 06sw07_perilbrook_props \
        06sw08_grenville_props 06sw09_primrose_prop 06sw_heartforge 06sw_medea terrain_softup_hdeform_a \
        terrain_softup_all_hdeform_c 06swindsward_lore 06swindsward_persistence; do
        [ -f "extract/cities/chunks/windsward_$layer.glb" ] && continue
        guarded nwbt_ models chunks "${common[@]}" --name "$layer" -f "windsward_$layer.glb" || echo "warning: $layer has no content"
    done
}
s11() {  # character, weapons, tools and animations (player and Lost)
    guarded python3 tools/build_set.py tools/sets/sword_shield.json
    guarded python3 tools/build_set.py tools/sets/lost_sword.json
}
s13() {  # game pieces for the test arena (Legion arena walls, targets, dummies, braziers, rocks)
    guarded tools/linux/convert_gatherables.sh tools/arena/meshes.txt extract/ue/arena
}
s12() {  # project Data/ (catalog, loot, texts, screens, map) and the textures the screens use
    # Windsward POIs for the map (position of each territory; tools/make_worldmap.py).
    [ -f extract/pois/slices/pois/territories/r_+04_+01_pois.dynamicslice.json ] ||
        guarded nwbt_ unpack -g "$GAME" -o extract/pois -e -w 1 $(objects_args) '^slices/pois/territories/r_\+04_\+01_pois\.dynamicslice$'
    tools/linux/make_data.sh
    local pat
    pat=$(cat extract/ui/*_textures.txt | sort -u | while read -r t; do [ -f "extract/ui/tex/${t%.dds}.png" ] || echo "$t"; done \
        | sed 's/[.+]/\\&/g' | paste -sd'|')
    if [ -n "$pat" ]; then
        guarded nwbt_ unpack -g "$GAME" -o extract/ui/tex -e --x-dds png -w 2 "^($pat)$"
    fi
}

# Game build must match the team's (tools/game.version).
tools/linux/game_version.sh || exit $?
step 00 "tools" s00
step 01 "pak list" s01
step 02 "tables and texts" s02
step 03 "interface screens" s03
step 04 "animation recipes" s04
step 05 "audio" s05
step 06 "region data" s06
step 07 "city" s07
step 08 "region terrain and water" s08
step 09 "gatherables" s09
step 10 "region scenery" s10
step 11 "characters and animations" s11
step 12 "project data and screen textures" s12
step 13 "test arena pieces" s13
# Area steps (docs/workflow.md): tools/steps/<area>.build.sh calls `step <area>-NN "description" command`.
for f in "$ROOT"/tools/steps/*.build.sh; do
    # shellcheck disable=SC1090
    [ -e "$f" ] && source "$f"
done
echo "Done. Next: close Unreal and run tools/linux/ue_import_all.sh"
