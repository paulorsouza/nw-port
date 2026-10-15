---
name: nw-windows
description: Comandos do port no Windows nativo (docs/windows-convert.md) — fila de trabalho pesado, extração, import, Data/, diagnóstico e build do nwbt.exe/ooz.dll (tools/windows/ e o Python do pipeline), e a alternativa pelo WSL2. Usar quando o Claude Code estiver no Windows ou uma skill de esteira mandar "pela skill da plataforma".
---

# Windows: comandos de plataforma

Sem WSL: o Python do pipeline (`tools/*.py`) é o mesmo do Linux; o que é só do Windows fica em `tools/windows/`.
Use o Python de python.org (o da Microsoft Store é recusado) e `python` no lugar de `python3`. Plano e estado:
`docs/windows-convert.md`; setup do zero: skill `setup`.

| Ação | Comando |
|---|---|
| Diagnóstico (ferramentas, nwbt do patch atual; `--full`: áudio, Blender, Unreal e teste do `@lista`) | `python tools/windows/doctor.py --full` |
| Fila de trabalho pesado | `python tools/pesado.py <comando>` |
| Extração inteira ou etapa NN | `python tools/build_all.py [NN]` (`--dry-run` lista) |
| Compilar e importar na Unreal, etapa NN (editor fechado) | `python tools/ue_import_all.py [NN]` |
| Data/ | `python tools/make_data.py` |
| Cidade, coletáveis, ambiente sonoro | `python tools/convert_city.py`, `convert_gatherables.py`, `render_ambient.py` |
| Versão do jogo | `python tools/jogo_versao.py --exe` |
| Checkout copiado do WSL | `python tools/windows/check_migration.py [--apply]` |

Variáveis: `NW_GAME` (jogo), `NW_UE` (engine), `NW_BLENDER` (blender.exe), `NW_BUDDY` (fork, se fora de
`vendor/nw-buddy`).

## nwbt e ooz

No Developer PowerShell x64 (Go, Git, Visual Studio com C++):

```powershell
./tools/windows/build_ooz.ps1
./tools/windows/build_nwbt.ps1 -Test
```

`build_nwbt.ps1` chama o `tools/build_nwbt.py` (o mesmo do Linux): commit base do fork + `patches/nwbt.patch`, versão
gravada no `nwbt.exe`. Depois de puxar um `patches/nwbt.patch` novo, recompile: o `doctor.py` e o `build_set.py`
recusam `nwbt.exe` de outro patch. Mudar o nwbt: `patches/README.md`.

Sem equivalente Windows ainda: `backend_dev`, `ui_shot`, `check` (use o `doctor.py` e `python -m unittest discover -s
tools/tests`), `package_server`, `rede`, `net_test`, `play_local`.

## Alternativa: WSL2

Para usar os scripts do Linux (`tools/linux/*.sh`) no Windows, rode o Claude Code dentro do WSL2 (Ubuntu) e siga a
skill `nw-linux`. Repositório no filesystem do Linux (`~/nw-port`, não `/mnt/c`); jogo em
`NW_GAME="/mnt/c/Program Files (x86)/Steam/steamapps/common/New World"`; memória do WSL em `.wslconfig`
(`memory=24GB`). A Unreal Windows lê `extract/` por `\\wsl$\Ubuntu\home\<user>\nw-port\extract`.
