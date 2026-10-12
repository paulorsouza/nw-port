# Etapas das esteiras

Cada esteira (`docs/esteiras.md`) acrescenta as próprias etapas aqui, sem mexer na lista numerada dos scripts
principais:

- `<esteira>.build.sh`: extração e conversão, lido no fim do `tools/build_all.sh`.
- `<esteira>.ue.sh`: import na Unreal, lido no fim do `tools/ue_import_all.sh`.

Dentro do arquivo, uma linha por etapa: `step <esteira>-NN "descrição" comando [args]` (no `ue_import_all.sh`, `py
<log> <script> <args>` roda um script da Unreal). Rodar uma só: `tools/ue_import_all.sh montarias-01`. As funções
auxiliares levam o prefixo da esteira (`montarias_cavalo() { ... }`) para não colidir com as dos outros.
