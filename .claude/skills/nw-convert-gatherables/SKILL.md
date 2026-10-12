---
name: nw-convert-gatherables
description: Converte as malhas dos coletáveis (minérios, árvores, plantas, pedras) listadas por make_gatherables.py, em lotes protegidos, com tools/convert_gatherables.sh. Usar quando a lista de coletáveis mudar ou faltarem malhas.
---

# Converter malhas dos coletáveis (tools/convert_gatherables.sh)

Antes: `python3 tools/make_gatherables.py` (gera `extract/ue/gatherables/meshes.txt`). Nada pesado junto.

```bash
tools/convert_gatherables.sh
```

Lotes de 8 malhas com 2 workers (cada chamada do nwbt carrega ~6 GB de índice); um vigia interrompe se a memória
livre cair abaixo de 10 GB. Pula o que já existe. Saída limpa em `extract/ue/gatherables/limpa/` (sem malhas de
colisão nem NoDraw).

Na Unreal: `tools/ue_import_all.sh 06` e depois `08` (materiais).
