<#
.SYNOPSIS
Builds the Windows dev demo: client (Windsward, Everfall, Arena; F10 for admin accounts), three native game servers
(one per map) and Postgres + nw-core in Docker Desktop. docs/tech/windows.md, "Demo build".

.DESCRIPTION
Run from any folder, in PowerShell, with the editor and the game closed:

    .\tools\windows\build_demo.ps1 -HostAddress 192.168.0.10 -Admin paulo

Steps (each one can be run again; compiling and cooking are incremental):
  1. Checks: engine built from source (NW_UE_SOURCE), installed engine (NW_UE), Docker Desktop running, editor closed,
     and the imported project has what the demo needs (looks, Windsward NPCs, new icons).
  2. Data/: python tools/make_data.py. With -Import, first the missing extraction and import steps
     (tools/build_all.py, tools/ue_import_all.py).
  3. Compiles NWPort and NWPortServer (Win64 Development) with the source engine.
  4. Cooks Windows + WindowsServer (installed editor, as tools/linux/package_server.sh) and stages/archives with the
     source engine's RunUAT into deploy\build\Windows and deploy\build\WindowsServer.
  5. deploy\.env (secrets generated when missing, never printed), nw-core image, docker compose up (db + core).
  6. Admin accounts (-Admin) and invites (-Invites).
  7. deploy\build\Servidores.bat, deploy\build\Jogar.bat, deploy\build\Windows\Jogar.bat and the client zip.

.PARAMETER HostAddress
Address the players use to reach this PC (game servers and nw-core). Default: this PC's IPv4 on the network with the
default gateway.

.PARAMETER BackendUrl
nw-core URL for the clients. Default: http://<HostAddress>:8090.

.PARAMETER Admin
Accounts that get the F10 menu (nw-core admin). An account that does not exist yet gets an invite to create it.

.PARAMETER Invites
Number of account invites to print (7 days).

.PARAMETER CookWith
Installed (default): cooks with the installed editor (NW_UE). Source: BuildCookRun cooks with the source engine.

.PARAMETER Import
Runs the missing extraction and Unreal import steps before building (hours on a project that was never imported;
minutes when only the new steps are missing). Without it, a project brought by the asset zips is used as it is.

.PARAMETER OpenFirewall
Creates the Windows firewall rules for the servers (UDP 7777-7779) and nw-core (TCP 8090). Needs PowerShell as
administrator.
#>
[CmdletBinding()]
param(
    [string]$HostAddress,
    [string]$BackendUrl,
    [string[]]$Admin = @(),
    [int]$Invites = 0,
    [ValidateSet('Installed', 'Source')]
    [string]$CookWith = 'Installed',
    [switch]$SkipData,
    [switch]$SkipCompile,
    [switch]$SkipCook,
    [switch]$SkipPackage,
    [switch]$SkipDocker,
    [switch]$SkipZip,
    [switch]$Import,
    [switch]$OpenFirewall
)
$ErrorActionPreference = 'Stop'
# powershell -File passes "-Admin a,b" as one string.
$Admin = @($Admin | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$uproject = Join-Path $root 'unreal\NWPort\NWPort.uproject'
$out = Join-Path $root 'deploy\build'
$envFile = Join-Path $root 'deploy\.env'
$composeFile = Join-Path $root 'deploy\compose.windows.yaml'
$logs = Join-Path $root 'unreal\NWPort\Saved\DemoLogs'
$python = if ($env:NW_PYTHON) { $env:NW_PYTHON } else { 'python' }
$maps = @(
    @{ Map = 'L_SouthWindsward'; Port = 7777; Name = 'Windsward' },
    @{ Map = 'L_QuedaEterna'; Port = 7778; Name = 'Everfall' },
    @{ Map = 'L_Arena'; Port = 7779; Name = 'Arena' }
)
$cookMaps = ($maps | ForEach-Object { '/Game/NW/Maps/' + $_.Map }) -join '+'

function Step([string]$Text) { Write-Host ''; Write-Host "== $Text" -ForegroundColor Cyan }
function Fail([string]$Text) { Write-Host "ERROR: $Text" -ForegroundColor Red; exit 1 }

# Heavy jobs go through the shared queue (docs/workflow.md): python tools/heavy.py <command>.
function Invoke-Heavy([string]$Exe, [string[]]$Arguments, [string]$What) {
    Write-Host "   $What"
    & $python (Join-Path $root 'tools\heavy.py') $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { Fail "$What failed (exit code $LASTEXITCODE)" }
}

# Prints what docker prints and returns only the exit code.
function Compose([string[]]$Arguments) {
    & docker compose -p nw-demo --env-file $envFile -f $composeFile @Arguments | Out-Host
    return $LASTEXITCODE
}

function Write-Text([string]$Path, [string[]]$Lines, [string]$NewLine) {
    $enc = New-Object Text.UTF8Encoding $false
    [IO.File]::WriteAllText($Path, (($Lines -join $NewLine) + $NewLine), $enc)
}

function New-Secret {
    $bytes = New-Object byte[] 32
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    $rng.GetBytes($bytes)
    $rng.Dispose()
    return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '')
}

