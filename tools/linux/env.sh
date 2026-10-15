# source tools/linux/env.sh — puts nwbt, texconv and blender on PATH.
root="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/../.." && pwd)"
export PATH="$root/bin:$HOME/.local/go/bin:$HOME/.local/opt/blender-5.2.2-linux-x64:$PATH"
export NWBT_OODLE="$root/bin/libooz.so"
source "$root/tools/linux/local_env.sh"
