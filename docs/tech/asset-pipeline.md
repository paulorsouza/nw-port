# Asset pipeline

How each kind of asset leaves the game and reaches Unreal. To build everything, follow [../setup.md](../setup.md);
this page is the technical detail behind the extraction and import steps. Every tool and script is listed in
[tools.md](tools.md).

## Tools

| Tool | Where | What for |
|---|---|---|
| nwbt (nw-buddy, MIT) | fork `nw-buddy` (branch `nw-port`) in `vendor/nw-buddy` → `bin/nwbt` | opens `.pak`, datasheets, models and animations → glTF |
| ooz (Kraken) | powzix/ooz + `patches/ooz.patch` → `bin/libooz.so` | Oodle decompression on Linux |
| texconv (DirectXTex) | `bin/texconv.exe` through `tools/linux/texconv` (wine) | DDS → PNG |
| Blender | `~/.local/opt/blender-*` (or `NW_BLENDER`) | checks models and animations, headless renders, FBX export |
| Cryengine-Converter | `vendor/Cryengine-Converter` | fallback if nwbt does not cover a format |

## Building the tools

```bash
tools/linux/get_tools.sh     # downloads texconv.exe and vgmstream-cli; builds nwbt, dbgwater and libooz.so
```

`get_tools.sh` skips what exists (`--force` redoes everything):

- `bin/texconv.exe` from the DirectXTex release `may2026`; `bin/texconv` links to the wine wrapper.
- `bin/vgmstream-cli` from the vgmstream release `r2117` (`vgmstream-linux.zip`).
- `bin/libooz.so` through `tools/linux/build_ooz.sh`.
- `bin/nwbt` and `bin/dbgwater`: clones the fork into `vendor/nw-buddy` if missing, then
  `tools/linux/build_nwbt.sh --test`. It also rebuilds when `python3 tools/build_nwbt.py --conferir` fails (binary
  from another patch).
- `oggenc` comes from apt (`vorbis-tools`); the script warns if it is missing.

Windows: `tools/windows/build_ooz.ps1`, `tools/windows/build_nwbt.ps1 -Test`, and the release downloads described
in the setup skill.

nwbt is built from the fork's base commit plus `patches/nwbt.patch`, with the patch version stamped in the binary
([../../patches/README.md](../../patches/README.md)). The fork carries these changes over upstream nw-buddy
`81b2302` (reference diff: `patches/nwbt-nwport.patch`):

- `utils/oodle_linux.go`: loads `libooz.so` (the original loads the DLL only on Windows).
- `formats/cgf/chunk_controller.go`: fixes `smallTree64BitQuat` (21+21+20 bits; the counter never advanced and the
  21-bit mask had 20). It affected the weapon layers (`rswrd_idle`, `rswrd_run`...).
- `commands/models`: `.caf` files passed with a `.cdf` in the `files` command go in as animations of the same glb;
  commands `slices` (one dynamicslice at the origin), `chunks` (pre-generated content of the `<PLOT>` entries of
  the capitals layers) and `slicestats`.

## Character with animations

```bash
source tools/linux/env.sh
nwbt models files -g "$GAME" -o extract/stage3 --binary --embed --ior 1.5 \
  objects/characters/player/male/player_male.cdf <.caf clips> <weapon .cgf> <shield .cgf>
blender -b -P tools/assemble_character.py -- <character.glb> <weapon.glb> <shield.glb> <output.blend> <previews_folder>
```

Locomotion actions are composed as in `player_1h_melee.adb`: base `unarmed_*` plus the `rswrd_*` layer.

## Audio

The audio is Wwise: `sounds/wwise/*.bnk` (events and short sounds) and `sounds/wwise/media/*.wem` (streams, almost
all music). Files only have numeric IDs. Names come from the FNV-1 hash of tokens found in the game's
.adb/.xml/.json/.timeline files.

```bash
python3 tools/extract_audio.py   # output in extract/audio/, index in extract/audio/INDEX.md
```

Stages: `nwbt unpack` → name list → wwiser makes a `.txtp` per event → `vgmstream-cli` decodes → `oggenc` writes
`.ogg`. Each stage reuses what is already in `extract/_raw/`.

It converts music (events, tracks and instruments), one-handed sword, shield, impacts, effort voice, footsteps,
dodge and jump. Ambience has only an index; `tools/linux/render_ambient.sh <territory>` renders a territory's
ambience.

Tools (sources in `vendor/`, binaries in `bin/`). The normal path is `get_tools.sh` (vgmstream release) and apt
(`vorbis-tools`). Building vgmstream and oggenc from source is the alternative:

```bash
# wwiser (bnnm), pure Python
git clone https://github.com/bnnm/wwiser vendor/wwiser          # 08e937b

# vgmstream-cli: no FFmpeg, MPEG (mpg123 does not build with GCC 15) or Speex (needs autoreconf)
git clone https://github.com/vgmstream/vgmstream vendor/vgmstream   # 7c520e8
X=$PWD/vendor/xiph
cd vendor/vgmstream
# 1st configure: only downloads ogg/vorbis into dependencies/ (the downloaded vorbis cannot find ogg.h)
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

# oggenc (vorbis-tools 1.4.3; the xiph tarball already has the configure script)
curl -L https://downloads.xiph.org/releases/vorbis/vorbis-tools-1.4.3.tar.gz | tar xz -C vendor
cd vendor/vorbis-tools-1.4.3
PKG_CONFIG_PATH=$X/lib/pkgconfig ./configure --disable-ogg123 --disable-oggdec --disable-ogginfo \
  --disable-vcut --disable-vorbiscomment --disable-nls --without-flac --without-speex --without-kate --without-curl
make -C share && make -C oggenc && cp oggenc/oggenc ../../bin/
cd ../..
```

