---
name: nw-esteira
description: Regras de trabalho de qualquer esteira do port (armas, coleta, montarias, sistemas) com outras sessões rodando no mesmo checkout ao mesmo tempo. Usar no começo de toda sessão de esteira e antes de compilar, importar, rodar trabalho pesado, testar ou commitar.
---

# Trabalhar numa esteira

Referência completa: `docs/esteiras.md` (regras comuns, donos dos arquivos, escopo de cada esteira). Leia antes.
Fato do executável que os dados não dão: skill `nw-re` (pergunte de novo; as respostas não ficam no repositório). Telas: `docs/hud/README.md`.

## Plataforma

Os comandos das skills de esteira valem no Linux e no Windows: Python do pipeline em `tools/*.py`, com `python3` no
Linux e `python` no Windows. O que depende do sistema (extração, compilar e importar, backend local, captura de
tela, conferência, build do nwbt) aparece pelo nome da ação, como "etapa NN do import", e o comando está na skill
da plataforma: `nw-linux` (`tools/linux/`) ou `nw-windows` (`tools/windows/`).

| Ação | Linux | Windows |
|---|---|---|
| Fila de trabalho pesado | `python3 tools/pesado.py <comando>` | `python tools/pesado.py <comando>` |
| Etapa NN da extração | `tools/linux/build_all.sh NN` | `python tools/build_all.py NN` |
| Etapa NN do import (01 = compilar) | `tools/linux/ue_import_all.sh NN` | `python tools/ue_import_all.py NN` |
| Data/ | `python3 tools/make_data.py` | `python tools/make_data.py` |
| Conferência antes de mesclar | `tools/linux/check.sh --cpp` | `python tools/windows/doctor.py --full` |

## Sempre

- Trabalho pesado só pela fila: `python3 tools/pesado.py <comando>` (nwbt, Blender, `build_set.py`, `convert_*`,
  `UnrealEditor-Cmd`, compilação). Extração e import entram na fila sozinhos.
- Editor é do dono. Precisa compilar ou importar: peça para fechar, faça tudo de uma vez, abra no mapa da esteira
  e peça o print. Nunca rodar jogo, cliente ou teste automático.
- Testes usam o nw-core local (`docs/teste-backend.md`; Linux: `tools/linux/backend_dev.sh`) antes de abrir o
  editor; o Play entra com a conta fixa do editor e salva bolsa, posição e níveis entre sessões.
- Arquivo de outra esteira (tabela "Donos dos arquivos"): não mexa. Compartilhado: releia antes, mudança mínima.
- C++ compilando ao fim de cada mudança sua. Erro em arquivo dos outros: avise e espere.
- Fonte de todo número nos dados do jogo; sem fonte, skill `nw-re`.
- Testes em `docs/testes/<esteira>.md` (numeração própria).
- Commit e push só com pedido do dono, só dos seus arquivos (`git add <caminhos>`), prefixo da esteira.
- Precisa de tecla, botão no F10, locomoção, montagem, texto ou etapa de import: use os pontos de extensão
  (`docs/esteiras.md`), não edite o arquivo compartilhado. `ABP_NW` tem dono único.
- Antes de mesclar um branch: a conferência da tabela acima (inclui nwbt do patch atual e teste do `@lista`).

## Ao fechar um item

1. Doc de estado da esteira e "Sem fonte" atualizados.
2. Item em `docs/testes/<esteira>.md`.
3. Linha da esteira em `docs/ESTADO.md`.
4. Editor aberto no mapa da esteira e pedido de print ao dono.
