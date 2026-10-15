---
name: nw-package-server
description: Empacota o servidor dedicado Linux e refaz as imagens Docker (tools/linux/package_server.sh, deploy/build_images.sh). Usar quando o usuário pedir para atualizar o servidor/Docker.
---

# Empacotar o servidor (tools/linux/package_server.sh + deploy/build_images.sh)

Pesado (compila e cozinha). Um de cada vez, nada pesado junto, New World original fechado.

1. Compilar o alvo de servidor na engine do código-fonte (`docs/plano-servidor.md`):
   `cd /mnt/jogos/UnrealEngine/src-5.8.3 && Engine/Build/BatchFiles/Linux/Build.sh NWPortServer Linux Development -Project=$HOME/workspace/nw-port/unreal/NWPort/NWPort.uproject -WaitMutex -MaxParallelActions=6`
2. `tools/linux/package_server.sh` (cozinha e empacota) ou `--skip-cook` se só o C++ mudou.
3. `deploy/build_images.sh` (usa `--network=host`; o DNS do build padrão não funciona aqui).
4. Subir: `docker compose -f deploy/compose.yaml up -d` (pare o nw-core local na porta 8090 antes).

Deploy em produção/nuvem só com pedido explícito do usuário.
