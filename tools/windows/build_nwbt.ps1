[CmdletBinding()]
param(
    [string]$Source = (Join-Path $PSScriptRoot '../../vendor/nw-buddy'),
    [string]$GoExe = 'go',
    [switch]$Test
)
# Builds bin/nwbt.exe and bin/dbgwater.exe with the same script as Linux (tools/build_nwbt.py): fork base commit
# + patches/nwbt.patch, version stamped in the binary ("nwbt --version"). The fork checkout is not modified.
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$python = if ($env:NW_PYTHON) { $env:NW_PYTHON } else { 'python' }
$arguments = @((Join-Path $repoRoot 'tools/build_nwbt.py'), '--source', [IO.Path]::GetFullPath($Source), '--go', $GoExe)
if ($Test) { $arguments += '--test' }
& $python @arguments
if ($LASTEXITCODE -ne 0) { throw 'nwbt build failed.' }
