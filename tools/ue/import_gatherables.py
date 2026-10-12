"""Importa as malhas dos coletáveis (glb limpos de tools/convert_gatherables.sh), uma malha por .cgf.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_gatherables.py <pasta limpa> [filtro|/Game/destino]"

Cria /Game/NW/Gatherables/<nome>/<nome> (malha única: as partes do glb são combinadas), com materiais e texturas
na mesma pasta (uma pasta por malha: materiais de malhas diferentes têm o mesmo nome, ex. "stone").
Pipelines = os do glTF do projeto (DefaultGLTFAssetsPipeline + DefaultGLTFPipeline, que fazem instâncias do
M_Default com as texturas, como na cidade) com "combinar malhas" ligado numa cópia do primeiro. Só o pipeline
genérico deixava os materiais sem textura (2026-10-11). Colisão por triângulo e Nanite ligados. Pula malhas que já existem (rodar de novo
só importa as novas). ANWGatherField carrega pelo mesmo nome.
"""
import os
import sys
import time

import unreal

SRC = sys.argv[1]
# Segundo argumento: filtro por nome (teste com uma malha) ou, começando com /Game/, outra pasta de destino
# (ex.: as peças da arena, tools/arena/meshes.txt → /Game/NW/Arena).
ARG = sys.argv[2] if len(sys.argv) > 2 else ""
ONLY = "" if ARG.startswith("/Game/") else ARG
DEST = ARG if ARG.startswith("/Game/") else "/Game/NW/Gatherables"
eal = unreal.EditorAssetLibrary
t0 = time.time()

im = unreal.InterchangeManager.get_interchange_manager_scripted()


def pipeline():
    # Cópia do pipeline de assets do glTF com as partes combinadas numa malha só.
    dst = "/Game/NW/Pipelines/NWGatherAssetsPipeline"
    if eal.does_asset_exist(dst):
        p = eal.load_asset(dst)
    else:
        # O registro de assets do commandlet não lista o conteúdo do plugin: carrega pelo caminho e duplica.
        src = unreal.load_object(None, "/Interchange/Pipelines/DefaultGLTFAssetsPipeline.DefaultGLTFAssetsPipeline")
        p = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset("NWGatherAssetsPipeline", "/Game/NW/Pipelines", src)
    mp = p.get_editor_property("mesh_pipeline")
    mp.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.VISIBLE_ONLY)
    mp.set_editor_property("import_skeletal_meshes", False)
    eal.save_asset(dst, only_if_is_dirty=False)
    return [unreal.SoftObjectPath(f"{dst}.NWGatherAssetsPipeline"),
            unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline")]


done, skipped, failed = 0, 0, []
PIPES = pipeline()
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".glb") or ONLY not in f:
        continue
    name = f[:-4]
    folder = f"{DEST}/{name}"
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        skipped += 1
        continue
    src = unreal.InterchangeManager.create_source_data(os.path.join(SRC, f))
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.set_editor_property("override_pipelines", PIPES)
    im.import_asset(folder, src, params)
    new = eal.list_assets(folder, recursive=True)
    meshes = [p for p in new if isinstance(eal.load_asset(p), unreal.StaticMesh)]
    if not meshes:
        failed.append(name)
        continue
    # O Interchange nomeia pela malha/arquivo; garante o nome do .cgf.
    mesh_path = meshes[0]
    if mesh_path.split(".")[0] != path:
        eal.rename_asset(mesh_path.split(".")[0], path)
    m = eal.load_asset(path)
    ns = m.get_editor_property("nanite_settings")
    ns.enabled = True
    m.set_editor_property("nanite_settings", ns)
    bs = m.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_asset(path, only_if_is_dirty=False)
    done += 1

for p in eal.list_assets(DEST, recursive=True):
    eal.save_asset(p, only_if_is_dirty=True)
unreal.log_warning(f"COLETAVEIS importadas={done} existentes={skipped} falhas={len(failed)} {failed[:20]} em {time.time() - t0:.0f}s")
