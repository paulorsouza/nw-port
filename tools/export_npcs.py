# blender -b -P tools/export_npcs.py -- extract/ue/npcs/lista.json
#
# Exporta cada NPC (glb do nwbt) como extract/ue/npcs/<SK_nome>/<SK_nome>.fbx (malha + esqueleto, sem animação) e
# grava materials.json (material -> canais em PNG) ao lado, no formato de export_ue.py / import_nw.py.
# Lista: [{"glb": "...", "mesh": "SK_<nome>", "cdf": "<.cdf extraído>"}], gerada por tools/convert_npcs.py.
#
# O .cdf dos NPCs é um modelo com todos os encaixes preenchidos (corpo nu masculino em shirt1, cavanhaque, capacete);
# o jogo esconde os de Flags="1" (CryEngine: escondido). Só ficam os encaixes de pele visíveis. shirt2 é a roupa
# do tronco aqui (no jogador o nwchar descarta shirt2).
import json
import os
import sys
import xml.etree.ElementTree as ET

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nwchar  # noqa: E402

lista_path = sys.argv[sys.argv.index("--") + 1]
root = os.path.dirname(lista_path)
for item in json.load(open(lista_path)):
    out = os.path.join(root, item["mesh"])
    os.makedirs(out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    hidden = {a.get("AName") for a in ET.parse(item["cdf"]).getroot().iter("Attachment")
              if a.get("Type") == "CA_SKIN" and int(a.get("Flags") or 0) & 1}
    arm = nwchar.import_character(item["glb"], drop={"xray", "Icosphere"} | hidden)
    arm.name = "Armature"  # o Unreal descarta o objeto de armadura como osso raiz só com este nome
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.parent == arm]
    nwchar.to_centimeters(arm, meshes)
    materials = nwchar.flatten_materials(os.path.join(out, "textures"))
    arm.animation_data_clear()
    bpy.ops.object.select_all(action="DESELECT")
    for o in [arm] + meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, item["mesh"] + ".fbx"), use_selection=True,
                             object_types={"ARMATURE", "MESH"}, bake_anim=False, path_mode="COPY",
                             embed_textures=True, add_leaf_bones=False, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE",
                             use_armature_deform_only=False)
    json.dump(materials, open(os.path.join(out, "materials.json"), "w"), indent=1)
    print(f"NPC {item['mesh']}: {len(meshes)} malhas, {len(materials)} materiais")
