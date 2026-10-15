[CmdletBinding()]
param(
    [string]$Source = (Join-Path $PSScriptRoot '../../vendor/nw-buddy'),
    [string]$GoExe = 'go',
    [switch]$Test
)
# Compila bin/nwbt.exe e bin/dbgwater.exe pelo mesmo script do Linux (tools/build_nwbt.py): commit base do fork
# + patches/nwbt.patch, versao gravada no binario ("nwbt --version"). O checkout do fork nao e alterado.
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$python = if ($env:NW_PYTHON) { $env:NW_PYTHON } else { 'python' }
$arguments = @((Join-Path $repoRoot 'tools/build_nwbt.py'), '--source', [IO.Path]::GetFullPath($Source), '--go', $GoExe)
if ($Test) { $arguments += '--test' }
& $python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Build nwbt falhou.' }
