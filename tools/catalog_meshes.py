"""Geometry hash of each mesh in the scenery glb files, for the single catalog (tools/ue/catalog.py).

    python3 tools/catalog_meshes.py [glb...]   # no arguments: extract/cities/chunks/*.glb and extract/cities/limpa/*.glb

Output: extract/ue/catalogo_malhas.json = {glb stem: [[hash, xyz extent, node name], ...] in the glb's mesh order}.
Interchange names mesh i of the glb <stem>_mesh_<i>; the extent confirms the match with the imported mesh.
Hash = attributes (POSITION, NORMAL, TEXCOORD...) + indices of each primitive, in order. Materials are left out:
Unreal compares the slots after merging the materials.
"""
import glob
import hashlib
import json
import mmap
import os
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "extract/ue/catalogo_malhas.json")


def meshes(path):
    with open(path, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        n, = struct.unpack_from("<I", m, 12)
        j = json.loads(m[20:20 + n])
        base = 20 + n + 8
        acc, views = j["accessors"], j["bufferViews"]

        def raw(i):
            a = acc[i]
            v = views[a["bufferView"]]
            o = base + v.get("byteOffset", 0) + a.get("byteOffset", 0)
            return m[o:o + v["byteLength"] - a.get("byteOffset", 0)]

        names = {}
        for node in j.get("nodes", []):
            if "mesh" in node and node.get("name") and node["name"] != "Merged":
                names.setdefault(node["mesh"], node["name"])
        out = []
        for i, me in enumerate(j["meshes"]):
            h = hashlib.sha1()
            lo, hi = [float("inf")] * 3, [float("-inf")] * 3
            for pr in me["primitives"]:
                h.update(b"|prim")
                for k in sorted(pr["attributes"]):
                    h.update(k.encode())
                    h.update(raw(pr["attributes"][k]))
                if "indices" in pr:
                    h.update(b"|idx")
                    h.update(raw(pr["indices"]))
                p = acc[pr["attributes"]["POSITION"]]
                lo = [min(a, b) for a, b in zip(lo, p["min"])]
                hi = [max(a, b) for a, b in zip(hi, p["max"])]
            out.append([h.hexdigest()[:16], [round(b - a, 3) for a, b in zip(lo, hi)], names.get(i, "")])
        return out


def main():
    paths = sys.argv[1:] or sorted(glob.glob(os.path.join(ROOT, "extract/cities/chunks/*.glb"))
                                   + glob.glob(os.path.join(ROOT, "extract/cities/limpa/*.glb")))
    data = json.load(open(OUT)) if os.path.exists(OUT) else {}
    for p in paths:
        stem = os.path.splitext(os.path.basename(p))[0]
        data[stem] = meshes(p)
        print(f"{stem}: {len(data[stem])} meshes", file=sys.stderr)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(data, f)
    total = sum(len(v) for v in data.values())
    uniq = len({m[0] for v in data.values() for m in v})
    print(f"{OUT}: {total} meshes in {len(data)} glb, {uniq} distinct geometries")


if __name__ == "__main__":
    main()
