#!/usr/bin/env bash
# Movido para tools/linux/rede.sh (2026-10-15); atalho temporário para as sessões que já estavam abertas.
exec "$(dirname "${BASH_SOURCE[0]}")/linux/rede.sh" "$@"
