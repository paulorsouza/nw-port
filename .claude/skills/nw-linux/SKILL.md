---
name: nw-linux
description: Comandos do port no Linux (máquina do dono) — fila de trabalho pesado, extração, import e compilação, backend local, captura de telas, conferência e build do nwbt/ooz (tools/linux/). Usar quando uma skill de esteira mandar "pela skill da plataforma" e a sessão estiver no Linux.
---

# Linux: comandos de plataforma

Scripts de sistema em `tools/linux/` (os `tools/<x>.sh` antigos são atalhos temporários para eles). O Python do
pipeline é um só, em `tools/`, e roda igual no Windows. Rode da raiz do repositório.

| Ação | Comando |
|---|---|
| Fila de trabalho pesado | `python3 tools/pesado.py <comando>` ou `tools/linux/pesado.sh <comando>` (mesmo lock) |
| PATH com `bin/`, Go e Blender (para chamar nwbt/blender à mão) | `source tools/linux/env.sh` |
| Extração inteira ou etapa NN | `tools/linux/build_all.sh [NN]` (skill `nw-build-all`; entra na fila sozinho) |
| Compilar C++ e importar na Unreal, etapa NN (editor fechado) | `tools/linux/ue_import_all.sh [NN]` (skill `nw-ue-import`) |
| Só compilar o alvo do jogo, com o editor aberto | `tools/linux/check.sh --cpp` |
| Data/ (tabelas, textos, telas) | `tools/linux/make_data.sh` (= `python3 tools/make_data.py`) |
| nw-core local para o Play do editor | `tools/linux/backend_dev.sh` (`docs/teste-backend.md`) |
| Captura de tela sem o jogo | `tools/linux/ui_shot.sh tools/ui_shots/<tela>.json` |
| Conferência antes de mesclar | `tools/linux/check.sh [--cpp]` |
| Cidade, coletáveis, ambiente sonoro | `tools/linux/convert_city.sh`, `convert_gatherables.sh`, `render_ambient.sh` (skills próprias) |
| Servidor, rede, jogo local (só a pedido) | `tools/linux/package_server.sh`, `rede.sh`, `net_test.sh`, `play_local.sh` |

## nwbt e ooz

- `bin/nwbt` e `bin/dbgwater` saem do fork + `patches/nwbt.patch`: `tools/linux/build_nwbt.sh --test` (fila,
  `tools/build_nwbt.py`, o mesmo do Windows). Mudar o nwbt: `patches/README.md`.
- `tools/linux/check.sh` acusa `bin/nwbt` de outro patch e testa a conversão por `@lista`; `python3
  tools/build_nwbt.py --conferir` só confere a versão. `build_set.py` e `convert_npcs.py` recusam nwbt desatualizado.
- `bin/libooz.so`: `tools/linux/build_ooz.sh` (powzix/ooz + `patches/ooz.patch`, as mesmas fontes da `ooz.dll`).
- Tudo de `bin/` de uma vez (fora do git): `tools/linux/get_tools.sh`.
- texconv: `bin/texconv` → `tools/linux/texconv` (wine com `bin/texconv.exe`).

## Cuidados

- Um trabalho pesado por vez (o `systemd-oomd` fecha programas); nunca cache do nwbt em `/tmp` (é RAM).
- `pgrep -f`/`pkill -f` casam com a própria linha do shell.
- Engine em `NW_UE` (padrão `/mnt/jogos/UnrealEngine/5.8`), jogo em `NW_GAME` (padrão `/mnt/jogos/Steam/...`).
