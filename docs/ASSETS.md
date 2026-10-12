# Assets: como cada parte sai do jogo

Detalhe técnico das ferramentas e formatos. Para montar tudo, use `docs/SETUP.md`.

## Ferramentas

| Ferramenta | Onde | Para quê |
|---|---|---|
| nwbt (nw-buddy, MIT) | `vendor/nw-buddy/tools/nwbt` → `bin/nwbt` | abrir `.pak`, datasheets, modelos/animações → glTF |
| ooz (Kraken) | `tools/ooz-linux` → `bin/libooz.so` | descompressão Oodle no Linux |
| texconv (DirectXTex) | `bin/texconv.exe` via `tools/texconv` (wine) | DDS → PNG |
| Blender | `~/.local/opt/blender-*` | conferir modelos e animações, render headless |
| Cryengine-Converter | `vendor/Cryengine-Converter` | reserva, se o nwbt não cobrir algum formato |

## Build das ferramentas

O passo a passo atual está em `docs/SETUP.md`; abaixo, o detalhe de cada ferramenta.

```bash
cd tools/ooz-linux && g++ -O2 -fPIC -shared -w -o ../../bin/libooz.so kraken.cpp bitknit.cpp lzna.cpp
cd vendor/nw-buddy/tools/nwbt && go build -o ../../../../bin/nwbt .
```

Mudanças no nw-buddy (repositório privado paulorsouza/nw-buddy, branch `nw-port`; diff em `patches/nwbt-nwport.patch`):

- `utils/oodle_linux.go`: carrega `libooz.so` (o original só carrega a DLL no Windows).
- `formats/cgf/chunk_controller.go`: corrige `smallTree64BitQuat` (21+21+20 bits; o contador nunca avançava e a máscara de 21 bits tinha 20). Afetava as camadas de arma (`rswrd_idle`, `rswrd_run`...).
- `commands/models`: `.caf` passados junto com um `.cdf` no comando `files` entram como animações do mesmo glb; comandos `slices` (uma dynamicslice na origem), `chunks` (conteúdo pré-gerado dos `<PLOT>` das camadas de capitals) e `slicestats`.

## Personagem com animações

```bash
nwbt models files -g "$GAME" -o extract/stage3 --binary --embed --ior 1.5 \
  objects/characters/player/male/player_male.cdf <clipes .caf> <arma .cgf> <escudo .cgf>
blender -b -P tools/assemble_character.py -- <personagem.glb> <arma.glb> <escudo.glb> <saida.blend> <pasta_previews>
```

As ações de locomoção são compostas como no `player_1h_melee.adb`: base `unarmed_*` + camada `rswrd_*`.

Antes de usar: `source tools/env.sh`.

## Áudio

O áudio é Wwise: `sounds/wwise/*.bnk` (eventos e sons curtos) e `sounds/wwise/media/*.wem` (streams, quase tudo música). Os arquivos só têm ID numérico. Os nomes saem do hash FNV-1 dos tokens encontrados nos .adb/.xml/.json/.timeline do jogo.

```bash
python3 tools/extract_audio.py   # saída em extract/audio/, ver extract/audio/INDEX.md
```

Etapas: `nwbt unpack` → lista de nomes → wwiser gera `.txtp` por evento → `vgmstream-cli` decodifica → `oggenc` grava .ogg. Cada etapa reaproveita o que já está em `extract/_raw/`.

Converte música (eventos, faixas e instrumentos), espada de uma mão, escudo, impactos, voz de esforço, passos, esquiva e pulo. Ambiente só tem índice.

Ferramentas (em `vendor/`, binários em `bin/`):

