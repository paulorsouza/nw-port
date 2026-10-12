---
name: nw-wsl
description: Rodar os scripts do projeto no Windows através do WSL2 (Ubuntu). Usar quando o Claude Code estiver rodando no Windows nativo e precisar de tools/*.sh, nwbt, Blender ou Python do pipeline.
---

# Windows: scripts pelo WSL2

O pipeline de extração (`tools/*.sh`, nwbt com ooz, texconv via wine, Blender headless) é Linux. No Windows:

Melhor opção: rodar o próprio Claude Code dentro do WSL2 (Ubuntu) para a extração; aí tudo funciona como no Linux.

Se estiver no Windows nativo, chame o WSL em cada comando:

```powershell
wsl.exe -d Ubuntu -- bash -lc 'cd ~/nw-port && tools/build_all.sh'
```

Regras:
- Repositório dentro do filesystem do Linux (`~/nw-port`), não em `/mnt/c` (I/O no `/mnt/c` é muito mais lento).
- O jogo instalado no Windows aparece em `/mnt/c/Program Files (x86)/Steam/steamapps/common/New World`:
  `export NW_GAME="/mnt/c/Program Files (x86)/Steam/steamapps/common/New World"`.
- Memória do WSL2: por padrão metade da RAM; para a extração, `C:\Users\<user>\.wslconfig` com `memory=24GB`.
- A Unreal roda no Windows: os imports (`tools/ue_import_all.sh`) ainda não têm versão Windows (pendente:
  `ue_import_all.ps1` chamando `UnrealEditor-Cmd.exe`). Os scripts `tools/ue/*.py` em si funcionam nos dois.
  Para o editor Windows ler `extract/`, use o caminho `\\wsl$\Ubuntu\home\<user>\nw-port\extract`.
- Pendente de validar numa máquina Windows real.
