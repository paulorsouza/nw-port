#!/usr/bin/env python3
"""Estilos de texto e cores do jogo (_common/uistyle.lua) -> extract/ui/uistyle.json.

    python3 tools/ui_styles.py [NOME ...]      # sem nome: grava o JSON; com nome: imprime esses estilos

O uistyle.lua decompilado é uma função só, linear (atribuições, tabelas e chamadas, sem if/for). Não há Lua na
máquina: cada linha vira Python e roda num ambiente com os stubs do motor (ColorRgba, enums eUi*) e o StyleHelpers
(ModifyStyle = cópia com campos trocados; Lightness = soma no canal).

Saída: {"NOME": valor}; cor = [r, g, b, a] (0–1), estilo de fonte = {font, fontSize, fontColor, characterSpacing, ...}.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "extract", "lua", "lyshineui", "_common", "uistyle.lua")
OUT = os.path.join(ROOT, "extract", "ui", "uistyle.json")


class Enum(str):
    """Valor de enum do motor (eUiHAlign_Center etc.): guarda o nome."""


def color(r, g, b, a=1):
    return {"r": r / 255.0, "g": g / 255.0, "b": b / 255.0, "a": a}


class Helpers(dict):
    def __init__(self):
        super().__init__()
        self["ModifyStyle"] = lambda _self, base, mod=None: {**(base or {}), **(mod or {})} if mod else base
        self["ChangeAndClampColorChannelValue"] = lambda v, d: min(max(v + d, 0), 1)

        def lightness(_self, c, pct=None):
            if not c:
                return color(255, 255, 255, 1)
            if not pct:
                return c
            k = pct / 100
            return {"r": min(max(c["r"] + k, 0), 1), "g": min(max(c["g"] + k, 0), 1),
                    "b": min(max(c["b"] + k, 0), 1), "a": c["a"]}
        self["Lightness"] = lightness


class Table(dict):
    """Tabela Lua: chave ausente é nil."""
    def __missing__(self, k):
        return None


class Env(dict):
    def __missing__(self, k):
        if k.startswith("e") or k.startswith("ScriptedEntityTweenerEasing"):
            return Enum(k)
        raise KeyError(k)


def to_py(line):
    out = []
    # Só fora das strings: true/false/nil e a.b -> a["b"].
    for i, part in enumerate(re.split(r'("(?:[^"\\]|\\.)*")', line.strip())):
        if i % 2 == 0:
            part = re.sub(r"\btrue\b", "True", part)
            part = re.sub(r"\bfalse\b", "False", part)
            part = re.sub(r"\bnil\b", "None", part)
            part = re.sub(r"(?<=[\w\]])\.([A-Za-z_]\w*)", r'["\1"]', part)
        out.append(part)
    return "".join(out)


def load():
    lines = open(SRC, encoding="utf-8", errors="replace").read().split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("function L2_1(A0_2)"))
    end = next(i for i in range(start + 1, len(lines)) if lines[i] == "end")
    A = Table()
    env = Env(A0_2=A, L0_1=Helpers(), ColorRgba=color, math={"min": min, "max": max})
    for n, line in enumerate(lines[start + 1:end], start + 2):
        s = line.strip()
        if not s or s.startswith("local "):
            continue
        try:
            exec(to_py(s), {}, env)
        except Exception as ex:   # noqa: BLE001
            print(f"uistyle.lua:{n}: {s} -> {ex}", file=sys.stderr)
            raise
    return A


def plain(v):
    if isinstance(v, dict):
        if set(v) == {"r", "g", "b", "a"}:
            return [round(v["r"], 4), round(v["g"], 4), round(v["b"], 4), round(v["a"], 4)]
        return {str(k): plain(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [plain(x) for x in v]
    return v


def main():
    st = {k: plain(v) for k, v in load().items()}
    if len(sys.argv) > 1:
        for k in sys.argv[1:]:
            print(k, json.dumps(st.get(k), ensure_ascii=False))
        return
    json.dump(st, open(OUT, "w"), indent=1, ensure_ascii=False)
    print(f"{OUT}: {len(st)} entradas, {sum(1 for v in st.values() if isinstance(v, dict) and 'fontSize' in v)} estilos de fonte")


if __name__ == "__main__":
    main()
