# Teste nativo no Windows e migração do WSL

Este roteiro testa a extração nativa e o reaproveitamento dos arquivos gerados no WSL na branch `windows-convert`. Docker pode continuar sendo usado pelo backend. A extração foi validada com testes sintéticos; o aceite com paks reais e Unreal ainda precisa de uma máquina com esses dados. O orquestrador de importação Unreal e o launcher ainda precisam de port para Windows.

## 1. Preparar a máquina

Use Windows x64, Python 3.11 ou superior de [python.org](https://www.python.org/downloads/windows/) (CI usa 3.13), Git, Go no `PATH` e Visual Studio com C++ e Windows SDK. Não use o Python da Microsoft Store: ele roda empacotado e os processos filhos (nwbt, Blender) saem do Job do Windows, sobrevivendo a um aborto; o pipeline e o `doctor.py` recusam esse Python. Abra o **Developer PowerShell x64** para compilar. Consulte [SETUP](docs/SETUP.md) para as versões de Blender e Unreal usadas pelo projeto e o dimensionamento da máquina; use a mesma versão da Unreal que gerou o `Content` antigo.

```powershell
git clone --branch windows-convert https://github.com/paulorsouza/nw-port.git C:\dev\nw-port
Set-Location C:\dev\nw-port
python --version
go version
git rev-parse HEAD
```

Se já existe um checkout, preserve alterações locais antes de mudar de branch. Execute os comandos seguintes na raiz do checkout Windows.

```powershell
$env:NW_GAME = 'C:\Program Files (x86)\Steam\steamapps\common\New World'
$env:NW_UE = 'C:\Epic Games\UE_5.8'
$env:NW_BLENDER = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
$env:PYTHONUTF8 = '1'
```

Adapte os caminhos à instalação real. Essas variáveis valem para a sessão PowerShell atual. Não reutilize `NWBT_OODLE` apontando para uma biblioteca Linux; o padrão Windows é `bin/ooz.dll`.

## 2. Compilar e instalar as ferramentas

```powershell
./tools/windows/build_ooz.ps1
./tools/windows/build_nwbt.ps1 -Test
python -m pip install numpy Pillow
git clone https://github.com/bnnm/wwiser.git vendor/wwiser
git -C vendor/wwiser checkout 08e937b
```

Pare se algum comando falhar. O build de nwbt requer acesso ao fork privado `paulorsouza/nw-buddy`; o script (que chama `tools/build_nwbt.py`) fixa o commit, aplica `patches/nwbt.patch` e grava a versão no `nwbt.exe`; o `doctor.py` acusa binário de outro patch. Para um checkout existente, use `-Source`; para um Go fora do PATH, use `-GoExe`. `texconv.exe` já está em `bin/`.

Para áudio, obtenha a distribuição Windows de [vgmstream](https://github.com/vgmstream/vgmstream/releases) e [oggenc2](https://www.rarewares.org/ogg-oggenc.php). Preserve as DLLs distribuídas junto dos executáveis e configure:

```powershell
$env:NW_VGMSTREAM_CLI = 'C:\tools\vgmstream\vgmstream-cli.exe'
$env:NW_OGGENC = 'C:\tools\oggenc\oggenc2.exe'
python tools/windows/doctor.py
python tools/windows/doctor.py --full
```

O diagnóstico básico confere jogo, nwbt, texconv e ooz. O completo acrescenta Blender, áudio, wwiser e Unreal. Cada `FALTA` deve ser resolvido antes da etapa que usa essa dependência. Os pacotes externos de áudio ainda precisam de validação com arquivos reais; registre as versões usadas.

## 3. Copiar os resultados antigos, se houver

Preserve o checkout WSL original. Copie para os mesmos caminhos relativos no checkout Windows:

- `extract/`, incluindo `.etapas`, manifests, fontes de áudio e assets convertidos;
- `unreal/NWPort/Content/` e `unreal/NWPort/Data/`.

Copie somente esses diretórios gerados. `Binaries` e `Intermediate` precisam de recompilação nativa. Não substitua o código da branch por uma cópia antiga. A transferência inicial pode usar o compartilhamento `\\wsl.localhost\Ubuntu\home\dev\workspace\nw-port`; depois dela, os comandos nativos não executam WSL ou Bash. Verifique se links simbólicos foram copiados: bancos de áudio ausentes serão recuperados pelo check quando o banco original estiver disponível.

Confirme que a extração antiga corresponde à versão registrada em `tools/jogo.versao`; o check de migração não consegue provar a origem dos caches. Confira a instalação atual:

```powershell
python tools/jogo_versao.py --exe
```

O comando deve terminar com código 0. Não use `NW_IGNORA_VERSAO` para o aceite. Se o jogo foi atualizado, resolva a compatibilidade antes de reutilizar os dados.

## 4. Conferir e corrigir a migração

O primeiro comando não altera os assets. `--report` grava somente o relatório solicitado:

```powershell
python tools/windows/check_migration.py --old-root '/home/dev/workspace/nw-port' --report extract/windows-migration-before.json
$LASTEXITCODE
```

Use o caminho exato do checkout antigo; repita `--old-root` se os arquivos vieram de mais de um checkout. Sem essa opção, a ferramenta tenta reconhecer raízes antigas pelos diretórios conhecidos, mas não resolve candidatos ambíguos. `--root` permite conferir outro checkout.

Leia `issues`, `changes` e `rerun` no JSON. Depois aplique:

```powershell
python tools/windows/check_migration.py --old-root '/home/dev/workspace/nw-port' --apply --report extract/windows-migration-after.json
$LASTEXITCODE
```

O check ajusta referências de arquivos em manifests, materiais e TXTP quando encontra o destino dentro do checkout. Preserva referências Unreal `/Game/` e `/Engine/`, copia bancos locais para substituir links de áudio e remove somente marcadores `.ok` cujas saídas conferidas estão ausentes ou inválidas. Não reconverte FBX, texturas ou áudio apenas por uma mudança de caminho.

Antes de alterar metadados ou remover marcadores, salva os originais em `extract/.migration-backups/<data-id>/files/`, com um `plan.json`. O caminho do backup aparece no terminal e no relatório. Para recuperar um arquivo regular, copie a versão de `files/` para o mesmo caminho relativo no checkout. Não restaure um marcador antes de corrigir a saída correspondente; restaurar um link antigo pode reintroduzir a dependência do WSL.

| Código | Interpretação |
|---|---|
| 0 | Nenhuma correção ou erro restante no escopo conferido. |
| 2 | Há correções propostas, erros ou etapas a refazer; também pode ocorrer depois de aplicar com sucesso, quando ainda há reprocessamento necessário. |
| 4 | Falha de leitura/aplicação; pare e consulte o erro e o backup, se criado. |

Argumentos inválidos também retornam 2, acompanhados de mensagem de uso. Um segundo check pode retornar 0 depois que os marcadores inválidos foram removidos: isso não significa que a extração foi concluída. O pipeline retomará as etapas sem `.ok`.

O check verifica referências e saídas representativas das etapas; para listas de meshes, confere os GLBs listados. A integridade estrutural de GLB não garante aparência, animação ou colisão correta. Não altera metadados internos de `.uasset`, FBX ou `.blend`; referências antigas usadas pelo reimport da Unreal podem exigir ajuste no editor.

## 5. Validar sem dados e com dados reais

```powershell
python -m unittest discover -s tools/tests -v
python tools/build_all.py --dry-run
```

Os testes usam fixtures temporárias e não precisam do jogo. O teste de DLL requer `bin/ooz.dll` no Windows; a fixture de link simbólico pode ser ignorada se a máquina não permitir criá-la. Consulte o motivo de qualquer teste ignorado.

Com ferramentas, versão e arquivos reais prontos, execute uma indexação explícita para testar nwbt e ooz nos paks, mesmo se houver um marcador antigo:

```powershell
python tools/build_all.py 01
```

Uma etapa informada explicitamente é refeita. Sem argumento, o pipeline retoma as etapas pendentes:

```powershell
python tools/build_all.py
```

Não apague todos os `.ok`. Use as indicações `REFAZER` do relatório para diagnosticar uma etapa específica. Arquivos ausentes ainda precisam ser copiados da origem ou gerados novamente; `--apply` não substitui essa geração. Logs do pipeline ficam em `extract/logs/<etapa>.log`; conversores também mantêm logs próprios nas pastas de saída.

Durante a extração, feche o jogo e o editor. Mantenha os limites de memória e disco do pipeline. Em máquinas com pouca folga (32 GB com outros programas abertos), estas variáveis opcionais ajustam o consumo; sem elas, o comportamento padrão não muda:

| Variável | Efeito | Valor testado |
|---|---|---|
| `NW_MIN_MEM_GIB` | teto para os mínimos de memória exigidos pelos scripts (`0` desliga a trava) | `4` na extração, `0` na importação Unreal |
| `NW_AUDIO_JOBS` | processos wwiser/vgmstream em paralelo na etapa 05 (~1,2 GiB cada) | `6` |
| `NW_NWBT_WORKERS` | workers de `nwbt models` sem `-w` (cidade, personagens, NPCs) | `3` |
| `GOMEMLIMIT` | limite de memória do Go no nwbt; a cidade da etapa 07 cai de ~13 GB para ~7 GB | `7GiB` | Se houver interrupção ou falha, a etapa não deve receber `.ok`; corrija a causa e retome. Para uma instalação nova, sem caches, todas as etapas serão executadas.

## 6. Testar o projeto Unreal reaproveitado

Com `Content` existente e a mesma versão da engine, compile o editor nativamente:

```powershell
$uproject = (Resolve-Path 'unreal/NWPort/NWPort.uproject').Path
& "$env:NW_UE/Engine/Build/BatchFiles/Build.bat" NWPortEditor Win64 Development "-Project=$uproject" -WaitMutex -MaxParallelActions=6
```

Somente após código 0, abra:

```powershell
& "$env:NW_UE/Engine/Binaries/Win64/UnrealEditor.exe" $uproject '/Game/NW/Maps/L_SouthWindsward'
```

Confira no editor materiais, terreno, colisões, animações e áudio; compare com o projeto WSL. Verifique também o reimport de um asset cujo caminho mudou.

Para `Content` novo, use o orquestrador nativo (equivale ao `ue_import_all.sh`; não execute o `.sh` em PowerShell):

```powershell
$env:NW_UE = 'C:\UnrealEngine'
python tools/ue_import_all.py --dry-run
python tools/ue_import_all.py
```

Antes, gere as entradas que o `build_all.py` ainda não produz: `build_set.py tools/sets/boar.json` e `wolf.json` (etapa ue_12); os `.cdf` dos NPCs com `bin/nwbt.exe unpack -g $env:NW_GAME -o extract/npcs -e -w 2 'objects/characters/npc/.*\.cdf$'`, depois `make_npcs.py`, `build_set.py tools/sets/npc_queda_eterna.json` e `convert_npcs.py` (etapa ue_sistemas-03). O cenário (ue_07) e os materiais (ue_08) chegam a ~15,5 GB no editor; com 32 GB de RAM, rode a importação com `NW_MIN_MEM_GIB=0`.

## 7. Registrar o resultado

Entregue ao responsável pelo port o commit testado, versões das ferramentas/engine, versão do jogo, relatórios antes/depois e logs das etapas que falharam. Registre separadamente: testes sintéticos, extração com paks reais, abertura do mapa e comportamento em Play. Não inclua assets do jogo no Git.

O reaproveitamento é aceito quando os arquivos existentes são encontrados no Windows, somente as etapas necessárias são refeitas e o projeto recompilado abre os assets corretamente. A execução completa sem WSL ainda depende de concluir e validar a importação/launcher nativos.
