# Ghidra e MCP (RE do NewWorld.exe)

Setup do Ghidra headless + ghidra-mcp e ferramentas usadas pelo agente `nw-re` na engenharia reversa do
`NewWorld.exe`. O projeto `.gpr` vive em `extract/ghidra` (fora do git). As respostas ficam só no disco de cada um
(`re-local/`, fora do git); o repositório guarda o método, não as respostas.

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

## Ferramentas

### Árvores de comportamento

`python3 tools/bt_dump.py <arquivo.behaviortree.json> [profundidade]` imprime a árvore compacta (tipo, nome, utilidade,
`noint`, parâmetros não padrão, condições como `$Chave opN valor`).

Extrair as árvores: `Server.pak` (depot 1063731) linkado sozinho em `extract/serverpak/game/assets/`, depois
`nwbt unpack -g extract/serverpak/game --x-objects json --crc-file .nwbt/rtti/nwt-crc.json --uuid-file .nwbt/rtti/nwt-types.json "sharedassets/ai/**"`.
Com só esse pak no diretório, o nwbt não carrega o índice dos outros.

### NewWorld.exe

- `python3 -I tools/re/xref.py str "texto" ...` — referências a uma string (RIP-relative no `.text` e ponteiros em
  `.rdata`/`.data`), com a função dona pela `.pdata` (segue `UNW_FLAG_CHAININFO` até o início real). `va 0x...` faz o
  mesmo para endereços (chamadores, slots de vtable). Varre o `.text` inteiro em ~2 s.
- `tools/re/dec.sh 0x... ...` — cria a função no Ghidra headless (porta 8089) e descompila.
- No módulo: `vtable(va)`, `cstr(va)`, `lambda_invokers("Metodo@Classe")` (corpo de lambda via typeid do AZStd::function).
- O exe tem RTTI só de lambdas/templates; classes de nó não têm. Para achar vtables: função com mensagem exclusiva →
  `xref.py va` da função → slot em `.rdata`.
- Enums: registros de reflexão (`lea rdx, nome` + `mov r8d, valor` antes ou depois do `lea`; conferir no disassembly).

### Descompilação em lote (análise restrita)

`tools/re/RangeDecompile.java` cria as funções listadas (uma por linha, `0x...`) e grava o C de cada uma. A lista
sai da `.pdata` (ver `xref.func_of`). Com o servidor MCP parado (ele trava o projeto):

    systemd-run --user --scope -p MemoryMax=14G -p MemorySwapMax=0 nice -n 10 env MAXMEM=10G \
      ~/.local/opt/ghidra_12.1.4_PUBLIC/support/analyzeHeadless extract/ghidra NewWorld -process NewWorld.exe \
      -noanalysis -readOnly -scriptPath tools/re -postScript RangeDecompile.java <lista.txt> <saida.c>

4.139 funções em ~10 min, sem pressão de memória. `-readOnly` não grava nada no projeto.
`tools/re/find_vcalls.py <saida.c>` lista funções por padrão de chamada virtual (veja o script).

### Reflexão e datasheets

- `xref.reflect_fields(início)`: campos de uma função de reflexão AZ (nome, offset). `xref.func_range(início)` junta os
  pedaços da `.pdata` da função.
- `xref.datasheet_columns(início)`: num parser de linha de datasheet, coluna → destino na struct (e bit, se for flag).
- `python3 -I tools/re/dis.py 0xINI 0xFIM`: disassembly com strings e floats resolvidos.
- Padrão do cvar CryEngine: `IConsole::Register(nome, &var, padrão, …)`; float padrão no 4º argumento (`xmm3`).

### CAGE e interface

- `python3 -I tools/re/cage_inventory.py [raiz]`: tipos de atividade e condição das actionlists, com contagem, uso por
  NPC/jogador e se o nome existe no exe (condições como `ActionCondition<nome>`).
- Endereço de uma classe CAGE: `xref.py str <Atividade>` ou `xref.py str ActionCondition<Condição>`.
- `python3 -I tools/re/cage_classes.py > extract/ghidra/dec/cage_classes.tsv`: todas as classes CAGE (e as usadas nas
  actionlists) com vtable e 24 slots.
- Scripts de interface: `extract/_raw/names/lyshineui/**/*.luac` (Lua 5.1, 2 bytes antes de `\x1bLua`);
  `python3 tools/luac_dump.py <arquivo> [filtro]` desmonta.
- unluac (decompilador Lua 5.1, Java): `~/.local/opt/unluac/unluac_2025_12_23.jar`, baixado em 2026-10-12 de
  `sourceforge.net/projects/unluac/files/Unstable/` (796.256 bytes, sha256 98be0fa8…5538fcc).
  `tools/re/luac_batch.sh [origem] [destino]` descompila tudo para `extract/lua/lyshineui/` (mesma árvore, `.lua`);
  falhas em `falhas.txt`. Os scripts vêm sem informação de depuração: locais saem como `L0_1`, mas campos, chamadas e
  strings ficam com os nomes reais.
- `python3 -I tools/re/fn.py 0xADDR [...] [-n linhas] [--decl]`: mostra a descompilação já salva em
  `extract/ghidra/dec/*.c` (índice em `.index.json`, refeito quando um `.c` muda). Funções folha sem entrada na `.pdata`
  não entram nos lotes; para elas, `tools/re/dis.py`.
- `python3 -I tools/re/lua_summary.py <arquivo.lua> [filtro]`: métodos de um script de interface descompilado, com
  as chamadas de barramento (`XBus.Event.Metodo`), chamadas ao próprio script, strings e números.

### Onde fica o resultado (fora do git)

- Descompilações: `extract/ghidra/dec/`.
- Respostas: `re-local/` (`re-local/README.md`). Pode apagar: o agente refaz a pergunta quando precisar.
