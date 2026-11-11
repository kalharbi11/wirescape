@echo off
:: SPDX-License-Identifier: GPL-3.0-only
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "ENGINE=%ROOT%\build\native\Release\wirescape-engine.exe"
if not exist "%ENGINE%" set "ENGINE=%ROOT%\build\native\wirescape-engine.exe"
if not exist "%ENGINE%" (
  echo Build wirescape-engine before launching Wirescape.
  exit /b 1
)
if not exist "%ROOT%\apps\desktop\build\main.js" (
  echo Run npm run build in apps\desktop first.
  exit /b 1
)
set "ELECTRON=%ROOT%\apps\desktop\node_modules\electron\dist\electron.exe"
if not exist "%ELECTRON%" (
  echo Electron is missing. Run tools\windows\setup.ps1 first.
  exit /b 1
)
:: Launch electron.exe directly. "start" on the npm .bin shim made cmd
:: strip the outer quotes ("The filename, directory name, or volume label
:: syntax is incorrect"), owner install report 2026-11-11.
start "Wirescape" "%ELECTRON%" "%ROOT%\apps\desktop\build\main.js" --engine "%ENGINE%" --confirm-audio --confirm-midi %*
