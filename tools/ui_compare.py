#!/usr/bin/env python3
"""Original em cima, port embaixo, no mesmo tamanho (esteira 4: ajuste de telas com tools/ui_shot.sh).

    python3 tools/ui_compare.py <print do jogo> <captura do port> <saída.png> [--crop x0,y0,x1,y1] [--width 1600]

--crop recorta a mesma região (em frações 0–1 da tela) das duas, para olhar detalhe.
"""
import argparse
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("orig")
ap.add_argument("port")
ap.add_argument("out")
ap.add_argument("--crop")
ap.add_argument("--width", type=int, default=1600)
a = ap.parse_args()
ims = [Image.open(p).convert("RGB") for p in (a.orig, a.port)]
# O print do jogo pode vir esticado (janela fora de 16:9): vai para o tamanho da captura.
ims[0] = ims[0].resize(ims[1].size)
if a.crop:
    f = [float(v) for v in a.crop.split(",")]
    W, H = ims[0].size
    ims = [im.crop((int(f[0] * W), int(f[1] * H), int(f[2] * W), int(f[3] * H))) for im in ims]
w = a.width
ims = [im.resize((w, int(im.height * w / im.width))) for im in ims]
out = Image.new("RGB", (w, ims[0].height + ims[1].height + 6), (255, 0, 255))
out.paste(ims[0], (0, 0))
out.paste(ims[1], (0, ims[0].height + 6))
out.save(a.out)
