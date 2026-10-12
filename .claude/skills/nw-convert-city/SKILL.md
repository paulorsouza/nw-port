---
name: nw-convert-city
description: Converte uma cidade/assentamento do New World (dynamicslice *_complete) em glb limpo + recorte de terreno com tools/convert_city.sh. Usar só quando o usuário pedir outra cidade (o escopo atual é uma região por vez).
---

# Converter uma cidade (tools/convert_city.sh)

Escopo combinado: uma região por vez (hoje Guardaventos). Só converta outra cidade se o usuário pedir.

Antes: nada pesado rodando, New World original fechado. O script espera 12 GB livres para começar e para se
cair abaixo de 8 GB.

```bash
tools/convert_city.sh settlement_southwindsward_complete
```

Saída: `extract/cities/limpa/<slice>.glb` e `extract/cities/<curto>_terrain.glb`. Log em
`extract/cities/logs/<slice>.log`. Para levar à Unreal: `tools/ue/import_city.py`, `fix_city.py`, `nanite_on.py`,
`import_terrain.py`, `fix_spawn.py` (ordem em `tools/ue_import_all.sh`, etapas 04 e 05).
