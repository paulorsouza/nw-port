"""Fila de trabalho nativa: python tools/pesado.py <comando> [argumentos]."""
import subprocess
import sys
import nwprocess


def main():
    command = sys.argv[1:]
    if command[:1] == ["--"]:
        command = command[1:]
    if not command:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        with nwprocess.heavy_lock():
            nwprocess.run(command)
    except nwprocess.ResourceError as exc:
        print(exc, file=sys.stderr)
        return 3
    except (OSError, subprocess.CalledProcessError) as exc:
        print(exc, file=sys.stderr)
        return 4
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