```bash
# wwiser (bnnm), Python puro
git clone https://github.com/bnnm/wwiser vendor/wwiser          # 08e937b

# vgmstream-cli: sem FFmpeg, MPEG (mpg123 não compila com GCC 15) e Speex (pede autoreconf)
git clone https://github.com/vgmstream/vgmstream vendor/vgmstream   # 7c520e8
X=$PWD/vendor/xiph
cd vendor/vgmstream
# 1ª configuração: só baixa ogg/vorbis em dependencies/ (o vorbis baixado não acha ogg.h)
cmake -S . -B build-fetch -DUSE_FFMPEG=OFF -DUSE_MPEG=OFF -DUSE_SPEEX=OFF -DBUILD_AUDACIOUS=OFF -DBUILD_V123=OFF
for d in ogg vorbis; do
  cmake -S dependencies/$d -B build-$d -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$X -DCMAKE_PREFIX_PATH=$X -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  cmake --build build-$d -j && cmake --install build-$d
done
PKG_CONFIG_PATH=$X/lib/pkgconfig cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DUSE_FFMPEG=OFF -DUSE_MPEG=OFF -DUSE_SPEEX=OFF -DBUILD_AUDACIOUS=OFF -DBUILD_V123=OFF \
  -DCMAKE_PREFIX_PATH=$X -DCMAKE_EXE_LINKER_FLAGS=-L$X/lib -DCMAKE_INSTALL_PREFIX=$PWD/../..
cmake --build build -j --target vgmstream_cli && cmake --install build
cd ../..

# oggenc (vorbis-tools 1.4.3, tarball da xiph já traz o configure)
curl -L https://downloads.xiph.org/releases/vorbis/vorbis-tools-1.4.3.tar.gz | tar xz -C vendor
cd vendor/vorbis-tools-1.4.3
PKG_CONFIG_PATH=$X/lib/pkgconfig ./configure --disable-ogg123 --disable-oggdec --disable-ogginfo \
  --disable-vcut --disable-vorbiscomment --disable-nls --without-flac --without-speex --without-kate --without-curl
make -C share && make -C oggenc && cp oggenc/oggenc ../../bin/
cd ../..
```

## Conjunto para o Unreal

```bash
source tools/env.sh
python3 tools/build_set.py tools/sets/sword_shield.json      # ~1 min
blender -b -P tools/preview_fbx.py -- extract/ue/sword_shield/manifest.json previews/ue A_rswrd_1h_lt_atk_01
```

`tools/sets/<conjunto>.json` lista blend spaces, loops e montagens. Cada animação é uma pilha de camadas, da base para cima, como o Mannequin toca: corpo inteiro (tags `1H_Melee+Sword`) e por cima o braço do escudo (`Shield_OH`). As receitas saem do `.adb`:

```bash
A=extract/models/animations/mannequin/adb/player
python3 tools/adb_resolve.py extract/manifest/paths.txt $A/player_sword.adb $A/player_1h_melee.adb $A/player.adb --tags 1H_Melee,Sword
python3 tools/adb_windows.py $A/player_sword.adb $A/player_1h_melee.adb $A/player.adb --tags 1H_Melee,Sword --fragment Attack_Primary
```

Saída em `extract/ue/<conjunto>/`:

- `fbx/SK_player_male.fbx`: malha + esqueleto (148 ossos, raiz `Xform`), texturas embutidas.
- `fbx/anim/A_*.fbx`: uma animação por arquivo, 30 fps. Montagens com root motion no `Xform`; loops e blend spaces no lugar.
- `fbx/SM_*.fbx`: armas, origem no ponto de pega (sockets `right_hand_attach` / `left_hand_attach`).
- `fbx/textures/*.png` e `manifest.json` (camadas, blend spaces, montagens, materiais, janelas de combate).

O FBX só de animação não tem bind pose: reimportado sozinho, o repouso dele é outro. Os valores são absolutos e o Unreal lê assim. O `preview_fbx.py` copia a pose em espaço de mundo por isso.

Materiais: o jogo usa specular/gloss (metal tem difuso escuro e a cor no specular). O `import_nw.py` cria `M_NW_SpecGloss`, que converte para metallic/roughness.

