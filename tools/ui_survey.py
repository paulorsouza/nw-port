"""Levantamento das telas LyShine do jogo: estrutura, textos e texturas de cada canvas.

    python3 tools/ui_survey.py [canvas...] > docs/hud-levantamento.md

Sem argumentos: as telas da lista HUD abaixo. Lê extract/ui/lyshineui/**/<nome>.dynamicuicanvas.json (via
tools/ui_canvas.py, mesmo conversor usado pelos ui_build_*.py) e escreve markdown: elementos de primeiro e segundo
nível com âncoras/tamanho, textos @chave traduzidos (pt-BR), texturas usadas e scripts Lua ligados.
"""
import collections
import glob
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_mastery import load_loc  # noqa: E402

HUD = ["escapemenu", "options", "equipmentv3", "paperdollstatuscanvas", "xp", "encumbrance", "compass", "navbar",
       "tooltip", "interactscreen", "lootticker2024", "damagenumbers", "targetlock", "statuseffectsv2",
       "notification", "warningbar", "magicmap", "actionbar", "vitals2024", "weaponquickslots",
       "consumablequickslots", "newinventory", "hitindicator", "afflictions2024"]


def find(name):
    hits = glob.glob(os.path.join(UI, "lyshineui", "**", f"{name}.dynamicuicanvas.json"), recursive=True)
    return hits[0] if hits else None


def convert(path):
    tmp = os.path.join(UI, "survey", os.path.basename(path).split(".")[0] + ".flat.json")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    if not os.path.exists(tmp) or os.path.getmtime(tmp) < os.path.getmtime(path):
        subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"), path, tmp, "--tex", os.path.join(UI, "tex")],
                       check=True, capture_output=True)
    return json.load(open(tmp))


def main():
    names = sys.argv[1:] or HUD
    loc = load_loc("pt-br")
    print("# Levantamento das telas do jogo (gerado por tools/ui_survey.py)\n")
    for name in names:
        path = find(name)
        if not path:
            print(f"## {name}\n\nnão encontrado\n")
            continue
        lay = convert(path)
        els = lay["elements"]
        rel = os.path.relpath(path, os.path.join(UI))
        print(f"## {name}\n\n`{rel}` — {len(els)} elementos, tela {lay['size'][0]}x{lay['size'][1]}\n")
        tops = [e for e in els if e["path"].count("/") in (1, 2)]
        print("| elemento | ligado | âncoras | offsets |\n|---|---|---|---|")
        for e in tops[:40]:
            ind = "&nbsp;&nbsp;" * (e["path"].count("/") - 1)
            print(f"| {ind}{e['path'].split('/')[-1]} | {'sim' if e.get('enabled', True) else 'não'} | "
                  f"{[round(v, 2) for v in e.get('anchors', [])]} | {[round(v) for v in e.get('offsets', [])]} |")
        if len(tops) > 40:
            print(f"| … mais {len(tops) - 40} | | | |")
        texts = collections.OrderedDict()
        for e in els:
            t = (e.get("text") or {}).get("text") or ""
            if t.startswith("@"):
                texts[t] = loc.get(t[1:].lower(), "?")
        if texts:
            print("\nTextos: " + "; ".join(f"`{k}` = {v}" for k, v in list(texts.items())[:30]))
        tex = sorted({e["image"]["texture"] for e in els if e.get("image") and e["image"].get("texture")})
        if tex:
            print(f"\nTexturas ({len(tex)}): " + ", ".join(f"`{t.split('/')[-1]}`" for t in tex[:25]) + (" …" if len(tex) > 25 else ""))
        scripts = sorted({s for e in els for s in (e.get("scripts") or []) if isinstance(s, str)}
        scripts = sorted({s.split("\"")[0] for s in scripts}))
        if scripts:
            print("\nLua: " + ", ".join(f"`{s}`" for s in scripts[:15]))
        print()


if __name__ == "__main__":
    main()
