#!/usr/bin/env python3
"""Desmonta um .luac do New World (Lua 5.1, 2 bytes de prefixo antes de "\\x1bLua") em texto legível.

    python3 tools/luac_dump.py extract/ui/luac/lyshineui/markers/staticworldmarker.luac [filtro]

Mostra cada função (linha de definição), as constantes e as instruções com os operandos resolvidos (K = constante,
R = registrador), no estilo do `luac -l -l`. Com filtro, só as funções que citam o texto. O nwbt converte .luac com
um decompilador externo (cLuaDecompiler) que não está na máquina; isto basta para ler regras e números.
"""
import struct
import sys

OPS = ["MOVE", "LOADK", "LOADBOOL", "LOADNIL", "GETUPVAL", "GETGLOBAL", "GETTABLE", "SETGLOBAL", "SETUPVAL",
       "SETTABLE", "NEWTABLE", "SELF", "ADD", "SUB", "MUL", "DIV", "MOD", "POW", "UNM", "NOT", "LEN", "CONCAT", "JMP",
       "EQ", "LT", "LE", "TEST", "TESTSET", "CALL", "TAILCALL", "RETURN", "FORLOOP", "FORPREP", "TFORLOOP", "SETLIST",
       "CLOSE", "CLOSURE", "VARARG"]
ABx = {"LOADK", "GETGLOBAL", "SETGLOBAL", "CLOSURE"}
AsBx = {"JMP", "FORLOOP", "FORPREP"}


class Reader:
    def __init__(self, data):
        self.d, self.p = data, 0

    def take(self, n):
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def byte(self):
        return self.take(1)[0]

    def int(self):
        return struct.unpack("<i", self.take(4))[0]

    def size(self):
        return struct.unpack("<Q", self.take(8))[0]

    def string(self):
        n = self.size()
        return self.take(n)[:-1].decode("utf-8", "replace") if n else None


def function(r):
    f = {"source": r.string(), "line": r.int(), "last": r.int()}
    f["nups"], f["params"], f["vararg"], f["stack"] = r.byte(), r.byte(), r.byte(), r.byte()
    f["code"] = [struct.unpack("<I", r.take(4))[0] for _ in range(r.int())]
    consts = []
    for _ in range(r.int()):
        t = r.byte()
        if t == 0:
            consts.append(None)
        elif t == 1:
            consts.append(bool(r.byte()))
        elif t == 3:
            consts.append(struct.unpack("<d", r.take(8))[0])
        elif t == 4:
            consts.append(r.string())
        else:
            raise SystemExit(f"constante de tipo {t} desconhecido")
    f["consts"] = consts
    f["protos"] = [function(r) for _ in range(r.int())]
    f["lines"] = [r.int() for _ in range(r.int())]
    f["locals"] = [(r.string(), r.int(), r.int()) for _ in range(r.int())]
    f["upvals"] = [r.string() for _ in range(r.int())]
    return f


def rk(f, x):
    if x & 0x100:
        k = f["consts"][x & 0xFF]
        return repr(k) if isinstance(k, str) else str(k)
    return f"R{x}"


def dump(f, depth, flt, out):
    lines = []
    for i, ins in enumerate(f["code"]):
        op = OPS[ins & 0x3F]
        a, c, b, bx = (ins >> 6) & 0xFF, (ins >> 14) & 0x1FF, (ins >> 23) & 0x1FF, ins >> 14
        line = f["lines"][i] if i < len(f["lines"]) else 0
        if op in ABx:
            arg = f"R{a} {repr(f['consts'][bx]) if op != 'CLOSURE' else f'fn{bx}'}"
        elif op in AsBx:
            arg = f"R{a} -> {i + 1 + bx - 131071}"
        elif op in ("GETTABLE", "SELF"):
            arg = f"R{a} = R{b}[{rk(f, c)}]"
        elif op == "SETTABLE":
            arg = f"R{a}[{rk(f, b)}] = {rk(f, c)}"
        elif op in ("ADD", "SUB", "MUL", "DIV", "MOD", "POW", "EQ", "LT", "LE"):
            arg = f"R{a} {rk(f, b)} {rk(f, c)}"
        else:
            arg = f"{a} {b} {c}"
        lines.append(f"  [{line:4}] {op:9} {arg}")
    text = "\n".join(lines)
    if not flt or flt.lower() in text.lower():
        out.append(f"{'  ' * depth}function linha {f['line']}-{f['last']} ({f['params']} params)")
        out.append(text)
    for p in f["protos"]:
        dump(p, depth + 1, flt, out)


def main():
    data = open(sys.argv[1], "rb").read()
    data = data[data.index(b"\x1bLua"):]
    r = Reader(data)
    r.take(12)  # cabeçalho: assinatura, versão 0x51, formato, endian, int, size_t, instrução, número, integral
    out = []
    dump(function(r), 0, sys.argv[2] if len(sys.argv) > 2 else "", out)
    print("\n".join(out))


if __name__ == "__main__":
    main()
