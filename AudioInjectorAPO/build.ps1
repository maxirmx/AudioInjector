#!/usr/bin/env pwsh
# build.ps1 - Simple PowerShell build script for AudioInjectorAPO
# Created: May 14, 2025

# Get command line arguments
param (
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [switch]$Clean
)

# MSBuild path
$msbuildPath = "MSBuild.exe"
$projectFile = "AudioInjectorAPO.vcxproj"

# Validate MSBuild exists
if (-not (Test-Path $msbuildPath)) {
    Write-Host "ERROR: MSBuild not found at $msbuildPath" -ForegroundColor Red
    exit 1
}

# Validate project file exists
if (-not (Test-Path $projectFile)) {
    Write-Host "ERROR: Project file $projectFile not found" -ForegroundColor Red
    exit 1
}

# Display build information
Write-Host ""
Write-Host "Building AudioInjectorAPO" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Cyan
Write-Host "Platform: $Platform" -ForegroundColor Cyan
Write-Host ""

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning project..." -ForegroundColor Yellow
    & $msbuildPath $projectFile /p:Configuration=$Configuration /p:Platform=$Platform /t:Clean
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Clean failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

# Build
Write-Host "Building project..." -ForegroundColor Yellow
& $msbuildPath $projectFile /p:Configuration=$Configuration /p:Platform=$Platform

# Check result
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
} else {
    Write-Host ""
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "Output: $Platform\$Configuration\AudioInjectorAPO.dll" -ForegroundColor Green
}

exit 0
