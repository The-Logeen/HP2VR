[CmdletBinding()]
param(
    [switch]$Match,
    [switch]$Simulate,
    [ValidateRange(0,300)][int]$Seconds=0
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$project=Split-Path $PSScriptRoot -Parent
$testRuntime=Join-Path $project 'runtime-progression-check'
if(Get-Process Game,HP2VRCompanion -ErrorAction SilentlyContinue){
    throw 'Close HP2 and its VR companion before starting this separate Quidditch run.'
}
if(!(Test-Path -LiteralPath (Join-Path $testRuntime '.hp2vr-private-runtime') -PathType Leaf)){
    throw 'The separate test runtime is missing. Nothing was changed in your normal runtime.'
}
$map=if($Match){'Quidditch.unr'}else{'Quidditch_Intro.unr'}
if(!(Test-Path -LiteralPath (Join-Path $testRuntime ('Maps\'+$map)) -PathType Leaf)){
    throw "Missing HP2 map: $map"
}
# Launch the already installed runtime. A player shortcut must not require
# disposable build artifacts or reinstall transport DLLs on every run.
foreach($entry in @(
    @('System\d3d11.dll','VR\gate2-d3d11-proxy.sha256'),
    @('System\HP2VREngine.dll','VR\gate3-engine-package.sha256'),
    @('VR\HP2VRCompanion.exe','VR\gate2-companion.sha256')
)){
    $installed=Join-Path $testRuntime $entry[0]
    $record=Join-Path $testRuntime $entry[1]
    if(!(Test-Path -LiteralPath $installed -PathType Leaf) -or !(Test-Path -LiteralPath $record -PathType Leaf)){
        throw "The test runtime needs repair: missing $($entry[0]) or its installation record."
    }
    if((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne [IO.File]::ReadAllText($record).Trim()){
        throw "The installed test file changed: $($entry[0]). Nothing overwritten."
    }
}
$settings=Join-Path $project 'runtime\System\HP2VRSettings.ini'
if(Test-Path -LiteralPath $settings -PathType Leaf){
    Copy-Item -LiteralPath $settings -Destination (Join-Path $testRuntime 'System\HP2VRSettings.ini') -Force
}
Write-Host "Starting $map in the separate test runtime. Normal campaign saves are not used."
# Double-click launch uses real tracking, no timeout and no automatic skipping.
# Explicit switches allow bounded automated smoke checks of this exact path.
& (Join-Path $PSScriptRoot 'Start-HP2VR.ps1') -Runtime $testRuntime -Simulate:$Simulate -Seconds $Seconds -UnrealUrl "$map -windowed -NOFRONTEND INI=Game.ini USERINI=User.ini LOG=HP2VR.log"
