---
name: nw-render-ambient
description: Renderiza o ambiente sonoro de um território (vida selvagem por bioma, assentamento) dos .txtp do jogo para .ogg com tools/render_ambient.sh. Usar ao preparar sons de uma região.
---

# Ambiente sonoro (tools/render_ambient.sh)

Leve. Precisa dos `.txtp` do `tools/extract_audio.py` (etapa 05 do `build_all.sh`).

```bash
tools/render_ambient.sh windsward
```

Saída em `extract/audio/ambient/<território>/`. Na Unreal: `tools/ue_import_all.sh 10` (importa em loop para
`/Game/NW/Audio`); quem toca é o `UNWAmbienceComponent`.
