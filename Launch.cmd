@echo off
set ELECTRON_RUN_AS_NODE=
cd /d "%~dp0"
if exist "dist\LocalFlow-win32-x64\LocalFlow.exe" (
  start "" "dist\LocalFlow-win32-x64\LocalFlow.exe"
) else (
  if exist "node_modules\electron\dist\electron.exe" (
    start "" "node_modules\electron\dist\electron.exe" .
  ) else (
    echo Run setup.ps1 first.
    pause
  )
)

