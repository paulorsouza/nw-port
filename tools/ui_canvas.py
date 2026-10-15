"""Converts a game LyShine screen (.dynamicuicanvas as JSON from `nwbt unpack --x-objects json`) into a flat layout.

    python3 tools/ui_canvas.py <screen.dynamicuicanvas.json> <output.json> [--preview preview.png] [--tex <png dir>]

Output: elements in draw order, each with an absolute rectangle on the reference screen (canvassize, 1920x1080),
image (texture, color, type, fill, 9-slice borders), text, opacity and visibility.

LyShine rules (UiTransform2dComponent): in the parent's space,
    left = parent.left + anchor.left * parent_width + offset.left   (same for right/top/bottom)
Pivot, rotation and scale rotate/scale around the pivot; here only scale goes into the rectangle (rotation is noted).
"""
import argparse
import json
import os
import re
import xml.etree.ElementTree as ET

IMAGE_TYPES = {0: "stretched", 1: "sliced", 2: "fixed", 3: "tiled", 4: "stretched_fit", 5: "stretched_fill"}
FILL_TYPES = {0: "none", 1: "linear", 2: "radial", 3: "radial_corner", 4: "radial_edge"}


def walk(o):
    if isinstance(o, dict):
        yield o
        for v in o.values():
            yield from walk(v)
    elif isinstance(o, list):
        for v in o:
            yield from walk(v)


def as_list(v):
    """nwbt JSON writes a single-item list as the object itself."""
    if v is None:
        return []
    return v if isinstance(v, list) else [v]


def components(ent):
    c = ent.get("components", {})
    return as_list(c.get("element")) if isinstance(c, dict) else as_list(c)


def find(ent, type_name):
    return next((c for c in components(ent) if c.get("__type") == type_name), None)


