[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

function Full([string]$path){[IO.Path]::GetFullPath($path).TrimEnd('\','/')}
function Under([string]$child,[string]$parent){
    $c=Full $child;$p=Full $parent
    $c.Equals($p,[StringComparison]::OrdinalIgnoreCase) -or $c.StartsWith($p+'\',[StringComparison]::OrdinalIgnoreCase)
}
$source=Full $GameRoot
$target=Full $Destination
if(!(Test-Path -LiteralPath (Join-Path $source 'System\Game.exe') -PathType Leaf)){throw 'GameRoot does not contain System\Game.exe.'}
if((Under $target $source) -or (Under $source $target)){throw 'The private runtime and source game must be separate folders.'}
if(Test-Path -LiteralPath $target){
    throw 'Destination must not already exist; nothing was overwritten.'
}
$baselinePath=Join-Path (Split-Path $PSScriptRoot -Parent) 'baseline\baseline.json'
if(!(Test-Path -LiteralPath $baselinePath -PathType Leaf)){throw 'Run Inventory-HP2.ps1 before staging a runtime.'}
$baseline=Get-Content -LiteralPath $baselinePath -Raw|ConvertFrom-Json
foreach($expected in $baseline.core){
    $path=Join-Path $source $expected.path.Replace('/','\')
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Required baseline file missing: $($expected.path)"}
    $actual=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if($actual -ne $expected.sha256){throw "Source changed since inventory: $($expected.path)"}
}
if(!$PSCmdlet.ShouldProcess($target,"copy a private HP2 runtime from $source")){return}
$staging=$target+'.staging-'+[Guid]::NewGuid().ToString('N')
New-Item -ItemType Directory -Path $staging | Out-Null
foreach($dir in 'System','Maps','Textures','Sounds','Music','help'){
    $from=Join-Path $source $dir
    if(!(Test-Path -LiteralPath $from)){continue}
    foreach($item in Get-ChildItem -LiteralPath $from -Recurse -Force){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Reparse points are not supported: $($item.FullName)"}
    }
    Copy-Item -LiteralPath $from -Destination $staging -Recurse
}
[IO.File]::WriteAllText((Join-Path $staging '.hp2vr-private-runtime'),"schema=1`r`nsource=$source`r`nstagedUtc=$([DateTime]::UtcNow.ToString('o'))`r`n")
New-Item -ItemType Directory -Path (Join-Path $staging 'Save'),(Join-Path $staging 'VR') | Out-Null
Move-Item -LiteralPath $staging -Destination $target
Write-Host "Private runtime staged without modifying the source: $target"

