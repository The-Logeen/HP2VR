$ErrorActionPreference='Stop'
$forbiddenNames=@('Game.exe','Core.dll','Engine.dll','Render.dll','Editor.dll','Window.dll','WinDrv.dll','D3D11Drv.dll','Effects11.dll','D3D11Drv.int','openvr_api.dll','vcredist_x86.exe','vcredist_2010_sp1_x86.exe','directx_Jun2010_redist.exe')
$files=@(Get-ChildItem -LiteralPath $PSScriptRoot -File -Recurse)
$blocked=@($files | Where-Object {$_.Name -in $forbiddenNames -or $_.Extension -in @('.dds','.fco','.fxh','.u','.utx','.unr','.uax','.umx','.ogg','.wav','.sav','.usa','.bmp','.log') -or $_.Extension -match '^\.[A-Za-z]{3}_uax$'})
if($blocked.Count){$blocked.FullName | Write-Output;throw 'Do not publish: external/base-game files were found in the package.'}
foreach($asset in @('payload\System\WandAtlas.pcx','payload\System\AvatarAtlas.pcx')){
    if(!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $asset))){throw "Missing retained mod asset: $asset"}
}
$rows=Import-Csv -LiteralPath (Join-Path $PSScriptRoot 'FILE-MANIFEST.csv')
foreach($row in $rows){
    $file=Join-Path $PSScriptRoot $row.path
    if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file).Hash -ne $row.sha256){throw "Manifest mismatch: $($row.path)"}
}
if($files.Count -ne $rows.Count+1){throw 'Unexpected unlisted files exist; review package before publication.'}
Write-Output "PASS: $($rows.Count) manifest entries verified; excluded third-party/base-game files absent; wand/avatar atlases retained."