def sprite_borders(tex_dir, texture):
    """9-slice lines in the .sprite next to the texture: fractions 0..1 from the left/top."""
    if not tex_dir or not texture:
        return None
    path = os.path.join(tex_dir, os.path.splitext(texture)[0] + ".sprite")
    if not os.path.exists(path):
        return None
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError:
        return None
    for el in root.iter("Sprite"):
        if el.get("m_left") is not None:
            return {k: float(el.get("m_" + k, 0)) for k in ("left", "right", "top", "bottom")}
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("canvas")
    ap.add_argument("out")
    ap.add_argument("--preview")
    ap.add_argument("--tex", help="folder where the textures were extracted as png (same tree as the pak)")
    ap.add_argument("--keep", default="", help="paths (comma-separated) whose subtrees are kept; the rest is dropped (ancestors are kept)")
    ap.add_argument("--enable", default="", help="element names forced on (the Lua enables them in game)")
    ap.add_argument("--exclude", default="HudLayoutElementDraggable",
                    help="names (comma-separated) whose subtrees are left out (e.g. edit-mode handles)")
    a = ap.parse_args()

    d = json.load(open(a.canvas))
    canvas = next((o for o in walk(d) if o.get("__type") == "UiCanvasComponent"), None)
    ents = {str(o["id"]["id"]): o for o in walk(d) if o.get("__type") == "AZ::Entity" and "id" in o}
    if canvas:
        cw, ch = canvas["canvassize"]
        roots = [str(canvas["rootelement"]["id"])]
    else:
        # Screen slice (.dynamicslice, e.g. banners): no canvas; reference screen 1920x1080 and root = element that
        # is no other element's child.
        cw, ch = 1920, 1080
        children = {str(k["childentityid"]["id"]) for o in walk(d) if isinstance(o, dict) and "childentityid" in o
                    for k in [o]}
        roots = [i for i, e in ents.items() if find(e, "UiElementComponent") and i not in children]

    out = []
    excluded = {x for x in a.exclude.split(",") if x}
    keep = [x for x in a.keep.split(",") if x]
    enable = {x for x in a.enable.split(",") if x}

    def kept(path):
        return not keep or any(path.startswith(k) or k.startswith(path + "/") or k == path for k in keep)

    def visit(eid, parent_rect, depth, parent_alpha, parent_visible, path="", parent_index=-1):
        ent = ents.get(eid)
        if ent is None or ent.get("name") in excluded:
            return
        path = f"{path}/{ent.get('name')}" if path else ent.get("name")
        if not kept(path):
            return
        tr = find(ent, "UiTransform2dComponent")
        el = find(ent, "UiElementComponent")
        L, T, R, B = parent_rect
        pw, ph = R - L, B - T
        if tr:
            an, of = tr["anchors"], tr["offsets"]
            l = L + an["left"] * pw + of["left"]
            r = L + an["right"] * pw + of["right"]
            t = T + an["top"] * ph + of["top"]
            b = T + an["bottom"] * ph + of["bottom"]
            sx, sy = tr.get("scale", [1, 1])
            px, py = tr.get("pivot", [0.5, 0.5])
            if sx != 1 or sy != 1:
                cx, cy = l + (r - l) * px, t + (b - t) * py
                l, r = cx - (cx - l) * sx, cx + (r - cx) * sx
                t, b = cy - (cy - t) * sy, cy + (b - cy) * sy
            rot = tr.get("rotation", 0)
        else:
            l, t, r, b, rot = L, T, R, B, 0
            sx = sy = 1
        fader = find(ent, "UiFaderComponent")
        alpha = parent_alpha * (fader["fade"] if fader else 1.0)
        on = (el is None or el.get("isenabled", True)) or ent.get("name") in enable
        visible = parent_visible and on
        item = {"id": eid, "name": ent.get("name"), "path": path, "depth": depth, "parent": parent_index,
                "rect": [round(l, 2), round(t, 2), round(r, 2), round(b, 2)],
                "rotation": rot, "alpha": round(alpha, 4), "visible": visible,
                # To redo the layout at another resolution (C++): the element's local values.
                "anchors": [an["left"], an["top"], an["right"], an["bottom"]] if tr else [0, 0, 1, 1],
                "offsets": [of["left"], of["top"], of["right"], of["bottom"]] if tr else [0, 0, 0, 0],
                "pivot": tr.get("pivot", [0.5, 0.5]) if tr else [0.5, 0.5], "scale": [sx, sy] if tr else [1, 1],
                "fade": fader["fade"] if fader else 1.0, "enabled": on}
        mask = find(ent, "UiMaskComponent")
        if mask and mask.get("enablemasking", True):
            # Mask: clips the children; its image only shows with drawbehind/drawinfront.
            item["mask"] = True
            item["mask_draw"] = bool(mask.get("drawbehind") or mask.get("drawinfront"))
        img = find(ent, "UiImageComponent")
        if img:
            tex = (img.get("spritetexture") or {}).get("hint", "")
            item["image"] = {
                "texture": tex, "color": img.get("color"), "alpha": img.get("alpha", 1),
                "type": IMAGE_TYPES.get(img.get("imagetype", 0), "stretched"),
                "blend": img.get("blendmode", 0),  # 0 normal, 1 additive, 2 screen, 3 darken, 4 lighten
                "fill": FILL_TYPES.get(img.get("filltype", 0), "none"), "fill_amount": img.get("fillamount", 1),
                "fill_start_angle": img.get("fillstartangle", 0), "fill_clockwise": img.get("fillclockwise", True),
                "fill_edge_origin": img.get("filledgeorigin", 0), "fill_center": img.get("fillcenter", True),
                "borders": sprite_borders(a.tex, tex),
            }
        fitter = find(ent, "UiLayoutFitterComponent")
        if fitter and fitter.get("horizontalfit"):
            item["fit_w"] = True   # width = the text's (UiLayoutFitter); children anchored to the edge follow
        txt = find(ent, "UiTextComponent")
        if txt:
            item["text"] = {"text": txt.get("text", ""), "font": (txt.get("fontfilename") or {}).get("baseclass1", {}).get("assetpath", ""),
                            "size": txt.get("fontsize", 16), "color": txt.get("color"), "alpha": txt.get("alpha", 1),
                            "halign": txt.get("texthalignment", 0), "valign": txt.get("textvalignment", 0),
                            "spacing": txt.get("characterspacing", 0), "case": txt.get("caseoverride", 0),
                            "wrap": txt.get("wraptextsetting", 0), "linespacing": txt.get("linespacing", 0),
                            "shrink": txt.get("shrinktofit", 0), "minshrink": txt.get("minshrinkscale", 0)}
        scripts = [c for c in components(ent) if c.get("__type") == "AzFramework::ScriptComponent"]
        if scripts:
            item["scripts"] = [re.sub(r".*hint\"?:\s*\"?", "", json.dumps(s.get("script", s)))[:120] for s in scripts]
        out.append(item)
        my_index = len(out) - 1
        if el:
            kids = sorted(as_list((el.get("childentityidorder") or {}).get("element")), key=lambda k: int(k.get("sortindex", 0)))
            for k in kids:
                visit(str(k["childentityid"]["id"]), (l, t, r, b), depth + 1, alpha, visible, path, my_index)
        # Index of the last descendant (to clip a mask's subtree).
        out[my_index]["end"] = len(out) - 1

    for root in roots:
        visit(root, (0, 0, cw, ch), 0, 1.0, True)
    json.dump({"canvas": a.canvas, "size": [cw, ch], "elements": out}, open(a.out, "w"), indent=1)
    textures = sorted({e["image"]["texture"] for e in out if e.get("image") and e["image"]["texture"]})
    print(f"{len(out)} elements, {len(textures)} textures, screen {cw}x{ch}")

    if a.preview:
        preview(out, (cw, ch), a.tex, a.preview)


