# Ferramentas

Tudo o que o projeto usa: programas de terceiros (de onde vêm, versão, como compilar) e cada script do
repositório. O passo a passo para montar do zero está em `docs/SETUP.md`.

## Programas de terceiros

Os binários prontos (Linux x86-64) estão em `bin/` no repositório; o código-fonte, quando é preciso recompilar,
vai para `vendor/` (fora do git, muito grande).

| Ferramenta | Para quê | Origem e versão | Binário | Recompilar |
|---|---|---|---|---|
| nwbt (nw-buddy) | abrir os `.pak`, tabelas, textos, telas, modelos e animações → glTF, cenário da região | `paulorsouza/nw-buddy` (privado), branch `nw-port`, sobre giniedp/nw-buddy `81b2302`. Licença MIT | `bin/nwbt`, `bin/dbgwater` (Windows: `.exe`) | `tools/linux/build_nwbt.sh --test` ou `tools/windows/build_nwbt.ps1 -Test`: commit base do fork + `patches/nwbt.patch` (`patches/README.md`) |
| ooz (Kraken) | descompressão Oodle no Linux (o jogo não traz a DLL) | powzix/ooz `0503806`; fonte em `tools/ooz-linux` | `bin/libooz.so` (Windows: `bin/ooz.dll`) | `tools/linux/build_ooz.sh` ou `tools/windows/build_ooz.ps1` |
| texconv (DirectXTex) | DDS → PNG (texturas) | microsoft/DirectXTex, release mais recente | `bin/texconv.exe`, no Linux chamado por `tools/linux/texconv` via wine (`bin/texconv`) | baixar o `.exe` das releases |
| wwiser | lê os bancos Wwise e gera `.txtp` por evento de som | bnnm/wwiser `08e937b` (Python puro) | — | `git clone https://github.com/bnnm/wwiser vendor/wwiser && git -C vendor/wwiser checkout 08e937b` |
| vgmstream-cli | toca `.txtp`/`.wem` (decodifica o áudio) | vgmstream/vgmstream `7c520e8` | `bin/vgmstream-cli` | `docs/ASSETS.md`, seção Áudio |
| oggenc (vorbis-tools 1.4.3) | grava `.ogg` | tarball da xiph | `bin/oggenc` | `docs/ASSETS.md`, seção Áudio |
| Blender 5.2.2 | monta personagem + animações e exporta FBX (`tools/export_ue.py`, `build_set.py`) | blender.org | `~/.local/opt/blender-5.2.2-linux-x64` | — |
| wine | roda o texconv | apt | — | — |
| Go 1.24+ | compilar o nwbt | go.dev | `~/.local/go` | — |
| Unreal Engine 5.8.3 | o jogo | binário da Epic em `/mnt/jogos/UnrealEngine/5.8`; servidor dedicado com a engine do código-fonte em `/mnt/jogos/UnrealEngine/src-5.8.3` (`docs/plano-servidor.md`) | — | — |
| Rust, Docker, Postgres 17 | backend e servidor | `backend/README.md`, `deploy/` | — | — |
| Cryengine-Converter | reserva para formatos que o nwbt não cobre (não usado hoje) | Markemp/Cryengine-Converter `7b951db` | — | — |
| Lumberyard (referência) | ler como a engine-base interpreta os dados | clone raso de aws/lumberyard em `reference/` (fora do git) | — | — |

`tools/linux/env.sh` põe `bin/`, Go e Blender no PATH e aponta o nwbt para o `libooz.so`: rode `source tools/linux/env.sh`
antes dos comandos manuais. Os scripts Python acham `bin/`, o `libooz.so` e o Blender sozinhos (`tools/nwplatform.py`).

O nwbt grava a versão do patch: `nwbt --version` mostra `nwport-patch=<base>-<hash>`. `python3
tools/build_nwbt.py --conferir` compara com o `patches/nwbt.patch` do checkout; `--teste-lista` converte um cubo do
jogo por `@lista` e falha se nada for gravado. O `check.sh` (Linux) e o `doctor.py` (Windows) rodam as duas
conferências, e `build_set.py` e `convert_npcs.py` recusam nwbt de outro patch.

## Pastas por sistema

- `tools/*.py`: o pipeline, um só para Linux e Windows (`nwplatform.py` resolve caminhos e executáveis,
  `nwprocess.py` a fila, memória e processos filhos, `saidas.py` confere o que nwbt e Blender gravaram).
- `tools/linux/`: scripts de sistema do Linux (`.sh`, wrapper do texconv, build do nwbt e do ooz). Os `tools/<x>.sh`
  antigos são atalhos temporários para eles (2026-10-15), até as sessões abertas reiniciarem.
- `tools/windows/`: o que é só do Windows (`.ps1` de build, `doctor.py`, `check_migration.py`); ver
  `docs/windows-convert.md`.
- `tools/etapas/`: etapas de extração e import das esteiras, lidas pelos orquestradores dos dois sistemas.

