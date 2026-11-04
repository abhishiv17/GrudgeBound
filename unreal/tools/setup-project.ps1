# Run once after creating unreal/Grudgebound (docs/UNREAL_SETUP.md, step 3). Safe to run again.
#   powershell -ExecutionPolicy Bypass -File unreal\tools\setup-project.ps1
#
# 1. Points the project at unreal/Plugins and turns on the GrudgeboundClient plugin
#    (server API, Lantern Ward greybox, tall grass, HUD, battle screen, Talisman Case).
# 2. Render settings for a 4 GB laptop GPU (RTX 3050): no Lumen, no Nanite, no virtual shadow maps,
#    no mesh distance fields. The game is a stylized top-down map; it doesn't need them.
$ErrorActionPreference = 'Stop'

$unreal = Split-Path -Parent $PSScriptRoot
$project = Join-Path $unreal 'Grudgebound\Grudgebound.uproject'
if (-not (Test-Path $project)) {
  Write-Host "Not found: $project" -ForegroundColor Red
  Write-Host "Create the project first: Unreal > Games > Top Down, C++, folder 'unreal', name 'Grudgebound'."
  exit 1
}
$utf8 = New-Object System.Text.UTF8Encoding($false)

# --- 1. plugin
$json = Get-Content $project -Raw | ConvertFrom-Json
if (-not ($json.PSObject.Properties.Name -contains 'AdditionalPluginDirectories')) {
  $json | Add-Member -NotePropertyName 'AdditionalPluginDirectories' -NotePropertyValue @('../Plugins')
} elseif (-not ($json.AdditionalPluginDirectories -contains '../Plugins')) {
  $json.AdditionalPluginDirectories = @($json.AdditionalPluginDirectories) + '../Plugins'
}
if (-not ($json.PSObject.Properties.Name -contains 'Plugins')) {
  $json | Add-Member -NotePropertyName 'Plugins' -NotePropertyValue @()
}
$plugins = @($json.Plugins)
$mine = $plugins | Where-Object { $_.Name -eq 'GrudgeboundClient' }
if ($mine) { $mine.Enabled = $true }
else { $plugins += [pscustomobject]@{ Name = 'GrudgeboundClient'; Enabled = $true } }
$json.Plugins = $plugins
[IO.File]::WriteAllText($project, ($json | ConvertTo-Json -Depth 20), $utf8)
Write-Host "Plugin GrudgeboundClient enabled in $project" -ForegroundColor Green

# --- 2. render settings
$engineIni = Join-Path $unreal 'Grudgebound\Config\DefaultEngine.ini'
$marker = '; Grudgebound: 4 GB GPU settings'
$block = @"

$marker (unreal/tools/setup-project.ps1)
[/Script/Engine.RendererSettings]
r.DynamicGlobalIlluminationMethod=0
r.ReflectionMethod=2
r.Shadow.Virtual.Enable=0
r.Nanite.ProjectEnabled=False
r.GenerateMeshDistanceFields=False
r.AntiAliasingMethod=2
"@
$current = if (Test-Path $engineIni) { Get-Content $engineIni -Raw } else { '' }
if ($current -notlike "*$marker*") {
  [IO.File]::WriteAllText($engineIni, $current + $block, $utf8)
  Write-Host "Render settings for a 4 GB GPU added to Config\DefaultEngine.ini" -ForegroundColor Green
} else {
  Write-Host "Render settings already present" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Next: right-click Grudgebound.uproject > Generate Visual Studio project files, then open it (it builds the plugin)."
Write-Host "Then follow docs/UNREAL_SETUP.md section 4 (greybox level) and press Play with  npm run server  running."
