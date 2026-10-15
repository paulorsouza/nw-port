#!/usr/bin/env bash
# Movido para tools/linux/convert_gatherables.sh (2026-10-15); atalho temporário para as sessões que já estavam abertas.
exec "$(dirname "${BASH_SOURCE[0]}")/linux/convert_gatherables.sh" "$@"
