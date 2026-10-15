# Equipment appearances: Unreal import. Sourced at the end of tools/linux/ue_import_all.sh.
# Before: tools/steps/appearances.build.sh (appearances-01) and the sword_shield set import (step 02).

appearances_import() {
    py appearances "$ROOT/tools/ue/import_appearances.py" "$ROOT/extract/ue/appearances"
}

step appearances-01 "Equipment appearances in /Game/NW/Appearances" appearances_import
