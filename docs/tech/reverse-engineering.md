# Reverse engineering

Setup of headless Ghidra + ghidra-mcp and the tools the `nw-re` agent uses to read `NewWorld.exe`. The repository
keeps the method, not the answers. How an area asks a question and applies the answer:
[../workflow.md](../workflow.md), "Reverse engineering on demand".

The Ghidra project (`.gpr`) lives in `extract/ghidra` (not in git). Answers stay only on each machine's disk
(`re-local/`, not in git).

## Setup

- Ghidra 12.1.4 and JDK 21 in `~/.local/opt/` (if the system Java is older, e.g. 17). `support/launch.properties`
  points `JAVA_HOME_OVERRIDE` to JDK 21.
- ghidra-mcp (bethington, v7.0.0), cloned next to the repository: the extension is built with Gradle and installed in
  `~/.config/ghidra/ghidra_12.1.4_PUBLIC/Extensions/GhidraMCP`; the Python bridge in `.venv`; registered in
  `~/.claude.json` as `ghidra-mcp` (`GHIDRA_MCP_URL=http://127.0.0.1:8089`).
- **A full auto-analysis of the executable (180 MB) used up the RAM (38 GB) and froze the machine.** Always run
  Ghidra with a cap: `systemd-run --user --scope -p MemoryMax=14G -p MemorySwapMax=0`, import with `-noanalysis`,
  and analyze function by function.
- Addresses are valid only for the exe of the build in `tools/game.version`. Check it first:
  `tools/linux/game_version.sh --exe`.

## Headless MCP server

From the ghidra-mcp checkout, over the project (`<repo>` is the nw-port checkout):

```bash
G=~/.local/opt/ghidra_12.1.4_PUBLIC; J=$(ls -d ~/.local/opt/jdk-21*); CP=build/libs/GhidraMCP-7.0.0.jar; for j in $G/Ghidra/Framework/*/lib/*.jar $G/Ghidra/Features/*/lib/*.jar $G/Ghidra/Processors/*/lib/*.jar; do CP="$CP:$j"; done; systemd-run --user --scope --quiet -p MemoryMax=14G -p MemorySwapMax=0 nice -n 10 $J/bin/java -Xmx10g -Djava.awt.headless=true -cp "$CP" com.xebyte.headless.GhidraMCPHeadlessServer --port 8089 --bind 127.0.0.1 --project <repo>/extract/ghidra/NewWorld.gpr --program NewWorld.exe
```

- It uses about 1.7 GB of RSS.
- `run_script_inline` stays off (it needs `GHIDRA_MCP_ALLOW_SCRIPTS=1`).
- Calls that trigger analysis (`disassemble_bytes`, `search_strings`) can exceed the MCP timeout. For small reads,
  `objdump -d -M intel --start-address=… --stop-address=…` on the executable is faster.
- The project has no functions from auto-analysis: create the function at the right start (after the `CC` padding)
  before decompiling.
- The server is shared by every session on the machine. It locks the project: stop it before a batch decompile.

## Tools

### Behavior trees

`python3 tools/bt_dump.py <file.behaviortree.json> [depth]` prints the tree compactly (type, name, utility,
`noint`, non-default parameters, conditions as `$Key opN value`).

Extracting the trees: link `Server.pak` (depot 1063731) alone into `extract/serverpak/game/assets/`, then:

```bash
nwbt unpack -g extract/serverpak/game --x-objects json --crc-file .nwbt/rtti/nwt-crc.json --uuid-file .nwbt/rtti/nwt-types.json "sharedassets/ai/**"
```

With only that pak in the folder, nwbt does not load the index of the others.

### NewWorld.exe

- `python3 -I tools/re/xref.py str "text" ...`: references to a string (RIP-relative in `.text` and pointers in
  `.rdata`/`.data`), with the owning function from `.pdata` (follows `UNW_FLAG_CHAININFO` to the real start).
  `va 0x...` does the same for addresses (callers, vtable slots). Scans the whole `.text` in ~2 s.
- `tools/re/dec.sh 0x... ...`: creates the function in headless Ghidra (port 8089) and decompiles it.
- In the module: `vtable(va)`, `cstr(va)`, `lambda_invokers("Method@Class")` (lambda body through the
  AZStd::function typeid).
- The exe has RTTI only for lambdas/templates; node classes have none. To find vtables: a function with a unique
  message → `xref.py va` of the function → slot in `.rdata`.
- Enums: reflection registrations (`lea rdx, name` + `mov r8d, value` before or after the `lea`; check in the
  disassembly).

### Batch decompile (restricted analysis)

`tools/re/RangeDecompile.java` creates the listed functions (one per line, `0x...`) and writes the C of each one.
The list comes from `.pdata` (see `xref.func_of`). With the MCP server stopped, through the heavy-job queue:

```bash
tools/linux/heavy.sh systemd-run --user --scope -p MemoryMax=14G -p MemorySwapMax=0 nice -n 10 env MAXMEM=10G \
  ~/.local/opt/ghidra_12.1.4_PUBLIC/support/analyzeHeadless extract/ghidra NewWorld -process NewWorld.exe \
  -noanalysis -readOnly -scriptPath tools/re -postScript RangeDecompile.java <list.txt> <output.c>
```

