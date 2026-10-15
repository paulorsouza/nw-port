# Run in the Visual Studio x64 Developer PowerShell, with C++ installed.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'Open the Visual Studio x64 Developer PowerShell (C++ workload).'
}
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'The DLL needs an x64 build environment.'
}
$python = if ($env:NW_PYTHON) { $env:NW_PYTHON } else { 'python' }
$sourceDir = (& $python (Join-Path $repoRoot 'tools/build_ooz.py') | Select-Object -Last 1)
if ($LASTEXITCODE -ne 0) { throw "tools/build_ooz.py failed: $LASTEXITCODE" }
$buildDir = Join-Path $repoRoot 'vendor/build/ooz-windows'
New-Item -ItemType Directory -Force $buildDir | Out-Null
Push-Location $buildDir
try {
    $sources = 'kraken.cpp', 'bitknit.cpp', 'lzna.cpp' | ForEach-Object {
        Join-Path $sourceDir $_
    }
    & cl.exe /nologo /O2 /EHsc /LD /MT @sources /link /OUT:ooz.dll
    if ($LASTEXITCODE -ne 0) { throw "ooz build failed: $LASTEXITCODE" }
    New-Item -ItemType Directory -Force (Join-Path $repoRoot 'bin') | Out-Null
    Copy-Item -LiteralPath 'ooz.dll' -Destination (Join-Path $repoRoot 'bin/ooz.dll')
} finally {
    Pop-Location
}
