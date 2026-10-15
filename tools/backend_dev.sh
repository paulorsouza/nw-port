#!/usr/bin/env bash
# Movido para tools/linux/backend_dev.sh (2026-10-15); atalho temporário para as sessões que já estavam abertas.
exec "$(dirname "${BASH_SOURCE[0]}")/linux/backend_dev.sh" "$@"