Janelas de combate: os `ProcLayer` dos fragmentos (`CanDodgeCancel`, `CanTransition`, `WeaponEffects`...). No Unreal viram curvas 0/1 nas sequências (`GetCurveValue`).

## Importar no Unreal

Com o plugin Python Editor Script ligado, no Output Log (modo Python):

```
py "<repo>/tools/ue/import_nw.py" "<repo>/extract/ue/sword_shield/manifest.json"
```

O script desliga o Interchange para FBX (o importador clássico respeita esqueleto, root motion e taxa).

## Galeria de armaduras

```bash
tools/gallery.sh previews/gallery/armaduras.png heroicheavy guardianheavy gryphon1 ...
```

## Projeto Unreal (`unreal/NWPort`)

Projeto C++ com o combate de espada e escudo (plano em `docs/plano-mecanicas.md`).

| Classe | Papel |
|---|---|
| `ANWCharacter` | câmera por cima do ombro, strafe/sprint, input (criado em código), espada e escudo nos sockets; sem controlador vira boneco de treino |
| `UNWCombatComponent` | vida, stamina, ações, varredura da lâmina, bloqueio, esquiva, reações, morte; servidor decide, montagens por multicast |
| `UNWAnimInstance` | base do `ABP_NW` (Speed, Direction, TurnRate, bSprinting, bBlocking) |
| `ANWLost` | Lost de espada (conjunto `lost_sword`): persegue a 15 m e alterna 4 ataques do jogo |
| `ANWHUD` | barras de vida/stamina e vida do alvo à frente |
| `UNWAnimNotify_RandomSound` | som com variação aleatória, gravado nas animações pelo `import_nw.py` |
| `FNWDamageRow` / `FNWActionRow` | linhas das DataTables (`tools/make_datatables.py` gera os CSV) |

Ordem na primeira vez:

1. `python3 tools/make_datatables.py extract/ue/sword_shield/manifest.json`
2. `python3 tools/make_sound_map.py extract/ue/sword_shield/manifest.json` (passos em terra, golpes, vozes, impactos)
3. Abrir `unreal/NWPort/NWPort.uproject` (compila o módulo).
4. No Output Log (Python): `py ".../tools/ue/import_nw.py" ".../extract/ue/sword_shield/manifest.json"` — malha, materiais, animações com janelas e sons, blend spaces, montagens, DataTables.
5. Criar `ABP_NW` em `/Game/NW/sword_shield/` com classe pai `NWAnimInstance` (blend spaces + slot DefaultSlot + loop de bloqueio). Repetir 1–4 para `tools/sets/lost_sword.json` e criar `ABP_Lost` em `/Game/NW/lost_sword/` (BS_Walk + slot).
6. `py ".../tools/ue/make_test_map.py"` — `L_Arena` com chão, céu, 3 bonecos (parado, de escudo, de costas) e a música de Brightwood.

Controles: WASD, mouse, Shift corre, botão esquerdo ataca (segurar = pesado), direito bloqueia, Espaço esquiva, Q/R/F habilidades.

## Referência: código do Lumberyard

`reference/lumberyard` (clone raso de aws/lumberyard, arquivado). O motor do jogo é um fork dele; o código explica como os dados são interpretados. Lugares úteis:

- `dev/Gems/CryLegacy/Code/Source/CryAnimation/GlobalAnimationHeaderLMG.cpp`: blend spaces (`.bspace`). Parâmetro sem `SetParaN` é calculado da translação do osso raiz × `PlaybackScale` (linha ~1505).
- `dev/Gems/CryLegacy/Code/Source/CryAnimation/SkeletonAnim_BlendMan.cpp`: eventos de animação usam tempo normalizado (`GetNormalizedTime`).
- `dev/Gems/CryLegacy/Code/Source/CryAction/Mannequin`: fragmentos, camadas e clipes procedurais do `.adb`.
