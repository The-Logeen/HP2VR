[CmdletBinding()]
param([string]$Runtime=(Join-Path (Split-Path $PSScriptRoot -Parent) 'runtime'))
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$project=Split-Path $PSScriptRoot -Parent
$runtimePath=[IO.Path]::GetFullPath($Runtime).TrimEnd('\','/')
$projectPath=[IO.Path]::GetFullPath($project).TrimEnd('\','/')
if(!$runtimePath.StartsWith($projectPath+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Gate 1 installs only into this project private runtime.'}
if(!(Test-Path -LiteralPath (Join-Path $runtimePath '.hp2vr-private-runtime') -PathType Leaf)){throw 'Private-runtime marker missing.'}
$source=Join-Path $project 'build\shfolder.dll'
$destination=Join-Path $runtimePath 'System\shfolder.dll'
if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw 'Build-Gate1.cmd must succeed before installation.'}
$sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$record=Join-Path $runtimePath 'VR\gate1-proxy.sha256'
if(Test-Path -LiteralPath $destination){
    $installedHash=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    if(Test-Path -LiteralPath $record){
        $recordedHash=[IO.File]::ReadAllText($record).Trim()
        if($installedHash -ne $recordedHash){throw 'Installed home proxy changed outside this installer; nothing overwritten.'}
    }else{
        $sourceRoot=((Get-Content -LiteralPath (Join-Path $runtimePath '.hp2vr-private-runtime'))|Where-Object {$_ -like 'source=*'}|Select-Object -First 1).Substring(7)
        if(Test-Path -LiteralPath (Join-Path $sourceRoot 'System\shfolder.dll')){throw 'The source game contains its own shfolder.dll; refusing to claim ownership.'}
    }
}
if(!(Test-Path -LiteralPath $destination) -or (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $sourceHash){
    $temporary=$destination+'.new'
    if(Test-Path -LiteralPath $temporary){throw "Stale temporary proxy requires review: $temporary"}
    Copy-Item -LiteralPath $source -Destination $temporary
    if((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $sourceHash){throw 'Temporary proxy copy failed verification.'}
    $old=$null
    if(Test-Path -LiteralPath $destination){
        $backup=Join-Path $runtimePath 'VR\gate1-backups'
        New-Item -ItemType Directory -Path $backup -Force|Out-Null
        $old=Join-Path $backup ('shfolder.dll.'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'.bak')
        Move-Item -LiteralPath $destination -Destination $old
    }
    try{Move-Item -LiteralPath $temporary -Destination $destination}catch{if($old -and (Test-Path -LiteralPath $old)){Move-Item -LiteralPath $old -Destination $destination};throw}
}
[IO.File]::WriteAllText($record,$sourceHash+"`r`n")

function Update-PrivateGameIni {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$SavePath,
        [Parameter(Mandatory)][string]$BackupTag
    )
    if(!(Test-Path -LiteralPath $Path -PathType Leaf)){return}
    $text=[IO.File]::ReadAllText($Path)
    $updated=$text
    $settings=[ordered]@{
        FirstRun='433'
        Reconfig='0'
        ForceSoftware='0'
        GameRenderDevice='D3D11Drv.D3D11RenderDevice'
        WindowedRenderDevice='D3D11Drv.D3D11RenderDevice'
        RenderDevice='D3D11Drv.D3D11RenderDevice'
        WindowedViewportX='1280'
        WindowedViewportY='960'
        WindowedColorBits='32'
        SavePath=$SavePath
    }
    foreach($setting in $settings.GetEnumerator()){
        $pattern='(?im)^'+[regex]::Escape([string]$setting.Key)+'=.*$'
        if(![regex]::IsMatch($updated,$pattern)){throw "Required setting $($setting.Key) is missing from $Path"}
        $updated=[regex]::Replace($updated,$pattern,([string]$setting.Key+'='+[string]$setting.Value))
    }
    # Renderer class defaults can load before the engine's Init override.
    # Pin quality in the private INI too, scoped to the actual D3D11 section.
    $updated=[regex]::Replace($updated,'(?ms)^\[D3D11Drv\.D3D11RenderDevice\]\r?\n.*?(?=^\[|\z)', {
        param($section)
        $quality=$section.Value
        foreach($pair in @(@('Antialiasing','4'),@('AlphaToCoverage','True'),@('Anisotropy','16'),@('FullMeshLOD','True'),@('DetailTextures','True'),@('LODBias','0'))){
            $quality=[regex]::Replace($quality,'(?m)^'+$pair[0]+'=[^\r\n]*',$pair[0]+'='+$pair[1])
        }
        return $quality
    })
    if($updated -eq $text){return}
    $backup=Join-Path $runtimePath 'VR\gate1-backups'
    New-Item -ItemType Directory -Path $backup -Force|Out-Null
    $stamp=[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
    $backupPath=Join-Path $backup ("Game.ini.$BackupTag.$stamp.bak")
    Copy-Item -LiteralPath $Path -Destination $backupPath
    $temporary=$Path+'.hp2vr-new'
    if(Test-Path -LiteralPath $temporary){throw "Stale temporary config requires review: $temporary"}
    try{
        [IO.File]::WriteAllText($temporary,$updated)
        Move-Item -LiteralPath $temporary -Destination $Path -Force
    }catch{
        if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}
        throw
    }
}

Update-PrivateGameIni -Path (Join-Path $runtimePath 'System\Game.ini') -SavePath '../Save' -BackupTag 'system'
Update-PrivateGameIni -Path (Join-Path $runtimePath 'Home\Harry Potter II\Game.ini') -SavePath (Join-Path $runtimePath 'Home\Harry Potter II\Save') -BackupTag 'home'
New-Item -ItemType Directory -Path (Join-Path $runtimePath 'Home\Harry Potter II'),(Join-Path $runtimePath 'Save') -Force|Out-Null
Write-Host "Installed Gate 1 isolation into private runtime: $runtimePath"
