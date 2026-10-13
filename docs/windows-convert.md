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
- [x] Orquestrador de extracao portado (validado com fixtures e --dry-run).
- [ ] Importacao Unreal Windows.

Os scripts Bash continuam disponiveis para o fluxo Linux existente. build_all.py agora cobre as etapas
de extracao, incluindo sistemas-01/02/03. ue_import_all.sh ainda precisa de uma entrada Windows nativa.

## Segunda entrega: trabalho sem os assets

- [x] tools/jogo_versao.py: manifesto Steam e hash SHA256 sem awk/sha256sum; aceita NW_GAME,
  --game, --exe e NW_IGNORA_VERSAO. Sem manifesto, exige o hash do executavel.
- [x] tools/make_data.py: sequencia de geracao de Data/, incluindo etapas opcionais e telas;
  usa o mesmo Python que iniciou o comando, diretorio do repositorio e argumentos sem shell.
- [x] --dry-run lista as etapas sem iniciar subprocessos nem escrever Data/.
- [x] Testes com arquivos sinteticos: bloqueio de build/hash incorretos, hash obrigatorio sem
  manifesto, codigo de saida e override explicito; caminhos com espacos/acentos, cwd correto e
  interrupcao na primeira falha do gerador.

```powershell
python -m unittest discover -s tools/tests -v
python tools/make_data.py --dry-run
# Quando os assets estiverem disponiveis:
python tools/jogo_versao.py --exe
python tools/make_data.py
```

Os quatro testes passaram em Windows com Python 3.13; a sequencia real do repositorio tambem foi
conferida com --dry-run. O gerador completo nao foi executado sem os arquivos de entrada.
O verificador usa hashlib.file_digest, disponivel no Python 3.11 ou mais recente.

Ainda podemos migrar e testar sem o jogo: fila/lock de trabalho, retomada e marcadores de etapas,
monitoramento de memoria/disco, encerramento de filhos, comandos Unreal e launcher local usando
processos de teste. Fixtures conhecidas podem exercitar mais caminhos dos conversores.
Esses testes validam o comportamento dos scripts, mas nao garantem fidelidade dos assets.

O aceite que precisa de outra maquina fica delimitado: descompactar os paks da build do time,
comparar JSON/texturas/malhas/animacoes/audio, montar o projeto na Unreal, verificar renderizacao
e colisao, jogar com persistencia/rede e medir memoria/tempo no volume real de dados.

## Terceira entrega: fila, supervisao e extracao

- [x] nwprocess.py: memoria fisica disponivel e espaco em disco sem ferramentas Linux externas.
- [x] pesado.py: lock de trabalho pesado, compartilhado entre processos e herdado por filhos.
- [x] Windows Job Objects: toda a arvore do trabalho e encerrada ao falhar/cancelar. Um wrapper
  aguarda a atribuicao ao Job antes de iniciar o comando, evitando filhos fora da supervisao.
- [x] Steps: etapas concluídas sao puladas; uma etapa forcada que falha perde o marcador e fica
  pendente. --dry-run nao cria locks, logs, caches ou marcadores.
- [x] build_all.py: 14 etapas principais e tres de Queda Eterna, com logs por etapa, versao do jogo,
  RTTI, validacao de processos abertos, extracao e conversao. Esteiras Bash futuras desconhecidas
  provocam erro explicito, para nao sumirem do fluxo Windows.
- [x] convert_city.py, convert_gatherables.py e render_ambient.py substituem os auxiliares Bash
  correspondentes. convert_npcs.py usa ferramentas nativas e supervisao.
- [x] Manifesto CSV do nwbt preserva nomes com espacos. GLBs truncados nao sao reutilizados;
  saídas limpas/terreno/audio sao promovidas depois de sucesso.
- [x] Patch incremental nwbt-platform-path.patch corrige AppendToPATH: separador do sistema e
  preservacao das entradas existentes. O build aplica ambos os patches e aceita o patch anterior
  ja instalado, permitindo atualizar um checkout usado pela primeira entrega.

```powershell
python tools/build_all.py --dry-run
python tools/build_all.py 07 --dry-run
python tools/convert_city.py settlement_southwindsward_complete --dry-run
python tools/convert_gatherables.py tools/arena/meshes.txt extract/ue/arena --dry-run
python tools/pesado.py python -c "print('fila nativa')"
python -m unittest discover -s tools/tests -v
```

NW_BUDDY permite indicar o checkout das tabelas RTTI quando ele nao esta em vendor/nw-buddy.
Depois de configurar as dependencias e NW_GAME, a entrada de extracao e:

```powershell
python tools/build_all.py
python tools/build_all.py 07           # refaz uma etapa, mesmo se ja tinha marcador
python tools/build_all.py sistemas-02
```

Os limites de memoria conservadores do fluxo original continuam: 10 GiB durante trabalhos pesados;
cidade espera 12 GiB e interrompe abaixo de 8 GiB; coletaveis usam lotes de oito com dois workers,
exigem 16000 MiB antes do lote e interrompem abaixo de 10000 MiB. Conversores conferem 30 GiB
livres nos discos envolvidos. Windows usa prioridade de CPU abaixo do normal; Linux usa nice 19.

Validacao nesta maquina: 18 testes Python passaram, incluindo concorrencia real entre processos,
timeout, falta de memoria, encerramento de neto sem afetar processo independente, retomada,
modo simulado sem escrita, regioes do terreno, manifesto CSV e limpeza real de GLB sintetico.
O pipeline de audio foi testado com programas sinteticos de decoder/encoder, inclusive falha;
isso nao valida codecs reais. Quatro testes Go passaram e os binarios foram recompilados com
os dois patches. A extracao de assets reais e a importacao Unreal seguem pendentes.

O workflow `.github/workflows/native-tools.yml` repete os testes com Python 3.13 em Windows e Linux,
sem jogo, Unreal ou acesso ao fork privado. No Windows tambem compila ooz.dll com MSVC e testa a
ABI, um bloco sem compressao, cabecalho invalido e limites do buffer. No Linux esse teste de DLL e
pulado. O workflow roda nos pushes de windows-convert e nos PRs que alteram as ferramentas.

Algumas falhas antes toleradas agora deixam a etapa pendente: capital ambiguo, heightmap ausente,
conversao de camada com erro de processo, GLB de capitals ausente e malhas com nomes de saida
iguais. Esses casos ficam explicitos nos logs, evitando considerar concluida uma geracao incompleta.

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

O roteiro para o desenvolvedor esta em [setup-windows-test.md](../setup-windows-test.md).
`tools/windows/check_migration.py` confere caches e referencias do checkout copiado do WSL;
`--apply` corrige caminhos e bancos de audio com backup e invalida somente marcadores afetados.
A conferencia e estrutural e nao substitui o aceite com dados reais e Unreal. O orquestrador
de importacao e o launcher Windows continuam pendentes.

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
