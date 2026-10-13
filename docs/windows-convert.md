# Windows nativo: plano e andamento

Branch: `windows-convert`. Base do nw-port: `eaa1a340277ab44b0785bcde3b45865e7af61422`.
Objetivo: extrair, converter, importar, compilar e jogar no Windows sem WSL no pipeline.
Docker continua permitido para Postgres e deploy. O servidor de producao Linux permanece como esta.
Estimativa inicial: 6–11 dias uteis, a revisar depois de extrair assets reais no Windows.

## Plano

1. **Ferramentas nativas:** compilar ooz.dll, nwbt.exe e dbgwater.exe; corrigir carregamento de DLL e
   resolucao de RTTI JSON; disponibilizar vgmstream-cli e oggenc Windows com versoes registradas.
2. **Configuracao compartilhada:** padronizar NW_GAME, NW_UE, caminhos de ferramentas e subprocessos;
   remover dependencias Linux dos scripts Python; conferir caminhos com espacos e Unicode.
3. **Orquestracao:** portar build_all, convert_city, convert_gatherables, render_ambient, make_data e
   jogo_versao para Python; preservar etapas retomaveis, logs, filas, limites de memoria/disco e
   encerramento de processos filhos. Incluir as etapas extras de tools/etapas/.
4. **Unreal:** portar ue_import_all e check para Build.bat/Win64 e UnrealEditor-Cmd.exe; preservar
   validacao por log de commandlet e conferir mapas, materiais, animacoes e audio.
5. **Execucao local:** portar play_local e net_test mantendo Docker para o banco; dar suporte a
   backend Rust nativo, UnrealEditor -server e cliente. Empacotamento Win64 dedicado vem depois,
   caso necessario; nao e requisito para manter o deploy Linux.
6. **Aceite:** montar do zero, interromper/retomar etapas, conferir versao do jogo, importar todos os
   assets, jogar com conta e persistencia e executar teste de rede. Atualizar SETUP e FERRAMENTAS.

## Primeira entrega

- [x] Shim C++ compartilhado MSVC/GCC e exportacao de OodleLZ_Decompress para Windows.
- [x] Script reprodutivel de build ooz.dll x64 com MSVC.
- [x] Patch do nwbt: NWBT_OODLE e ooz.dll no Windows; JSON RTTI sem exec.LookPath/PATHEXT.
- [x] Build nwbt.exe/dbgwater.exe com dependencia fixada e patch reaplicavel.
- [x] Configuracao Python compartilhada; build_set, make_sound_map e extract_audio adaptados.
- [x] Audio no Windows sem exigir privilegios de symlink.
- [x] Diagnostico das ferramentas Windows.
- [ ] Extracao real de JSON, textura, audio, malha e animacao no Windows.
- [ ] Orquestrador completo e importacao Unreal Windows.

Os scripts Bash continuam sendo o fluxo completo existente. Esta primeira entrega prepara os binarios
e os scripts Python; ainda nao substitui build_all.sh nem ue_import_all.sh no Windows.

## Compilar e conferir a primeira entrega

Pre-requisitos: Python, Git, Go e Visual Studio com C++/SDK Windows. No Developer PowerShell **x64**:

```powershell
./tools/windows/build_ooz.ps1
./tools/windows/build_nwbt.ps1 -Test
$env:NW_GAME = 'D:\SteamLibrary\steamapps\common\New World'
$env:NW_UE = 'D:\UnrealEngine\5.8'
$env:NW_BLENDER = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
python tools/windows/doctor.py --full
```

build_nwbt.ps1 clona o fork privado em vendor/nw-buddy se necessario e fixa o commit
`267f86568977c5801b716f02684f7bc69e0982d0`. O patch incremental esta em
`patches/nwbt-windows.patch`, aplicado sobre esse commit da branch nw-port (que ja inclui o patch Linux).
Um checkout existente em outro commit provoca erro; o script nao reseta alteracoes locais.
Use -Source e -GoExe para escolher um checkout e um executavel Go existentes.

Os binarios Windows gerados ficam fora do Git e podem ser recompilados pelos scripts.
texconv.exe ja esta em bin/ e e chamado diretamente no Windows, sem wine.
NW_GAME prevalece sobre GAME, que continua aceito para compatibilidade. NWBT_OODLE permite indicar
uma DLL explicitamente; no Windows, uma configuracao inexistente falha sem fallback silencioso.

## Validacao

MSVC x64: ooz.dll compilou; nwbt.exe e dbgwater.exe tambem compilaram nativamente. O script de build
foi exercitado em checkout limpo e repetido com o patch ja aplicado; ambos passaram. texconv.exe
executou diretamente no Windows. Os scripts Python alterados passaram em compilacao e --help;
a configuracao foi conferida com NW_GAME, GAME, DLL customizada e valores padrao Linux/Windows.
Testes Go nativos passaram para resolucao de JSON com espacos, DLL
explicitamente configurada, bloco Kraken sem compressao e rejeicao de cabecalho invalido.
O teste de bloco valida carregamento, ABI e copia do resultado; ainda nao valida os blocos comprimidos
dos paks reais. O aceite de extracao completa depende de uma instalacao do jogo e da Unreal.

O diagnostico desta maquina encontrou os quatro binarios principais, mas nao encontrou o jogo no
caminho padrao nem as dependencias completas de audio/Blender/Unreal. Nenhuma extracao real foi executada.
