#!/usr/bin/env python3
"""Crafting station UI sounds -> unreal/NWPort/Data/Audio/ui_sounds.json (area 4, craftroll-equipamento.md §9).

    python3 tools/make_ui_sounds.py      (after tools/extract_audio.py, group "ui")

For each event in extract/audio/ui/crafting, the layers that play together and, in each layer, the random variations,
read from the wwiser .txtp (extract/_raw/txtp/ui): "group = -R<n>" joins the last n entries into a random pick, "-L<n>"
into a set of layers. The names are the assets of tools/ue/import_ui_sounds.py (S_<file>).
"""
import csv
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIO = os.path.join(ROOT, "extract", "audio")
TXTP = os.path.join(ROOT, "extract", "_raw", "txtp", "ui")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Audio", "ui_sounds.json")


def layers(txtp, by_wem):
    stack = []
    for line in open(txtp, encoding="utf-8"):
        line = line.split("#", 1)[0] if line.lstrip().startswith("#") else line
        m = re.search(r"##(\d+)\.wem", line)
        if m:
            stack.append(("wem", by_wem.get(m.group(1))))
            continue
        g = re.match(r"\s*group\s*=\s*-([RL])(\d+)", line)
        if g:
            n = int(g.group(2))
            items, stack = stack[-n:], stack[:-n]
            stack.append(("rand" if g.group(1) == "R" else "layer", items))

    def flat(node):   # variations of a random pick (or a single file)
        kind, v = node
        return [v] if kind == "wem" else [x for c in v for x in flat(c)]
    if len(stack) == 1 and stack[0][0] == "layer":
        out = [flat(c) for c in stack[0][1]]
    else:
        out = [x for node in stack for x in [flat(node)]] if len(stack) > 1 else [flat(stack[0])] if stack else []
        if len(stack) > 1:
            out = [sum(out, [])]   # several loose entries without a group: treat as a random pick
    return [[n for n in layer if n] for layer in out if any(layer)]


def main():
    by_wem, events = {}, {}
    with open(os.path.join(AUDIO, "sfx_index.csv")) as f:
        for r in csv.DictReader(f):
            if r["file"].startswith("ui/crafting/"):
                by_wem[r["wem_id"]] = "S_" + os.path.splitext(os.path.basename(r["file"]))[0]
                events.setdefault(r["event"], r["txtp"])
    out = {}
    for event, txtp in sorted(events.items()):
        path = os.path.join(TXTP, txtp + ".txtp")
        if os.path.exists(path):
            out[event.removeprefix("play_")] = layers(path, by_wem)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(out, open(OUT, "w"), indent=1)
    print(f"{OUT}: {len(out)} events, layers {[(k, [len(l) for l in v]) for k, v in out.items()]}")


if __name__ == "__main__":
    main()
