"""C++ string table in the clan's two languages (pt-BR and en-US).

    python3 tools/build_strings.py

Merges the files in tools/strings/ (one per area, docs/workflow.md): "texts" (ours, pt-br and en-us) and
"game_keys" (game strings the C++ uses directly, read from extract/loc/localization/<language>). The same string
key in two files is an error. Writes unreal/NWPort/Data/UI/nw_strings.json: {key: {"pt-br": ..., "en-us": ...}}.
"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_mastery import LANGS, load_loc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STRINGS = os.path.join(ROOT, "tools", "strings")
GATHERABLES = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "guardaventos_gatherables.json")


def clean(key, text):
    text = re.sub(r"<[^>]+>", "", text.replace("&lt;", "<").replace("&gt;", ">").replace("\\n", "\n"))
    # Only the first paragraph of the delete warning; the rest is about the store.
    return text.split("\n\n")[0] if key == "mm_deletecharwarning" else text


def load_strings():
    """(our strings, game keys) from all files in tools/strings/."""
    out, keys, owner = {}, [], {}
    for name in sorted(os.listdir(STRINGS)):
        if not name.endswith(".json"):
            continue
        d = json.load(open(os.path.join(STRINGS, name)))
        # "textos" and "chaves_do_jogo": old key names, still accepted.
        for k, v in [*d.get("texts", {}).items(), *d.get("textos", {}).items()]:
            if k in owner:
                raise SystemExit(f"string {k} in {owner[k]} and {name}")
            owner[k] = name
            out[k] = v
        keys += [k for k in [*d.get("game_keys", []), *d.get("chaves_do_jogo", [])] if k not in keys]
    return out, keys


def main():
    out, keys = load_strings()
    locs = {lang: load_loc(lang) for lang in LANGS}
    # Map POI names (tools/make_worldmap.py).
    worldmap = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "worldmap.json")
    if os.path.exists(worldmap):
        keys += sorted({p["name"] for p in json.load(open(worldmap)).get("pois", [])})
    spawns = os.path.join(os.path.dirname(GATHERABLES), "guardaventos_spawns.json")
    if os.path.exists(spawns):  # creature and carcass names (tools/make_spawns.py)
        cs = json.load(open(spawns))["creatures"].values()
        keys += sorted({c["name_key"] for c in cs if c["name_key"]} | {c["skin"]["name_key"] for c in cs if c.get("skin")})
    if os.path.exists(GATHERABLES):  # gatherable names (tools/make_gatherables.py)
        keys += sorted({v["name_key"].lstrip("@").lower() for v in json.load(open(GATHERABLES))["variants"].values()
                        if v["name_key"]})
    for key in keys:
        out[key] = {lang: clean(key, locs[lang].get(key, key)) for lang in LANGS}
    missing = [k for k, v in out.items() for lang in LANGS if lang not in v]
    if missing:
        raise SystemExit(f"missing translations: {sorted(set(missing))}")
    path = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", "nw_strings.json")
    json.dump(out, open(path, "w"), ensure_ascii=False, indent=1)
    print(f"{len(out)} strings -> {path}")


if __name__ == "__main__":
    main()
