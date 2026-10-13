# Execute no Developer PowerShell x64 do Visual Studio, com C++ instalado.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'Abra o Developer PowerShell x64 do Visual Studio (workload C++).'
}
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'A DLL precisa de um ambiente de compilacao x64.'
}
$buildDir = Join-Path $repoRoot 'vendor/build/ooz-windows'
New-Item -ItemType Directory -Force $buildDir | Out-Null
Push-Location $buildDir
try {
    $sources = 'kraken.cpp', 'bitknit.cpp', 'lzna.cpp' | ForEach-Object {
        Join-Path $repoRoot "tools/ooz-linux/$_"
    }
    & cl.exe /nologo /O2 /EHsc /LD /MT @sources /link /OUT:ooz.dll
    if ($LASTEXITCODE -ne 0) { throw "Build ooz falhou: $LASTEXITCODE" }
    Copy-Item -LiteralPath 'ooz.dll' -Destination (Join-Path $repoRoot 'bin/ooz.dll')
} finally {
    Pop-Location
}
