param([Parameter(Mandatory=$true)][string]$Package, [string]$LocalFont)
$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path -LiteralPath $Package).Path
$executable = Join-Path $packageRoot 'wirescape_studio.exe'
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
    if ($LocalFont) { $shortcut.Arguments = '--local-font "' + $LocalFont + '"' }
    $shortcut.IconLocation = "$executable,0"
    $shortcut.Description = 'Wirescape Studio - standalone modular sound environment'
    $shortcut.Save()
    $verify = $shell.CreateShortcut($destination)
    if ($verify.TargetPath -ne $executable) { throw "Shortcut verification failed: $destination" }
    Write-Output "Created $destination -> $executable"
}
