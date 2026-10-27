@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo WIRESCAPE / Windows setup - reuses your existing compatible Surge build.
py -3.12 -c "import sys" >nul 2>nul || (echo ERROR: Python 3.12 x64 is required for your existing compiled surgepy. & exit /b 1)
py -3.12 -m pip install -r requirements.txt || exit /b 1
if exist "modules\chaos\surgepy_bin\surgepy*.pyd" (
  echo Found Surge inside modules\chaos\surgepy_bin. Reusing it; no rebuild.
) else if exist "..\CHAOS\surgepy_bin\surgepy*.pyd" (
  echo Found adjacent CHAOS surgepy. Reusing it; no rebuild.
) else (
  echo Startup will check SURGEPY_DIR and known Surge binary directories.
  echo Put a compatible compiled surgepy in modules\chaos\surgepy_bin as needed.
)
py -3.12 -m unittest discover -s tests -q || exit /b 1
echo.
echo Setup finished. Use run_windows.bat to launch Wirescape.
