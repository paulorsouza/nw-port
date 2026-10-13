# Esteira 4 (Sistemas e Queda Eterna): extração e conversão. Lido no fim do tools/build_all.sh.

sistemas_cidade() {
    tools/convert_city.sh settlement_northwindsward_complete
}
sistemas_slices() {
    # Entidades da cidade (NPCs, estações, armazém, posto de trocas) e lotes de casa, para posicionar no mapa.
    guarded nwbt_ unpack -g "$GAME" -o extract/slicejson -e -w 2 $(objects_args) \
        'slices/(structures/settlements/v2_settlements/settlement_northwindsward_complete|housing/plots/northwindsward/[^/]+)\.dynamicslice$'
    # NPCs da cidade: spawners gerados dos capitals ef_lore (7b53567b), ef_persistence (ba5fa8e9), 04nwindsward_lore (8d7747d3).
    guarded nwbt_ unpack -g "$GAME" -o extract/slicejson -e -w 2 $(objects_args) \
        'coatgen/(7b53567b|ba5fa8e9|8d7747d3)/[^/]*npc[^/]*\.dynamicslice$'
}

sistemas_timelines() {
    # Timelines das animações dos NPCs (parado, cumprimento, interação): dizem que .caf cada uma toca.
    guarded nwbt_ unpack -g "$GAME" -o extract/timelines -e -w 2 $(objects_args) 'libs/timeline/npc/.*\.timeline$'
}

step sistemas-01 "cidade e terreno de Queda Eterna" sistemas_cidade
step sistemas-02 "slices de Queda Eterna (entidades e lotes)" sistemas_slices
step sistemas-03 "timelines dos NPCs" sistemas_timelines
