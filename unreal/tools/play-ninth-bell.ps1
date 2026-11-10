param(
    [Alias('Host')][switch]$Listen,
    [ValidatePattern('^[A-Za-z0-9.:-]+$')][string]$Join,
    [ValidateSet('Solo','Host','Join')][string]$Mode = 'Solo',
    [ValidatePattern('^[A-Za-z0-9.:-]+$')][string]$Address = '127.0.0.1',
    [ValidateRange(1024,65535)][int]$Port = 7777,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
if ($Listen -and $Join) { throw 'Choose -Host or -Join <ip>.' }
if ($Listen) { $Mode = 'Host' }
if ($Join) { $Mode = 'Join'; $Address = $Join }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Join-Path $repoRoot 'unreal\GrudgeBound\GrudgeBound.uproject'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
if (!(Test-Path -LiteralPath $editor)) { throw 'Set -Engine to your installed UE 5.8 folder.' }
$map = '/Game/Grudgebound/NinthBell'
if ($Mode -eq 'Host') { $map += '?listen' }
if ($Mode -eq 'Join') { $map = '{0}:{1}' -f $Address,$Port }
# This is an interactive game launcher; show its window so the player can control it.
& $editor $project $map -game -windowed -ResX=1280 -ResY=720 "-port=$Port" -nosplash
