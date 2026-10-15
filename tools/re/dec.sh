#!/bin/bash
# Decompiles functions in headless Ghidra (port 8089), creating the function first. Usage: tools/re/dec.sh 0x1426... [...]
for a in "$@"; do
  curl -s -m 60 -X POST -H 'Content-Type: application/json' -d "{\"address\":\"$a\"}" http://127.0.0.1:8089/create_function >/dev/null
  echo "//==== $a"
  curl -s -m 180 "http://127.0.0.1:8089/force_decompile?function=$a" | python3 -c 'import json,sys
t=sys.stdin.read()
try:
  d=json.loads(t); print(d.get("decompiled") or d.get("code") or d.get("result") or json.dumps(d)[:3000])
except Exception: print(t)'
done
