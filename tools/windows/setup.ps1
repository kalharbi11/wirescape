# SPDX-License-Identifier: GPL-3.0-only
# Qt-free source bootstrap; never installs software, starts devices or edits old profiles.
[CmdletBinding()]
param([switch]$CheckOnly, [string]$Ref = '', [switch]$NoLaunch)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$paSha = '147dd722548358763a8b649b3e4b41dfffbcfbb6'

function Run([string]$exe, [string[]]$parameters) {
    & $exe @parameters
    if ($LASTEXITCODE -ne 0) { throw "$exe failed (exit $LASTEXITCODE). Do not reuse old build outputs." }
}
function Preflight {
    if (-not [Environment]::Is64BitProcess -or $env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
        throw 'Use Windows x64 Developer PowerShell for VS 2022.'
    }
    foreach ($tool in @('git','py','cmake','ninja','node','npm','cl.exe')) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "$tool missing. Install the x64 tool and reopen VS 2022 Developer PowerShell."
        }
    }
    if (-not $env:WindowsSdkDir) { throw 'Windows SDK not active; open VS 2022 x64 Developer PowerShell.' }
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'VS 2022 C++ Build Tools missing.' }
    $vs = & $vswhere -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'Install VS 2022 C++ x64 workload and Windows SDK.' }
    $py = & py -3.12 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor} {sys.maxsize > 2**32}")'
    if ($LASTEXITCODE -ne 0 -or $py -ne '3.12 True') { throw 'Python 3.12 x64 with py launcher required.' }
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
    $dirty = & git -C $root status --porcelain
    if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Source worktree is dirty; preserve your changes.' }
    $cache = Join-Path $root 'build\native\CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $lines = Get-Content -LiteralPath $cache
        $home = @($lines | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' })
        $expected = (Join-Path $root 'native') -replace '\\','/'
        if ($home.Count -ne 1 -or (($home[0] -split '=',2)[1] -replace '\\','/') -ne $expected -or
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
    Run cmake @('-S',$paSrc,'-B',$paBuild,'-G','Visual Studio 17 2022','-A','x64',
        '-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DPA_BUILD_SHARED=ON','-DPA_BUILD_STATIC=OFF',
        '-DPA_BUILD_TESTS=OFF','-DPA_BUILD_EXAMPLES=OFF','-DPA_USE_ASIO=OFF')
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
    Run cmake @('-S',(Join-Path $root 'native'),'-B',(Join-Path $root 'build\native'),
        '-G','Visual Studio 17 2022','-A','x64','-DWIRESCAPE_BUILD_UI=OFF')
    Run cmake @('--build',(Join-Path $root 'build\native'),'--config','Release','--parallel','2')
    $engine = Join-Path $root 'build\native\Release\wirescape-engine.exe'
    if (-not (Test-Path -LiteralPath $engine)) { throw 'Sidecar executable missing.' }
    $runtime = Join-Path (Split-Path $engine) 'libportaudio64bit.dll'
    Copy-Item -LiteralPath $dll -Destination $runtime -Force
    $hash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath $runtime -Algorithm SHA256).Hash -ne $hash) {
        throw 'Staged PortAudio hash mismatch.'
    }
    @("source=$Ref","portaudio=$paSha","portaudio_sha256=$hash","status=native-built") | Set-Content -LiteralPath $summary
    $output = & ctest --test-dir (Join-Path $root 'build\native') -C Release --output-on-failure --no-tests=error -R '^(abi2_sidecar_unit_tests|abi2_sidecar_protocol_tests|sidecar_golden)$' 2>&1
    $exit = $LASTEXITCODE
    $output | Out-File -LiteralPath (Join-Path $logs 'protocol-tests.log') -Encoding utf8
    if ($exit -ne 0 -or (($output -join [Environment]::NewLine) -match '(?im)\b(Skipped|Not Run)\b')) {
        throw 'Native protocol tests failed or skipped. Inspect protocol-tests.log.'
    }
    Run npm @('--prefix',(Join-Path $root 'apps\studio'),'ci')
    Run npm @('--prefix',(Join-Path $root 'apps\studio'),'run','build')
    Run npm @('--prefix',(Join-Path $root 'apps\desktop'),'ci')
    Run npm @('--prefix',(Join-Path $root 'apps\desktop'),'run','build')
    if (-not (Test-Path -LiteralPath (Join-Path $root 'apps\studio\dist\index.html')) -or
        -not (Test-Path -LiteralPath (Join-Path $root 'apps\desktop\build\main.js'))) {
        throw 'Electron or Studio build assets missing.'
    }
    @("source=$Ref","portaudio=$paSha","portaudio_sha256=$hash","status=build-and-native-tests-passed") | Set-Content -LiteralPath $summary
    if ($NoLaunch) {
        Write-Host 'Native/Studio/Desktop PASS; Electron live window/round-trip NOT run.'
        return
    }
    # Explicit Electron app-path isolation BEFORE importing the desktop main.
    $profile = Join-Path $tools ('session-' + [Guid]::NewGuid().ToString('N'))
    $env:WIRESCAPE_SETUP_PROFILE = $profile
    $wrapper = Join-Path $tools 'isolated-electron.cjs'
    @'
const { app } = require('electron');
const fs = require('node:fs');
const path = require('node:path');
const base = process.env.WIRESCAPE_SETUP_PROFILE;
if (!base || !path.isAbsolute(base)) throw new Error('Fresh profile required');
for (const [key, suffix] of [['userData','user'],['sessionData','session']]) {
  const dir = path.join(base, suffix);
  fs.mkdirSync(dir, {recursive:true});
  app.setPath(key, dir);
}
require(path.join(process.cwd(), 'apps', 'desktop', 'build', 'main.js'));
'@ | Set-Content -LiteralPath $wrapper -Encoding utf8
    $electron = Join-Path $root 'apps\desktop\node_modules\electron\dist\electron.exe'
    if (-not (Test-Path -LiteralPath $electron)) { throw 'Electron.exe missing.' }
    $settings = Join-Path $profile 'native-settings.json'
    $stderr = Join-Path $logs 'electron-stderr.log'
    $stdout = Join-Path $logs 'electron-stdout.log'
    $argsString = ('"{0}" --engine "{1}" --null-audio --settings "{2}"' -f $wrapper,$engine,$settings)
    $child = Start-Process -FilePath $electron -WorkingDirectory $root -ArgumentList $argsString -PassThru -RedirectStandardError $stderr -RedirectStandardOutput $stdout
    try { [void]$child.WaitForExit(30000) }
    finally {
        if (-not $child.HasExited) { & taskkill.exe /PID $child.Id /T /F | Out-Null }
        [void]$child.WaitForExit(5000)
    }
    if (-not (Test-Path -LiteralPath $stderr) -or
        -not (Select-String -LiteralPath $stderr -SimpleMatch 'boot-to-first-hello-ms=' -Quiet)) {
        throw 'Electron sidecar hello not observed; inspect electron-stderr.log.'
    }
    Write-Host 'Real hello observed device-off; UI/window/command round trip still require separate acceptance.'
    @("source=$Ref","portaudio=$paSha","portaudio_sha256=$hash","status=hello-only-ui-not-verified") | Set-Content -LiteralPath $summary
} catch {
    Write-Error "W1 failed: $($_.Exception.Message). Local diagnostic directory: $logs."
    throw
}