## Scripts principais (os que se rodam no dia a dia)

| Script | Para quê | Pesado? |
|---|---|---|
| `tools/linux/build_all.sh [NN]` | extrai e converte tudo da instalação do jogo, 14 etapas retomáveis | sim, 2 a 4 h |
| `tools/linux/ue_import_all.sh [NN]` | monta o projeto Unreal (editor fechado), 10 etapas retomáveis | sim, ~1 h 30 |
| `tools/linux/make_data.sh` (= `python3 tools/make_data.py`) | regenera `unreal/NWPort/Data/` (itens, saque, coletáveis, textos, telas) | não |
| `tools/linux/build_nwbt.sh [--test]` | recompila `bin/nwbt` e `bin/dbgwater` do fork + `patches/nwbt.patch` | sim, ~1 min |
| `tools/linux/build_ooz.sh` | recompila `bin/libooz.so` | não |
| `tools/linux/convert_city.sh <slice>` | converte uma cidade (`*_complete`) em glb limpo + recorte de terreno | sim |
| `tools/linux/convert_gatherables.sh [lista] [saída]` | converte as malhas dos coletáveis em lotes (ou outra lista de `.cgf`, ex. `tools/arena/meshes.txt`) | sim |
| `tools/linux/render_ambient.sh <território>` | renderiza o ambiente sonoro de um território | não |
| `tools/linux/play_local.sh [mapa]` | backend + servidor dedicado + cliente na tela de entrada (para uma pessoa jogar) | sim |
| `tools/linux/package_server.sh [--skip-cook]` | empacota o servidor dedicado Linux em `deploy/build/LinuxServer` | sim |
| `deploy/build_images.sh` | imagens Docker do backend e do servidor | médio |
| `tools/linux/net_test.sh [n] [mapa]` | teste de rede com clientes automáticos e print. Uso manual de quem desenvolve; não automatizar com IA | sim |

## Extração e dados (`tools/*.py`)

| Script | Para quê |
|---|---|
| `extract_audio.py` | áudio Wwise → `.ogg` (`extract/audio`, índice em `extract/audio/INDEX.md`) |
| `item_log.py` | log de itens do nw-core (`--personagem`, `--item`, `--serie`, `--desde`): o que entrou e saiu, de onde, por quê |
| `make_items.py` | catálogo de itens (armas, ferramentas, poções, recursos do saque) e tabelas de saque do jogo |
| `make_gatherables.py` | coletáveis de Guardaventos: capitals + `region.distribution` → `Data/World/guardaventos_gatherables.json` |
| `make_terrain.py` | malha do terreno (recorte em volta da cidade ou `--region` inteira) a partir do heightmap |
| `catalog_meshes.py` | hash da geometria de cada malha dos glb de cenário → `extract/ue/catalogo_malhas.json` (para `tools/ue/catalog.py`) |
| `make_abilities.py` | habilidades por arma para a HUD e a maestria |
| `make_worldmap.py` | mapa do mundo (M): referencial da cidade, limites da região e tiles do jogo (níveis 2 e 3) em volta de Guardaventos → `Data/World/worldmap.json` e `extract/ui/worldmap_textures.txt` |
| `make_progression.py` | nível do personagem (XP total por nível, pontos de atributo) e efeito de cada valor de atributo → `Data/Progression/progression.json` |
| `make_datatables.py` | CSV das DataTables de dano, ações (por arma) e tremidas de câmera |
| `make_ai.py` | IA das criaturas: árvore do Server.pak + perfil do slice → `Data/AI/<conjunto>.json` (`docs/ia-criaturas.md`) |
| `ai_sim.py <conjunto> Chave=valor...` | simula o planejamento da árvore para um blackboard (conferir sem a Unreal) |
| `linux/check.sh [--cpp]` | conferência antes de mesclar um branch: conflitos esquecidos, JSON, chaves repetidas entre esteiras, Python, shell, nwbt do patch atual e teste do `@lista` (~20 s na fila), versão do jogo; `--cpp` compila |
| `build_nwbt.py` | compila o nwbt (Linux e Windows) e confere o binário (`--conferir`, `--teste-lista`); `--preparar`/`--gerar-patch` para mudar o patch |
| `saidas.py` | conferência das saídas do nwbt e do Blender (Blender com `--python-exit-code 1`; arquivos esperados regravados) |
| `pesado.py <comando>` (Linux também `linux/pesado.sh`) | fila de trabalho pesado (um por vez na máquina, mesmo lock nos dois; `docs/esteiras.md`) |
| `jogo_versao.py` / `linux/jogo_versao.sh [--exe]` | confere se a instalação é a build do time (`tools/jogo.versao`) |
| `setspec.py` | junta as partes de um conjunto (`tools/sets/player/<esteira>.json` no do jogador) para `build_set.py` e `make_datatables.py` |
| `make_rules.py` | opcional: cache local da RE (`re-local/valores/`) → `Data/regras.json`, que sobrescreve os padrões do `FNWRules` |
| `re_check.py` | opcional, com cache local da RE (`re-local/`): o que dela já entrou no port: valores aplicados e faltando, `Data/regras.json` em dia, colunas da damagetable que o cliente do jogo lê e o port não exporta |
| `weapon_report.py <arma>` | relatório do que o jogo define sobre o combate de uma arma (fragmentos, janelas, dano, stamina, recargas, habilidades, status) → `extract/research/` |
| `make_sound_map.py` | liga sons às animações (passos, golpes, vozes) |
| `build_strings.py` | tabela de textos pt-BR/en-US do C++ (`Data/UI/nw_strings.json`), de `tools/strings/<esteira>.json` (textos nossos + chaves do jogo) |
| `build_set.py` | conjunto personagem + armas + animações → FBX + manifest (`tools/sets/*.json`) |
| `export_ue.py` | parte Blender do `build_set.py` (FBX em cm, camadas de animação, armas nos sockets) |
| `nwchar.py` | utilitários Blender de personagem/animação usados pelos outros |
| `assemble_character.py` | monta personagem + animações num `.blend` (conferência) |
| `adb_resolve.py`, `adb_windows.py` | lê as receitas do Mannequin (.adb): clipes por tags e janelas de combate |
| `glb_strip_nodraw.py` | tira de um glb as primitivas que o jogo não desenha (colisão, NoDraw, efeitos) |
| `ui_canvas.py` | tela LyShine do jogo (`.dynamicuicanvas` ou slice `.dynamicslice`, ex.: banners) → layout plano (base dos `ui_build_*.py`) |
| `ui_build_hud.py`, `ui_build_login.py`, `ui_build_mastery.py`, `ui_build_respawn.py`, `ui_build_group.py`, `ui_build_inventory.py`, `ui_build_tradeskills.py`, `ui_build_attributes.py` | layouts das telas do jogo para o C++ (`Data/UI/*_layout*.json`) e listas de texturas; a HUD inclui a barra de XP (`hud2024/xp`); ofícios e atributos são abas da janela K |
| `ui_textures_extra.py` | listas de texturas das telas desenhadas direto pelo C++ (bússola, sobrecarga, cartões, números de dano, aviso de saque, tooltip, banner de nível, ícones de ofício, indicador de golpe, faixa de aviso, barra de navegação) |
| `gallery.py`, `linux/gallery.sh` | galeria de armaduras (escolha visual) |
| `render_preview.py`, `preview_fbx.py` | prévias headless no Blender |

