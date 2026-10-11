[CmdletBinding()]
param(
    [string]$Snapshot=(Join-Path (Split-Path $PSScriptRoot -Parent) 'baseline\external-before-gate1.json'),
    [string]$ExternalRoot=(Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Harry Potter II')
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$before=@(Get-Content -LiteralPath $Snapshot -Raw|ConvertFrom-Json)
$differences=@()
foreach($record in $before){
    if(!(Test-Path -LiteralPath $record.Path -PathType Leaf)){$differences+="REMOVED $($record.Path)";continue}
    $item=Get-Item -LiteralPath $record.Path
    $hash=(Get-FileHash -LiteralPath $record.Path -Algorithm SHA256).Hash
    if([int64]$item.Length -ne [int64]$record.Length -or $hash -ne [string]$record.SHA256){$differences+="CHANGED $($record.Path)"}
}
$known=@($before|ForEach-Object Path)
foreach($file in Get-ChildItem -LiteralPath $ExternalRoot -Recurse -File){if($file.FullName -notin $known){$differences+="ADDED $($file.FullName)"}}
if($differences.Count){$differences|ForEach-Object {Write-Error $_};exit 1}
Write-Host "External isolation verified: $($before.Count) files unchanged by length and SHA-256; no files added or removed."
