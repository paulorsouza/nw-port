---
name: nw-esteira
description: Regras de trabalho de qualquer esteira do port (armas, coleta, montarias, sistemas) com outras sessões rodando no mesmo checkout ao mesmo tempo. Usar no começo de toda sessão de esteira e antes de compilar, importar, rodar trabalho pesado, testar ou commitar.
---

# Trabalhar numa esteira

Referência completa: `docs/esteiras.md` (regras comuns, donos dos arquivos, escopo de cada esteira). Leia antes.
O que a RE já leu do cliente, para todas as esteiras: `docs/analise-re/resumo-plano-a.md`. Telas: `docs/hud/README.md`.

## Sempre

- Trabalho pesado só pela fila: `tools/pesado.sh <comando>` (nwbt, Blender, `build_set.py`, `convert_*.sh`,
  `UnrealEditor-Cmd`, compilação). `build_all.sh` e `ue_import_all.sh` entram na fila sozinhos.
- Editor é do dono. Precisa compilar ou importar: peça para fechar, faça tudo de uma vez, abra no mapa da esteira
  e peça o print. Nunca rodar jogo, cliente ou teste automático.
- Arquivo de outra esteira (tabela "Donos dos arquivos"): não mexa. Compartilhado: releia antes, mudança mínima.
- C++ compilando ao fim de cada mudança sua. Erro em arquivo dos outros: avise e espere.
- Fonte de todo número nos dados do jogo; sem fonte, skill `nw-re`.
- Testes em `docs/testes/<esteira>.md` (numeração própria).
- Commit e push só com pedido do dono, só dos seus arquivos (`git add <caminhos>`), prefixo da esteira.
- Precisa de tecla, botão no F10, locomoção, montagem, texto ou etapa de import: use os pontos de extensão
  (`docs/esteiras.md`), não edite o arquivo compartilhado. `ABP_NW` tem dono único.
- Antes de mesclar um branch: `tools/check.sh --cpp`.

## Ao fechar um item

1. Doc de estado da esteira e "Sem fonte" atualizados.
2. Item em `docs/testes/<esteira>.md`.
3. Linha da esteira em `docs/ESTADO.md`.
4. `python3 tools/re_check.py` se usou algo da RE.
5. Editor aberto no mapa da esteira e pedido de print ao dono.
