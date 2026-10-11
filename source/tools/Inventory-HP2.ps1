[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string]$OutputRoot=(Join-Path (Split-Path $PSScriptRoot -Parent) 'baseline')
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$game=[IO.Path]::GetFullPath($GameRoot).TrimEnd('\','/')
if(!(Test-Path -LiteralPath (Join-Path $game 'System\Game.exe') -PathType Leaf)){throw "Not an HP2 game root: $game"}
if(!(Test-Path -LiteralPath (Join-Path $game 'Maps') -PathType Container)){throw "Maps folder missing: $game"}
$out=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
New-Item -ItemType Directory -Path $out -Force | Out-Null

function Record([string]$path,[string]$kind){
    $item=Get-Item -LiteralPath $path
    [ordered]@{
        kind=$kind
        path=$item.FullName.Substring($game.Length+1).Replace('\','/')
        bytes=$item.Length
        lastWriteUtc=$item.LastWriteTimeUtc.ToString('o')
        sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}

$coreNames=@('Game.exe','Core.dll','Engine.dll','Render.dll','Window.dll','WinDrv.dll','D3DDrv.dll','d3d11drv.dll','Effects11.dll','Core.u','Engine.u','hgame.u','UWindow.u')
$core=@()
foreach($name in $coreNames){$path=Join-Path $game "System\$name";if(Test-Path -LiteralPath $path){$core+=Record $path 'core'}}
$packages=@(Get-ChildItem -LiteralPath (Join-Path $game 'System') -File -Filter '*.u' | Sort-Object Name | ForEach-Object {Record $_.FullName 'package'})
$maps=@(Get-ChildItem -LiteralPath (Join-Path $game 'Maps') -File -Filter '*.unr' | Sort-Object Name | ForEach-Object {Record $_.FullName 'map'})
$versionPath=Join-Path $game 'System\Version.txt'
$version=if(Test-Path -LiteralPath $versionPath){@(Get-Content -LiteralPath $versionPath)}else{@()}
$config=@()
foreach($name in 'Game.ini','default.ini','User.ini'){
    $path=Join-Path $game "System\$name"
    if(Test-Path -LiteralPath $path){
        $lines=Select-String -LiteralPath $path -Pattern '^(GameRenderDevice|WindowedRenderDevice|RenderDevice|Language|SavePath|Map|LocalMap)='
        foreach($line in $lines){$config+=[ordered]@{file=$name;line=$line.LineNumber;value=$line.Line}}
    }
}
$inventory=[ordered]@{
    schema=1
    observedUtc=[DateTime]::UtcNow.ToString('o')
    gameRoot=$game
    access='read-only inventory; no source files changed'
    version=$version
    core=$core
    packages=$packages
    maps=$maps
    config=$config
}
$json=$inventory|ConvertTo-Json -Depth 7
[IO.File]::WriteAllText((Join-Path $out 'baseline.json'),$json)

$renderer=($core|Where-Object path -eq 'System/d3d11drv.dll'|Select-Object -First 1)
$gameExe=($core|Where-Object path -eq 'System/Game.exe'|Select-Object -First 1)
$hgame=($core|Where-Object path -eq 'System/hgame.u'|Select-Object -First 1)
$md=@"
# HP2 baseline

Observed: $($inventory.observedUtc)

- Reference game root: ``$game``
- Access: read-only inventory; no reference file changed
- Version: ``$($version -join ' / ')``
- Core architecture: x86 (verified separately from the PE header)
- Active renderer: ``D3D11Drv.D3D11RenderDevice``
- Renderer SHA-256: ``$($renderer.sha256)``
- Game.exe SHA-256: ``$($gameExe.sha256)``
- hgame.u SHA-256: ``$($hgame.sha256)``
- Script packages: $($packages.Count)
- Maps: $($maps.Count)

The installation is a graphics-modded baseline (DX11 1.6.2 plus 3,126 external
DDS replacements), not a stock baseline. ``baseline.json`` contains the complete
core/package/map hash inventory and selected configuration lines.

## Verification state

- Observed in installed files: complete.
- Previously documented flat DX11 run: the installation's ``MODS_INSTALLED.md``
  records a responsive 20-second ``PrivetDr.unr`` run on 2026-09-28.
- HP2 VR host flat run: not run yet; a separate marked private runtime is required.
- Stereo/headset/wand/gameplay: not implemented or tested yet.
"@
[IO.File]::WriteAllText((Join-Path $out 'BASELINE.md'),$md)
Write-Host "Wrote read-only HP2 inventory: $out"

