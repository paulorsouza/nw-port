"""Remove de um glb do nwbt as primitivas que o jogo não desenha.

    python3 tools/glb_strip_nodraw.py <entrada.glb> <saida.glb> [--sem-atributos]

O nwbt mantém materiais NoDraw (volumes de colisão/bloqueio, ex. "NoCollide" verde, "COL8" vermelho) e
efeitos (geometryfog, geometrybeam, meshparticle) com opacidade 0.1; o importador do UE ignora a opacidade
e desenha tudo opaco. O shader e as flags vêm na extensão EXT_nw_mtl do material. Só o JSON muda;
o bloco binário é copiado como está.
"""
import collections
import json
import struct
import sys

MTL_FLAG_NODRAW = 0x0400
HIDDEN_SHADERS = {"nodraw", "geometryfog", "geometrybeam", "geometrybeamsimple", "meshparticle"}


# --sem-atributos: também some o material sem atributos do jogo (EXT_nw_mtl vazio). Nos coletáveis são as malhas de
# colisão ("coll", "collision", "NoCollide1"...), que o importador desenhava como blocos cinza.
NO_ATTRS = "--sem-atributos" in sys.argv


def hidden(mat):
    attrs = (mat.get("extensions") or {}).get("EXT_nw_mtl", {}).get("attrs", {})
    if NO_ATTRS and not attrs:
        return True
    shader = str(attrs.get("Shader", "")).lower()
    return shader in HIDDEN_SHADERS or int(attrs.get("MtlFlags", 0)) & MTL_FLAG_NODRAW


def main(src, dst):
    with open(src, "rb") as f:
        magic, version, _ = struct.unpack("<III", f.read(12))
        jlen, _ = struct.unpack("<II", f.read(8))
        j = json.loads(f.read(jlen))
        blen, btype = struct.unpack("<II", f.read(8))
        binary = f.read(blen)

    drop = {i for i, m in enumerate(j["materials"]) if hidden(m)}
    removed = collections.Counter()
    empty = set()
    for mi, mesh in enumerate(j["meshes"]):
        keep = [p for p in mesh["primitives"] if p.get("material") not in drop]
        for p in mesh["primitives"]:
            if p.get("material") in drop:
                removed[j["materials"][p["material"]].get("name")] += 1
        if keep:
            mesh["primitives"] = keep
        else:
            empty.add(mi)
    # Malha sem primitivas é inválida no glTF: o nó perde a malha (continua como nó vazio na hierarquia).
    for nd in j["nodes"]:
        if nd.get("mesh") in empty:
            del nd["mesh"]
    nodes_without = sum(1 for nd in j["nodes"] if "mesh" not in nd)
    for mi in empty:
        j["meshes"][mi]["primitives"] = j["meshes"][mi]["primitives"][:1]  # mantém índice válido; nenhum nó usa

    js = json.dumps(j, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    with open(dst, "wb") as f:
        f.write(struct.pack("<III", magic, version, 12 + 8 + len(js) + 8 + len(binary)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", blen, btype) + binary)
    print(f"materiais ocultos: {len(drop)}; primitivas removidas: {sum(removed.values())}; "
          f"malhas vazias: {len(empty)}; nós sem malha agora: {nodes_without}")
    for name, c in removed.most_common(15):
        print(f"  {c:5d} {name}")


if __name__ == "__main__":
    main(*sys.argv[1:3])
