# SPDX-License-Identifier: GPL-3.0-only
param([Parameter(Mandatory=$true)][string]$Package, [string]$LocalFont)
$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path -LiteralPath $Package).Path
$checkout = Split-Path -Parent $PSScriptRoot
# Share target validation with fake-tree unit tests, before writing any shortcut.
$executable = & python (Join-Path $PSScriptRoot 'foundation/launcher_runtime.py') --checkout $checkout --package $packageRoot
if ($LASTEXITCODE -ne 0) { throw 'Package is not verified for this active checkout.' }
$executable = $executable.Trim()
if ($LocalFont) { $LocalFont = (Resolve-Path -LiteralPath $LocalFont).Path }
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Packaged Wirescape app was not found.' }
$shell = New-Object -ComObject WScript.Shell
$destinations = @(
    (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Wirescape.lnk'),
    (Join-Path ([Environment]::GetFolderPath('Programs')) 'Wirescape.lnk')
)
foreach ($destination in $destinations) {
    if (Test-Path -LiteralPath $destination) {
        $existing = $shell.CreateShortcut($destination)
        if ([IO.Path]::GetFileName($existing.TargetPath) -ne 'wirescape_studio.exe') {
            throw "An unrelated shortcut already exists: $destination"
        }
    }
}
foreach ($destination in $destinations) {
    $shortcut = $shell.CreateShortcut($destination)
    $shortcut.TargetPath = $executable
    $shortcut.WorkingDirectory = $packageRoot
    $shortcut.Arguments = ''
    if ($LocalFont) { $shortcut.Arguments = '--local-font "' + $LocalFont + '"' }
    $shortcut.IconLocation = "$executable,0"
    $shortcut.Description = 'Wirescape Studio - standalone modular sound environment'
    $shortcut.Save()
    $verify = $shell.CreateShortcut($destination)
    if ($verify.TargetPath -ne $executable) { throw "Shortcut verification failed: $destination" }
    Write-Output "Created $destination -> $executable"
}
