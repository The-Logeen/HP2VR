[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$target=(Resolve-Path -LiteralPath $Destination).Path
if(!(Test-Path -LiteralPath (Join-Path $target 'FILE-MANIFEST.csv'))){throw 'Expected release staging manifest is missing.'}
# Only generated configuration examples are altered, never the working runtime.
foreach($name in @('Game.ini','User.ini')){
    $source=Join-Path $project ('runtime\Home\Harry Potter II\'+$name)
    $text=[IO.File]::ReadAllText($source)
    $text=[regex]::Replace($text,'(?m)^SavePath=[^\r\n]*','SavePath=../Save')
    $text=[regex]::Replace($text,'(?m)^Paths=[A-Za-z]:[^\r\n]*','Paths=../Save/*.usa')
    [IO.File]::WriteAllText((Join-Path $target ('configuration\'+$name+'.example')),$text)
}
$manifest=Import-Csv -LiteralPath (Join-Path $target 'FILE-MANIFEST.csv')
$categories=@{}
foreach($entry in $manifest){$categories[$entry.path]=$entry.category}
$rows=@(foreach($file in Get-ChildItem -LiteralPath $target -File -Recurse){
    $relative=$file.FullName.Substring($target.Length+1).Replace('\','/')
    if($relative -eq 'FILE-MANIFEST.csv'){continue}
    if($file.Extension -in @('.u','.utx','.unr','.umx','.uax','.ogg','.wav','.sav') -or
       $file.Name -in @('Game.exe','Core.dll','Engine.dll','Render.dll','Window.dll','WinDrv.dll','D3DDrv.dll','SoftDrv.dll','Fire.dll','Editor.dll')){
        throw "Excluded game file found: $relative"
    }
    $category=$categories[$relative]
    if(!$category){$category=if($relative.StartsWith('prerequisites/')){'official-microsoft-installer'}else{'generated-configuration-or-metadata'}}
    [pscustomobject]@{path=$relative;category=$category;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
})
$rows | Export-Csv -LiteralPath (Join-Path $target 'FILE-MANIFEST.csv') -NoTypeInformation -Encoding UTF8
"Final manifest: $($rows.Count) files, $(($rows | Measure-Object bytes -Sum).Sum) bytes; no excluded base-game files."