4,139 functions in ~10 min, without memory pressure. `-readOnly` writes nothing to the project. Start the MCP server
again afterwards.

`tools/re/find_vcalls.py <output.c>` lists functions by virtual call pattern (see the script).

### Reflection and datasheets

- `xref.reflect_fields(start)`: fields of an AZ reflection function (name, offset). `xref.func_range(start)` joins
  the function's `.pdata` pieces.
- `xref.datasheet_columns(start)`: in a datasheet row parser, column → destination in the struct (and bit, for a
  flag).
- `tools/re/columns.py`: columns the `NewWorld.exe` datasheet parsers read → `re-local/colunas.json` (used by
  `tools/re_check.py`).
- `python3 -I tools/re/dis.py 0xSTART 0xEND`: disassembly with strings and floats resolved.
- CryEngine cvar pattern: `IConsole::Register(name, &var, default, …)`; the float default is the 4th argument
  (`xmm3`).

### CAGE and interface

- `python3 -I tools/re/cage_inventory.py [root]`: activity and condition types of the actionlists, with count, use
  by NPC/player and whether the name exists in the exe (conditions as `ActionCondition<name>`).
- Address of a CAGE class: `xref.py str <Activity>` or `xref.py str ActionCondition<Condition>`.
- `python3 -I tools/re/cage_classes.py > extract/ghidra/dec/cage_classes.tsv`: every CAGE class (and the ones used in
  the actionlists) with vtable and 24 slots.
- Interface scripts: `extract/_raw/names/lyshineui/**/*.luac` (Lua 5.1, 2 bytes before `\x1bLua`);
  `python3 tools/luac_dump.py <file> [filter]` disassembles.
- unluac (Lua 5.1 decompiler, Java): `~/.local/opt/unluac/unluac_2025_12_23.jar`, from
  `sourceforge.net/projects/unluac/files/Unstable/` (796,256 bytes, sha256 98be0fa8…5538fcc).
  `tools/re/luac_batch.sh [source] [destination]` decompiles everything into `extract/lua/lyshineui/` (same tree,
  `.lua`); already decompiled files are skipped; failures go to `falhas.txt`. The scripts have no debug info:
  locals come out as `L0_1`, but fields, calls and strings keep their real names.
- `python3 -I tools/re/fn.py 0xADDR [...] [-n lines] [--decl]`: shows the decompilation already saved in
  `extract/ghidra/dec/*.c` (index in `.index.json`, rebuilt when a `.c` changes). Leaf functions without a `.pdata`
  entry are not in the batches; for them, `tools/re/dis.py`.
- `python3 -I tools/re/lua_summary.py <file.lua> [filter]`: methods of a decompiled interface script, with bus calls
  (`XBus.Event.Method`), calls to the script itself, strings and numbers.

### Where results go (not in git)

- Decompilations: `extract/ghidra/dec/`.
- Answers: `re-local/`. It can be deleted: the agent asks again when needed.

## Method (the `nw-re` agent)

The agent answers one question given in its prompt and returns the answer in its final report. It only reads the
game; it does not change the port.

Before:

- Check the build: `tools/linux/game_version.sh --exe`. If it is another exe, stop and report.
- Read this page and the doc the question cites.
- `re-local/` is a cache of previous answers on this machine and may not exist. If it exists, look there before
  reversing (`simbolos.md`, `perguntas/`, `valores/`). Nothing in the repository depends on it.

Rules:

- Read-only on the game. Write only to `re-local/`, `extract/ghidra/` and `tools/re/`. Do not change `unreal/`,
  other `tools/`, docs; do not run the port, Unreal or New World. No official servers.
- If the MCP server is not up, start it with the command above (it already has a memory cap). A batch decompile
  stops the MCP server and runs through the queue; start the server again afterwards.
- nwbt only through the queue (`tools/linux/heavy.sh nwbt ...`), with as few calls as possible.
- Start from strings (`python3 -I tools/re/xref.py str "..."`), then `va`, `dis.py`, function-by-function
  decompilation. Datasheet parsers: `xref.datasheet_columns`.
- Every claim has an evidence grade: C (code read), R (reflection or data parser), H (hypothesis).
- If the answer is "it is server-side, not in the client", say so with the evidence (empty facet, string absent)
  and stop. Damage, critical, grit and mitigation are server-side.
- `tools/re/` gets only generic tools, with no fixed address and no results.

Output:

1. Cache in `re-local/` (created if missing): question and answer in `perguntas/<id>.md`, addresses in
   `simbolos.md`, numbers in `valores/<area>.json`
   (`{"valores": {"<area>.<name>": {"valor", "unidade", "prova", "o_que"}}}`).
2. A final report of up to 15 lines, which is what the asking session uses: answer (value, unit, formula or rule),
   grade C/R/H, and where the port should apply it. Addresses only in the cache, never in the report.

`tools/make_rules.py` can turn `re-local/valores/` into `Data/regras.json` (overrides the `FNWRules` defaults), and
`tools/re_check.py` reports what of the cache is already in the port. Both are optional: the port works without
`re-local/`.
