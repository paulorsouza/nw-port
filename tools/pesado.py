"""Native heavy-job queue: python tools/pesado.py <command> [arguments]."""
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
    # "python3"/"python" become the Python that ran the queue: the same command works on Linux and Windows.
    if command[0] in ("python", "python3"):
        command[0] = sys.executable
    try:
        with nwprocess.heavy_lock():
            # Same directory and exit code as the caller, like tools/linux/pesado.sh.
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
