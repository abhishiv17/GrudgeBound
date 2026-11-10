# Packages Grudgebound for Windows (P4-U8): cook LanternWard + NinthBell, pak, stage, archive and zip.
# Close every Unreal editor first (and make sure no other Unreal build/cook is running: 16 GB RAM laptop).
# Also close Chrome and other heavy apps: the first run compiles and cooks everything.
#
#   powershell -ExecutionPolicy Bypass -File unreal\tools\package-windows.ps1 -ServerUrl https://<game-server>
#   powershell -ExecutionPolicy Bypass -File unreal\tools\package-windows.ps1 -ServerUrl http://127.0.0.1:8787 -Config Development
#
# KNOWN BUG (10 Nov, open): the packaged game ignores ServerUrl from every config file (it comes out empty, so
# requests go to "/state"); launching with  GrudgeBound.exe -GbServer=<url>  works. See docs/TASKS.md T-13.
# HTTPS needs a CA bundle in the package: the script copies the engine's cacert.pem to
# Content/Certificates/ (git-ignored) and stages it as a loose file, where the engine looks for it.
#
# The server URL is written into DefaultGame.ini only for the duration of the package run, then the committed
# file is put back exactly as it was. Players can still override it with  GrudgeBound.exe -GbServer=<url>.
# Output: unreal\Packaged\<yyyyMMdd-HHmm>-<Config>\Windows\ and Grudgebound-Win64-<version>.zip beside it
# (git-ignored: packaged builds go on a GitHub Release, never into the repo).
# This script never uploads, tags or publishes anything.
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^https?://[A-Za-z0-9.\-:\[\]]+(/.*)?$')][string]$ServerUrl,
    [ValidateSet('Shipping', 'Development')][string]$Config = 'Shipping',
    [string]$Version = (Get-Date -Format 'yyyy.MM.dd-HHmm'),
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    # 16 GB laptop: 12 parallel compiles ran out of memory on 9 Nov. 4 is slower but fits beside other apps.
    [ValidateRange(1, 32)][int]$MaxParallelActions = 4
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Join-Path $repo 'unreal\GrudgeBound\GrudgeBound.uproject'
$gameIni = Join-Path $repo 'unreal\GrudgeBound\Config\DefaultGame.ini'
$uat = Join-Path $Engine 'Engine\Build\BatchFiles\RunUAT.bat'
if (!(Test-Path -LiteralPath $uat)) { throw "RunUAT.bat not found: set -Engine to your UE 5.8 folder." }
if (Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue) { throw 'Close every Unreal editor window first (UnrealEditor is running).' }
if ($Config -eq 'Shipping' -and $ServerUrl -notmatch '^https://') { throw 'A Shipping build for judges must point at an HTTPS server.' }

$stamp = Get-Date -Format 'yyyyMMdd-HHmm'
$archive = Join-Path $repo "unreal\Packaged\$stamp-$Config"
$original = [System.IO.File]::ReadAllText($gameIni)
if ($original.Contains('written by package-windows.ps1')) {
    throw "DefaultGame.ini still holds a section from an interrupted package run. Restore it first: git checkout -- unreal/GrudgeBound/Config/DefaultGame.ini (backup: unreal\GrudgeBound\Saved\DefaultGame.ini.before-package)"
}
$backupDir = Join-Path $repo 'unreal\GrudgeBound\Saved'
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
[System.IO.File]::WriteAllText((Join-Path $backupDir 'DefaultGame.ini.before-package'), $original)
# Without this bundle every HTTPS request fails (curl error 60). Engine search path: <Project>/Content/Certificates/cacert.pem.
$caSource = Join-Path $Engine 'Engine\Content\Certificates\ThirdParty\cacert.pem'
if (!(Test-Path -LiteralPath $caSource)) { throw "CA bundle not found: $caSource" }
$caDir = Join-Path $repo 'unreal\GrudgeBound\Content\Certificates'
New-Item -ItemType Directory -Force -Path $caDir | Out-Null
Copy-Item -LiteralPath $caSource -Destination (Join-Path $caDir 'cacert.pem') -Force
$ninthBell = Join-Path $repo 'unreal\GrudgeBound\Content\Grudgebound\NinthBell.umap'
$ninthLine = ''
if (Test-Path -LiteralPath $ninthBell) { $ninthLine = '+MapsToCook=(FilePath="/Game/Grudgebound/NinthBell")' }
else { Write-Warning 'NinthBell.umap not found: packaging without The Ninth Bell.' }
$section = @"

; --- written by package-windows.ps1 for this package run only (restored afterwards) ---
[/Script/GrudgeboundClient.GbSettings]
ServerUrl=$ServerUrl

[/Script/UnrealEd.ProjectPackagingSettings]
+MapsToCook=(FilePath="/Game/Grudgebound/LanternWard")
$ninthLine
+DirectoriesToAlwaysCook=(Path="/Game/Grudgebound")
+DirectoriesToAlwaysStageAsNonUFS=(Path="Certificates")
bUseIoStore=True
bShareMaterialShaderCode=True
IncludeDebugFiles=False
"@
$started = Get-Date
try {
    [System.IO.File]::WriteAllText($gameIni, $original + $section)
    & $uat BuildCookRun "-project=$project" -noP4 -platform=Win64 "-clientconfig=$Config" -build -cook -stage -pak -iostore `
        -archive "-archivedirectory=$archive" -prereqs -nodebuginfo -utf8output `
        "-ubtargs=-MaxParallelActions=$MaxParallelActions"
    if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed (exit $LASTEXITCODE). The first 'Error:' line in the output above is the cause." }
}
finally {
    # Put the committed ini back byte for byte, whatever happened.
    [System.IO.File]::WriteAllText($gameIni, $original)
}

$windows = Join-Path $archive 'Windows'
if (!(Test-Path -LiteralPath $windows)) { $windows = $archive }
if (!(Get-ChildItem -LiteralPath $windows -Recurse -Filter 'cacert.pem' -ErrorAction SilentlyContinue)) { throw 'cacert.pem was not staged: HTTPS would fail in this build.' }
$zip = Join-Path $archive "Grudgebound-Win64-$Version.zip"
Compress-Archive -Path (Join-Path $windows '*') -DestinationPath $zip -Force
$sizeMb = [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1)
$minutes = [math]::Round(((Get-Date) - $started).TotalMinutes, 1)
Write-Host ''
Write-Host "Packaged $Config for $ServerUrl in $minutes min."
Write-Host "Game folder: $windows"
Write-Host "Zip ($sizeMb MB): $zip"
Write-Host 'Next (owner): play-test the exe, then create the GitHub Release by hand (see docs/UNREAL_SETUP.md section 7).'
