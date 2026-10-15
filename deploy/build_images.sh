#!/usr/bin/env bash
# Builds the compose images. --network=host: the default build DNS does not resolve on this machine.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
docker build --network=host --build-context tools="$here/../tools" -t nw-core:local "$here/../backend"
docker build --network=host -t nw-gameserver:local -f "$here/Dockerfile.gameserver" "$here"
