@echo off
:: build.cmd - Simple build script for AudioInjectorAPO
:: Created: May 14, 2025

:: MSBuild path - adjust as needed
set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
set PROJECT_FILE=AudioInjectorAPO.vcxproj
set CONFIG=Debug
set PLATFORM=x64

echo Building AudioInjectorAPO
echo Configuration: %CONFIG%
echo Platform: %PLATFORM%

%MSBUILD_PATH% %PROJECT_FILE% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM%

if errorlevel 1 (
    echo Build failed with error level %errorlevel%
    exit /b %errorlevel%
) else (
    echo Build completed successfully!
)

exit /b 0
