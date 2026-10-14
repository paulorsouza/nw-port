# Esteira 4 (Sistemas e Queda Eterna): import na Unreal. Lido no fim do tools/ue_import_all.sh.
# Antes: tools/pesado.sh tools/convert_city.sh settlement_northwindsward_complete
# (Queda Eterna = capital ef_persistence em r_+04_+02, slice settlement_northwindsward_complete).

sistemas_cidade() {
    py sistemas_city "$ROOT/tools/ue/import_city.py" \
        "$ROOT/extract/cities/limpa/settlement_northwindsward_complete.glb" QuedaEterna
    py sistemas_fix_city "$ROOT/tools/ue/fix_city.py" QuedaEterna
    py sistemas_nanite "$ROOT/tools/ue/nanite_on.py" /Game/NW/Cities/QuedaEterna
}
sistemas_terreno() {
    # Texturas de floresta de Guardaventos até o material por bioma existir (docs/pendencias.md).
    local T="$ROOT/extract/terrain/textures/terrain/forest"
    py sistemas_terrain "$ROOT/tools/ue/import_terrain.py" "$ROOT/extract/cities/northwindsward_terrain.glb" QuedaEterna \
        "$T/jav_fst_feildgrass_a_diff.png" "$T/jav_fst_forestsoil_a_diff.png"
    py sistemas_materials "$ROOT/tools/ue/fix_nanite_materials.py" /Game/NW/Cities/QuedaEterna
}

step sistemas-01 "cidade de Queda Eterna (L_QuedaEterna)" sistemas_cidade
step sistemas-02 "terreno de Queda Eterna e materiais" sistemas_terreno
# Ponto de partida: com o editor aberto no mapa, python3 tools/ue/remote.py tools/ue/spawn_trace.py (o fix_spawn.py
# deixava o personagem abaixo do piso da praça).

sistemas_npcs() {
    # Antes: tools/pesado.sh python3 tools/build_set.py tools/sets/npc_queda_eterna.json e
    # tools/pesado.sh python3 tools/convert_npcs.py (ambos depois de python3 tools/make_npcs.py).
    py sistemas_npc_anims "$ROOT/tools/ue/import_nw.py" "$ROOT/extract/ue/NpcQuedaEterna/manifest.json"
    py sistemas_npcs "$ROOT/tools/ue/import_npcs.py" "$ROOT/extract/ue/npcs" NpcQuedaEterna L_QuedaEterna
}
step sistemas-03 "NPCs de Queda Eterna (animações, aparências, campo no mapa)" sistemas_npcs

sistemas_exposicao() {
    # Exposição automática só neste mapa, com a faixa do ciclo de dia do jogo (tools/ue/city_exposure.py).
    py sistemas_exposure "$ROOT/tools/ue/city_exposure.py" L_QuedaEterna
}
step sistemas-04 "exposição automática de Queda Eterna" sistemas_exposicao

sistemas_sons_ui() {
    # Antes: python3 tools/extract_audio.py (grupo "ui" -> extract/audio/ui/crafting).
    py sistemas_ui_sounds "$ROOT/tools/ue/import_ui_sounds.py"
}
step sistemas-05 "sons da interface das estações" sistemas_sons_ui
