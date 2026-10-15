---
name: nw-make-data
description: Regenera unreal/NWPort/Data (catálogo de itens, saque, coletáveis, textos pt/en, layouts das telas) com tools/linux/make_data.sh. Usar depois de mudar tools/make_*.py, ui_build_*.py, strings_nw.json ou as receitas.
---

# Regenerar Data/ (tools/linux/make_data.sh)

Leve (só Python, segundos). Não precisa fechar nada.

```bash
tools/linux/make_data.sh
```

Ordem fixa dentro do script: coletáveis → itens (o saque dos coletáveis define itens) → habilidades → textos →
telas. Se uma tela nova usar texturas que ainda não existem em PNG, rode `tools/linux/build_all.sh 12` e depois
`tools/linux/ue_import_all.sh 09`.

Texto novo do C++: em `tools/strings/<esteira>.json`, `textos` (pt-br e en-us) ou, se vier do jogo,
`chaves_do_jogo`. Arquivo de outra esteira: não mexa (`docs/esteiras.md`).

No Windows nativo: `python tools/make_data.py` (skill `nw-windows`). Os scripts `.sh` daqui ficam em `tools/linux/` (skill `nw-linux`).
