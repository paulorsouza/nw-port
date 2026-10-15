#!/usr/bin/env python3
"""Original on top, port below, at the same size (area 4: screen tuning with tools/linux/ui_shot.sh).

    python3 tools/ui_compare.py <game screenshot> <port capture> <output.png> [--crop x0,y0,x1,y1] [--width 1600]

--crop cuts the same region (in 0–1 screen fractions) from both, to look at detail.
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
# The game screenshot may be stretched (window not 16:9): it is resized to the capture size.
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
