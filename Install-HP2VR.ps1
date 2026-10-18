[CmdletBinding()]
param([string]$GameRoot,[string]$Destination,[string]$DependencyRoot,[string]$OpenVRDll,[string]$TextureRoot,[string]$SourceInstallation,[switch]$ValidateOnly,[string]$StatusFile)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Set-SetupPhase([string]$Name){$global:HP2SetupPhase=$Name;Write-Host ("[{0}] {1}" -f [DateTime]::Now.ToString('HH:mm:ss'),$Name)}
Set-SetupPhase 'Load setup helpers'
. (Join-Path $PSScriptRoot 'installer\Setup.Common.ps1')
foreach($name in @('GameRoot','Destination','DependencyRoot','OpenVRDll','TextureRoot','SourceInstallation')){
    Set-Variable -Name $name -Value (Normalize-SetupPath (Get-Variable -Name $name -ValueOnly))
}
if($StatusFile){
    Set-SetupPhase 'Check requirements and write status report'
    $report=Get-HP2SetupStatus -Package $PSScriptRoot -GameRoot $GameRoot -RendererRoot $DependencyRoot -OpenVRDll $OpenVRDll -TextureRoot $TextureRoot -Destination $Destination -SourceInstallation $SourceInstallation
    foreach($item in $report.Items){Write-Host "CHECK $($item.Name): $($item.State) - $($item.Detail)"}
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $StatusFile -Encoding UTF8
    return
}
if($SourceInstallation){
    Set-SetupPhase 'Validate update source'
    $SourceInstallation=(Resolve-Path -LiteralPath $SourceInstallation).Path.TrimEnd('\')
    Assert-NoLinks $SourceInstallation
    if(!(Test-Path (Join-Path $SourceInstallation 'installation.json')) -or !(Test-Path (Join-Path $SourceInstallation 'runtime\.hp2vr-private-runtime')) -or (Test-Path (Join-Path $SourceInstallation 'INSTALL-INCOMPLETE.txt'))){throw 'Select a complete installed HP2VR folder.'}
    $GameRoot=Join-Path $SourceInstallation 'runtime'
    $DependencyRoot=$GameRoot
    $OpenVRDll=Join-Path $GameRoot 'VR\openvr_api.dll'
}
if(![Environment]::Is64BitOperatingSystem){throw 'HP2VR requires 64-bit Windows for its VR companion.'}
foreach($dependency in @('msvcp100.dll','msvcr100.dll','d3dx11_43.dll')){
    if(!(Test-Path -LiteralPath (Join-Path $env:WINDIR ('SysWOW64\'+$dependency)))){
        Write-Warning "Missing 32-bit runtime dependency $dependency. See DOWNLOADS-AND-INSTALL.md for Microsoft's downloads."
    }
}
function Choose-Folder([string]$Title){
    Add-Type -AssemblyName System.Windows.Forms
    $dialog=New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description=$Title
    try{if($dialog.ShowDialog() -ne 'OK'){throw 'Installation cancelled.'};return $dialog.SelectedPath}finally{$dialog.Dispose()}
}
if(!$GameRoot){$GameRoot=Choose-Folder 'Select your HP2 game folder (the folder containing System and Maps).'}
Set-SetupPhase 'Validate game path and destination'
$game=(Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
if(!(Test-Path -LiteralPath (Join-Path $game 'System\Game.exe'))){throw 'Select the game folder containing System\Game.exe, not System itself.'}
if(!$Destination){$Destination=Join-Path $PSScriptRoot 'Installed-HP2VR'}
$target=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
Write-Host "Resolved source: $game"
Write-Host "Resolved destination: $target"
try{$drive=[IO.DriveInfo]::new([IO.Path]::GetPathRoot($target));Write-Host "Destination volume: $($drive.Name); available bytes: $($drive.AvailableFreeSpace)"}catch{Write-Warning 'Could not query destination free space; copy errors will be reported if space is unavailable.'}
if($SourceInstallation -and ($target -eq $SourceInstallation -or $target.StartsWith($SourceInstallation+'\',[StringComparison]::OrdinalIgnoreCase))){throw 'Update destination must be outside the old installation.'}
if($target -eq $game -or $game.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Destination must not contain or replace the source game.'}
foreach($folder in @('System','Maps','Textures','Sounds','Music','help')){
    $sourceFolder=Join-Path $game $folder
    if($target -eq $sourceFolder -or $target.StartsWith($sourceFolder+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Destination cannot be inside a source data directory.'}
}
if(Test-Path -LiteralPath $target){throw 'Destination already exists. Choose a new empty installation path; saves and existing installs are never overwritten.'}
if(Get-Process Game,HP2VRCompanion -ErrorAction SilentlyContinue){throw 'Close HP2 and HP2VR before installing.'}
# Use the exact same base-game identities enforced by the native safety host.
$identity=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'source\src\HP2BuildIdentity.h') -Raw
Set-SetupPhase 'Verify supported base-game hashes'
$matches=[regex]::Matches($identity,'\{L"(System\\\\[^"\r\n]+)",\s*L"([A-F0-9]{64})"\}')
if($matches.Count -ne 9){throw 'Invalid supported-build identity list.'}
foreach($match in $matches){
    $relative=$match.Groups[1].Value.Replace('\\','\')
    $path=Join-Path $game $relative
    if(!(Test-Path -LiteralPath $path)){throw "Missing base-game file: $path"}
    $actual=(Get-FileHash -LiteralPath $path).Hash
    Write-Host "Game file: $path; expected=$($match.Groups[2].Value); actual=$actual"
    if($actual -ne $match.Groups[2].Value){throw "Unsupported game version or changed file: $relative. No installation created."}
}
Write-Host 'Supported HP2 game version verified.'
# External components are supplied by the user, never shipped in this package.
if(!$DependencyRoot){$DependencyRoot=$game}
$DependencyRoot=(Resolve-Path -LiteralPath $DependencyRoot).Path
if(!$OpenVRDll){$OpenVRDll=Join-Path $PSScriptRoot 'Dependencies\VR\openvr_api.dll'}
$OpenVRDll=Resolve-OpenVRFile $OpenVRDll
if(!(Test-Path -LiteralPath $OpenVRDll) -and !$ValidateOnly){
    Add-Type -AssemblyName System.Windows.Forms
    $picker=New-Object System.Windows.Forms.OpenFileDialog
    $picker.Title='Select OpenVR SDK 2.15.6 bin\win64\openvr_api.dll (see DOWNLOADS-AND-INSTALL.md)'
    $picker.Filter='OpenVR runtime (openvr_api.dll)|openvr_api.dll'
    try{if($picker.ShowDialog() -eq 'OK'){$OpenVRDll=$picker.FileName}}finally{$picker.Dispose()}
}
$external=Import-Csv -LiteralPath (Join-Path $PSScriptRoot 'DEPENDENCY-HASHES.csv')
Set-SetupPhase 'Verify external renderer, shaders and OpenVR'
foreach($entry in $external){
    $path=if($entry.path -eq 'VR/openvr_api.dll'){$OpenVRDll}else{Resolve-RendererFile $DependencyRoot $entry.path}
    $actual=if(Test-Path -LiteralPath $path -PathType Leaf){(Get-FileHash -LiteralPath $path).Hash}else{'MISSING'}
    Write-Host "External file: $path; expected=$($entry.sha256); actual=$actual"
    if($actual -ne $entry.sha256){
        throw "Missing or unsupported external component: $($entry.path). See DOWNLOADS-AND-INSTALL.md. No installation created."
    }
}
# Verify every shipped installation input before copying or launching it.
$manifest=Import-Csv -LiteralPath (Join-Path $PSScriptRoot 'FILE-MANIFEST.csv')
Set-SetupPhase 'Verify shipped mod files against manifest'
foreach($entry in $manifest){
    if($entry.path -notmatch '^(payload/|utilities/|configuration/|installer/|source/tools/Start-HP2VR.ps1$)'){continue}
    $path=Join-Path $PSScriptRoot $entry.path
    $actual=if(Test-Path -LiteralPath $path -PathType Leaf){(Get-FileHash -LiteralPath $path).Hash}else{'MISSING'}
    Write-Host "PACKAGE CHECK $path expected=$($entry.sha256) actual=$actual"
    if($actual -ne $entry.sha256){throw "Missing or modified package file: $path; expected=$($entry.sha256); actual=$actual. Check security history if the file disappeared."}
}
if($TextureRoot){
    $TextureRoot=Resolve-TextureFolder $TextureRoot
    Assert-NoLinks $TextureRoot
    if(!@(Get-ChildItem -LiteralPath $TextureRoot -Recurse -File -Filter '*.dds').Count){throw 'Selected texture folder contains no DDS files.'}
}
if($ValidateOnly){Write-Host 'Preflight passed; nothing installed.';return}
Set-SetupPhase 'Create destination and incomplete-install marker'
New-Item -ItemType Directory -Path $target | Out-Null
$runtime=Join-Path $target 'runtime'
New-Item -ItemType Directory -Path $runtime,(Join-Path $target 'build'),(Join-Path $target 'tools') | Out-Null
[IO.File]::WriteAllText((Join-Path $target 'INSTALL-INCOMPLETE.txt'),'Do not launch until installation finishes. On failure, keep this folder for diagnosis and choose another destination for retry.')
# Copy only game data and known runtime libraries, never saved games or logs.
$systemDlls=@('ALAudio.dll','Core.dll','Engine.dll','Render.dll','Window.dll','WinDrv.dll','D3DDrv.dll','SoftDrv.dll','Fire.dll','Editor.dll','ogg.dll','ogg_d.dll','vorbis.dll','vorbis_d.dll','vorbisfile.dll','vorbisfile_d.dll','OpenAL32.dll')
foreach($folder in @('System','Maps','Textures','Sounds','Music','help')){
    Set-SetupPhase "Copy base-game folder: $folder"
    $root=Join-Path $game $folder
    New-Item -ItemType Directory -Path (Join-Path $runtime $folder) -Force | Out-Null
    if($SourceInstallation -and !(Test-Path -LiteralPath $root) -and $folder -in @('Sounds','Music','help')){continue}
    if(!(Test-Path -LiteralPath $root)){throw "Missing game data directory: $folder"}
    if((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked source directories are not supported.'}
    foreach($item in Get-ChildItem -LiteralPath $root -Recurse -Force){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Linked source item is not supported: $($item.FullName)"}
        if($item.PSIsContainer){continue}
        $relative=$item.FullName.Substring($root.Length+1)
        $include=if($folder -eq 'System'){
            $item.Extension -in @('.u','.int','.usa','.che','.gre','.jap','.kor','.pol','.rus','.sim','.tha','.tra','.exec','.bmp','.ico') -or
            ($relative -notmatch '\\' -and ($item.Name -in $systemDlls -or $item.Name -in @('Game.exe','Version.txt')))
        }else{$item.Extension -in @('.unr','.utx','.uax','.umx','.ogg','.wav','.bmp') -or $item.Extension -match '^\.[A-Za-z]{3}_uax$' -or ($SourceInstallation -and $folder -eq 'Textures' -and $item.Extension -eq '.dds')}
        if(!$include){continue}
        $to=Join-Path $runtime ($folder+'\'+$relative)
        New-Item -ItemType Directory -Path (Split-Path $to -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $item.FullName -Destination $to
        Write-Host "COPIED $($item.FullName) -> $to ($($item.Length) bytes)"
    }
}
Write-Host 'Game data copied to isolated runtime; original game and saves were not modified.'
Set-SetupPhase 'Copy verified external dependencies'
foreach($entry in $external){
    $from=if($entry.path -eq 'VR/openvr_api.dll'){$OpenVRDll}else{Resolve-RendererFile $DependencyRoot $entry.path}
    $to=Join-Path $runtime $entry.path
    New-Item -ItemType Directory -Path (Split-Path $to -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $from -Destination $to -Force
    if((Get-FileHash -LiteralPath $to).Hash -ne $entry.sha256){throw "External component copy failed: $($entry.path)"}
}
foreach($folder in @('System','VR','Textures')){
    Set-SetupPhase "Copy mod payload: $folder"
    $root=Join-Path $PSScriptRoot ('payload\'+$folder)
    if(!(Test-Path -LiteralPath $root)){continue}
    foreach($item in Get-ChildItem -LiteralPath $root -File -Recurse){
        $to=Join-Path $runtime ($folder+'\'+$item.FullName.Substring($root.Length+1))
        New-Item -ItemType Directory -Path (Split-Path $to -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $item.FullName -Destination $to -Force
    }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'utilities\HP2VRHost.exe') -Destination (Join-Path $target 'build\HP2VRHost.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'payload\System\shfolder.dll') -Destination (Join-Path $target 'build\shfolder.dll')
$hp2Home=Join-Path $runtime 'Home\Harry Potter II'
Set-SetupPhase 'Create private saves and configuration'
New-Item -ItemType Directory -Path $hp2Home,(Join-Path $hp2Home 'Save'),(Join-Path $runtime 'Save') -Force | Out-Null
$ini=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'configuration\Game.ini.example'))
$ini=[regex]::Replace($ini,'(?m)^Paths=[A-Za-z]:[^\r\n]*','Paths=../Save/*.usa')
$ini=[regex]::Replace($ini,'(?m)^SavePath=[^\r\n]*','SavePath=../Save')
[IO.File]::WriteAllText((Join-Path $runtime 'System\Game.ini'),$ini)
[IO.File]::WriteAllText((Join-Path $runtime 'System\Default.ini'),$ini)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'configuration\User.ini.example') -Destination (Join-Path $runtime 'System\DefUser.ini')
$homeIni=[regex]::Replace($ini,'(?m)^SavePath=[^\r\n]*',('SavePath='+ (Join-Path $hp2Home 'Save')))
[IO.File]::WriteAllText((Join-Path $hp2Home 'Game.ini'),$homeIni)
foreach($root in @($hp2Home,(Join-Path $runtime 'System'))){Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'configuration\User.ini.example') -Destination (Join-Path $root 'User.ini')}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'configuration\HP2VRSettings.example.ini') -Destination (Join-Path $runtime 'System\HP2VRSettings.ini')
if($TextureRoot){
    foreach($file in Get-ChildItem -LiteralPath $TextureRoot -Recurse -File -Filter '*.dds'){
        Set-SetupPhase "Copy optional HD texture: $($file.FullName)"
        $to=Join-Path $runtime ('Textures\'+$file.FullName.Substring($TextureRoot.TrimEnd('\').Length+1))
        New-Item -ItemType Directory -Path (Split-Path $to) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $to -Force
    }
    Write-Host 'Selected HD textures copied.'
}
if($SourceInstallation){
    foreach($relative in @('Home\Harry Potter II\Save','Save')){
        Set-SetupPhase "Migrate save directory: $relative"
        $old=Join-Path $game $relative
        if(Test-Path -LiteralPath $old){Get-ChildItem -LiteralPath $old -Force | Copy-Item -Destination (Join-Path $runtime $relative) -Recurse -Force}
    }
    foreach($relative in @('System\HP2VRSettings.ini','System\User.ini','Home\Harry Potter II\User.ini')){
        $old=Join-Path $game $relative
        if(Test-Path -LiteralPath $old){Copy-Item -LiteralPath $old -Destination (Join-Path $runtime $relative) -Force}
    }
    Write-Host 'Saves, input preferences and VR settings migrated; old installation unchanged. Renderer/game defaults refreshed.'
}
[IO.File]::WriteAllText((Join-Path $runtime '.hp2vr-private-runtime'),"schema=1`r`nsource=$game`r`n")
# Keep the tested launcher, but remove its development-time installer dependency.
$launcher=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'source\tools\Start-HP2VR.ps1'))
Set-SetupPhase 'Generate installed launcher and first-run guide'
$launcher=$launcher.Replace("[string]`$Runtime=(Join-Path (Split-Path `$PSScriptRoot -Parent) 'runtime'),",'[string]$Runtime,')
$launcher=$launcher.Replace('Set-StrictMode -Version Latest',"Set-StrictMode -Version Latest`r`nif(!`$Runtime){`$Runtime=Join-Path (Split-Path `$PSScriptRoot -Parent) 'runtime'}")
$launcher=$launcher.Replace("    & (Join-Path `$PSScriptRoot 'Install-Gate1.ps1') -Runtime `$runtimePath",'    # Preconfigured by the portable installer; no build-folder reinstall.')
if($launcher.Contains("'Install-Gate1.ps1'")){throw 'Unexpected launcher format; installation stopped.'}
[IO.File]::WriteAllText((Join-Path $target 'tools\Start-HP2VR.ps1'),$launcher)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'installer\Start-HP2VR.cmd') -Destination (Join-Path $target 'Start-HP2VR.cmd')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'installer\INSTALLED-README.txt') -Destination (Join-Path $target 'README.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'installer\FIRST-RUN.html') -Destination (Join-Path $target 'FIRST-RUN.html')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SETUP-GUIDE.html') -Destination (Join-Path $target 'SETUP-GUIDE.html')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'FRAME-CONTROLS.md') -Destination (Join-Path $target 'FRAME-CONTROLS.md')
# Verify payload survived copying (including real-time antivirus scanning).
foreach($entry in $manifest | Where-Object {$_.path.StartsWith('payload/')}){
    Set-SetupPhase "Verify installed payload: $($entry.path)"
    $installed=Join-Path $runtime $entry.path.Substring(8)
    if((Get-FileHash -LiteralPath $installed).Hash -ne $entry.sha256){throw "Installed payload verification failed: $($entry.path)"}
}
[IO.File]::WriteAllText((Join-Path $target 'installation.json'),([ordered]@{schema=2;sourceGame=$game;previousInstallation=$SourceInstallation;installedUtc=[DateTime]::UtcNow.ToString('o');isolatedSaves=(Join-Path $hp2Home 'Save')} | ConvertTo-Json))
Remove-Item -LiteralPath (Join-Path $target 'INSTALL-INCOMPLETE.txt')
Set-SetupPhase 'Installation complete'
Write-Host "Installed successfully. Start SteamVR, then open: $target\Start-HP2VR.cmd"
Write-Host 'External downloads are documented in DOWNLOADS-AND-INSTALL.md; nothing was downloaded or installed system-wide automatically.'
