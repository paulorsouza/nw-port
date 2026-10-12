# Ghidra e MCP (RE do NewWorld.exe)

Setup do Ghidra headless + ghidra-mcp usado na engenharia reversa do `NewWorld.exe`. Compartilhado entre
o plano A (nw-port) e o plano B (cliente original (fora deste repositório)); o projeto `.gpr` vive em `nw-port/extract/ghidra`.


- Ghidra 12.1.4 e JDK 21 em `~/.local/opt/` (o Java do sistema é 17). `support/launch.properties` aponta
  `JAVA_HOME_OVERRIDE` para o JDK 21.
- ghidra-mcp (bethington, v7.0.0) em `~/workspace/ghidra-mcp`: extensão compilada por Gradle e instalada em
  `~/.config/ghidra/ghidra_12.1.4_PUBLIC/Extensions/GhidraMCP`; ponte Python em `.venv`; registrada em
  `~/.claude.json` como `ghidra-mcp` (`GHIDRA_MCP_URL=http://127.0.0.1:8089`).
- **A análise automática completa do executável (180 MB) estourou a RAM (38 GB) e travou a máquina.** Rodar o
  Ghidra sempre com teto: `systemd-run --user --scope -p MemoryMax=14G -p MemorySwapMax=0`, importação com
  `-noanalysis` e análise por função.

Servidor headless do MCP sobre o projeto:

```bash
cd ~/workspace/ghidra-mcp && G=~/.local/opt/ghidra_12.1.4_PUBLIC; J=$(ls -d ~/.local/opt/jdk-21*); CP=build/libs/GhidraMCP-7.0.0.jar; for j in $G/Ghidra/Framework/*/lib/*.jar $G/Ghidra/Features/*/lib/*.jar $G/Ghidra/Processors/*/lib/*.jar; do CP="$CP:$j"; done; systemd-run --user --scope --quiet -p MemoryMax=14G -p MemorySwapMax=0 nice -n 10 $J/bin/java -Xmx10g -Djava.awt.headless=true -cp "$CP" com.xebyte.headless.GhidraMCPHeadlessServer --port 8089 --bind 127.0.0.1 --project ~/workspace/nw-port/extract/ghidra/NewWorld.gpr --program NewWorld.exe
```

O servidor headless sobe com esse comando (verificado em 2026-10-11, ~1,7 GB de RSS). O `run_script_inline` fica
desligado (falta `GHIDRA_MCP_ALLOW_SCRIPTS=1`). Chamadas que disparam análise (`disassemble_bytes`, `search_strings`)
podem estourar o tempo do MCP; para leituras pequenas, `objdump -d -M intel --start-address=… --stop-address=…`
direto no executável é mais rápido. O projeto não tem funções criadas pela análise automática: crie a função no
início certo (depois do padding `CC`) antes de descompilar.

