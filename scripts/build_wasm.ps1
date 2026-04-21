param(
    [string]$BuildDir = "build/wasm-release",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = Join-Path $repoRoot $BuildDir

function Require-Command([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        throw "Required command '$name' was not found on PATH. Install and activate emsdk first, then rerun this script."
    }
    return $cmd
}

Require-Command "emcmake" | Out-Null
Require-Command "cmake" | Out-Null

Write-Host "Configuring OpenSnowstorm WASM build in $resolvedBuildDir"
& emcmake cmake `
    -S $repoRoot `
    -B $resolvedBuildDir `
    --preset wasm-release `
    -D CMAKE_BUILD_TYPE=$Configuration
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Building gfxtest WebAssembly target"
& cmake --build $resolvedBuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$htmlOut = Join-Path $resolvedBuildDir "gfxtest.html"
if (Test-Path $htmlOut) {
    Write-Host "WASM build completed: $htmlOut"
} else {
    Write-Warning "Build finished but gfxtest.html was not found in $resolvedBuildDir"
}
