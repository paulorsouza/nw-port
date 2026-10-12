---
name: nw-make-data
description: Regenera unreal/NWPort/Data (catálogo de itens, saque, coletáveis, textos pt/en, layouts das telas) com tools/make_data.sh. Usar depois de mudar tools/make_*.py, ui_build_*.py, strings_nw.json ou as receitas.
---

# Regenerar Data/ (tools/make_data.sh)

Leve (só Python, segundos). Não precisa fechar nada.

```bash
tools/make_data.sh
```

Ordem fixa dentro do script: coletáveis → itens (o saque dos coletáveis define itens) → habilidades → textos →
telas. Se uma tela nova usar texturas que ainda não existem em PNG, rode `tools/build_all.sh 12` e depois
`tools/ue_import_all.sh 09`.

Texto novo do C++: chave em `tools/strings_nw.json` (pt-br e en-us) ou, se vier do jogo, em `GAME_KEYS` de
`tools/build_strings.py`.
