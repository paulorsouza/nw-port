[CmdletBinding()]
param(
    [string]$Source = (Join-Path $PSScriptRoot '../../vendor/nw-buddy'),
    [string]$GoExe = 'go',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$Source = [IO.Path]::GetFullPath($Source)
$revision = '267f86568977c5801b716f02684f7bc69e0982d0'
if (-not (Test-Path -LiteralPath $Source)) {
    New-Item -ItemType Directory -Force (Split-Path $Source) | Out-Null
    & git clone --branch nw-port https://github.com/paulorsouza/nw-buddy.git $Source
    if ($LASTEXITCODE -ne 0) { throw 'Clone nw-buddy falhou.' }
    & git -C $Source checkout --detach $revision
    if ($LASTEXITCODE -ne 0) { throw 'Checkout nw-buddy falhou.' }
}
$actual = & git -C $Source rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $revision) {
    throw "Use nw-buddy no commit $revision; encontrado: $actual"
}
foreach ($patchName in @('nwbt-windows.patch', 'nwbt-platform-path.patch', 'nwbt-pak-root-slash.patch', 'nwbt-files-argfile.patch')) {
    $patch = Join-Path $repoRoot "patches/$patchName"
    # O fork contem fontes CRLF; aceite tambem checkouts normalizados para LF.
    & git -C $Source apply --ignore-space-change --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) {
        & git -C $Source apply --ignore-space-change $patch
        if ($LASTEXITCODE -ne 0) { throw "Aplicacao do patch falhou: $patchName" }
    } else {
        & git -C $Source apply --ignore-space-change --reverse --check $patch 2>$null
        if ($LASTEXITCODE -ne 0) { throw "Patch nao aplica: $patchName; confira as alteracoes locais." }
    }
}
$module = Join-Path $Source 'tools/nwbt'
& $GoExe -C $module build -o (Join-Path $repoRoot 'bin/nwbt.exe') .
if ($LASTEXITCODE -ne 0) { throw 'Build nwbt falhou.' }
& $GoExe -C $module build -o (Join-Path $repoRoot 'bin/dbgwater.exe') ./dbgwater
if ($LASTEXITCODE -ne 0) { throw 'Build dbgwater falhou.' }
if ($Test) {
    $dll = Join-Path $repoRoot 'bin/ooz.dll'
    if (-not (Test-Path -LiteralPath $dll)) { throw 'Compile ooz.dll antes de usar -Test.' }
    $previous = $env:NWBT_TEST_OOZ
    try {
        $env:NWBT_TEST_OOZ = $dll
        & $GoExe -C $module test ./utils ./utils/env -run 'TestResolveDataFile|TestOodleWindowsOverride|TestOozWindowsStoredBlock|TestAppendToPATH' -v
        if ($LASTEXITCODE -ne 0) { throw 'Testes nativos falharam.' }
    } finally {
        $env:NWBT_TEST_OOZ = $previous
    }
}
