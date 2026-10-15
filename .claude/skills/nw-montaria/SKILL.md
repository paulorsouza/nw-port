---
name: nw-montaria
description: Esteira 3 — porta as montarias do New World (cavalo, lobo, felino, urso...) com malha, animações da montaria e do cavaleiro, invocar e desmontar, movimento, câmera, som, HUD, aparências e tinturas. Usar quando pedirem algo de montaria.
---

# Montarias (esteira 3)

Regras de trabalho: skill `nw-esteira` e `docs/esteiras.md`. Estado e levantamento: `docs/montarias.md` (crie no
primeiro item). Testes: `docs/testes/montarias.md`, mapa `L_SouthWindsward`.
RE já feita: `re-local/montarias.md` (velocidade por nível em 5 faixas, galope e stamina, giro montado) e as
regras de ações em `re-local/resumo-plano-a.md`.

## Fontes do jogo

| O quê | Onde |
|---|---|
| Montarias, tipos, movimento | `datatables/mounts/javelindata_mounts`, `_mounttypes`, `_mount_movement` |
| Aparências e tinturas | `datatables/javelindata_itemdefinitions_mount{itemappearances,dye}`, `..._weaponappearances_mountattachments` |
| Animações da montaria | `animations/mannequin/adb/mounts/mount_<tipo>_anims.adb` (horse, wolf, cat, bear, grunt, turkey) |
| Animações do cavaleiro | `animations/mannequin/adb/player/player_mount.adb` (`Mount*`, `MountDisMount_<tipo>`, `MountFall*`...) |
| Malha e materiais | coluna `Mesh`/`Material` de `javelindata_mounts`; slices das montarias |
| Lógica (invocar, desmontar, galope) | actionlists CAGE de montaria em `extract/serverpak/client/.../cage/`; slice do jogador |
| Telas | ícones `IconPath` da tabela; HUD de montaria do jogo (`docs/hud-original.md`) |

`datatables/` = `extract/data/sharedassets/springboardentitites/datatables/`; `.adb` em
`extract/models/animations/mannequin/adb/`. Arquivo não extraído: `python3 tools/pesado.py nwbt cat -g "$NW_GAME" <caminho>`.

## Por montaria (ordem: cavalo, lobo, felino, depois os outros tipos)

1. Levantamento em `docs/montarias.md`: dados da tabela, fragmentos e janelas, regras das actionlists.
2. Conjunto novo `tools/sets/mount_<tipo>.json` (modelo: `tools/sets/wolf.json` e `boar.json`); montagens do
   cavaleiro com prefixo `AM_Mt_` em `tools/sets/player/montaria.json` (parte do conjunto do jogador).
   Conversão pela fila (`python3 tools/pesado.py python3 tools/build_set.py ...`).
3. C++: o `UNWMountComponent` já existe no personagem (`Mount`, vazio); a montaria em si é `ANWMount` (novo, seu).
   Locomoção montada pelo `SetLocomotionOverride`, teclas pelo `NW_REGISTER_INPUT`, botões de teste pelo
   `NW_REGISTER_DEV`: sem mexer no `ANWCharacter` nem no `ABP_NW`.
4. Movimento com os números de `mount_movement`; galope e o recurso dele; invocar e desmontar com tempos e regras
   das actionlists. Sem fonte: skill `nw-re`.
5. Som (`tools/extract_audio.py`, grupo novo da montaria), câmera, HUD, rede (replicação do estado montado).
6. Compilar e importar com o editor fechado, abrir em `L_SouthWindsward`, pedir print.
