"""Xrefs no NewWorld.exe sem análise do Ghidra: referências RIP-relative (disp32) no .text e ponteiros absolutos
de 64 bits em .rdata/.data, com a função dona tirada da tabela .pdata.

    python3 -I tools/re/xref.py str  "UtilitySelector" ...    # strings exatas (terminadas em \\0)
    python3 -I tools/re/xref.py va   0x1480xxxxx ...          # endereços
    python3 -I tools/re/xref.py func 0x1400xxxxx ...          # limites da função que contém o endereço
"""
import mmap, struct, sys, bisect
import numpy as np

EXE = "/mnt/jogos/Steam/steamapps/common/New World/Bin64/NewWorld.exe"
f = open(EXE, "rb"); mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
pe = struct.unpack_from("<I", mm, 0x3C)[0]
nsec = struct.unpack_from("<H", mm, pe + 6)[0]; optsz = struct.unpack_from("<H", mm, pe + 20)[0]
BASE = struct.unpack_from("<Q", mm, pe + 24 + 24)[0]
SECS = {}
for i in range(nsec):
    o = pe + 24 + optsz + 40 * i
    name = mm[o:o + 8].rstrip(b"\0").decode()
    vsz, va, rsz, raw = struct.unpack_from("<IIII", mm, o + 8)
    SECS[name] = (BASE + va, vsz, raw, rsz)

def off2va(off):
    for n, (va, vsz, raw, rsz) in SECS.items():
        if raw <= off < raw + rsz: return va + off - raw
def va2off(v):
    for n, (va, vsz, raw, rsz) in SECS.items():
        if va <= v < va + rsz: return raw + v - va

