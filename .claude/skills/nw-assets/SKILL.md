---
name: nw-assets
description: Exporta o Content/Data importados em zips (tools/assets_export.py) ou importa os zips de outra máquina (tools/assets_import.py); upload e download pelo OneDrive são do dono. Usar no fim do dia depois de um import, ou ao começar numa máquina (Linux ou Windows) que não importou.
---

# Assets entre máquinas (zips pelo OneDrive)

Roteiro: `docs/sincronizar-assets.md`. Pasta padrão `~/nw-port-assets` (`--dir` ou `NW_ASSETS`).

Antes: peça ao usuário para fechar o editor e para baixar o `manifest.json` mais novo do OneDrive para a pasta.

Exportar (máquina que importou; código commitado e no GitHub):

```bash
python3 tools/assets_export.py
```

Relate ao usuário a lista "Suba..." e "pode apagar..." da saída: ele faz o upload à mão. Mudança de outras esteiras
sem commit em `Source/` ou `tools/`: pergunte ao dono se exporta com `--allow-dirty`.

Importar (outra máquina, depois de `git pull`):

```bash
python3 tools/assets_import.py --check
```

Saída 2 = faltam zips: passe a lista ao usuário para baixar e rode de novo. Mostre o resumo, confirme com ele e
aplique com `--yes`.

Nunca use `--force` sem o dono pedir. Depois do import, abra o editor para o usuário (a Unreal compila os shaders do
que mudou).
