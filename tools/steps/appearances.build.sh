# Equipment appearances (equipped items change the player's look): conversion. Sourced at the end of
# tools/linux/build_all.sh. Needs the sword_shield set (build_set.py) and Data/Items/items.pt-br.json (make_items.py).
# Output in extract/ue/appearances/{Body,Armor,Weapons}; Data/Items/appearances.json (tools/make_appearances.py).

appearances_convert() {
    python3 tools/convert_appearances.py
}

step appearances-01 "Equipment appearances (body parts, armor, weapons) to FBX" appearances_convert
