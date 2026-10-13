"""Importa os CSV de tools/make_datatables.py (extract/ue/<conjunto>/data) como DataTables em /Game/NW/Data.

Chamado pelo import_nw.py; sozinho, para atualizar só as tabelas (sem reimportar malha e animações):
    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_tables.py <manifest>..."
"""
import os
import sys

import unreal


def import_tables(manifest):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    log = unreal.log
    data_dir = os.path.join(os.path.dirname(manifest), "data")
    if not os.path.isdir(data_dir):
        unreal.log_warning(f"sem {data_dir}: DataTables nao importadas (rode tools/make_datatables.py {manifest})")
    for csv_name in sorted(os.listdir(data_dir)) if os.path.isdir(data_dir) else []:
        table = os.path.splitext(csv_name)[0]
        struct_path = ("/Script/NWPort.NWCameraShakeRow" if "CameraShake" in table
                       else "/Script/NWPort.NWDamageRow" if "Damage" in table else "/Script/NWPort.NWActionRow")
        csv_path = os.path.join(data_dir, csv_name)
        struct = unreal.load_object(None, struct_path)
        if not os.path.exists(csv_path) or struct is None:
            unreal.log_warning(f"{table}: pulei (csv ausente ou struct {struct_path} não carregada; compile o projeto e rode tools/make_datatables.py)")
            continue
        factory = unreal.CSVImportFactory()
        settings = factory.get_editor_property("automated_import_settings")
        settings.set_editor_property("import_row_struct", struct)
        settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
        factory.set_editor_property("automated_import_settings", settings)
        task = unreal.AssetImportTask()
        task.filename = csv_path
        task.destination_path = "/Game/NW/Data"
        task.destination_name = table
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.factory = factory
        tools.import_asset_tasks([task])
        log(f"{table}: {list(task.imported_object_paths)}")
    unreal.EditorAssetLibrary.save_directory("/Game/NW/Data", only_if_is_dirty=False, recursive=True)


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        if arg.endswith(".json"):
            import_tables(arg)
