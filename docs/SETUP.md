# Montar o projeto do zero

Sem IA: cada passo é um comando. Os assets saem da instalação do New World de quem monta; nada do jogo entra no
repositório. Tempo total na primeira vez: ~4 a 6 horas, quase tudo máquina trabalhando sozinha.

Testado em Ubuntu 26.04 (Linux). No Windows, a parte de extração (passos 3 e 4) roda no WSL2 com Ubuntu, e a
Unreal (passos 5 a 7) no Windows; isso ainda não foi validado de ponta a ponta.

## Requisitos

- New World instalado pela Steam.
- 32 GB de RAM (64 GB recomendado), placa com 8 GB de VRAM, ~150 GB livres (60 GB para `extract/`, ~25 GB para a
  Unreal e o projeto).
- Unreal Engine 5.8.3 (binário da Epic). Só o servidor dedicado precisa da engine compilada do código-fonte
  (`docs/plano-servidor.md`); para jogar e desenvolver, o binário basta.

## Versão do jogo

Todo mundo usa a mesma build do New World: a de `tools/jogo.versao` (buildid da Steam e sha256 do `NewWorld.exe`).
Dados extraídos e endereços da engenharia reversa valem só para ela. `tools/jogo_versao.sh` confere; o
`tools/build_all.sh` para se a build for outra. Atualizar só junto com o time, refazendo a extração.

## 1. Pacotes do sistema

```bash
sudo apt install git build-essential cmake python3 python3-numpy python3-pil wine curl
```

- Go 1.24+ em `~/.local/go` (https://go.dev/dl).
- Blender 5.2 em `~/.local/opt/blender-5.2.2-linux-x64` (https://www.blender.org/download).
- Rust (rustup) e Docker, só para o backend e o servidor (`backend/README.md`).

## 2. Repositórios e ferramentas

```bash
git clone https://github.com/paulorsouza/nw-port.git ~/workspace/nw-port
cd ~/workspace/nw-port
```

Os binários das ferramentas (Linux x86-64) já vêm em `bin/`. Só recompile se não rodarem na sua máquina:

```bash
mkdir -p vendor

# nw-buddy (nwbt) com as mudanças do port: branch nw-port do repositório privado.
git clone -b nw-port https://github.com/paulorsouza/nw-buddy.git vendor/nw-buddy
(cd vendor/nw-buddy/tools/nwbt && ~/.local/go/bin/go build -o ../../../../bin/nwbt .)

# ooz (Oodle no Linux): o jogo não traz a DLL da Oodle.
git clone https://github.com/powzix/ooz tools/ooz-linux
(cd tools/ooz-linux && g++ -O2 -fPIC -shared -w -o ../../bin/libooz.so kraken.cpp bitknit.cpp lzna.cpp)

# texconv (DDS → PNG), roda pelo wine.
curl -L -o bin/texconv.exe https://github.com/microsoft/DirectXTex/releases/latest/download/texconv.exe
```

O wwiser (Python, usado pela extração de áudio) não tem binário: clone sempre.

```bash
git clone https://github.com/bnnm/wwiser vendor/wwiser && git -C vendor/wwiser checkout 08e937b
```

vgmstream-cli e oggenc do zero: `docs/ASSETS.md`, seção Áudio. Lista completa de ferramentas e versões:
`docs/FERRAMENTAS.md`. Com o Claude Code no Windows: skill `nw-wsl`.

O caminho do jogo vem de `NW_GAME` (padrão `/mnt/jogos/Steam/steamapps/common/New World`):

```bash
export NW_GAME="$HOME/.steam/steam/steamapps/common/New World"
```

## 3. Extrair e converter os assets

```bash
tools/build_all.sh
```

Etapas (cada uma pula se já foi feita; `tools/build_all.sh 07` refaz só a 07):

| Etapa | O que faz | Tempo |
|---|---|---|
| 00 | confere ferramentas e o caminho do jogo | — |
| 01 | lista de arquivos dos paks (`extract/manifest/paths.txt`) | 5 min |
| 02 | tabelas do jogo (JSON) e textos pt-BR/en-US | 10 min |
| 03 | telas da interface, fontes, Lua | 10 min |
| 04 | receitas de animação (.adb) | 2 min |
| 05 | áudio: música, golpes, passos, ambiente | 40 min |
| 06 | dados da região de Guardaventos (heightmap, capitals, distribuição, água) | 5 min |
| 07 | cidade de Guardaventos | 20 min |
| 08 | terreno da região (2 km) | 1 min |
| 09 | coletáveis: 100 mil pontos e 435 malhas | 30 min |
| 10 | cenário pré-gerado da região (penhascos, ruínas, props) | 25 min |
| 11 | personagem, armas, ferramentas, 150 animações | 15 min |
| 12 | `unreal/NWPort/Data/` e texturas das telas | 5 min |

Durante a extração: não abra o New World nem a Unreal. O Ubuntu fecha programas por pressão de memória
(`systemd-oomd`) mesmo com RAM sobrando no htop. Se uma etapa parar por memória, é só rodar de novo.

## 4. Conferir

```bash
ls extract/.etapas      # 00.ok … 13.ok
du -sh extract          # ~30 GB
```

## 5. Montar o projeto Unreal

Com o editor fechado:

```bash
tools/ue_import_all.sh
```

| Etapa | O que faz | Tempo |
|---|---|---|
| 01 | compila o C++ (módulo NWPort) | 10 min |
| 02 | personagem e Lost: malhas, animações, montagens, blend spaces | 5 min |
| 03 | mapa de treino `L_Arena` | 1 min |
| 04 | cidade (`L_SouthWindsward`) | 5 min |
| 05 | terreno da região | 5 min |
| 06 | 435 malhas dos coletáveis | 25 min |
| 07 | cenário da região (35 mil peças) | 5 min |
| 08 | materiais prontos para Nanite e instâncias | 2 min |
| 09 | texturas da interface | 2 min |
| 10 | sons de fundo | 1 min |

As Animation Blueprints `ABP_NW` e `ABP_Lost` vêm do repositório (foram montadas à mão no editor).

## 6. Abrir

```bash
/mnt/jogos/UnrealEngine/5.8/Engine/Binaries/Linux/UnrealEditor unreal/NWPort/NWPort.uproject /Game/NW/Maps/L_SouthWindsward
```

Play no editor. F10 abre o menu de dev ("Dar ferramentas", "Ir ao minério", invocar inimigos).

## 7. Jogar em rede (servidor + conta)

```bash
tools/play_local.sh      # nw-core + servidor dedicado + cliente na tela de entrada
```

Convites de conta, servidor na nuvem e Docker: `docs/plano-servidor.md` e `backend/README.md`.

## Quando algo dá errado

- Etapa parou: o log está na saída do terminal; a etapa é refeita inteira ao rodar de novo.
- Material cinza no jogo: `tools/ue_import_all.sh 08`.
- O resto das armadilhas conhecidas está em `docs/ESTADO.md`.
