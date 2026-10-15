"""Runs Python in the open Unreal editor (remote execution of the Python Editor Script Plugin).

    python3 tools/ue/remote.py file.py             runs the file in the editor
    python3 tools/ue/remote.py -c "print(1)"       runs the code

Requires in the project: Project Settings > Plugins > Python > "Enable Remote Execution?" (bRemoteExecution).
Uses the engine's own client (PythonScriptPlugin/Content/Python/remote_execution.py). Shows the print output
and the editor's errors.
"""
import os
import sys
import time

UE = os.environ.get("NW_UE") or os.environ.get("UE_DIR") or (
    "C:/UnrealEngine" if os.name == "nt" else "/mnt/jogos/UnrealEngine/5.8")
sys.path.insert(0, os.path.join(UE, "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python"))
import remote_execution  # noqa: E402


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "-c":
        code = sys.argv[2]
    elif len(sys.argv) >= 2:
        code = open(sys.argv[1]).read()
    else:
        sys.exit(__doc__)
    cfg = remote_execution.RemoteExecutionConfig()
    # Same bind address as the editor. On Linux a socket bound to 127.0.0.1 does not receive multicast: both sides
    # use 0.0.0.0 (TTL 0 = does not leave the machine) and the lo interface needs multicast (sudo ip link set lo multicast on).
    cfg.multicast_bind_address = os.environ.get("UE_REMOTE_BIND", "0.0.0.0")
    rx = remote_execution.RemoteExecution(cfg)
    rx.start()
    try:
        for _ in range(80):
            if rx.remote_nodes:
                break
            time.sleep(0.1)
        if not rx.remote_nodes:
            sys.exit("no editor found (editor open with remote execution on?)")
        rx.open_command_connection(rx.remote_nodes[0]["node_id"])
        res = rx.run_command(code, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)
        for o in res.get("output", []):
            print(o.get("output", "").rstrip())
        if not res.get("success"):
            print("ERROR:", res.get("result"))
            sys.exit(1)
    finally:
        rx.stop()


if __name__ == "__main__":
    main()
