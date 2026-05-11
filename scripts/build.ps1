$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"
$distExe = Join-Path $root "dist\minidb.exe"

cmake -S $root -B $buildDir
cmake --build $buildDir --config Debug

if (!(Test-Path $distExe)) {
    throw "Build completed, but dist\minidb.exe was not found."
}

Write-Host "Build complete."
Write-Host "Run MiniDB with:"
Write-Host "  .\dist\minidb.exe"
