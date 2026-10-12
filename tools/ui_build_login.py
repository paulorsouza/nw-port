"""Tela de entrada do clã a partir das telas do jogo.

    python3 tools/ui_build_login.py [--preview prévia.png] [--background fundo.png]

- landing: lyshineui/main menu/landingscreenv2 — logo, painel de personagens (3 vagas) e botão Jogar; o popup
  do Twitch vira a janela de login.
- input: campo de nome da criação de personagem (charactercreation, NameInputField), desenhado pelo C++ em cada
  campo de texto.

Gera unreal/NWPort/Data/UI/login_layout.<idioma>.json e extract/ui/login_textures.txt. Aplica o que o Lua faria ao abrir
(estilo da moldura, fundos das vagas e textos pt-BR).
"""
import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_mastery import LANGS, load_loc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
TEX = os.path.join(UI, "tex")
SH = "_root/LandingMaster/ScreenHolder"
LANDING_KEEP = [
    f"{SH}/NavMenuHolder/NavMenuBg",
    f"{SH}/NavMenuHolder/NewWorldLogo",
    f"{SH}/CharacterSelection/CharacterInfoHolder/FrameMultiBg",
    f"{SH}/CharacterSelection/CharacterInfoHolder/CharcterFrameHeader",
    f"{SH}/CharacterSelection/CharacterInfoHolder/CharacterListScrollbox",
    f"{SH}/CharacterSelection/PlayButton",
    f"{SH}/TwitchInfoPopup/PopupScrim",
    f"{SH}/TwitchInfoPopup/PopupContainer",
]
LANDING_EXCLUDE = [
    "CharacterListScrollbar", "CharacterGuildCrest", "CharacterMoreInfoButton", "MoreInfoButton", "MouseoverTooltipSetter",
    "CharacterTransferButton", "CharacterMergeHint", "CharacterMergeTime", "CharacterEmptyLockIcon", "Statement1", "Statement2",
    "Statement3", "Statement4", "DescriptionText1", "DescriptionText2", "buttonHintHolder", "buttonIconHolder",
    "buttonSecondaryIconHolder", "buttonSecondaryIconSprite", "heroImageSequence", "heroImageSequenceDisabled",
    "SelectedSequence", "FocusMask", "buttonTooltipSetter", "spinner", "StatusSpinnerEntity", "buttonTextPrefix",
    "ButtonClose", "buttonFocusHero",
]
INPUT_KEEP = ["_root/CharacterCreationMaster/ScreenHolder/CharacterCreationIdentity/NameInputField"]
INPUT_EXCLUDE = ["NameValidationIcon"]
FRAME_BG = "lyshineui/images/slices/framemultibg/framestyles/framebgdefault2023.dds"
LIST_ITEM_BG = "lyshineui/images/slices/listitembg/listitembg.dds"
# Trocadas pelo C++ conforme o estado (selecionada / passando o mouse).
EXTRA = [FRAME_BG, LIST_ITEM_BG, "lyshineui/images/slices/listitembg/focushighlight.dds",
         "lyshineui/images/slices/listitembg/selectedsequence/listitem_long17.dds"]


def sprite_borders(texture):
    sp = os.path.join(TEX, os.path.splitext(texture)[0] + ".sprite")
    if os.path.exists(sp):
        m = re.search(r'm_left="([\d.]+)" m_right="([\d.]+)" m_top="([\d.]+)" m_bottom="([\d.]+)"', open(sp).read())
        if m:
            return dict(zip(("left", "right", "top", "bottom"), map(float, m.groups())))
    return None


def set_image(e, texture, kind="sliced"):
    img = e.setdefault("image", {"color": [1, 1, 1, 1], "alpha": 1, "blend": 0, "fill": "none", "fill_amount": 1,
                                 "fill_start_angle": 0, "fill_clockwise": True, "fill_edge_origin": 0, "fill_center": True})
    img["texture"] = texture
    img["type"] = kind
    img["borders"] = sprite_borders(texture) if kind == "sliced" else None


