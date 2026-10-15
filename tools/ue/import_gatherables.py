"""Imports the gatherable meshes (clean glb files from tools/linux/convert_gatherables.sh), one mesh per .cgf.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_gatherables.py <limpa folder> [filter|/Game/dest]"

Creates /Game/NW/Gatherables/<name>/<name> (single mesh: the glb parts are combined), with materials and textures
in the same folder (one folder per mesh: materials of different meshes share names, e.g. "stone").
Pipelines = the project's glTF ones (DefaultGLTFAssetsPipeline + DefaultGLTFPipeline, which make M_Default
instances with the textures, as in the city) with "combine meshes" on in a copy of the first. The generic
pipeline alone left the materials without textures (2026-10-11). Per-triangle collision and Nanite on. Skips meshes that already exist (running
again only imports the new ones). ANWGatherField loads by the same name.
"""
import os
import sys
import time

import unreal

SRC = sys.argv[1]
# Second argument: name filter (test with one mesh) or, when it starts with /Game/, another destination folder
# (e.g. the arena pieces, tools/arena/meshes.txt → /Game/NW/Arena).
ARG = sys.argv[2] if len(sys.argv) > 2 else ""
ONLY = "" if ARG.startswith("/Game/") else ARG
DEST = ARG if ARG.startswith("/Game/") else "/Game/NW/Gatherables"
eal = unreal.EditorAssetLibrary
t0 = time.time()

im = unreal.InterchangeManager.get_interchange_manager_scripted()


def pipeline():
    # Copy of the glTF assets pipeline with the parts combined into a single mesh.
    dst = "/Game/NW/Pipelines/NWGatherAssetsPipeline"
    if eal.does_asset_exist(dst):
        p = eal.load_asset(dst)
    else:
        # The commandlet's asset registry does not list the plugin content: load by path and duplicate.
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
    # Interchange names by mesh/file; enforce the .cgf name.
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
unreal.log_warning(f"GATHERABLES imported={done} existing={skipped} failed={len(failed)} {failed[:20]} in {time.time() - t0:.0f}s")
