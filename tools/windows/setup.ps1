# SPDX-License-Identifier: GPL-3.0-only
# Qt-free source bootstrap; never installs software, starts devices or edits old profiles.
[CmdletBinding()]
param([switch]$CheckOnly, [string]$Ref = '', [switch]$NoLaunch, [switch]$NoAsio)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$paSha = '147dd722548358763a8b649b3e4b41dfffbcfbb6'
# Steinberg ASIO SDK (dual licence; used under its GPL-3.0 option, built locally,
# never committed or redistributed). Pin: native/io/audio/DESIGN.md.
$asioUrl = 'https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip'
$asioSha = 'D5EBF0C20DD2C5F43771FD0C1418F4B361BF52434EE670097CFA6B3A335E2ECA'

# BEGIN OUTPUT GUARD (also executed directly by the filesystem regressions)
function Assert-GeneratedOutputs {
    $base = [IO.Path]::GetFullPath($root).TrimEnd('\','/')
    foreach ($relative in @('.tools', '.tools/windows-setup',
        '.tools/windows-setup/npm-cache', '.tools/windows-setup/electron-cache',
        '.tools/windows-setup/portaudio-src', '.tools/windows-setup/portaudio-build',
        '.tools/windows-setup/asiosdk',
        'build_reports', 'build_reports/windows-setup', 'build_reports/windows-setup/electron-smoke',
        'build', 'build/native',
        'apps/studio/node_modules', 'apps/studio/dist',
        'apps/desktop/node_modules', 'apps/desktop/build')) {
        $current = [IO.Path]::GetFullPath((Join-Path $base $relative))
        while ($true) {
            if ((Test-Path -LiteralPath $current) -and
                ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw "Refusing redirected output path: $current"
            }
            if ($current -eq $base) { break }
            $current = Split-Path -Parent $current
            if (-not $current) { throw 'Output path escaped the repository root.' }
        }
    }
}
# END OUTPUT GUARD

function Run([string]$exe, [string[]]$parameters) {
    & $exe @parameters
    if ($LASTEXITCODE -ne 0) { throw "$exe failed (exit $LASTEXITCODE). Do not reuse old build outputs." }
}
# Prefer the pinned pip CMake (often not first on PATH, e.g. behind a system
# CMake 4.4). Process-local PATH only; changes no files. Owner report 2026-11-11.
function Use-PinnedCMake {
    $want = 'cmake version 4.2.3'
    $found = ''
    if (Get-Command cmake -ErrorAction SilentlyContinue) { $found = [string](& cmake --version | Select-Object -First 1) }
    if ($found -eq $want) { return }
    if (-not (Get-Command py -ErrorAction SilentlyContinue)) { return }
    foreach ($kind in @('nt_user', 'nt')) {
        $dir = [string](& py -3.12 -c "import sysconfig;print(sysconfig.get_path('scripts','$kind'))")
        if ($dir -and (Test-Path -LiteralPath (Join-Path $dir.Trim() 'cmake.exe'))) {
            $env:Path = $dir.Trim() + ';' + $env:Path
            if ([string](& cmake --version | Select-Object -First 1) -eq $want) {
                Write-Host "Using pinned CMake 4.2.3 from $($dir.Trim()) (this run only)."
                return
            }
        }
    }
    Write-Warning "CMake 4.2.3 is the tested version; found '$found'. Install it: py -3.12 -m pip install cmake==4.2.3 ninja==1.13.0"
}
function Preflight {
    Assert-GeneratedOutputs
    Use-PinnedCMake
    if (-not [Environment]::Is64BitProcess -or $env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
        throw 'Use Windows x64 Developer PowerShell for VS 2022/2026.'
    }
    foreach ($tool in @('git','py','cmake','ninja','node','npm','cl.exe')) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "$tool missing. Install the x64 tool and reopen VS 2022/2026 Developer PowerShell."
        }
    }
    if (-not $env:WindowsSdkDir) { throw 'Windows SDK not active; open VS 2022/2026 x64 Developer PowerShell.' }
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'VS 2022/2026 C++ Build Tools missing.' }
    $vs = & $vswhere -latest -version '[17.0,19.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'Install VS 2022/2026 C++ x64 workload and Windows SDK.' }
    $py = & py -3.12 -c "import sys; print(str(sys.version_info.major)+'.'+str(sys.version_info.minor)+' '+str(sys.maxsize > 2**32))"
    if ($LASTEXITCODE -ne 0 -or $py -ne '3.12 True') { throw 'Python 3.12 x64 with py launcher required.' }
    $script:generator = & py -3.12 -B -c "import sys; sys.path.insert(0, sys.argv[1]); from foundation.helpers import discover_toolchain; print(discover_toolchain()['generator'])" (Join-Path $root 'tools')
    if ($LASTEXITCODE -ne 0 -or $script:generator -notmatch '^Visual Studio (17 2022|18 2026)$') {
        throw 'A supported VS 2022/2026 x64 C++ generator is required.'
    }
    $nodeVersion = & node --version
    $nodeArch = & node -p 'process.arch'
    if ($LASTEXITCODE -ne 0 -or $nodeVersion -notmatch '^v22\.' -or $nodeArch -ne 'x64') {
        throw 'Node.js 22 LTS x64 required.'
    }
    Run npm @('--version')
    Run cmake @('--version')
    Run ninja @('--version')
    $actual = (& git -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -notmatch '^[0-9a-f]{40}$') { throw 'Invalid Git checkout.' }
    if ($Ref -and ($Ref -notmatch '^[0-9a-fA-F]{40}$' -or $actual -ne $Ref.ToLowerInvariant())) {
        throw "Source does not match supplied accepted SHA; actual=$actual"
    }
    $dirty = & git --no-optional-locks -C $root status --porcelain
    if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Source worktree is dirty; preserve your changes.' }
    $cache = Join-Path $root 'build\native\CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $lines = Get-Content -LiteralPath $cache
        $cmakeHome = @($lines | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' })
        $expected = (Join-Path $root 'native') -replace '\\','/'
        if ($cmakeHome.Count -ne 1 -or (($cmakeHome[0] -split '=',2)[1] -replace '\\','/') -ne $expected -or
            -not ($lines -contains 'WIRESCAPE_BUILD_UI:BOOL=OFF')) {
            throw 'Stale/Qt-enabled CMake cache. Preserve it; use a fresh checkout.'
        }
    }
    foreach ($lock in @('apps\studio\package-lock.json','apps\desktop\package-lock.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $lock))) { throw "Missing npm lockfile: $lock" }
    }
    Write-Host "Preflight PASS: $actual; Node=$nodeVersion (no files changed)."
}
Preflight
if ($CheckOnly) {
    Write-Host 'CheckOnly only read local sources, versions, cache and lockfiles.'
    return
}
if ($Ref -notmatch '^[0-9a-fA-F]{40}$') {
    throw 'Specify -Ref with the accepted full SHA AFTER #463 merges.'
}
# Process-local quarantine of inherited Qt; no global PATH or installer mutations.
foreach ($name in @('WIRESCAPE_QT_DIR','WIRESCAPE_PORTAUDIO','CMAKE_PREFIX_PATH',
                    'QT_PLUGIN_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM_PLUGIN_PATH',
                    'ELECTRON_RUN_AS_NODE','WIRESCAPE_TEST_STUB')) {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}
