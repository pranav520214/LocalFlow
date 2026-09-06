param([switch]$Sanitize)
$ErrorActionPreference='Stop'
$appRoot=Split-Path $PSScriptRoot -Parent
$vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Install Visual Studio C++ desktop build tools with a Windows SDK.'}
$native=Join-Path $appRoot 'native\injection'
$out=Join-Path $appRoot 'native\build'
New-Item -ItemType Directory -Force $out | Out-Null
$flags=if($Sanitize){'/fsanitize=address /Zi'}else{'/O2'}
$script=Join-Path $out 'compile.cmd'
$sources=@('target.cpp','windows_uia.cpp','clipboard.cpp','focused.cpp','tracker.cpp','module.cpp') | ForEach-Object {'"'+(Join-Path $native $_)+'"'}
$lines=@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars64.bat" >nul'),('cd /d "'+$out+'"'),('cl /nologo /std:c++20 /EHsc /W4 /WX /MD /utf-8 /DUNICODE /D_UNICODE '+$flags+' /LD '+($sources -join ' ')+' /link /OUT:flow-injection.dll user32.lib ole32.lib oleaut32.lib uiautomationcore.lib'))
Set-Content -LiteralPath $script -Value $lines -Encoding ascii
& $env:ComSpec /d /c $script
if($LASTEXITCODE -ne 0){throw 'Native injection build failed.'}
