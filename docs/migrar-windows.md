# Migrar o port do Linux para o Windows

Para quem vai trocar o Linux pelo Windows na mesma máquina. O código está no GitHub (`main`); o HD leva o que não vai
para o git: o que a Unreal importou e o que foi extraído do jogo. Com isso, o Windows não precisa converter nada de
novo.

## O que vai no HD

| Arquivo | Conteúdo | Tamanho | Precisa? |
|---|---|---|---|
| `nwport-content.zip` | `unreal/NWPort/Content` e `unreal/NWPort/Data` (tudo que a Unreal importou e os dados gerados) | ~15 GB | sim; sem ele, refazer horas de import |
| `nwport-extract.zip` | `extract/` (assets convertidos, áudio, tabelas, projeto do Ghidra) | ~39 GB | só para converter coisa nova ou fazer engenharia reversa |
| `nwport-local.zip` | `deploy/.env` (senhas do servidor; fora do git) | KB | sim, se for subir o servidor |
| `nw-core-db.dump`, `deploy-db.dump` | contas e personagens de teste (nw-core) | MB | opcional |
| `New World/` + `appmanifest_1063730.acf` | instalação do jogo | ~72 GB | opcional; evita baixar de novo |
| `migrar-windows.md` | este roteiro | — | — |

Ficam de fora (o Windows refaz ou não usa): `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, `.nwbt/`,
`vendor/`, `backend/target`, `deploy/build`, `reference/`.

O HD precisa estar em **NTFS** ou **exFAT**. FAT32 não aceita arquivo acima de 4 GB. Espaço: ~60 GB sem o jogo, ~135 GB
com ele.

## Parte 1: no Linux

### 1. Antes

1. Feche a Unreal e o New World.
2. Tudo no GitHub: `git -C ~/workspace/nw-port status` deve mostrar "Your branch is up to date with 'origin/main'"
   e nada modificado. Sessões de esteira com trabalho aberto: peça o commit e o push antes.
3. Ligue o HD e veja o formato: `lsblk -f` (coluna FSTYPE: `ntfs` ou `exfat`). Se for `vfat` ou outro, formate pelo
   app Discos como NTFS. **Formatar apaga tudo do HD.**
4. Ache onde ele montou (`ls /media/$USER`) e crie a pasta:

```bash
HD=/media/$USER/NOME_DO_HD/nwport
mkdir -p "$HD"
```

### 2. Zipar

Cada `zip` mostra um ponto por GB processado. O de `extract/` leva de 30 a 60 minutos.

```bash
cd ~/workspace/nw-port
zip -r -1 -q -dg -ds 1g "$HD/nwport-content.zip" unreal/NWPort/Content unreal/NWPort/Data
zip -r -1 -q -dg -ds 1g "$HD/nwport-extract.zip" extract
zip -q "$HD/nwport-local.zip" deploy/.env
cp docs/migrar-windows.md "$HD/"
```

### 3. Banco de teste (opcional)

```bash
docker start nw-core-db
docker exec nw-core-db sh -c 'pg_dump -U "$POSTGRES_USER" -Fc "$POSTGRES_DB"' > "$HD/nw-core-db.dump"
docker exec deploy-db-1 sh -c 'pg_dump -U "$POSTGRES_USER" -Fc "$POSTGRES_DB"' > "$HD/deploy-db.dump"
```

### 4. Jogo (opcional)

```bash
cp -r "/mnt/jogos/Steam/steamapps/common/New World" "$HD/"
cp /mnt/jogos/Steam/steamapps/appmanifest_1063730.acf "$HD/"
```

### 5. Conferir e tirar o HD

```bash
cd "$HD"
unzip -tq nwport-content.zip
unzip -tq nwport-extract.zip
sha256sum $(ls | grep -E '\.(zip|dump)$') > SHA256SUMS.txt
cd ~ && sync
```

Os dois `unzip -tq` têm que dizer "No errors detected". Depois, ejete o HD pelo gerenciador de arquivos.

## Parte 2: no Windows

### 1. Instalar

- Git for Windows.
- Epic Games Launcher → Unreal Engine **5.8.3**.
- Visual Studio 2022 Community com as cargas "Desenvolvimento para desktop com C++" e "Desenvolvimento de jogos com
  C++" (traz o Windows SDK). Use a versão que a Epic indica para a 5.8.
- 7-Zip.
- Docker Desktop: só para subir o servidor e o nw-core.
- WSL2 com Ubuntu (`wsl --install -d Ubuntu`): só para converter coisa nova (o pipeline é Linux; skill `nw-wsl`).

### 2. Clonar

No PowerShell. O `autocrlf false` mantém os scripts com fim de linha do Linux (o WSL quebra com CRLF):

```powershell
git config --global core.autocrlf false
git clone https://github.com/paulorsouza/nw-port.git C:\nw-port
```

Pasta curta de propósito: o caminho mais longo do `Content` tem 170 caracteres e o Windows limita a 260.

### 3. Copiar do HD e conferir

Troque `E:` pela letra do HD.

```powershell
New-Item -ItemType Directory C:\nw-migracao
Copy-Item E:\nwport\*.zip, E:\nwport\*.dump, E:\nwport\SHA256SUMS.txt C:\nw-migracao\
Get-Content C:\nw-migracao\SHA256SUMS.txt | ForEach-Object {
    $h, $f = $_ -split '\s+', 2
    "{0}  {1}" -f ((Get-FileHash -Algorithm SHA256 "C:\nw-migracao\$f").Hash -eq $h), $f
}
```

Todas as linhas têm que começar com `True`.

### 4. Gravar os dados convertidos no projeto

```powershell
& "C:\Program Files\7-Zip\7z.exe" x C:\nw-migracao\nwport-content.zip -oC:\nw-port
& "C:\Program Files\7-Zip\7z.exe" x C:\nw-migracao\nwport-local.zip -oC:\nw-port
```

Confira que ficou `C:\nw-port\unreal\NWPort\Content\NW\Maps` (e não uma pasta a mais no meio).

Opcional: excluir `C:\nw-port` do antivírus do Windows (Segurança do Windows → Proteção contra vírus → Exclusões)
deixa a compilação e a primeira abertura bem mais rápidas.

### 5. Primeira abertura

1. Duplo clique em `C:\nw-port\unreal\NWPort\NWPort.uproject`. A Unreal avisa que faltam os módulos do NWPort e
   pergunta se compila: **Sim** (compila o C++ para Windows; alguns minutos).
2. Se a compilação falhar: clique direito no `.uproject` → "Generate Visual Studio project files", abra o
   `NWPort.sln` no Visual Studio e compile "Development Editor | Win64" para ver o erro.
3. Na primeira abertura a Unreal compila os shaders do Windows (pode passar de uma hora). É uma vez só.
4. Abra `/Game/NW/Maps/L_Arena` e `L_SouthWindsward` e dê Play.

### 6. Jogo (opcional)

Com a Steam fechada, copie `E:\nwport\New World` para `<biblioteca da Steam>\steamapps\common\` e o
`appmanifest_1063730.acf` para `<biblioteca>\steamapps\`. Abra a Steam: ela confere os arquivos em vez de baixar.
A build tem que continuar a de `tools/jogo.versao`. Se a Steam atualizar o jogo, os dados extraídos e os endereços
da engenharia reversa deixam de valer até o time atualizar junto.

### 7. `extract/` no WSL (só para converter coisa nova)

O pipeline roda no Ubuntu do WSL, com o repositório dentro do Linux (no `C:` fica lento demais):

```bash
git clone https://github.com/paulorsouza/nw-port.git ~/nw-port
cd ~/nw-port && unzip -q /mnt/c/nw-migracao/nwport-extract.zip
```

Ferramentas do pipeline: `docs/SETUP.md` (seção 2) e skill `nw-wsl`. O import do WSL para a Unreal do Windows ainda
não tem script (`ue_import_all.ps1` pendente, na skill `nw-wsl`).

### 8. Banco de teste (opcional)

```powershell
cd C:\nw-port\backend
docker compose up -d
docker cp C:\nw-migracao\nw-core-db.dump nw-core-db:/tmp/db.dump
docker exec nw-core-db sh -c 'pg_restore -U "$POSTGRES_USER" -d "$POSTGRES_DB" --clean --if-exists /tmp/db.dump'
```

O `deploy-db.dump` volta do mesmo jeito no container `deploy-db-1`, depois de `docker compose up -d` em
`C:\nw-port\deploy`.

## Fora deste roteiro

Este roteiro cobre só o port. Antes de formatar o Linux, salve também o que só existe nesta máquina: outros
projetos em `~/workspace`, chaves em `~/.ssh`, as memórias do Claude Code em `~/.claude` e bancos Docker de outros
projetos.
