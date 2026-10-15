"""Checks what nwbt and Blender wrote (both can exit 0 without producing anything).

Blender returns 0 when the -P script fails, unless it gets --python-exit-code; the old nwbt treated
"@list" as a pattern and exited 0 without converting. The caller checks that each expected output was (re)written.
"""
import os
import sys
import time

import nwplatform


def now():
    # 1 s margin for file systems with coarse mtime.
    return time.time() - 1


def stale(paths, since):
    """Paths that do not exist, are empty or were not written since `since`."""
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
        sys.exit(f"{who} did not write {len(missing)} of {len(paths)} expected outputs:\n  {listed}")


def blender(script, *args):
    """Background Blender command line that exits with 1 if the script fails."""
    return [nwplatform.executable("blender"), "-b", "--python-exit-code", "1", "-P", str(script), "--", *map(str, args)]


def require_nwbt():
    """Stops if the installed nwbt is not from this checkout's patch (tools/build_nwbt.py)."""
    import build_nwbt
    ok, msg = build_nwbt.check_binary()
    if not ok:
        sys.exit(msg)
