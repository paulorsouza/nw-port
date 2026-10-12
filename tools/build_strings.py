"""Tabela de textos do C++ nos dois idiomas do clã (pt-BR e en-US).

    python3 tools/build_strings.py

Junta os arquivos de tools/strings/ (um por esteira, docs/esteiras.md): "textos" (nossos, pt-br e en-us) e
"chaves_do_jogo" (textos do jogo que o C++ usa direto, lidos de extract/loc/localization/<idioma>). A mesma chave de
texto em dois arquivos é erro. Gera unreal/NWPort/Data/UI/nw_strings.json: {chave: {"pt-br": ..., "en-us": ...}}.
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
    # Do aviso de exclusão só o primeiro parágrafo; o resto fala da loja.
    return text.split("\n\n")[0] if key == "mm_deletecharwarning" else text


def load_strings():
    """(textos nossos, chaves do jogo) de todos os arquivos de tools/strings/."""
    out, keys, owner = {}, [], {}
    for name in sorted(os.listdir(STRINGS)):
        if not name.endswith(".json"):
            continue
        d = json.load(open(os.path.join(STRINGS, name)))
        for k, v in d.get("textos", {}).items():
            if k in owner:
                raise SystemExit(f"texto {k} em {owner[k]} e {name}")
            owner[k] = name
            out[k] = v
        keys += [k for k in d.get("chaves_do_jogo", []) if k not in keys]
    return out, keys


def main():
    out, keys = load_strings()
    locs = {lang: load_loc(lang) for lang in LANGS}
    # Nomes dos POIs do mapa (tools/make_worldmap.py).
    worldmap = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "worldmap.json")
    if os.path.exists(worldmap):
        keys += sorted({p["name"] for p in json.load(open(worldmap)).get("pois", [])})
    spawns = os.path.join(os.path.dirname(GATHERABLES), "guardaventos_spawns.json")
    if os.path.exists(spawns):  # nomes das criaturas e das carcaças (tools/make_spawns.py)
        cs = json.load(open(spawns))["creatures"].values()
        keys += sorted({c["name_key"] for c in cs if c["name_key"]} | {c["skin"]["name_key"] for c in cs if c.get("skin")})
    if os.path.exists(GATHERABLES):  # nomes dos coletáveis (tools/make_gatherables.py)
        keys += sorted({v["name_key"].lstrip("@").lower() for v in json.load(open(GATHERABLES))["variants"].values()
                        if v["name_key"]})
    for key in keys:
        out[key] = {lang: clean(key, locs[lang].get(key, key)) for lang in LANGS}
    missing = [k for k, v in out.items() for lang in LANGS if lang not in v]
    if missing:
        raise SystemExit(f"faltam traduções: {sorted(set(missing))}")
    path = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", "nw_strings.json")
    json.dump(out, open(path, "w"), ensure_ascii=False, indent=1)
    print(f"{len(out)} textos -> {path}")


if __name__ == "__main__":
    main()
