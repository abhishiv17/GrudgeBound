# Builds the LanternWard level headless (runs make-lantern-ward.py). Close the Unreal editor first.
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')
$project = Join-Path $PSScriptRoot '..\Grudgebound\Grudgebound.uproject' | Resolve-Path
$script = Join-Path $PSScriptRoot 'make-lantern-ward.py'
& "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project -run=pythonscript "-script=$script" -unattended -nosplash -nullrhi
Write-Host "Done (exit $LASTEXITCODE). Log: unreal\Grudgebound\Saved\Logs\GrudgeBound.log (search 'Grudgebound:')."
