#!/usr/bin/env python3
"""Log de itens do nw-core (backend/src/routes/journal.rs): quem, o quê, de onde para onde e por quê.

Uso:
  tools/item_log.py --personagem Exilado [--item OreT1] [--serie <número>] [--desde 2026-10-14T10:00:00Z] [--limite 200]

Lê NW_BACKEND e NW_SERVER_SECRET do ambiente ou de backend/dev.env. Mais novo primeiro; quantidade + entrou, − saiu.
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
    ap.add_argument("--personagem")
    ap.add_argument("--id")
    ap.add_argument("--item")
    ap.add_argument("--serie")
    ap.add_argument("--desde")
    ap.add_argument("--limite", type=int, default=200)
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    e = env()
    q = {k: v for k, v in {"character": a.personagem, "character_id": a.id, "item_id": a.item, "serial": a.serie,
                           "since": a.desde, "limit": a.limite}.items() if v}
    req = urllib.request.Request(e.get("NW_BACKEND", "http://127.0.0.1:8090") + "/internal/item-log?" + urllib.parse.urlencode(q),
                                 headers={"X-NW-Server-Secret": e.get("NW_SERVER_SECRET", "")})
    rows = json.load(urllib.request.urlopen(req))
    if a.json:
        print(json.dumps(rows, ensure_ascii=False, indent=1))
        return
    for r in rows:
        item = r["item_id"] or "moedas"
        extra = []
        if r["gear_score"]:
            extra.append(f"GS {r['gear_score']}")
        if (r.get("props") or {}).get("serial"):
            extra.append(f"série {r['props']['serial']}")
        seq = f"{r['session']}.{r['seq']}" if r["session"] is not None else "nw-core"
        print(f"{r['at'][:19]}  {seq:>9}  {r['reason']:<28} {r['place']:<14} {r['quantity']:+8d}  {item} {' '.join(extra)}")


if __name__ == "__main__":
    main()
