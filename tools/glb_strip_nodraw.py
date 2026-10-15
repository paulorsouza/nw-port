"""Removes from an nwbt glb the primitives the game does not draw.

    python3 tools/glb_strip_nodraw.py <input.glb> <output.glb> [--sem-atributos]

nwbt keeps NoDraw materials (collision/blocking volumes, e.g. green "NoCollide", red "COL8") and
effects (geometryfog, geometrybeam, meshparticle) with opacity 0.1; the UE importer ignores opacity
and draws everything opaque. The shader and flags come in the material's EXT_nw_mtl extension. Only the JSON
changes; the binary chunk is copied as is.
"""
import collections
import json
import struct
import sys

MTL_FLAG_NODRAW = 0x0400
HIDDEN_SHADERS = {"nodraw", "geometryfog", "geometrybeam", "geometrybeamsimple", "meshparticle"}


# --sem-atributos: also drops materials with no game attributes (empty EXT_nw_mtl). In gatherables these are the
# collision meshes ("coll", "collision", "NoCollide1"...), which the importer drew as grey blocks.
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
    # A mesh without primitives is invalid in glTF: the node loses the mesh (stays as an empty node in the hierarchy).
    for nd in j["nodes"]:
        if nd.get("mesh") in empty:
            del nd["mesh"]
    nodes_without = sum(1 for nd in j["nodes"] if "mesh" not in nd)
    for mi in empty:
        j["meshes"][mi]["primitives"] = j["meshes"][mi]["primitives"][:1]  # keeps the index valid; no node uses it

    js = json.dumps(j, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    with open(dst, "wb") as f:
        f.write(struct.pack("<III", magic, version, 12 + 8 + len(js) + 8 + len(binary)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", blen, btype) + binary)
    print(f"hidden materials: {len(drop)}; primitives removed: {sum(removed.values())}; "
          f"empty meshes: {len(empty)}; nodes without mesh now: {nodes_without}")
    for name, c in removed.most_common(15):
        print(f"  {c:5d} {name}")


if __name__ == "__main__":
    main(*sys.argv[1:3])
