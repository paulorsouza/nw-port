# Levar os assets de uma máquina para outra (zips pelo OneDrive)

Importar uma vez, numa máquina, e levar o resultado para as outras (Linux ↔ Windows, Windows ↔ Windows). Código
continua no GitHub. O que o git não leva vai em zips por uma pasta compartilhada do OneDrive; upload e download são
feitos à mão. Scripts: `tools/assets_export.py` e `tools/assets_import.py`.

## O que vai e o que não vai

| Pasta | Vai? | Motivo |
|---|---|---|
| `unreal/NWPort/Content` (menos os arquivos versionados no git) | sim | resultado do import; `.uasset`/`.umap` são iguais no Linux e no Windows na mesma versão da Unreal |
| `unreal/NWPort/Data` | sim | gerado pelos `make_*.py` |
| lista dos `extract/.etapas/ue_*.ok` | sim, dentro do manifest | a outra máquina não refaz no `ue_import_all.sh` o import que veio pronto |
| `extract/` | não | só serve para importar de novo ou converter coisa nova; cada máquina extrai o seu |
| `DerivedDataCache`, `Intermediate`, `Saved`, `Binaries` | não | dependem da plataforma; cada máquina refaz |
| `ABP_NW.uasset`, `ABP_Lost.uasset` | não | estão no git |

## A pasta

Cada máquina tem uma pasta local que espelha a do OneDrive. Padrão `~/nw-port-assets` (no Windows,
`C:\Users\<user>\nw-port-assets`); outra com `--dir` ou `NW_ASSETS`.

```
nw-port-assets/
  manifest.json      geração, id, máquina, data, commit, versão da Unreal, build do jogo, marcadores, lista dos zips
  bundles/*.zip      um zip por pasta do Content de até 1 GiB
```

O nome do zip leva o hash do conteúdo (`Content__NW__Region__06sw02_cascadeharbor_props-1a2b3c4d5e6f.zip`): zip
novo nunca tem o nome de um antigo. Medido em 2026-10-15: 514 zips, ~18 GB de origem.

## Exportar (fim do dia, na máquina que importou)

1. Editor fechado. Código commitado e no GitHub.
2. Baixe o `manifest.json` do OneDrive para a pasta (se outra máquina exportou depois do seu último import, o
   export para e pede o import antes).
3. Rode:

   ```bash
   python3 tools/assets_export.py
   ```

   Windows: `py tools\assets_export.py`.

4. O script lista o que subir e o que apagar. Suba para a pasta do OneDrive os zips novos (para `bundles/`) e, por
   último, o `manifest.json`. Depois apague do OneDrive os zips que ele mandou apagar.

Na primeira vez sobe tudo: a pasta `bundles/` inteira e o `manifest.json`.

## Importar (na outra máquina)

1. Editor fechado. `git pull`.
2. Baixe só o `manifest.json` do OneDrive para a pasta e rode:

   ```bash
   python3 tools/assets_import.py
   ```

3. O script compara com os arquivos daqui e lista os zips que faltam. Baixe esses zips para `bundles/` (ou soltos na
   pasta) e rode de novo.
4. Ele confere o sha256 de cada zip, mostra o resumo e pergunta. Aplicado, a máquina fica na geração da publicação.

Na primeira vez: baixe a pasta `bundles/` inteira.

`--check` só mostra o que mudaria. `--yes` não pergunta.

## O que os scripts conferem

Export:

- editor fechado; no Linux, entra na fila do `tools/pesado.sh`;
- commit atual no GitHub e nada sem commit em `unreal/NWPort/Source`, `Config`, `.uproject` ou `tools/`
  (`--allow-dirty` passa por cima);
- o `manifest.json` da pasta é o da publicação que esta máquina tem (exportou ou importou por último); se não for,
  outra máquina exportou depois: importe antes, ou `--force` para descartar a publicação dela;
- mesma versão da Unreal;
- nomes que funcionam no Windows/OneDrive (maiúscula/minúscula, caracteres, nomes reservados, caminho até 240 a
  partir de `C:\nw-port`);
- menos da metade dos arquivos da publicação atual (clone novo, pasta apagada): para;
- nada mudou: não cria geração nova.

Import:

- mesma versão da Unreal, mesma build do jogo (`tools/jogo.versao`), commit da publicação já no `HEAD` local
  (`--force` passa por cima);
- tamanho e sha256 de cada zip contra o manifest (download incompleto não é aplicado);
- zip só extrai dentro da própria pasta;
- arquivo daqui substituído ou que não está na publicação vai para `unreal/NWPort/Saved/assets_backup/<data>/`
  (ficam os 3 últimos); arquivos do git não são tocados.

Depois do import, a Unreal compila na primeira abertura os shaders dos assets que mudaram.

## Regras

- Uma máquina importa por vez. Duas importando no mesmo dia: a segunda a exportar é barrada e escolhe (importar e
  refazer o import dela, ou `--force`).
- Sempre baixe o `manifest.json` mais novo antes de exportar ou importar.
- Reimport da Unreal regrava o `.uasset` mesmo sem mudança real: uma pasta reimportada gera zip novo.
- Estado da máquina em `unreal/NWPort/Saved/assets_state.json`; hashes em cache em `assets_hashcache.json` (só
  arquivo mexido é lido de novo; a primeira leitura do Content inteiro leva ~2 min).

## Windows

- Python: `winget install Python.Python.3.13`.
- Unreal fora de `C:\Program Files\Epic Games\UE_5.*`: `NW_UE`.
- Pipeline no WSL: `$env:NW_EXTRACT = "\\wsl$\Ubuntu\home\<user>\nw-port\extract"` para os marcadores irem para o
  `extract/` do WSL.

## Pendente

- Import em lote no Windows (`ue_import_all.ps1`, skill `nw-wsl`). Até existir, Windows → Linux vale para o que for
  importado à mão no editor do Windows.
