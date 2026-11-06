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
start "" "%ROOT%\apps\desktop\node_modules\.bin\electron.cmd" "%ROOT%\apps\desktop\build\main.js" --engine "%ENGINE%" --confirm-audio --confirm-midi %*
