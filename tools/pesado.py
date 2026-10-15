"""Fila de trabalho nativa: python tools/pesado.py <comando> [argumentos]."""
import os
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
    # "python3"/"python" viram o Python que rodou a fila: o mesmo comando vale no Linux e no Windows.
    if command[0] in ("python", "python3"):
        command[0] = sys.executable
    try:
        with nwprocess.heavy_lock():
            # Mesmo diretorio e codigo de saida de quem chamou, como o tools/linux/pesado.sh.
            nwprocess.run(command, cwd=os.getcwd())
    except nwprocess.ResourceError as exc:
        print(exc, file=sys.stderr)
        return 3
    except subprocess.CalledProcessError as exc:
        return exc.returncode if exc.returncode > 0 else 4
    except OSError as exc:
        print(exc, file=sys.stderr)
        return 4
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
