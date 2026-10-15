# Area 4 (Systems and Everfall): Unreal import. Sourced at the end of tools/linux/ue_import_all.sh.
# Before: tools/linux/pesado.sh tools/linux/convert_city.sh settlement_northwindsward_complete
# (Everfall = capital ef_persistence in r_+04_+02, slice settlement_northwindsward_complete).

sistemas_cidade() {
    py sistemas_city "$ROOT/tools/ue/import_city.py" \
        "$ROOT/extract/cities/limpa/settlement_northwindsward_complete.glb" QuedaEterna
    py sistemas_fix_city "$ROOT/tools/ue/fix_city.py" QuedaEterna
    py sistemas_nanite "$ROOT/tools/ue/nanite_on.py" /Game/NW/Cities/QuedaEterna
}
sistemas_terreno() {
    # Windsward forest textures until the per-biome material exists (docs/backlog.md).
    local T="$ROOT/extract/terrain/textures/terrain/forest"
    py sistemas_terrain "$ROOT/tools/ue/import_terrain.py" "$ROOT/extract/cities/northwindsward_terrain.glb" QuedaEterna \
        "$T/jav_fst_feildgrass_a_diff.png" "$T/jav_fst_forestsoil_a_diff.png"
    py sistemas_materials "$ROOT/tools/ue/fix_nanite_materials.py" /Game/NW/Cities/QuedaEterna
}

step sistemas-01 "Everfall city (L_QuedaEterna)" sistemas_cidade
step sistemas-02 "Everfall terrain and materials" sistemas_terreno
# Starting point: with the editor open on the map, python3 tools/ue/remote.py tools/ue/spawn_trace.py (fix_spawn.py
# left the character below the plaza floor).

sistemas_npcs() {
    # Before: tools/linux/pesado.sh python3 tools/build_set.py tools/sets/npc_queda_eterna.json and
    # tools/linux/pesado.sh python3 tools/convert_npcs.py (both after python3 tools/make_npcs.py).
    py sistemas_npc_anims "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/NpcQuedaEterna/manifest.json"
    py sistemas_npcs "$ROOT/tools/ue/import_npcs.py" "$ROOT/extract/ue/npcs" NpcQuedaEterna L_QuedaEterna
}
step sistemas-03 "Everfall NPCs (animations, appearances, field on the map)" sistemas_npcs

sistemas_exposicao() {
    # Auto exposure only on this map, with the range of the game's day cycle (tools/ue/city_exposure.py).
    py sistemas_exposure "$ROOT/tools/ue/city_exposure.py" L_QuedaEterna
}
step sistemas-04 "Everfall auto exposure" sistemas_exposicao

sistemas_sons_ui() {
    # Before: python3 tools/extract_audio.py (group "ui" -> extract/audio/ui/crafting).
    py sistemas_ui_sounds "$ROOT/tools/ue/import_ui_sounds.py"
}
step sistemas-05 "station interface sounds" sistemas_sons_ui