$env:Path = (($env:Path -split ';') | Where-Object {
    $_ -and $_ -notmatch '(?i)(^|[\\/])Qt([\\/]|$)|wirescape'
}) -join ';'
$tools = Join-Path $root '.tools\windows-setup'
$logs = Join-Path $root 'build_reports\windows-setup'
foreach ($dir in @((Join-Path $root '.tools'),$tools,(Join-Path $root 'build_reports'),$logs)) {
    if ((Test-Path -LiteralPath $dir) -and
        ((Get-Item -LiteralPath $dir -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing redirected output path: $dir"
    }
}
New-Item -Force -ItemType Directory $tools,$logs | Out-Null
$env:npm_config_cache = Join-Path $tools 'npm-cache'
$env:ELECTRON_CACHE = Join-Path $tools 'electron-cache'
$summary = Join-Path $logs 'summary.txt'
@("source=$Ref","portaudio=$paSha","status=incomplete") | Set-Content -LiteralPath $summary
try {
    $paSrc = Join-Path $tools 'portaudio-src'
    $paBuild = Join-Path $tools 'portaudio-build'
    foreach ($dir in @($paSrc,$paBuild)) {
        if ((Test-Path -LiteralPath $dir) -and
            ((Get-Item -LiteralPath $dir -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing redirected dependency/build directory: $dir"
        }
    }
    if (-not (Test-Path -LiteralPath $paSrc)) {
        Run git @('clone','https://github.com/PortAudio/portaudio.git',$paSrc)
        Run git @('-C',$paSrc,'checkout','--detach',$paSha)
    }
    $paHead = (& git -C $paSrc rev-parse HEAD).Trim()
    $paDirty = & git -C $paSrc status --porcelain
    if ($LASTEXITCODE -ne 0 -or $paHead -ne $paSha -or $paDirty) {
        throw 'PortAudio source pin/cleanliness mismatch; do not reuse old DLL.'
    }
    # ASIO host API: lists installed ASIO drivers (interface drivers, FlexASIO,
    # ASIO4ALL) beside WASAPI/MME. Download only; nothing is installed.
    $asioArgs = @('-DPA_USE_ASIO=OFF')
    if (-not $NoAsio) {
        $asioDir = Join-Path $tools 'asiosdk'
        $asioZip = Join-Path $tools 'asiosdk.zip'
        if (-not (Test-Path -LiteralPath $asioZip)) {
            Invoke-WebRequest -UseBasicParsing -Uri $asioUrl -OutFile $asioZip
        }
        if ((Get-FileHash -LiteralPath $asioZip -Algorithm SHA256).Hash -ne $asioSha) {
            throw "ASIO SDK checksum mismatch; delete $asioZip and retry, or run with -NoAsio."
        }
        $asioRoot = Join-Path $asioDir 'ASIOSDK'
        if (-not (Test-Path -LiteralPath (Join-Path $asioRoot 'common\asio.h'))) {
            Expand-Archive -LiteralPath $asioZip -DestinationPath $asioDir -Force
        }
        if (-not (Test-Path -LiteralPath (Join-Path $asioRoot 'host\pc\asiolist.cpp'))) {
            throw 'ASIO SDK layout unexpected; run with -NoAsio.'
        }
        $asioArgs = @('-DPA_USE_ASIO=ON',"-DASIOSDK_ROOT_DIR=$($asioRoot -replace '\\','/')")
    }
    Run cmake (@('-S',$paSrc,'-B',$paBuild,'-G',$script:generator,'-A','x64',
        '-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DPA_BUILD_SHARED=ON','-DPA_BUILD_STATIC=OFF',
        '-DPA_BUILD_TESTS=OFF','-DPA_BUILD_EXAMPLES=OFF') + $asioArgs)
    Assert-GeneratedOutputs
    Run cmake @('--build',$paBuild,'--config','Release','--parallel','2')
    $dll = Join-Path $paBuild 'Release\portaudio_x64.dll'
    if (-not (Test-Path -LiteralPath $dll)) { throw 'Pinned PortAudio DLL missing.' }
    $reader = [IO.BinaryReader]::new([IO.File]::OpenRead($dll))
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'PortAudio is not PE.' }
        $reader.BaseStream.Position = 0x3C
        $offset = $reader.ReadUInt32()
        if ($offset -gt $reader.BaseStream.Length - 6) { throw 'Invalid DLL header.' }
        $reader.BaseStream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw 'PortAudio DLL is not x64.'
        }
    } finally { $reader.Dispose() }
    Assert-GeneratedOutputs
    Run cmake @('-S',(Join-Path $root 'native'),'-B',(Join-Path $root 'build\native'),
        '-G',$script:generator,'-A','x64','-DWIRESCAPE_BUILD_UI=OFF')
    Run cmake @('--build',(Join-Path $root 'build\native'),'--config','Release','--parallel','2')
    $engine = Join-Path $root 'build\native\Release\wirescape-engine.exe'
    if (-not (Test-Path -LiteralPath $engine)) { throw 'Sidecar executable missing.' }
    $runtime = Join-Path (Split-Path $engine) 'libportaudio64bit.dll'
    Copy-Item -LiteralPath $dll -Destination $runtime -Force
    $hash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath $runtime -Algorithm SHA256).Hash -ne $hash) {
        throw 'Staged PortAudio hash mismatch.'
    }
    @("source=$Ref","portaudio=$paSha","portaudio_sha256=$hash","asio=$(-not $NoAsio)","status=native-built") | Set-Content -LiteralPath $summary
    $output = & ctest --test-dir (Join-Path $root 'build\native') -C Release --output-on-failure --no-tests=error -R '^(abi2_sidecar_unit_tests|abi2_sidecar_protocol_tests|sidecar_golden|abi2_sidecar_audio_recovery_tests)$' 2>&1
    $exit = $LASTEXITCODE
    $output | Out-File -LiteralPath (Join-Path $logs 'protocol-tests.log') -Encoding utf8
    if ($exit -ne 0 -or (($output -join [Environment]::NewLine) -match '(?im)\b(Skipped|Not Run)\b')) {
        throw 'Native protocol tests failed or skipped. Inspect protocol-tests.log.'
    }
    # Loading the library and resolving exports does not initialize PortAudio or devices.
    Run py @('-3.12','-c',
        "import ctypes,sys; lib=ctypes.WinDLL(sys.argv[1]); [getattr(lib,n) for n in ('Pa_Initialize','Pa_Terminate','Pa_GetDeviceCount','Pa_GetHostApiInfo','Pa_OpenStream','Pa_StartStream','Pa_StopStream','Pa_CloseStream')]",
        $runtime)
    Assert-GeneratedOutputs
    Run npm @('--prefix',(Join-Path $root 'apps\studio'),'ci')
    Run npm @('--prefix',(Join-Path $root 'apps\studio'),'run','build')
    Run npm @('--prefix',(Join-Path $root 'apps\desktop'),'ci')
    Run npm @('--prefix',(Join-Path $root 'apps\desktop'),'run','build')
    if (-not (Test-Path -LiteralPath (Join-Path $root 'apps\studio\dist\index.html')) -or
        -not (Test-Path -LiteralPath (Join-Path $root 'apps\desktop\build\main.js'))) {
        throw 'Electron or Studio build assets missing.'
    }
    @("source=$Ref","portaudio=$paSha","portaudio_sha256=$hash","asio=$(-not $NoAsio)","status=build-and-native-tests-passed") | Set-Content -LiteralPath $summary
    if ($NoLaunch) {
        Write-Host 'Native/Studio/Desktop PASS; Electron live window/round-trip NOT run.'
        return
    }
    # Real production Electron/preload/sidecar, isolated profiles, no devices.
    $env:WIRESCAPE_SETUP_ROOT = $root
    $env:WIRESCAPE_REAL_ENGINE = $engine
    Run node @((Join-Path $root 'apps\studio\node_modules\playwright\cli.js'),
        'test','--config',(Join-Path $root 'apps\desktop\playwright.config.cjs'),
        'windows-setup.spec.cjs','--output',(Join-Path $logs 'electron-smoke'))
    @("source=$Ref","portaudio=$paSha","portaudio_sha256=$hash","asio=$(-not $NoAsio)","status=windows-device-off-smoke-passed") | Set-Content -LiteralPath $summary
    Write-Host "Setup PASS. Summary: $summary"
    Write-Host "Launch with sound and MIDI: $(Join-Path $root 'tools\windows\Wirescape.cmd')"
} catch {
    Write-Error "W1 failed: $($_.Exception.Message). Local diagnostic directory: $logs."
    throw
}