# ---------------------------------------------------------------- 1. checks
Step 'Checks'
$found = & $python -c "import os, sys; sys.path.insert(0, sys.argv[1]); import nwplatform; print(nwplatform.ue_directory() or ''); print(os.environ.get('NW_UE_SOURCE', ''))" (Join-Path $root 'tools')
if ($LASTEXITCODE -ne 0) { Fail "Python did not run ($python). Install Python from python.org or set NW_PYTHON." }
$installed = "$($found[0])".Trim()
$source = "$($found[1])".Trim()
if (-not $source) { Fail 'Set NW_UE_SOURCE (Unreal 5.8.3 built from source) in local.env at the repository root.' }
if (-not (Test-Path (Join-Path $source 'Engine\Build\BatchFiles\RunUAT.bat'))) { Fail "NW_UE_SOURCE has no Engine\Build\BatchFiles\RunUAT.bat: $source" }
if (Test-Path (Join-Path $source 'Engine\Build\InstalledBuild.txt')) { Fail "NW_UE_SOURCE is an installed engine, not built from source: $source" }
Write-Host "   source engine: $source"
if ($CookWith -eq 'Installed' -and -not $SkipCook) {
    if (-not $installed -or -not (Test-Path (Join-Path $installed 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'))) {
        Fail 'Installed Unreal 5.8.3 not found: set NW_UE in local.env (or use -CookWith Source).'
    }
    Write-Host "   installed engine (cook): $installed"
}
$running = Get-Process -Name 'UnrealEditor', 'UnrealEditor-Cmd', 'NWPort', 'NWPortServer' -ErrorAction SilentlyContinue
if ($running) { Fail "Close these first: $(($running | Select-Object -ExpandProperty ProcessName -Unique) -join ', ') (editor, demo client or servers)." }
if (-not $SkipDocker) {
    if (-not (Get-Command docker -ErrorAction SilentlyContinue)) { Fail 'docker not found: install Docker Desktop.' }
    # PowerShell 5.1 turns redirected stderr of a native command into errors; 'Stop' would abort here.
    $ErrorActionPreference = 'Continue'
    & docker info --format '{{.ServerVersion}}' *> $null
    $dockerCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($dockerCode -ne 0) { Fail 'Docker Desktop is not running. Start it and wait for "Engine running".' }
    Write-Host '   Docker Desktop: running'
}
if (-not $HostAddress) {
    $HostAddress = Get-NetIPConfiguration |
        Where-Object { $_.IPv4DefaultGateway -and $_.NetAdapter.Status -eq 'Up' } |
        ForEach-Object { $_.IPv4Address.IPAddress } | Select-Object -First 1
    if (-not $HostAddress) { Fail 'Could not find this PC''s IPv4. Pass -HostAddress <IP or name>.' }
}
if (-not $BackendUrl) { $BackendUrl = "http://${HostAddress}:8090" }
if ($OpenFirewall) {
    $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $admin) { Fail '-OpenFirewall needs PowerShell opened as administrator.' }
    foreach ($rule in @(@{ Name = 'NW Exiles game servers'; Protocol = 'UDP'; Port = '7777-7779' }, @{ Name = 'NW Exiles nw-core'; Protocol = 'TCP'; Port = '8090' })) {
        if (-not (Get-NetFirewallRule -DisplayName $rule.Name -ErrorAction SilentlyContinue)) {
            New-NetFirewallRule -DisplayName $rule.Name -Direction Inbound -Action Allow -Protocol $rule.Protocol -LocalPort $rule.Port | Out-Null
        }
        Write-Host "   firewall: $($rule.Name) ($($rule.Protocol) $($rule.Port))"
    }
}
$BackendUrl = $BackendUrl.TrimEnd('/')
$mapServers = ($maps | ForEach-Object { "$($_.Map)=${HostAddress}:$($_.Port)" }) -join ','
Write-Host "   players reach: game $HostAddress (UDP 7777-7779), nw-core $BackendUrl"
New-Item -ItemType Directory -Force $logs, $out | Out-Null

# ---------------------------------------------------------------- 2. extraction, import, Data/
if ($Import) {
    Step 'Missing extraction steps (tools/build_all.py)'
    & $python (Join-Path $root 'tools\build_all.py')
    if ($LASTEXITCODE -ne 0) { Fail "tools/build_all.py failed (exit code $LASTEXITCODE)" }
    Step 'Missing Unreal import steps (tools/ue_import_all.py)'
    & $python (Join-Path $root 'tools\ue_import_all.py')
    if ($LASTEXITCODE -ne 0) { Fail "tools/ue_import_all.py failed (exit code $LASTEXITCODE)" }
}
if (-not $SkipData) {
    Step 'Data/ (tools/make_data.py)'
    & $python (Join-Path $root 'tools\make_data.py')
    if ($LASTEXITCODE -ne 0) { Fail "tools/make_data.py failed (exit code $LASTEXITCODE)" }
}
if (-not (Test-Path (Join-Path $root 'unreal\NWPort\Data\UI\nw_strings.json'))) { Fail 'unreal\NWPort\Data is not generated: run python tools/make_data.py.' }
# What the demo needs from the imported project (Content is not in git: each machine imports or restores the zips).
$content = Join-Path $root 'unreal\NWPort\Content\NW'
$needed = [ordered]@{
    'Maps\L_SouthWindsward.umap' = 'Windsward map (import step 04)'
    'Maps\L_QuedaEterna.umap' = 'Everfall map (import step sistemas-01)'
    'Maps\L_Arena.umap' = 'arena (import step 03)'
    'NpcGuardaventos' = 'Windsward NPCs (import step sistemas-06)'
    'Appearances\Armor' = 'equipment looks (import step appearances-01)'
    'UI\lyshineui\images\equipment\T_iconquickslot.uasset' = 'quick slot and bag icons (import step 09)'
}
$missing = @($needed.Keys | Where-Object { -not (Test-Path (Join-Path $content $_)) })
if ($missing) {
    foreach ($m in $missing) { Write-Host "   missing Content\NW\$m : $($needed[$m])" -ForegroundColor Yellow }
    Fail 'The imported project is missing what the demo needs. Run again with -Import (or restore up-to-date asset zips: tools/assets_import.py).'
}
Write-Host '   imported project: complete for the demo'

# ---------------------------------------------------------------- 3. compile
$sourceBuild = Join-Path $source 'Engine\Build\BatchFiles\Build.bat'
if (-not $SkipCompile) {
    Step 'Compile (source engine, Win64 Development)'
    foreach ($target in 'NWPort', 'NWPortServer') {
        Invoke-Heavy $sourceBuild @($target, 'Win64', 'Development', "-Project=$uproject", '-WaitMutex', '-MaxParallelActions=6') "$target Win64 Development"
    }
}

# ---------------------------------------------------------------- 4. cook, stage, archive
$uat = Join-Path $source 'Engine\Build\BatchFiles\RunUAT.bat'
$common = @("-project=$uproject", '-platform=Win64', "-map=$cookMaps", '-skipbuild', '-stage', '-pak', '-archive',
    "-archivedirectory=$out", '-unattended', '-nop4', '-utf8output')
$cookFlag = '-skipcook'
if ($CookWith -eq 'Installed') {
    if (-not $SkipCook) {
        Step 'Cook Windows + WindowsServer (installed editor)'
        $installedBuild = Join-Path $installed 'Engine\Build\BatchFiles\Build.bat'
        Invoke-Heavy $installedBuild @('NWPortEditor', 'Win64', 'Development', "-Project=$uproject", '-WaitMutex', '-MaxParallelActions=6') 'NWPortEditor (installed engine)'
        $cookLog = Join-Path $logs 'cook.log'
        $editor = Join-Path $installed 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
        Write-Host '   cooking (long the first time)'
        & $python (Join-Path $root 'tools\heavy.py') $editor $uproject '-run=cook' '-targetplatform=Windows+WindowsServer' "-map=$cookMaps" '-unattended' '-nopause' '-nosplash' "-abslog=$cookLog"
        $code = $LASTEXITCODE
        $ok = (Test-Path $cookLog) -and (Select-String -Path $cookLog -Pattern 'Success - 0 error\(s\)' -Quiet)
        if ($code -ne 0 -and -not $ok) {
            if (Test-Path $cookLog) { Get-Content $cookLog -Tail 20 }
            Fail "cook failed (exit code $code), see $cookLog"
        }
    }
} elseif (-not $SkipCook) {
    $cookFlag = '-cook'
}
if (-not $SkipPackage) {
    Step 'Package client -> deploy\build\Windows'
    Invoke-Heavy $uat (@('BuildCookRun') + $common + @($cookFlag, '-clientconfig=Development')) 'BuildCookRun client'
    Step 'Package server -> deploy\build\WindowsServer'
    Invoke-Heavy $uat (@('BuildCookRun') + $common + @($cookFlag, '-server', '-serverplatform=Win64', '-noclient', '-serverconfig=Development')) 'BuildCookRun server'
}
$clientExe = Join-Path $out 'Windows\NWPort.exe'
$serverExe = Join-Path $out 'WindowsServer\NWPortServer.exe'
foreach ($exe in $clientExe, $serverExe) {
    if (-not (Test-Path $exe)) { Fail "missing $exe (packaging did not produce it)" }
}

# ---------------------------------------------------------------- 5. nw-core in Docker
if (-not $SkipDocker) {
    Step 'deploy\.env'
    $lines = @()
    if (Test-Path $envFile) { $lines = @(Get-Content $envFile) }
    $set = @{}
    foreach ($line in $lines) { if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=') { $set[$Matches[1]] = $true } }
    foreach ($key in 'NW_DB_PASSWORD', 'NW_SERVER_SECRET') {
        if (-not $set.ContainsKey($key)) {
            $lines += "$key=$(New-Secret)"
            Write-Host "   $key generated"
        } else {
            Write-Host "   $key kept"
        }
    }
    # Not a secret: where each map's server is. Rewritten on every run (the host may change).
    $lines = @($lines | Where-Object { $_ -notmatch '^\s*NW_MAP_SERVERS\s*=' }) + "NW_MAP_SERVERS=$mapServers"
    Write-Text $envFile $lines "`n"
    Write-Host "   NW_MAP_SERVERS=$mapServers"

    Step 'nw-core image'
    & docker build --build-context "tools=$(Join-Path $root 'tools')" -t nw-core:local (Join-Path $root 'backend')
    if ($LASTEXITCODE -ne 0) { Fail 'docker build of nw-core failed' }

    Step 'Postgres + nw-core (docker compose project nw-demo)'
    if ((Compose @('up', '-d')) -ne 0) { Fail 'docker compose up failed (is port 8090 free?)' }
    $up = $false
    for ($i = 0; $i -lt 60 -and -not $up; $i++) {
        try {
            $up = (Invoke-WebRequest -UseBasicParsing -TimeoutSec 2 'http://127.0.0.1:8090/health').StatusCode -eq 200
        } catch {
            $up = $false
        }
        if (-not $up) { Start-Sleep -Seconds 1 }
    }
    if (-not $up) { Compose @('logs', '--tail', '30', 'core') | Out-Null; Fail 'nw-core did not answer on http://127.0.0.1:8090/health' }
    Write-Host '   nw-core: up on port 8090'

    # ---------------------------------------------------------------- 6. accounts
    foreach ($name in $Admin) {
        Step "Admin: $name"
        if ((Compose @('exec', '-T', 'core', 'nw-core', 'admin', $name)) -ne 0) {
            Write-Host "   Account '$name' does not exist yet. Create it on the game's login screen with this invite, then run the script again with -Admin $name (or: docker compose -p nw-demo -f deploy\compose.windows.yaml exec core nw-core admin $name)." -ForegroundColor Yellow
            Compose @('exec', '-T', 'core', 'nw-core', 'invite', '--count', '1', '--days', '7') | Out-Null
        }
    }
    if ($Invites -gt 0) {
        Step "Invites ($Invites, 7 days)"
        Compose @('exec', '-T', 'core', 'nw-core', 'invite', '--count', "$Invites", '--days', '7') | Out-Null
    }
}

# ---------------------------------------------------------------- 7. launchers and zip
Step 'Launchers'
# Servidores.bat [windsward|everfall|arena]: all servers, or only one (e.g. after closing its window).
$servers = @(
    '@echo off',
    'rem Generated by tools\windows\build_demo.ps1: the game servers, one process per map. nw-core runs in Docker.',
    'rem Servidores.bat starts all; Servidores.bat arena starts only that one.',
    'setlocal',
    'set "HERE=%~dp0"',
    'set "ONLY=%~1"',
    'set "NW_SERVER_SECRET="',
    'for /f "usebackq eol=# tokens=1,* delims==" %%a in ("%HERE%..\.env") do if /i "%%a"=="NW_SERVER_SECRET" set "NW_SERVER_SECRET=%%b"',
    'if not defined NW_SERVER_SECRET goto nosecret',
    'if not exist "%HERE%WindowsServer\NWPortServer.exe" goto noserver',
    'if not exist "%HERE%logs" mkdir "%HERE%logs"'
)
foreach ($m in $maps) {
    $servers += "call :run $($m.Name.ToLower()) $($m.Map) $($m.Port) $($m.Name)"
}
$servers += @(
    'exit /b 0',
    ':run',
    'if not "%ONLY%"=="" if /i not "%ONLY%"=="%1" exit /b 0',
    'start "NW %4 :%3" "%HERE%WindowsServer\NWPortServer.exe" /Game/NW/Maps/%2 -log -port=%3 -nwadmin -nwbackend=http://127.0.0.1:8090 -abslog="%HERE%logs\%1.log"',
    'exit /b 0',
    ':nosecret',
    'echo NW_SERVER_SECRET not found in deploy\.env. Run tools\windows\build_demo.ps1.',
    'pause',
    'exit /b 1',
    ':noserver',
    'echo deploy\build\WindowsServer\NWPortServer.exe is missing. Run tools\windows\build_demo.ps1.',
    'pause',
    'exit /b 1'
)
Write-Text (Join-Path $out 'Servidores.bat') $servers "`r`n"
Write-Text (Join-Path $out 'Jogar.bat') @('@echo off', "start `"`" `"%~dp0Windows\NWPort.exe`" -nwbackend=$BackendUrl") "`r`n"
Write-Text (Join-Path $out 'Windows\Jogar.bat') @('@echo off', "start `"`" `"%~dp0NWPort.exe`" -nwbackend=$BackendUrl") "`r`n"
Write-Host "   $out\Servidores.bat, $out\Jogar.bat, $out\Windows\Jogar.bat"

if (-not $SkipZip) {
    Step 'Client zip for friends'
    $zip = Join-Path $out 'NWExiles-demo-client.zip'
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    # .NET ZipFile writes Zip64 (the .pak is larger than 4 GB); Compress-Archive in PowerShell 5.1 does not.
    [IO.Compression.ZipFile]::CreateFromDirectory((Join-Path $out 'Windows'), $zip, [IO.Compression.CompressionLevel]::Fastest, $true)
    Write-Host "   $zip ($([math]::Round((Get-Item $zip).Length / 1GB, 1)) GB): private, only for testers who own the game"
}

Step 'Done'
Write-Host "   1. $out\Servidores.bat   (three server windows; allow them in the Windows firewall on first start)"
Write-Host "   2. $out\Jogar.bat        (log in; F10 needs an admin account)"
Write-Host "   Friends: unzip NWExiles-demo-client.zip and run Windows\Jogar.bat. Open UDP 7777-7779 and TCP 8090 to this PC"
Write-Host "   (-OpenFirewall does the Windows firewall; the router port forwarding is manual)."
Write-Host "   Teleporter: [E] at each town's teleporter takes the player to the other town's server."
Write-Host "   Stop nw-core: docker compose -p nw-demo -f deploy\compose.windows.yaml down (the database is kept)."
