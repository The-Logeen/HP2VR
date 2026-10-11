[CmdletBinding()]
param([string]$Runtime=(Join-Path (Split-Path $PSScriptRoot -Parent) 'runtime'))
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$project=Split-Path $PSScriptRoot -Parent
$runtimePath=[IO.Path]::GetFullPath($Runtime).TrimEnd('\','/')
$projectPath=[IO.Path]::GetFullPath($project).TrimEnd('\','/')
if(!$runtimePath.StartsWith($projectPath+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Gate 2 installs only into this project private runtime.'}
if(!(Test-Path -LiteralPath (Join-Path $runtimePath '.hp2vr-private-runtime') -PathType Leaf)){throw 'Private-runtime marker missing.'}

function Install-OwnedFile([string]$BuildName,[string]$RuntimeRelative,[string]$RecordName){
    $source=Join-Path $project ('build\'+$BuildName)
    $destination=Join-Path $runtimePath $RuntimeRelative
    $runtimeName=[IO.Path]::GetFileName($RuntimeRelative)
    $record=Join-Path $runtimePath ('VR\'+$RecordName)
    if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw "Build-Gate2.cmd must produce $BuildName before installation."}
    $sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash

    if(Test-Path -LiteralPath $destination){
        if(!(Test-Path -LiteralPath $record -PathType Leaf)){throw "An unowned $RuntimeName already exists in the private runtime; refusing to overwrite it."}
        $installedHash=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $recordedHash=[IO.File]::ReadAllText($record).Trim()
        if($installedHash -ne $recordedHash){throw "Installed $RuntimeName changed outside this installer; nothing overwritten."}
    }

    if(!(Test-Path -LiteralPath $destination) -or (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $sourceHash){
        $temporary=$destination+'.hp2vr-new'
        if(Test-Path -LiteralPath $temporary){throw "Stale temporary file requires review: $temporary"}
        Copy-Item -LiteralPath $source -Destination $temporary
        if((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $sourceHash){throw "Temporary $RuntimeName copy failed verification."}
        $old=$null
        if(Test-Path -LiteralPath $destination){
            $backup=Join-Path $runtimePath 'VR\gate2-backups'
            New-Item -ItemType Directory -Path $backup -Force|Out-Null
            $old=Join-Path $backup ($RuntimeName+'.'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'.bak')
            Move-Item -LiteralPath $destination -Destination $old
        }
        try{Move-Item -LiteralPath $temporary -Destination $destination}catch{
            if($old -and (Test-Path -LiteralPath $old)){Move-Item -LiteralPath $old -Destination $destination}
            throw
        }
    }
    [IO.File]::WriteAllText($record,$sourceHash+"`r`n")
    Write-Host "Installed owned Gate 2 file: $destination"
}

Install-OwnedFile 'd3d11.dll' 'System\d3d11.dll' 'gate2-d3d11-proxy.sha256'
Install-OwnedFile 'WandAtlas.pcx' 'System\WandAtlas.pcx' 'gate2-wand-atlas.sha256'
Install-OwnedFile 'AvatarAtlas.pcx' 'System\AvatarAtlas.pcx' 'gate2-avatar-atlas.sha256'
Install-OwnedFile 'tessellation.cfg' 'System\tessellation.cfg' 'gate2-tessellation-exclusions.sha256'
Install-OwnedFile 'HP2VRProbe.dll' 'System\HP2VRProbe.dll' 'gate2-live-probe.sha256'
Install-OwnedFile 'HP2VRCompanion.exe' 'VR\HP2VRCompanion.exe' 'gate2-companion.sha256'
Install-OwnedFile 'openvr_api.dll' 'VR\openvr_api.dll' 'gate2-openvr-api.sha256'
Install-OwnedFile 'actions_hp2_v1.json' 'VR\actions_hp2_v1.json' 'gate2-actions.sha256'
Install-OwnedFile 'bindings_hp2_oculus_touch.json' 'VR\bindings_hp2_oculus_touch.json' 'gate2-bindings-touch.sha256'
Install-OwnedFile 'bindings_hp2_knuckles.json' 'VR\bindings_hp2_knuckles.json' 'gate2-bindings-index.sha256'
Install-OwnedFile 'bindings_hp2_vive_controller.json' 'VR\bindings_hp2_vive_controller.json' 'gate2-bindings-vive.sha256'
Write-Host 'Gate 2 transport, live ABI probe, and x64 SteamVR companion are installed only in the private runtime.'
