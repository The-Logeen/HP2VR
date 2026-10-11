[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$project=Split-Path $PSScriptRoot -Parent
$runtime=Join-Path $project 'runtime'
$destinationPath=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $destinationPath){throw 'Destination already exists; nothing overwritten.'}
$manifest=[Collections.Generic.List[object]]::new()
New-Item -ItemType Directory -Path $destinationPath | Out-Null
[IO.File]::WriteAllText((Join-Path $destinationPath 'STAGING-INCOMPLETE.txt'),'Incomplete until packaging verification finishes.')
function Add-File([string]$Source,[string]$Relative,[string]$Category){
    $item=Get-Item -LiteralPath $Source
    if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Not a regular file: $Source"}
    $target=Join-Path $destinationPath $Relative
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    if(Test-Path -LiteralPath $target){throw "Duplicate output: $Relative"}
    $before=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $Source -Destination $target
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $before){throw "Copy verification failed: $Relative"}
    $manifest.Add([pscustomobject]@{path=$Relative.Replace('\','/');category=$Category;bytes=$item.Length;sha256=$before})
}
foreach($dir in @('src','tests','tools','docs','assets','vr')){
    $root=Join-Path $project $dir
    foreach($file in Get-ChildItem -LiteralPath $root -File -Recurse){
        if($file.Extension -notin @('.cpp','.h','.def','.ps1','.cmd','.py','.md','.json','.pcx','.cfg')){continue}
        Add-File $file.FullName ('source\'+$dir+'\'+$file.FullName.Substring($root.Length+1)) 'source-and-mod-assets'
    }
}
foreach($file in Get-ChildItem -LiteralPath $project -File){
    if($file.Extension -eq '.cmd' -or $file.Name -in @('README.md','.gitignore')){
        Add-File $file.FullName ('source\'+$file.Name) 'source-build-launch-files'
    }
}
# Explicit allowlist: never copy the System directory wholesale.
foreach($name in @('d3d11.dll','HP2VREngine.dll','HP2VRProbe.dll','shfolder.dll','WandAtlas.pcx','AvatarAtlas.pcx','tessellation.cfg')){
    Add-File (Join-Path $runtime ('System\'+$name)) ('payload\System\'+$name) 'mod-runtime'
}
foreach($name in @('d3d11drv.dll','Effects11.dll','D3D11Drv.int')){
    Add-File (Join-Path $runtime ('System\'+$name)) ('payload\System\'+$name) 'third-party-renderer'
}
$shaderRoot=Join-Path $runtime 'System\d3d11drv'
foreach($file in Get-ChildItem -LiteralPath $shaderRoot -File -Recurse){
    Add-File $file.FullName ('payload\System\d3d11drv\'+$file.FullName.Substring($shaderRoot.Length+1)) 'third-party-renderer-shader'
}
foreach($name in @('HP2VRCompanion.exe','openvr_api.dll','actions_hp2_v1.json','bindings_hp2_oculus_touch.json','bindings_hp2_knuckles.json','bindings_hp2_vive_controller.json')){
    Add-File (Join-Path $runtime ('VR\'+$name)) ('payload\VR\'+$name) 'vr-runtime'
}
foreach($name in @('HP2VRHost.exe','HP2VRInspector.exe','HP2PackageInspector.exe')){
    Add-File (Join-Path $project ('build\'+$name)) ('utilities\'+$name) 'mod-utility'
}
$textureRoot=Join-Path $runtime 'Textures'
foreach($file in Get-ChildItem -LiteralPath $textureRoot -File -Recurse -Filter '*.dds'){
    Add-File $file.FullName ('payload\Textures\'+$file.FullName.Substring($textureRoot.Length+1)) 'third-party-esrgan-replacement'
}
Add-File (Join-Path $runtime 'System\HP2VRSettings.ini') 'configuration\HP2VRSettings.example.ini' 'current-vr-preference-example'
if(!$env:HP2VR_OPENVR_SDK){throw 'Set HP2VR_OPENVR_SDK to your extracted OpenVR SDK folder. See BUILDING.md.'}
$openvr=Join-Path $env:HP2VR_OPENVR_SDK 'LICENSE'
Add-File $openvr 'third-party\OpenVR-LICENSE.txt' 'license'
Add-File (Join-Path $project 'packaging\README.md') 'README.md' 'packaging-instructions'
Add-File (Join-Path $project 'packaging\DEPENDENCIES.md') 'DEPENDENCIES.md' 'dependency-notices'
$portable=Join-Path $project 'packaging\portable'
if(Test-Path -LiteralPath $portable){
    foreach($file in Get-ChildItem -LiteralPath $portable -File -Recurse){
        Add-File $file.FullName ($file.FullName.Substring($portable.Length+1)) 'portable-installer'
    }
}
# Export a data-only compatibility list, without original machine paths.
$baseline=Get-Content -LiteralPath (Join-Path $project 'baseline\baseline.json') -Raw | ConvertFrom-Json
$compatibility=[ordered]@{version=$baseline.version;files=@($baseline.core | Select-Object path,bytes,sha256)}
$compatibility | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destinationPath 'SUPPORTED-FILE-HASHES.json') -Encoding UTF8
$forbidden=@(Get-ChildItem -LiteralPath $destinationPath -File -Recurse | Where-Object {
    $_.Extension -in @('.u','.utx','.unr','.umx','.uax','.ogg','.wav','.sav') -or
    $_.Name -in @('Game.exe','Core.dll','Engine.dll','Render.dll','Window.dll','WinDrv.dll','D3DDrv.dll','SoftDrv.dll','Fire.dll','Editor.dll')
})
if($forbidden.Count){throw 'Base-game exclusion audit failed.'}
$manifest | Export-Csv -LiteralPath (Join-Path $destinationPath 'FILE-MANIFEST.csv') -NoTypeInformation -Encoding UTF8
# The exact staging marker is the only file deleted; no source/runtime cleanup.
Remove-Item -LiteralPath (Join-Path $destinationPath 'STAGING-INCOMPLETE.txt')
$total=($manifest | Measure-Object bytes -Sum).Sum
Write-Output "Verified $($manifest.Count) copied files; $total bytes. Base-game exclusion audit passed."
Write-Output "Staging folder: $destinationPath"
