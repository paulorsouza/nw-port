#!/usr/bin/env python3
"""nw-core item log (backend/src/routes/journal.rs): who, what, from where to where and why.

Usage:
  tools/item_log.py --character Exilado [--item OreT1] [--serial <number>] [--since 2026-10-14T10:00:00Z] [--limit 200]

Reads NW_BACKEND and NW_SERVER_SECRET from the environment or backend/dev.env. Newest first; quantity + in, − out.
"""
import argparse
import json
import os
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def env():
    vals = {}
    path = os.path.join(ROOT, "backend", "dev.env")
    if os.path.exists(path):
        for line in open(path):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                vals[k] = v.strip().strip('"')
    vals.update({k: v for k, v in os.environ.items() if k.startswith("NW_")})
    return vals


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--character")
    ap.add_argument("--id")
    ap.add_argument("--item")
    ap.add_argument("--serial")
    ap.add_argument("--since")
    ap.add_argument("--limit", type=int, default=200)
    ap.add_argument("--json", action="store_true")
    # Old spellings, still accepted (hidden from --help).
    for old, dest in (("--personagem", "character"), ("--serie", "serial"), ("--desde", "since")):
        ap.add_argument(old, dest=dest, help=argparse.SUPPRESS)
    ap.add_argument("--limite", dest="limit", type=int, help=argparse.SUPPRESS)
    a = ap.parse_args()
    e = env()
    q = {k: v for k, v in {"character": a.character, "character_id": a.id, "item_id": a.item, "serial": a.serial,
                           "since": a.since, "limit": a.limit}.items() if v}
    req = urllib.request.Request(e.get("NW_BACKEND", "http://127.0.0.1:8090") + "/internal/item-log?" + urllib.parse.urlencode(q),
                                 headers={"X-NW-Server-Secret": e.get("NW_SERVER_SECRET", "")})
    rows = json.load(urllib.request.urlopen(req))
    if a.json:
        print(json.dumps(rows, ensure_ascii=False, indent=1))
        return
    for r in rows:
        item = r["item_id"] or "coins"
        extra = []
        if r["gear_score"]:
            extra.append(f"GS {r['gear_score']}")
        if (r.get("props") or {}).get("serial"):
            extra.append(f"serial {r['props']['serial']}")
        seq = f"{r['session']}.{r['seq']}" if r["session"] is not None else "nw-core"
        print(f"{r['at'][:19]}  {seq:>9}  {r['reason']:<28} {r['place']:<14} {r['quantity']:+8d}  {item} {' '.join(extra)}")


if __name__ == "__main__":
    main()
