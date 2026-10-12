"""Tabela de textos do C++ nos dois idiomas do clã (pt-BR e en-US).

    python3 tools/build_strings.py

Junta tools/strings_nw.json (textos nossos) com as chaves do jogo que o C++ usa direto (GAME_KEYS), lidas de
extract/loc/localization/<idioma>. Gera unreal/NWPort/Data/UI/nw_strings.json: {chave: {"pt-br": ..., "en-us": ...}}.
"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_mastery import LANGS, load_loc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Textos do jogo usados pelo C++ (telas de entrada e morte, chat).
GAME_KEYS = ["ui_play", "ui_enter", "ui_cancel", "ui_back", "ui_characters", "ui_createcharacter", "ui_deletecharacter",
             "mm_deletechartitle", "mm_deletecharwarning", "ui_confirm_delete_character_placeholder", "ui_charactername",
             "ui_confirm", "mm_disconnected", "ui_deathrecap_killedby", "ui_respawn", "ui_deathrecap_respawn_points",
             "ui_deathrecap_respawn_ready", "ui_chat_name_local", "ui_chat_name_group", "ui_error_systemname",
             "ui_chat_placeholder",
             # Coleta (F4): ações, ferramenta exigida e nomes das ferramentas.
             "ui_mine", "ui_harvest", "ui_log", "ui_notoolitem", "2hpick_groupname", "sickle_groupname", "2haxe_groupname",
             "1hskinningknife_groupname"]
GATHERABLES = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "guardaventos_gatherables.json")


def clean(key, text):
    text = re.sub(r"<[^>]+>", "", text.replace("&lt;", "<").replace("&gt;", ">").replace("\\n", "\n"))
    # Do aviso de exclusão só o primeiro parágrafo; o resto fala da loja.
    return text.split("\n\n")[0] if key == "mm_deletecharwarning" else text


def main():
    out = json.load(open(os.path.join(ROOT, "tools", "strings_nw.json")))
    locs = {lang: load_loc(lang) for lang in LANGS}
    keys = list(GAME_KEYS)
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
