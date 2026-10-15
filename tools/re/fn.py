"""Mostra a descompilação de funções já salvas em extract/ghidra/dec/*.c (saída do RangeDecompile.java).

    python3 -I tools/re/fn.py 0x<endereço> [0x...] [-n 120] [--decl]

Mantém um índice endereço -> (arquivo, posição) em extract/ghidra/dec/.index.json, refeito quando algum .c muda.
Sem --decl, esconde as linhas de declaração de variáveis locais.
"""
import glob, json, os, re, sys

D = os.path.join(os.path.dirname(__file__), "../../extract/ghidra/dec")
IDX = os.path.join(D, ".index.json")
DECL = re.compile(r"^\s+(undefined\d*|longlong|char|int|float|ulonglong|uint|bool|code|byte|short|ushort|double|"
                  r"undefined\d+ \*|longlong \*)\s*\*?\s*[A-Za-z_0-9\[\] ]*;$")

def index():
    files = sorted(glob.glob(os.path.join(D, "*.c")))
    sig = {f: os.path.getmtime(f) for f in files}
    if os.path.exists(IDX):
        d = json.load(open(IDX))
        if d.get("sig") == sig:
            return d["idx"]
    idx = {}
    for f in files:
        with open(f, "rb") as h:
            pos = 0
            for line in h:
                if line.startswith(b"//==== 0x"):
                    a = line[7:].strip().decode()
                    idx.setdefault(a, [f, pos])
                pos += len(line)
    json.dump({"sig": sig, "idx": idx}, open(IDX, "w"))
    return idx

def show(a, n, decl):
    e = index().get(a)
    if not e:
        print(f"######## {a}: não descompilada"); return
    print(f"######## {a}  ({os.path.basename(e[0])})")
    with open(e[0], "rb") as h:
        h.seek(e[1]); h.readline()
        k = 0
        for line in h:
            if line.startswith(b"//==== "):
                break
            s = line.decode("utf-8", "replace").rstrip()
            if not s.strip() or (not decl and DECL.match(s)):
                continue
            print(s); k += 1
            if k >= n:
                print("  …"); break

args = sys.argv[1:]
n = 120
if "-n" in args:
    i = args.index("-n"); n = int(args[i + 1]); del args[i:i + 2]
decl = "--decl" in args
for a in args:
    if a.startswith("0x"):
        show(hex(int(a, 16)), n, decl)
