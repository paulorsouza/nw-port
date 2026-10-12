"""Tela de morte do jogo (lyshineui/respawn/respawn.dynamicuicanvas).

    python3 tools/ui_build_respawn.py [--preview prévia.png] [--background fundo.png]

Fica: vinheta, "Morto Por" com o nome, contador e o botão Renascer com o título dos pontos de renascimento.
Sai: Twitch, câmera da morte, estatísticas, mapa, itens perdidos e o tutorial. Gera
unreal/NWPort/Data/UI/respawn_layout.<idioma>.json e extract/ui/respawn_textures.txt.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_login import ROOT, TEX, UI, convert, relayout, set_image, FRAME_BG  # noqa: E402
from ui_build_mastery import LANGS, load_loc  # noqa: E402

KEEP = [
    "_root/Vignette",
    "_root/BlueFadeCornerBg",
    "_root/Window/KilledByContainer",
    "_root/Window/RespawnTimerHolder/RespawnTimer",
    "_root/Window/RespawnMapHolder/RespawnMapButtonAccept",
    "_root/Window/RespawnMapHolder/RespawnMapTitleContainer",
]
EXCLUDE = ["TwitchViewersHolder", "SequenceFogLoop", "KilledByNameTweenValue", "DeathCamButton", "AnimationSpinner",
           "buttonHintHolder", "buttonIconHolder", "buttonSecondaryIconHolder", "buttonSecondaryIconSprite",
           "heroImageSequence", "heroImageSequenceDisabled", "buttonTooltipSetter", "spinner", "buttonFocusHero",
           "buttonTextPrefix"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    ap.add_argument("--background")
    a = ap.parse_args()
    lay = convert(os.path.join(UI, "lyshineui", "respawn", "respawn.dynamicuicanvas.json"), KEEP, EXCLUDE, [])
    for e in lay["elements"]:
        if e["path"] == "_root":
            # A raiz do canvas sai como caixa de 100x100 no centro; a tela inteira é o pai da vinheta e do canto.
            e["anchors"], e["offsets"] = [0, 0, 1, 1], [0, 0, 0, 0]
        if e["path"].endswith("FrameMultiBg/FrameBg") and e.get("image") and not e["image"]["texture"]:
            set_image(e, FRAME_BG)
    for lang in LANGS:
        loc = load_loc(lang)
        elements = json.loads(json.dumps(lay["elements"]))
        for e in elements:
            t = e.get("text")
            if t and t["text"].startswith("@"):
                t["text"] = loc.get(t["text"][1:].lower(), t["text"])
        out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"respawn_layout.{lang}.json")
        json.dump({"canvases": [{"name": "respawn", "size": lay["size"], "elements": elements}]}, open(out, "w"), separators=(",", ":"))
    textures = sorted({e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]})
    open(os.path.join(UI, "respawn_textures.txt"), "w").write("\n".join(textures))
    print(f"{len(lay['elements'])} elementos, {len(textures)} texturas -> {out}")
    if a.preview:
        import ui_canvas
        relayout(lay["elements"], lay["size"])
        ui_canvas.preview(lay["elements"], tuple(lay["size"]), TEX, a.preview, a.background)


if __name__ == "__main__":
    main()
