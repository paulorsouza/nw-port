# source tools/linux/local_env.sh — exporta NW_GAME, NW_UE e o resto de local.env (o ambiente vence o arquivo).
eval "$(python3 -I "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/nwplatform.py" --shell)"