def nine_slice(src, w, h, b):
    """Corners at the texture's original size, edges and center stretched."""
    from PIL import Image
    sw, sh = src.size
    xs = [0, int(b["left"] * sw), int(b["right"] * sw), sw]
    ys = [0, int(b["top"] * sh), int(b["bottom"] * sh), sh]
    lw, rw = xs[1], sw - xs[2]
    th, bh = ys[1], sh - ys[2]
    if lw + rw > w:
        k = w / max(lw + rw, 1); lw, rw = int(lw * k), int(rw * k)
    if th + bh > h:
        k = h / max(th + bh, 1); th, bh = int(th * k), int(bh * k)
    dx = [0, lw, w - rw, w]
    dy = [0, th, h - bh, h]
    out = Image.new("RGBA", (w, h))
    for i in range(3):
        for j in range(3):
            box = (xs[i], ys[j], xs[i + 1], ys[j + 1])
            size = (dx[i + 1] - dx[i], dy[j + 1] - dy[j])
            if size[0] > 0 and size[1] > 0 and box[2] > box[0] and box[3] > box[1]:
                out.paste(src.crop(box).resize(size), (dx[i], dy[j]))
    return out


def preview(elements, size, tex_dir, path, background=None):
    from PIL import Image, ImageDraw
    img = Image.open(background).convert("RGBA").resize(size) if background else Image.new("RGBA", size, (40, 44, 40, 255))
    draw = ImageDraw.Draw(img)
    cache = {}

    def texture(p):
        if p not in cache:
            f = os.path.join(tex_dir or "", os.path.splitext(p)[0] + ".png")
            cache[p] = Image.open(f).convert("RGBA") if tex_dir and os.path.exists(f) else None
        return cache[p]

    clips = []  # (end of subtree, rectangle)
    for i, e in enumerate(elements):
        while clips and i > clips[-1][0]:
            clips.pop()
        if not e["visible"] or e["alpha"] <= 0.01:
            continue
        l, t, r, b = e["rect"]
        if e.get("mask"):
            clips.append((e.get("end", i), (l, t, r, b)))
            if not e.get("mask_draw"):
                continue
        if clips:
            cl, ct, cr, cb = clips[-1][1]
            for _, (ml, mt, mr, mb) in clips:
                cl, ct, cr, cb = max(cl, ml), max(ct, mt), min(cr, mr), min(cb, mb)
            if cr <= cl or cb <= ct:
                continue
        w, h = int(round(r - l)), int(round(b - t))
        if w <= 0 or h <= 0:
            continue
        if e.get("image"):
            im = e["image"]
            col = im.get("color") or [1, 1, 1, 1]
            a = e["alpha"] * im.get("alpha", 1)
            src = texture(im["texture"]) if im["texture"] else None
            if src is None:
                tile = Image.new("RGBA", (w, h), tuple(int(c * 255) for c in col[:3]) + (int(255 * a),))
            else:
                tile = nine_slice(src, w, h, im.get("borders")) if im["type"] == "sliced" and im.get("borders") else src.resize((w, h))
                mult = Image.new("RGBA", (w, h), tuple(int(c * 255) for c in col[:3]) + (255,))
                tile = Image.composite(Image.blend(tile, Image.new("RGBA", (w, h)), 0), tile, Image.new("L", (w, h), 0))
                r_, g_, b_, a_ = tile.split()
                r_ = r_.point(lambda v, k=col[0]: int(v * k)); g_ = g_.point(lambda v, k=col[1]: int(v * k)); b_ = b_.point(lambda v, k=col[2]: int(v * k))
                a_ = a_.point(lambda v, k=a: int(v * k))
                tile = Image.merge("RGBA", (r_, g_, b_, a_))
            if im["fill"] == "linear" and im["fill_amount"] < 1:
                cut = int(w * im["fill_amount"])
                mask = Image.new("L", (w, h), 0); ImageDraw.Draw(mask).rectangle([0, 0, cut, h], fill=255)
                tile.putalpha(Image.composite(tile.getchannel("A"), mask, mask))
            if im.get("blend") == 1:
                # Additive: adds the color (weighted by alpha) to what is already drawn.
                from PIL import ImageChops
                x0, y0 = int(round(l)), int(round(t))
                region = img.crop((x0, y0, x0 + w, y0 + h)).convert("RGB")
                rgb = Image.merge("RGB", [ImageChops.multiply(c, tile.getchannel("A")) for c in tile.convert("RGB").split()])
                img.paste(ImageChops.add(region, rgb).convert("RGBA"), (x0, y0))
                continue
            if clips:
                box = (int(max(0, cl - l)), int(max(0, ct - t)), int(min(w, cr - l)), int(min(h, cb - t)))
                if box[2] <= box[0] or box[3] <= box[1]:
                    continue
                tile = tile.crop(box)
                img.alpha_composite(tile, (int(round(l)) + box[0], int(round(t)) + box[1]))
            else:
                img.alpha_composite(tile, (int(round(l)), int(round(t))))
        if e.get("text") and e["text"]["text"]:
            tx = e["text"]
            col = tuple(int(c * 255) for c in (tx.get("color") or [1, 1, 1])[:3])
            draw.text((l + 2, t + (b - t) / 2 - 6), tx["text"][:40], fill=col)
    img.save(path)
    print("preview:", path)


if __name__ == "__main__":
    main()
