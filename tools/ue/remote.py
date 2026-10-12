"""Roda Python no editor da Unreal aberto (execução remota do Python Editor Script Plugin).

    python3 tools/ue/remote.py arquivo.py          roda o arquivo no editor
    python3 tools/ue/remote.py -c "print(1)"       roda o código

Requer no projeto: Configurações do Projeto > Plugins > Python > "Ativar execução remota?" (bRemoteExecution).
Usa o cliente da própria engine (PythonScriptPlugin/Content/Python/remote_execution.py). Mostra a saída do print
e os erros do editor.
"""
import os
import sys
import time

UE = os.environ.get("UE_DIR", "/mnt/jogos/UnrealEngine/5.8")
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
    # Mesmo endereço de ligação do editor. No Linux um socket preso em 127.0.0.1 não recebe multicast: os dois lados
    # usam 0.0.0.0 (TTL 0 = não sai da máquina) e a interface lo precisa de multicast (sudo ip link set lo multicast on).
    cfg.multicast_bind_address = os.environ.get("UE_REMOTE_BIND", "0.0.0.0")
    rx = remote_execution.RemoteExecution(cfg)
    rx.start()
    try:
        for _ in range(80):
            if rx.remote_nodes:
                break
            time.sleep(0.1)
        if not rx.remote_nodes:
            sys.exit("nenhum editor encontrado (editor aberto com execução remota ligada?)")
        rx.open_command_connection(rx.remote_nodes[0]["node_id"])
        res = rx.run_command(code, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)
        for o in res.get("output", []):
            print(o.get("output", "").rstrip())
        if not res.get("success"):
            print("ERRO:", res.get("result"))
            sys.exit(1)
    finally:
        rx.stop()


if __name__ == "__main__":
    main()
