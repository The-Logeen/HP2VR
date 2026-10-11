[CmdletBinding()]
param(
    [string]$Runtime=(Join-Path (Split-Path $PSScriptRoot -Parent) 'runtime'),
    [string]$UnrealUrl='Startup.unr -windowed INI=Game.ini USERINI=User.ini LOG=HP2VR.log',
    [ValidateRange(0,300)][int]$Seconds=0,
    [switch]$Simulate
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$project=Split-Path $PSScriptRoot -Parent
$runtimePath=[IO.Path]::GetFullPath($Runtime).TrimEnd('\','/')
$projectPath=[IO.Path]::GetFullPath($project).TrimEnd('\','/')
if(!$runtimePath.StartsWith($projectPath+'\',[StringComparison]::OrdinalIgnoreCase)){
    throw 'Start-HP2VR launches only this project private runtime.'
}
if(!(Test-Path -LiteralPath (Join-Path $runtimePath '.hp2vr-private-runtime') -PathType Leaf)){
    throw 'Private-runtime marker missing.'
}
$hostPath=Join-Path $project 'build\HP2VRHost.exe'
$companionPath=Join-Path $runtimePath 'VR\HP2VRCompanion.exe'
foreach($required in @($hostPath,$companionPath,(Join-Path $runtimePath 'System\HP2VREngine.dll'))){
    if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Required HP2 VR file is missing: $required"}
}
if(Get-Process Game,HP2VRCompanion -ErrorAction SilentlyContinue){
    throw 'Close the existing Game.exe and HP2VRCompanion.exe before starting HP2 VR.'
}
if($Simulate -and $Seconds -eq 0){$Seconds=20}

$savedRun=[Environment]::GetEnvironmentVariable('HP2VR_RUN_SECONDS','Process')
$savedUntil=[Environment]::GetEnvironmentVariable('HP2VR_RUN_UNTIL_EXIT','Process')
$savedSimulation=[Environment]::GetEnvironmentVariable('HP2VR_SIMULATE_SECONDS','Process')
$savedManualMenu=[Environment]::GetEnvironmentVariable('HP2VR_MANUAL_MENU','Process')
$savedRuntime=[Environment]::GetEnvironmentVariable('HP2VR_RUNTIME','Process')
$companion=$null
try{
    & (Join-Path $PSScriptRoot 'Install-Gate1.ps1') -Runtime $runtimePath
    $env:HP2VR_RUNTIME=$runtimePath
    if($Seconds -gt 0){
        $env:HP2VR_RUN_SECONDS=[string]$Seconds
        Remove-Item Env:HP2VR_RUN_UNTIL_EXIT -ErrorAction SilentlyContinue
    }else{
        $env:HP2VR_RUN_UNTIL_EXIT='1'
        Remove-Item Env:HP2VR_RUN_SECONDS -ErrorAction SilentlyContinue
    }
    if($Simulate){
        $env:HP2VR_SIMULATE_SECONDS=[string]([Math]::Min(300,$Seconds+20))
    }else{
        Remove-Item Env:HP2VR_SIMULATE_SECONDS -ErrorAction SilentlyContinue
    }
    # Leave the native main menu open for the player; never auto-start a game.
    $env:HP2VR_MANUAL_MENU='1'
    $startParameters=@{
        FilePath=$companionPath
        WorkingDirectory=(Join-Path $runtimePath 'VR')
        WindowStyle='Hidden'
        PassThru=$true
    }
    if($Simulate){$startParameters.ArgumentList='--simulate'}
    $companion=Start-Process @startParameters
    Start-Sleep -Milliseconds 750
    $companion.Refresh()
    if($companion.HasExited){
        throw "HP2VRCompanion exited during startup with code $($companion.ExitCode). See runtime\VR\HP2VRCompanion.log."
    }
    & $hostPath $runtimePath $UnrealUrl
    if($LASTEXITCODE -ne 0){throw "HP2VRHost exited with code $LASTEXITCODE."}
} finally {
    [Environment]::SetEnvironmentVariable('HP2VR_RUN_SECONDS',$savedRun,'Process')
    [Environment]::SetEnvironmentVariable('HP2VR_RUN_UNTIL_EXIT',$savedUntil,'Process')
    [Environment]::SetEnvironmentVariable('HP2VR_SIMULATE_SECONDS',$savedSimulation,'Process')
    [Environment]::SetEnvironmentVariable('HP2VR_MANUAL_MENU',$savedManualMenu,'Process')
    [Environment]::SetEnvironmentVariable('HP2VR_RUNTIME',$savedRuntime,'Process')
    if($companion){
        $companion.Refresh()
        if(!$companion.HasExited){
            Wait-Process -Id $companion.Id -Timeout 10 -ErrorAction SilentlyContinue
            $companion.Refresh()
        }
        if(!$companion.HasExited){Stop-Process -Id $companion.Id -Force}
    }
}
