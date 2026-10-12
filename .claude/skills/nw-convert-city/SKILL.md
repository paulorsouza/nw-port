---
name: nw-convert-city
description: Converte uma cidade/assentamento do New World (dynamicslice *_complete) em glb limpo + recorte de terreno com tools/convert_city.sh. Usar para a capital de Queda Eterna (esteira 4) ou quando o dono pedir outra cidade.
---

# Converter uma cidade (tools/convert_city.sh)

Cidades combinadas: Guardaventos (pronta) e a capital de Queda Eterna (esteira 4, `docs/esteiras.md`). Outra cidade,
só com pedido do dono.

Antes: New World original fechado. Roda pela fila de trabalho pesado (`tools/pesado.sh`). O script espera 12 GB
livres para começar e para se cair abaixo de 8 GB.

```bash
tools/pesado.sh tools/convert_city.sh settlement_southwindsward_complete
```

Saída: `extract/cities/limpa/<slice>.glb` e `extract/cities/<curto>_terrain.glb`. Log em
`extract/cities/logs/<slice>.log`. Para levar à Unreal: `tools/ue/import_city.py`, `fix_city.py`, `nanite_on.py`,
`import_terrain.py`, `fix_spawn.py` (ordem em `tools/ue_import_all.sh`, etapas 04 e 05).