def relayout(elements, size):
    """Recalcula "rect" como o FNWUILayout::DrawCanvas (anchors + offsets sobre o pai, escala no pivô)."""
    for e in elements:
        par = e.get("parent", -1)
        L, T, R, B = elements[par]["rect"] if 0 <= par < len(elements) else (0, 0, size[0], size[1])
        w, h = R - L, B - T
        a, o = e["anchors"], e["offsets"]
        l, t, r, b = L + a[0] * w + o[0], T + a[1] * h + o[1], L + a[2] * w + o[2], T + a[3] * h + o[3]
        sx, sy = e.get("scale", [1, 1])
        if sx != 1 or sy != 1:
            px, py = e.get("pivot", [0, 0])
            cx, cy = l + (r - l) * px, t + (b - t) * py
            l, r, t, b = cx - (cx - l) * sx, cx + (r - cx) * sx, cy - (cy - t) * sy, cy + (b - cy) * sy
        e["rect"] = [l, t, r, b]


def convert(canvas, keep, exclude, enable):
    tmp = os.path.join(UI, os.path.basename(canvas).split(".")[0] + ".login.json")
    subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"), canvas, tmp, "--tex", TEX,
                    "--keep", ",".join(keep), "--exclude", ",".join(exclude), "--enable", ",".join(enable)], check=True)
    return json.load(open(tmp))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    ap.add_argument("--background")
    a = ap.parse_args()
    landing = convert(os.path.join(UI, "lyshineui", "main menu", "landingscreenv2.dynamicuicanvas.json"), LANDING_KEEP,
                      LANDING_EXCLUDE, ["TwitchInfoPopup", "CharacterSelection"])
    inp = convert(os.path.join(UI, "lyshineui", "main menu", "charactercreation.dynamicuicanvas.json"), INPUT_KEEP,
                  INPUT_EXCLUDE, ["NameInputField", "CharacterCreationIdentity"])

    for e in landing["elements"]:
        p = e["path"]
        if p.endswith("FrameMultiBg/FrameBg") and e.get("image") and not e["image"]["texture"]:
            set_image(e, FRAME_BG)
        if p.endswith("ListItemBg/itemBg"):
            set_image(e, LIST_ITEM_BG)
        # Janela de login: menor que o popup do Twitch.
        if p.endswith("TwitchInfoPopup/PopupContainer"):
            e["offsets"] = [-330, -250, 330, 250]
    # Campo na origem da tela; o C++ posiciona cada campo com Translate.
    for e in inp["elements"]:
        if e["path"].endswith("NameInputField"):
            e["anchors"], e["offsets"] = [0, 0, 0, 0], [0, 0, 497, 54]
        elif "NameInputField" not in e["path"]:
            e["anchors"], e["offsets"], e["scale"] = [0, 0, 1, 1], [0, 0, 0, 0], [1, 1]
    # Um arquivo por idioma; os textos que o C++ escolhe na hora ficam em nw_strings.json (tools/build_strings.py).
    for lang in LANGS:
        loc = load_loc(lang)
        canvases = []
        for name, lay in (("landing", landing), ("input", inp)):
            elements = json.loads(json.dumps(lay["elements"]))
            for e in elements:
                t = e.get("text")
                if t and t["text"].startswith("@"):
                    t["text"] = loc.get(t["text"][1:].lower(), t["text"])
            canvases.append({"name": name, "size": lay["size"], "elements": elements})
        out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"login_layout.{lang}.json")
        json.dump({"canvases": canvases}, open(out, "w"), separators=(",", ":"))
    textures = sorted({e["image"]["texture"].lower() for lay in (landing, inp) for e in lay["elements"]
                       if e.get("image") and e["image"]["texture"]} | set(EXTRA))
    open(os.path.join(UI, "login_textures.txt"), "w").write("\n".join(textures))
    print(f"{len(landing['elements'])} + {len(inp['elements'])} elementos, {len(textures)} texturas -> {out}")
    if a.preview:
        import ui_canvas
        relayout(landing["elements"], landing["size"])
        ui_canvas.preview(landing["elements"], tuple(landing["size"]), TEX, a.preview, a.background)


if __name__ == "__main__":
    main()
