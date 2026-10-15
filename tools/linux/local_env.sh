# source tools/linux/local_env.sh — exports NW_GAME, NW_UE and the rest of local.env (the environment wins over the file).
eval "$(python3 -I "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/nwplatform.py" --shell)"