On Windows, vgmstream (`vgmstream-win64.zip`, release `r2117`) and oggenc2 go into a folder of your choice, with
`NW_VGMSTREAM_CLI` and `NW_OGGENC` set in `local.env`.

## Set for Unreal

```bash
source tools/linux/env.sh
python3 tools/build_set.py tools/sets/sword_shield.json      # ~1 min
blender -b -P tools/preview_fbx.py -- extract/ue/sword_shield/manifest.json previews/ue A_rswrd_1h_lt_atk_01
```

`tools/sets/<set>.json` lists blend spaces, loops and montages. The player set is split in parts, one per area
(`tools/sets/player/<area>.json`), merged by `tools/setspec.py`. Each animation is a stack of layers, from the base
up, as the Mannequin plays it: full body (tags `1H_Melee+Sword`) and on top the shield arm (`Shield_OH`). The
recipes come from the `.adb`:

```bash
A=extract/models/animations/mannequin/adb/player
python3 tools/adb_resolve.py extract/manifest/paths.txt $A/player_sword.adb $A/player_1h_melee.adb $A/player.adb --tags 1H_Melee,Sword
python3 tools/adb_windows.py $A/player_sword.adb $A/player_1h_melee.adb $A/player.adb --tags 1H_Melee,Sword --fragment Attack_Primary
```

Output in `extract/ue/<set>/`:

- `fbx/SK_player_male.fbx`: mesh + skeleton (148 bones, root `Xform`), embedded textures.
- `fbx/anim/A_*.fbx`: one animation per file, 30 fps. Montages with root motion on `Xform`; loops and blend spaces
  in place.
- `fbx/SM_*.fbx`: weapons, origin at the grip point (sockets `right_hand_attach` / `left_hand_attach`).
- `fbx/textures/*.png` and `manifest.json` (layers, blend spaces, montages, materials, combat windows).

An animation-only FBX has no bind pose: reimported alone, its rest pose is different. The values are absolute and
Unreal reads them that way. That is why `preview_fbx.py` copies the pose in world space.

Materials: the game uses specular/gloss (metal has a dark diffuse and the color in specular). `import_nw.py`
creates `M_NW_SpecGloss`, which converts to metallic/roughness.

Combat windows: the `ProcLayer` entries of the fragments (`CanDodgeCancel`, `CanTransition`, `WeaponEffects`...).
In Unreal they become 0/1 curves on the sequences (`GetCurveValue`).

## Import into Unreal

Import step 02 ([../setup.md](../setup.md)) imports the player and Lost sets. To import one set by hand, with the
Python Editor Script plugin on, in the Output Log (Python mode):

```
py "<repo>/tools/ue/import_nw.py" "<repo>/extract/ue/sword_shield/manifest.json"
```

The script turns Interchange off for FBX (the classic importer respects skeleton, root motion and frame rate).

Order for a set by hand:

1. `python3 tools/make_datatables.py extract/ue/sword_shield/manifest.json`
2. `python3 tools/make_sound_map.py extract/ue/sword_shield/manifest.json` (footsteps on dirt, hits, voices, impacts)
3. Open `unreal/NWPort/NWPort.uproject` (compiles the module).
4. In the Output Log (Python): `py ".../tools/ue/import_nw.py" ".../extract/ue/sword_shield/manifest.json"`: mesh,
   materials, animations with windows and sounds, blend spaces, montages, DataTables.
5. `ABP_NW` (`/Game/NW/sword_shield/`, parent class `NWAnimInstance`) and `ABP_Lost` (`/Game/NW/lost_sword/`, from
   `tools/sets/lost_sword.json`) come from the repository; they were built by hand. `tools/ue/make_abp.py` only
   creates empty ones.
6. `py ".../tools/ue/make_test_map.py"`: `L_Arena`.

## Armor gallery

```bash
tools/linux/gallery.sh previews/gallery/armaduras.png heroicheavy guardianheavy gryphon1 ...
```

Renders armor sets side by side to pick visually (`tools/gallery.py`).

## Reference: Lumberyard code

`reference/lumberyard` (shallow clone of aws/lumberyard, archived; not in git). The game's engine is a fork of it;
its code explains how the data is interpreted. Useful places:

- `dev/Gems/CryLegacy/Code/Source/CryAnimation/GlobalAnimationHeaderLMG.cpp`: blend spaces (`.bspace`). A
  parameter without `SetParaN` is computed from the root bone translation × `PlaybackScale` (line ~1505).
- `dev/Gems/CryLegacy/Code/Source/CryAnimation/SkeletonAnim_BlendMan.cpp`: animation events use normalized time
  (`GetNormalizedTime`).
- `dev/Gems/CryLegacy/Code/Source/CryAction/Mannequin`: fragments, layers and procedural clips of the `.adb`.