_pd = None
def pdata():
    global _pd
    if _pd is None:
        va, vsz, raw, rsz = SECS[".pdata"]
        a = np.frombuffer(mm, dtype="<u4", count=vsz // 4, offset=raw).reshape(-1, 3)
        a = a[a[:, 0] != 0]; a = a[np.argsort(a[:, 0])]
        _pd = a
    return _pd
def func_of(v, primary=True):
    """(início, fim) do trecho da .pdata que contém v; com primary, segue UNW_FLAG_CHAININFO até a função dona."""
    a = pdata(); r = v - BASE
    i = np.searchsorted(a[:, 0], r, side="right") - 1
    if not (i >= 0 and a[i, 0] <= r < a[i, 1]): return None
    b, e, u = (int(x) for x in a[i])
    while primary:
        o = va2off(BASE + u)
        if o is None or not (mm[o] >> 3) & 4: break
        n = mm[o + 2]; o += 4 + 2 * (n + (n & 1))
        b, e2, u = struct.unpack_from("<III", mm, o)
    return BASE + b, BASE + e

def func_range(start, span=0x40000):
    """(início, fim) cobrindo todos os pedaços da .pdata encadeados à função que começa em start."""
    a = pdata(); r0 = start - BASE
    i = np.searchsorted(a[:, 0], r0); end = start
    while i < len(a) and a[i, 0] < r0 + span:
        f = func_of(BASE + int(a[i, 0]))
        if f and f[0] == start: end = max(end, BASE + int(a[i, 1]))
        i += 1
    return start, end

def find_str(s):
    b = s.encode() + b"\0"; out = []; i = mm.find(b)
    while i >= 0:
        if i == 0 or mm[i - 1] == 0: out.append(off2va(i))
        i = mm.find(b, i + 1)
    return out

def xrefs(targets):
    targets = np.array(sorted(set(targets)), dtype=np.int64); res = []
    va, vsz, raw, rsz = SECS[".text"]
    for k in range(4):
        n = (rsz - k - 4) // 4
        d = np.frombuffer(mm, dtype="<i4", count=n, offset=raw + k).astype(np.int64)
        pos = va + k + 4 * np.arange(n, dtype=np.int64)
        t = pos + 4 + d
        hit = np.nonzero(np.isin(t, targets))[0]
        res += [(int(pos[h]), int(t[h]), "rip") for h in hit]
        del d, pos, t
    for sec in (".rdata", ".data"):
        va, vsz, raw, rsz = SECS[sec]
        for k in (0,):
            n = min(vsz, rsz) // 8
            q = np.frombuffer(mm, dtype="<u8", count=n, offset=raw).astype(np.int64)
            hit = np.nonzero(np.isin(q, targets))[0]
            res += [(va + 8 * int(h), int(q[h]), "ptr@" + sec) for h in hit]
    return sorted(res)

if __name__ == "__main__":
    mode, args = sys.argv[1], sys.argv[2:]
    if mode == "func":
        for a in args:
            fn = func_of(int(a, 16)); print(a, "->", fn and (hex(fn[0]), hex(fn[1])))
        sys.exit()
    names = {}
    if mode == "str":
        for s in args:
            for v in find_str(s): names[v] = s
    else:
        for a in args: names[int(a, 16)] = a
    for at, t, kind in xrefs(list(names)):
        fn = func_of(at) if kind == "rip" else None
        print(f"{names[t]!r:40} {hex(t)} <- {hex(at)} {kind}" + (f"  fn {hex(fn[0])}-{hex(fn[1])}" if fn else ""))

def vtable(v):
    """Volta enquanto os qwords apontam para .text; devolve (início, [slots])."""
    tva, tvsz = SECS[".text"][0], SECS[".text"][1]
    isc = lambda q: tva <= q < tva + tvsz
    o = va2off(v)
    while isc(struct.unpack_from("<Q", mm, o - 8)[0]): o -= 8
    start = off2va(o); slots = []
    while isc(q := struct.unpack_from("<Q", mm, o)[0]): slots.append(q); o += 8
    return start, slots

def cstr(v, n=200):
    o = va2off(v)
    if o is None: return ""
    e = mm.find(b"\0", o, o + n)
    return mm[o:e if e >= 0 else o + n].decode("latin1")

def rtti_vtables(name_substr):
    """Vtables cujo RTTI (TypeDescriptor) tem o nome com name_substr: [(nome, vtable_va, [slots])]."""
    out = []; pat = name_substr.encode(); i = mm.find(pat)
    rva = lambda v: (v - BASE).to_bytes(4, "little")
    while i >= 0:
        s = mm.rfind(b"\0", 0, i) + 1; e = mm.find(b"\0", i)
        name = mm[s:e].decode("latin1")
        if name.startswith(".?A"):
            td = off2va(s) - 0x10
            j = mm.find(rva(td))
            while j >= 0:
                col = off2va(j) - 12
                if struct.unpack_from("<I", mm, j - 12)[0] in (0, 1):
                    k = mm.find(col.to_bytes(8, "little"))
                    while k >= 0:
                        vt = off2va(k) + 8; out.append((name, vt, vtable(vt)[1] if vtable(vt)[0] == vt else []))
                        k = mm.find(col.to_bytes(8, "little"), k + 1)
                j = mm.find(rva(td), j + 1)
        i = mm.find(pat, e)
    return out

def lambda_invokers(name_substr):
    """AZStd::function de uma lambda pelo nome do RTTI: [(nome, manager, invoker)]. O manager faz lea do
    TypeDescriptor (typeid); na .rdata o par {manager, invoker} fica lado a lado."""
    out = []; pat = name_substr.encode(); i = mm.find(pat)
    while i >= 0:
        s = mm.rfind(b"\0", 0, i) + 1; e = mm.find(b"\0", i)
        td = off2va(s) - 0x10
        for a, t, k in xrefs([td]):
            f = func_of(a)
            if not f: continue
            for p, _, kk in xrefs([f[0]]):
                if kk.startswith("ptr"):
                    nxt = struct.unpack_from("<Q", mm, va2off(p + 8))[0]
                    out.append((mm[s:e].decode("latin1"), f[0], nxt))
        i = mm.find(pat, e)
    return out

def reflect_fields(fn_start):
    """Campos de uma função de reflexão AZ: (nome, offset, tamanho). Dois padrões: (a) SerializeContext::Field —
    lea rdx,<nome> ... mov qword [reg+0x28],<offset> ... mov qword [reg+0x20],<tamanho>; (b) registro por chamada —
    mov r8d,<offset> ... lea rdx,<nome> ... call (tamanho 0 = desconhecido)."""
    import subprocess, re
    b, e = func_range(fn_start)
    out = subprocess.run(["objdump", "-d", "-M", "intel", "--no-show-raw-insn", f"--start-address={hex(b)}",
                          f"--stop-address={hex(e)}", EXE], capture_output=True, text=True).stdout.splitlines()
    res = []; name = None; off = None; r8 = None
    for l in out:
        m = re.search(r"mov\s+r8d,(0x[0-9a-f]+)$", l)
        if m: r8 = int(m.group(1), 16)
        m = re.search(r"lea\s+rdx,\[rip\+0x[0-9a-f]+\]\s+# 0x([0-9a-f]+)", l)
        if m and r8 is not None:
            s2 = cstr(int(m.group(1), 16), 80)
            if s2.isprintable() and 2 < len(s2) < 60: res.append((s2, r8, 0)); r8 = None; continue
        m = re.search(r"lea\s+rdx,\[rip\+0x[0-9a-f]+\]\s+# 0x([0-9a-f]+)", l)
        if m:
            s = cstr(int(m.group(1), 16), 80)
            if s.isprintable() and 2 < len(s) < 60: name = s; off = None
        m = re.search(r"mov\s+QWORD PTR \[\w+\+0x28\],(0x[0-9a-f]+)$", l)
        if m and name: off = int(m.group(1), 16)
        m = re.search(r"mov\s+QWORD PTR \[\w+\+0x20\],(0x[0-9a-f]+)$", l)
        if m and name and off is not None:
            res.append((name, off, int(m.group(1), 16))); name = None
    return res

def datasheet_columns(fn_start):
    """Parser de linha de datasheet (lê coluna por nome e grava numa struct local): [(coluna, destino, bit)].
    Padrão: lea r8,<coluna> ... call <ler célula> ... call <converter> ... grava em [rbp±X] (ou bts/btr num campo de flags)."""
    import subprocess, re
    b, e = func_range(fn_start)
    out = subprocess.run(["objdump", "-d", "-M", "intel", "--no-show-raw-insn", f"--start-address={hex(b)}",
                          f"--stop-address={hex(e)}", EXE], capture_output=True, text=True).stdout.splitlines()
    res = []; col = None; bit = None
    for l in out:
        m = re.search(r"lea\s+r8,\[rip\+0x[0-9a-f]+\]\s+# 0x([0-9a-f]+)", l)
        if m:
            s = cstr(int(m.group(1), 16), 60)
            if s.isprintable() and len(s) > 1: col = s; bit = None
            continue
        if not col: continue
        m = re.search(r"bt[sr]\s+\w+,(0x[0-9a-f]+|\d+)$", l)
        if m: bit = int(m.group(1), 0); continue
        m = re.search(r"mov(?:ss|zx)?\s+(?:DWORD|BYTE|QWORD|WORD) PTR \[(rbp|rsp)([+-]0x[0-9a-f]+)\],(\w+)$", l)
        if m and not m.group(3).startswith("r8"):
            res.append((col, m.group(1) + m.group(2), bit)); col = None
    return res
