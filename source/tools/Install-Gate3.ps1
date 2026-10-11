[CmdletBinding()]
param(
    [string]$Runtime=(Join-Path (Split-Path $PSScriptRoot -Parent) 'runtime'),
    [switch]$Rollback,
    [ValidateSet('Both','RenderOnly','EngineOnly')][string]$Mode='Both'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$project=Split-Path $PSScriptRoot -Parent
$runtimePath=[IO.Path]::GetFullPath($Runtime).TrimEnd('\','/')
$projectPath=[IO.Path]::GetFullPath($project).TrimEnd('\','/')
if(!$runtimePath.StartsWith($projectPath+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Gate 3 installs only into this project private runtime.'}
if(!(Test-Path -LiteralPath (Join-Path $runtimePath '.hp2vr-private-runtime') -PathType Leaf)){throw 'Private-runtime marker missing.'}
if(Get-Process Game -ErrorAction SilentlyContinue | Where-Object {$_.Path -and [IO.Path]::GetFullPath($_.Path).StartsWith($runtimePath+'\',[StringComparison]::OrdinalIgnoreCase)}){throw 'Close the private HP2 Game.exe before installing Gate 3.'}

if($Rollback){
    $backupRoot=Join-Path $runtimePath 'VR\gate3-backups'
    foreach($scope in @('System','Home')){
        $backup=Get-ChildItem -LiteralPath $backupRoot -File -Filter "Game.ini.$scope.*.bak" | Sort-Object Name -Descending | Select-Object -First 1
        if(!$backup){throw "No Gate 3 $scope backup is available."}
        $destination=if($scope -eq 'System'){Join-Path $runtimePath 'System\Game.ini'}else{Join-Path $runtimePath 'Home\Harry Potter II\Game.ini'}
        Copy-Item -LiteralPath $backup.FullName -Destination $destination -Force
        Write-Host "Restored $scope private Game.ini from $($backup.Name)"
    }
    [IO.File]::WriteAllText((Join-Path $runtimePath 'VR\gate3-engine-integration.sha256'),"ROLLED BACK`r`n")
    return
}

$sourceModule=Join-Path $project 'build\HP2VREngine.dll'
$installedModule=Join-Path $runtimePath 'System\HP2VREngine.dll'
$moduleRecord=Join-Path $runtimePath 'VR\gate3-engine-package.sha256'
$backupRoot=Join-Path $runtimePath 'VR\gate3-backups'
New-Item -ItemType Directory -Path $backupRoot -Force|Out-Null
if(!(Test-Path -LiteralPath $sourceModule -PathType Leaf)){throw 'Build-Gate3.cmd must succeed before installation.'}
$moduleHash=(Get-FileHash -LiteralPath $sourceModule -Algorithm SHA256).Hash
if(Test-Path -LiteralPath $installedModule){
    if(!(Test-Path -LiteralPath $moduleRecord -PathType Leaf)){throw 'An unowned HP2VREngine.dll exists; refusing to overwrite it.'}
    if((Get-FileHash -LiteralPath $installedModule -Algorithm SHA256).Hash -ne [IO.File]::ReadAllText($moduleRecord).Trim()){throw 'Installed HP2VREngine.dll changed outside this installer.'}
}
$moduleTemporary=$installedModule+'.hp2vr-new'
if(Test-Path -LiteralPath $moduleTemporary){throw "Stale Gate 3 module temporary requires review: $moduleTemporary"}
Copy-Item -LiteralPath $sourceModule -Destination $moduleTemporary
if((Get-FileHash -LiteralPath $moduleTemporary -Algorithm SHA256).Hash -ne $moduleHash){throw 'Gate 3 module copy failed verification.'}
if(Test-Path -LiteralPath $installedModule){
    $moduleBackup=Join-Path $backupRoot ('HP2VREngine.dll.'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'.bak')
    Move-Item -LiteralPath $installedModule -Destination $moduleBackup
}
Move-Item -LiteralPath $moduleTemporary -Destination $installedModule
[IO.File]::WriteAllText($moduleRecord,$moduleHash+"`r`n")

$configs=@(
    (Join-Path $runtimePath 'System\Game.ini'),
    (Join-Path $runtimePath 'Home\Harry Potter II\Game.ini')
)
$changes=[ordered]@{
    'GameEngine'=if($Mode -eq 'RenderOnly'){'Engine.GameEngine'}else{'HP2VREngine.HP2VRGameEngine'}
    'Render'=if($Mode -eq 'EngineOnly'){'Render.Render'}else{'HP2VREngine.HP2VRRender'}
}
$prepared=@()
foreach($config in $configs){
    if(!(Test-Path -LiteralPath $config -PathType Leaf)){throw "Missing private configuration: $config"}
    $lines=[Collections.Generic.List[string]]::new()
    $lines.AddRange([string[]][IO.File]::ReadAllLines($config))
    foreach($key in $changes.Keys){
        $indices=@()
        for($i=0;$i -lt $lines.Count;$i++){if($lines[$i] -match ('^'+[regex]::Escape($key)+'=')){$indices+=$i}}
        if($indices.Count -ne 1){throw "Expected exactly one $key entry in $config; found $($indices.Count)."}
        $lines[$indices[0]]=$key+'='+$changes[$key]
    }
    $temporary=$config+'.hp2vr-gate3-new'
    if(Test-Path -LiteralPath $temporary){throw "Stale Gate 3 temporary file requires review: $temporary"}
    [IO.File]::WriteAllLines($temporary,$lines,[Text.UTF8Encoding]::new($false))
    $prepared += [pscustomobject]@{Config=$config;Temporary=$temporary;Backup=$null}
}

$stamp=[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
try{
    foreach($item in $prepared){
        $scope=if($item.Config -like '*\System\Game.ini'){'System'}else{'Home'}
        $item.Backup=Join-Path $backupRoot ("Game.ini.$scope.$stamp.bak")
        Copy-Item -LiteralPath $item.Config -Destination $item.Backup
        Move-Item -LiteralPath $item.Temporary -Destination $item.Config -Force
    }
}catch{
    foreach($item in $prepared){
        if($item.Backup -and (Test-Path -LiteralPath $item.Backup)){Copy-Item -LiteralPath $item.Backup -Destination $item.Config -Force}
        if(Test-Path -LiteralPath $item.Temporary){Remove-Item -LiteralPath $item.Temporary}
    }
    throw
}
[IO.File]::WriteAllText((Join-Path $runtimePath 'VR\gate3-engine-integration.sha256'),$moduleHash+"`r`n")
Write-Host "Installed HP2 VR Gate 3 mode $Mode into both private Game.ini files."
