"""Conferência do que o nwbt e o Blender gravaram (os dois podem sair com 0 sem gerar nada).

O Blender devolve 0 quando o script -P falha, a menos que receba --python-exit-code; o nwbt antigo tratava
"@lista" como padrão e saía 0 sem converter. Quem chama confere que cada saída esperada foi (re)gravada.
"""
import os
import sys
import time

import nwplatform


def now():
    # Margem de 1 s para sistemas de arquivos com mtime grosso.
    return time.time() - 1


def stale(paths, since):
    """Caminhos que não existem, estão vazios ou não foram gravados desde `since`."""
    out = []
    for p in paths:
        try:
            st = os.stat(p)
        except OSError:
            out.append(p)
            continue
        if st.st_size == 0 or st.st_mtime < since:
            out.append(p)
    return out


def require(paths, since, who):
    paths = [p for p in paths if p]
    missing = stale(paths, since)
    if missing:
        listed = "\n  ".join(missing[:20]) + ("\n  ..." if len(missing) > 20 else "")
        sys.exit(f"{who} não gravou {len(missing)} de {len(paths)} saídas esperadas:\n  {listed}")


def blender(script, *args):
    """Linha de comando do Blender em segundo plano que sai com 1 se o script falhar."""
    return [nwplatform.executable("blender"), "-b", "--python-exit-code", "1", "-P", str(script), "--", *map(str, args)]


def require_nwbt():
    """Para se o nwbt instalado não for do patch deste checkout (tools/build_nwbt.py)."""
    import build_nwbt
    ok, msg = build_nwbt.check_binary()
    if not ok:
        sys.exit(msg)
