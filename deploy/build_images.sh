#!/usr/bin/env bash
# Gera as imagens do compose. --network=host: o DNS do build padrão não resolve nesta máquina.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
docker build --network=host -t nw-core:local "$here/../backend"
docker build --network=host -t nw-gameserver:local -f "$here/Dockerfile.gameserver" "$here"
