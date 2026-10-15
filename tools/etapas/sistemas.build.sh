# Area 4 (Systems and Everfall): extraction and conversion. Sourced at the end of tools/linux/build_all.sh.

sistemas_cidade() {
    tools/linux/convert_city.sh settlement_northwindsward_complete
}
sistemas_slices() {
    # City entities (NPCs, stations, Storage, Trading Post) and housing plots, to place on the map.
    guarded nwbt_ unpack -g "$GAME" -o extract/slicejson -e -w 2 $(objects_args) \
        'slices/(structures/settlements/v2_settlements/settlement_northwindsward_complete|housing/plots/northwindsward/[^/]+)\.dynamicslice$'
    # City NPCs: spawners generated from the capitals ef_lore (7b53567b), ef_persistence (ba5fa8e9), 04nwindsward_lore (8d7747d3).
    guarded nwbt_ unpack -g "$GAME" -o extract/slicejson -e -w 2 $(objects_args) \
        'coatgen/(7b53567b|ba5fa8e9|8d7747d3)/[^/]*npc[^/]*\.dynamicslice$'
}

sistemas_timelines() {
    # Timelines of the NPC animations (idle, greeting, interaction): they say which .caf each one plays.
    guarded nwbt_ unpack -g "$GAME" -o extract/timelines -e -w 2 $(objects_args) 'libs/timeline/npc/.*\.timeline$'
}

step sistemas-01 "Everfall city and terrain" sistemas_cidade
step sistemas-02 "Everfall slices (entities and plots)" sistemas_slices
step sistemas-03 "NPC timelines" sistemas_timelines
