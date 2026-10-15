#!/usr/bin/env bash
# Empacota o servidor dedicado em deploy/build/LinuxServer.
#   tools/linux/package_server.sh [--skip-cook]
# 1. Cozinha o conteúdo para LinuxServer com o editor instalado (mesmo CompatibleChangelist da engine do fonte).
# 2. Monta a pasta (binário NWPortServer já compilado pela engine do fonte + .pak) com o UAT da engine do fonte.
# O resultado contém assets do jogo extraídos da sua instalação: fica só em máquinas suas.
set -euo pipefail
# O MSBuild do UAT deixa nós vivos para reaproveitar, e eles herdam o cadeado da fila de trabalho pesado
# (tools/linux/pesado.sh): a fila ficava presa depois do pacote.
export MSBUILDDISABLENODEREUSE=1 DOTNET_CLI_USE_MSBUILD_SERVER=0
root="$(cd "$(dirname "$0")/../.." && pwd)"
uproject="$root/unreal/NWPort/NWPort.uproject"
installed=/mnt/jogos/UnrealEngine/5.8
source_engine=/mnt/jogos/UnrealEngine/src-5.8.3
out="$root/deploy/build"
# Mapas cozidos (NW_SERVER_MAPS troca a lista).
maps="${NW_SERVER_MAPS:-/Game/NW/Maps/L_SouthWindsward+/Game/NW/Maps/L_QuedaEterna+/Game/NW/Maps/L_Arena}"

if [ "${1:-}" != "--skip-cook" ]; then
  "$installed/Engine/Binaries/Linux/UnrealEditor-Cmd" "$uproject" -run=cook -targetplatform=LinuxServer -map="$maps" \
    -unattended -nopause -nosplash -abslog="$root/unreal/NWPort/Saved/Logs/cook-server.log"
fi

"$source_engine/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$uproject" -platform=Linux \
  -server -serverplatform=Linux -noclient -skipbuild -skipcook -stage -pak -archive -archivedirectory="$out" \
  -unattended -nop4 -utf8output
ls -la "$out/LinuxServer"
