# OpenSnowstorm Packaging Script
# This script builds the Release version and packages it into a ZIP file with all dependencies.

$ErrorActionPreference = "Stop"

$RepoRoot = Get-Item $PSScriptRoot\..
Set-Location $RepoRoot.FullName

Write-Host "--- OpenSnowstorm: Starting Distribution Build ---" -ForegroundColor Cyan

# 1. Configuration
$BuildDir = "build"
$DistDir = "dist_temp"
$ZipFile = "OpenSnowstorm_Release.zip"

# Ensure Build Directory exists and is configured
if (-not (Test-Path $BuildDir)) {
    Write-Host "Build directory not found. Configuring..."
    cmake -B $BuildDir -DOPENSNOWSTORM_BUILD_UI=ON -DOPENSNOWSTORM_BUILD_BWAPI=ON
}

# 2. Build Release
Write-Host "Building Release binaries..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release -j

# 3. Prepare Distribution Folder
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
New-Item -ItemType Directory -Path $DistDir | Out-Null

$ReleaseBinDir = Join-Path $BuildDir "Release"
$GfxtestExe = Join-Path $ReleaseBinDir "gfxtest.exe"

if (-not (Test-Path $GfxtestExe)) {
    Write-Host "Error: gfxtest.exe not found in $ReleaseBinDir" -ForegroundColor Red
    exit 1
}

Write-Host "Collecting files..." -ForegroundColor Yellow

# Copy Binaries and DLLs
Copy-Item $GfxtestExe $DistDir
Copy-Item (Join-Path $ReleaseBinDir "*.dll") $DistDir

# Copy Configs and Docs
$RootFiles = @("README.md", "LICENSE", "options.ini", "ROADMAP.md")
foreach ($file in $RootFiles) {
    if (Test-Path $file) {
        Copy-Item $file $DistDir
    }
}

# 4. Create ZIP
if (Test-Path $ZipFile) { Remove-Item $ZipFile }
Write-Host "Creating $ZipFile..." -ForegroundColor Yellow
Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipFile

# 5. Cleanup
Remove-Item -Recurse -Force $DistDir

Write-Host "--- Done! ---" -ForegroundColor Green
Write-Host "Package created: $ZipFile"
$ZipInfo = Get-Item $ZipFile
Write-Host "Size: $([math]::Round($ZipInfo.Length / 1MB, 2)) MB"
