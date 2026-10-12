---
name: nw-ue-import
description: Monta o projeto Unreal a partir de extract/ (tools/ue_import_all.sh, 10 etapas retomáveis com commandlets). Usar depois do nw-build-all, ou para refazer um import (malhas, cenário, materiais, interface, sons).
---

# Importar na Unreal (tools/ue_import_all.sh)

Antes:
1. Peça ao usuário para fechar o editor da Unreal (os commandlets gravam nos mesmos assets) e confirme.
2. Nada pesado rodando junto; New World original fechado.

```bash
tools/ue_import_all.sh          # todas as que faltam
tools/ue_import_all.sh 08       # só uma (ex.: 08 = materiais Nanite/instâncias)
```

Cada etapa confere "Python script executed successfully" no log (`unreal/NWPort/Saved/ImportLogs/`); os commandlets
devolvem código de erro por avisos de material do glTF, isso é normal.

Depois: abra o editor para o usuário e peça o print do que quer verificar. Não rode o jogo nem testes automáticos
por conta própria.

```bash
/mnt/jogos/UnrealEngine/5.8/Engine/Binaries/Linux/UnrealEditor unreal/NWPort/NWPort.uproject /Game/NW/Maps/L_SouthWindsward &
```

Material cinza no jogo → etapa 08. Armadilhas da Unreal: `docs/ESTADO.md`.
