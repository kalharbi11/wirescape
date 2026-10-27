@echo off
REM CHAOS - one-time setup on Windows. Builds Surge XT's Python module (surgepy) and installs deps.
REM Needs: Visual Studio 2022 (or newer) with "Desktop development with C++", Git, CMake, Python 3.12 (64-bit).
REM Takes 10-20 minutes the first time. Keep this folder at a short path, e.g. <drive>/chaos
setlocal
cd /d "%~dp0"

where git >nul 2>nul || (echo [!] Git not found. Install: winget install Git.Git & exit /b 1)
where cmake >nul 2>nul || (echo [!] CMake not found. Install: winget install Kitware.CMake & exit /b 1)
py -3.12 -c "import sys" >nul 2>nul || (echo [!] Python 3.12 not found. Install: winget install Python.Python.3.12 & exit /b 1)
for /f "delims=" %%i in ('py -3.12 -c "import sys;print(sys.executable)"') do set PYEXE=%%i
echo Using Python: %PYEXE%

echo === 1/4 Python packages
"%PYEXE%" -m pip install -r requirements.txt || exit /b 1

echo === 2/4 Get Surge XT source
if not exist surge\CMakeLists.txt (
  git clone --depth 1 https://github.com/surge-synthesizer/surge.git surge || exit /b 1
)
cd surge
git submodule update --init --recursive --depth 1 || exit /b 1

echo === 3/4 Build surgepy (Release)
cmake -B ignore\bpy -A x64 -DSURGE_BUILD_PYTHON_BINDINGS=ON -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE="%PYEXE%" || exit /b 1
cmake --build ignore\bpy --config Release --parallel --target surgepy || exit /b 1
cd ..
if not exist surgepy_bin mkdir surgepy_bin
for /r surge\ignore\bpy\src\surge-python %%f in (surgepy*.pyd) do copy /y "%%f" surgepy_bin\ >nul

echo === 4/4 Check
"%PYEXE%" -c "import surge_path, surgepy; print('surgepy', surgepy.getVersion(), 'OK')" || exit /b 1
echo.
echo Done. Start with run.bat
