# Area 4 (Systems and Everfall): extraction and conversion. Sourced at the end of tools/linux/build_all.sh.
# Step IDs keep the area's first name (`sistemas-NN`): they are the marker names in extract/.etapas.

systems_city() {
    tools/linux/convert_city.sh settlement_northwindsward_complete
}
systems_slices() {
    # City entities (NPCs, stations, Storage, Trading Post) and housing plots, to place on the map.
    guarded nwbt_ unpack -g "$GAME" -o extract/slicejson -e -w 2 $(objects_args) \
        'slices/(structures/settlements/v2_settlements/settlement_northwindsward_complete|housing/plots/northwindsward/[^/]+)\.dynamicslice$'
    # City NPCs: spawners generated from the capitals ef_lore (7b53567b), ef_persistence (ba5fa8e9), 04nwindsward_lore (8d7747d3).
    guarded nwbt_ unpack -g "$GAME" -o extract/slicejson -e -w 2 $(objects_args) \
        'coatgen/(7b53567b|ba5fa8e9|8d7747d3)/[^/]*npc[^/]*\.dynamicslice$'
}

systems_timelines() {
    # Timelines of the NPC animations (idle, greeting, interaction): they say which .caf each one plays.
    guarded nwbt_ unpack -g "$GAME" -o extract/timelines -e -w 2 $(objects_args) 'libs/timeline/npc/.*\.timeline$'
}

step sistemas-01 "Everfall city and terrain" systems_city
step sistemas-02 "Everfall slices (entities and plots)" systems_slices
step sistemas-03 "NPC timelines" systems_timelines

systems_windsward_slices() {
    # Windsward town (city guardaventos): settlement slice (stations), NPC spawners of the capitals
    # 06swindsward_lore (8492f4cf), 06swindsward_persistence (8d616dd7), 06sw01_furybay_lore (f80302f) and the mission
    # NPCs (Characters/Mission/NPC_06*). Also every NPC .cdf (1260 small XML files: skeleton and attachments).
    guarded nwbt_ unpack -g "$GAME" -o extract/slicejson -e -w 2 $(objects_args) \
        '(slices/structures/settlements/v2_settlements/settlement_southwindsward_complete|coatgen/(8492f4cf|8d616dd7|f80302f)/[^/]*npc[^/]*|slices/characters/mission/npc_06[^/]*)\.dynamicslice$'
    guarded nwbt_ unpack -g "$GAME" -o extract/npcs -e -w 2 '^objects/characters/npc/.*\.cdf$'
}
systems_windsward_npcs() {
    # NPC list and set (tools/make_npcs.py), idle animations (build_set.py) and appearances (convert_npcs.py).
    python3 tools/make_npcs.py guardaventos
    guarded env NW_NWBT_WORKERS=2 python3 tools/build_set.py tools/sets/npc_guardaventos.json
    guarded env NW_NWBT_WORKERS=2 python3 tools/convert_npcs.py guardaventos
}
step sistemas-04 "Windsward slices and NPC .cdf" systems_windsward_slices
step sistemas-05 "Windsward NPCs (list, animations, appearances)" systems_windsward_npcs