Dados escritos à mão: `tools/strings/<esteira>.json` (textos nossos pt/en e chaves do jogo usadas pelo C++),
`tools/sets/*.json` (receitas dos conjuntos; o do jogador em partes: `tools/sets/player/<esteira>.json`).

## Dentro da Unreal (`tools/ue/*.py`, rodam com `UnrealEditor-Cmd ... -run=pythonscript`)

| Script | Para quê |
|---|---|
| `import_nw.py <manifest>` | personagem, armas, animações com janelas e sons, blend spaces, montagens, DataTables |
| `make_abp.py` | cria `ABP_NW`/`ABP_Lost` vazias (o grafo é feito à mão; as prontas estão no repositório) |
| `make_test_map.py` | arena de testes `L_Arena` (peças do jogo em `/Game/NW/Arena`, zonas de tiro, bonecos, duelo, travessia) |
| `import_city.py <glb> <Nome>` | cidade num mapa próprio |
| `fix_city.py <Nome>` | colisão, luz móvel, chão de segurança |
| `nanite_on.py <pastas>` | liga Nanite nas malhas |
| `import_terrain.py <glb> <Nome> <grama> <terra>` | terreno no mapa da cidade |
| `fix_spawn.py <Nome>` | PlayerStart num ponto livre |
| `import_gatherables.py <pasta> [filtro]` | malhas dos coletáveis (uma pasta por malha, pipeline glTF) |
| `import_region_scene.py <glb> <Nome>` | cenário da região (mundo → referencial do mapa) |
| `catalog.py [--dry-run]` | catálogo único: texturas, materiais e malhas repetidos entre camadas, cidades e coletáveis viram um original em `/Game/NW/Catalogo`; rodar depois de cada import de cenário |
| `fix_nanite_materials.py <pastas>` | flags de Nanite e instâncias; copia os pais do Interchange para o projeto |
| `cap_textures.py <pasta> <max\|0>` | limita (ou libera, com 0) o tamanho das texturas |
| `import_ui.py` | texturas e fontes das telas |
| `import_audio.py` | música e ambiente em loop |

## Skills do Claude Code

Em `.claude/skills/` há uma skill por script principal, com quando usar, pré-condições e o que conferir no
final. Servem para quem usa o Claude Code no projeto; os scripts funcionam sem IA.
