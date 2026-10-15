function Normalize-SetupPath([string]$Path){
    if(!$Path){return ''}
    return $Path.Trim().Trim('"').Trim()
}
function Resolve-RendererFile([string]$Root,[string]$Relative){
    $Root=Normalize-SetupPath $Root
    if(!$Root){return ''}
    $candidates=@((Join-Path $Root $Relative))
    $candidates+=Join-Path $Root $Relative.Substring(7)
    $parent=Split-Path $Root -Parent
    if($parent){
        $candidates+=Join-Path $parent $Relative
        if($Relative -eq 'System/d3d11drv.dll'){$candidates+=Join-Path $parent 'Harry Potter 2\d3d11drv.dll'}
        else{$candidates+=Join-Path $parent ('Common\'+$Relative.Substring(7))}
    }
    if($Relative -eq 'System/d3d11drv.dll'){
        $candidates+=Join-Path $Root 'Harry Potter 2\d3d11drv.dll'
    }else{
        $candidates+=Join-Path $Root ('Common\'+$Relative.Substring(7))
    }
    foreach($candidate in $candidates){if(Test-Path -LiteralPath $candidate -PathType Leaf){return $candidate}}
    return $candidates[0]
}
function Resolve-OpenVRFile([string]$Path){
    $Path=Normalize-SetupPath $Path
    if(!$Path){return ''}
    if(Test-Path -LiteralPath $Path -PathType Container){
        foreach($relative in @('bin\win64\openvr_api.dll','win64\openvr_api.dll','openvr_api.dll')){
            $candidate=Join-Path $Path $relative
            if(Test-Path -LiteralPath $candidate -PathType Leaf){return $candidate}
        }
        return Join-Path $Path 'bin\win64\openvr_api.dll'
    }
    return $Path
}
function Resolve-TextureFolder([string]$Path){
    if(!$Path){return ''}
    if(Test-Path -LiteralPath (Join-Path $Path 'Textures') -PathType Container){return Join-Path $Path 'Textures'}
    return $Path
}
function Assert-NoLinks([string]$Root){
    $cursor=Get-Item -LiteralPath $Root
    while($cursor){
        if($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Linked paths are not supported: $Root"}
        $cursor=$cursor.Parent
    }
    foreach($item in Get-ChildItem -LiteralPath $Root -Recurse -Force){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Linked path: $($item.FullName)"}
    }
}
function Get-HP2SetupStatus {
    param([string]$Package,[string]$GameRoot,[string]$RendererRoot,[string]$OpenVRDll,[string]$TextureRoot,[string]$Destination,[string]$SourceInstallation)
    $GameRoot=Normalize-SetupPath $GameRoot
    $RendererRoot=Normalize-SetupPath $RendererRoot
    $OpenVRDll=Normalize-SetupPath $OpenVRDll
    $TextureRoot=Normalize-SetupPath $TextureRoot
    $Destination=Normalize-SetupPath $Destination
    $SourceInstallation=Normalize-SetupPath $SourceInstallation
    $items=[Collections.Generic.List[object]]::new()
    function Add-Status($name,$state,$detail,$required){$items.Add([pscustomobject]@{Name=$name;State=$state;Detail=$detail;Required=$required})}
    if($SourceInstallation){
        $valid=(Test-Path -LiteralPath (Join-Path $SourceInstallation 'installation.json')) -and (Test-Path -LiteralPath (Join-Path $SourceInstallation 'runtime\.hp2vr-private-runtime')) -and !(Test-Path -LiteralPath (Join-Path $SourceInstallation 'INSTALL-INCOMPLETE.txt'))
        Add-Status 'Update source' $(if($valid){'Found'}else{'Missing'}) 'Updates create a new sibling installation; original remains intact.' $true
        $GameRoot=Join-Path $SourceInstallation 'runtime'
        $RendererRoot=$GameRoot
        $OpenVRDll=Join-Path $GameRoot 'VR\openvr_api.dll'
    }
    $identity=Get-Content -LiteralPath (Join-Path $Package 'source\src\HP2BuildIdentity.h') -Raw
    $ids=[regex]::Matches($identity,'\{L"(System\\\\[^"\r\n]+)",\s*L"([A-F0-9]{64})"\}')
    $state='Found';$detail='Supported HP2 build; nine identities match.'
    if(!$GameRoot){$state='Missing';$detail='Choose the folder containing System and Maps.'}
    elseif($ids.Count -ne 9){$state='Error';$detail='Invalid package identity list.'}
    else{foreach($id in $ids){
        $relative=$id.Groups[1].Value.Replace('\\','\');$file=Join-Path $GameRoot $relative
        try{if(!(Test-Path -LiteralPath $file -PathType Leaf)){$state='Missing';$detail=$relative;break}
            $hash=(Get-FileHash -LiteralPath $file).Hash
            Write-Host "GAME CHECK $file expected=$($id.Groups[2].Value) actual=$hash"
            if($hash -ne $id.Groups[2].Value){$state='Wrong version';$detail="$file expected=$($id.Groups[2].Value) actual=$hash";break}
        }catch{$state='Blocked';$detail=$_.Exception.Message;break}
    }}
    Add-Status 'HP2 game' $state $detail $true
    if(!$RendererRoot){$RendererRoot=$GameRoot}
    $external=Import-Csv -LiteralPath (Join-Path $Package 'DEPENDENCY-HASHES.csv')
    foreach($group in @('Renderer','OpenVR')){
        $state='Found';$detail=if($group -eq 'Renderer'){'HP2 DX11 1.6.2 and all shaders match.'}else{'OpenVR 2.15.6 x64 matches.'}
        foreach($entry in $external | Where-Object {($_.path -eq 'VR/openvr_api.dll') -eq ($group -eq 'OpenVR')}){
            $file=if($group -eq 'OpenVR'){Resolve-OpenVRFile $OpenVRDll}else{Resolve-RendererFile $RendererRoot $entry.path}
            try{if(!$file -or !(Test-Path -LiteralPath $file -PathType Leaf)){$state='Missing';$detail="Required: $($entry.path). Searched: $file. Extract the complete download; use Full setup guide for exact folder examples.";break}
                $hash=(Get-FileHash -LiteralPath $file).Hash
                Write-Host "DEPENDENCY CHECK $file expected=$($entry.sha256) actual=$hash"
                if($hash -ne $entry.sha256){$state='Wrong version';$detail="$file expected=$($entry.sha256) actual=$hash";break}
            }catch{$state='Blocked';$detail=$_.Exception.Message;break}
        }
        if($state -eq 'Found'){
            $resolved=if($group -eq 'OpenVR'){Resolve-OpenVRFile $OpenVRDll}else{Resolve-RendererFile $RendererRoot 'System/d3d11drv.dll'}
            $detail+=" Resolved: $resolved"
        }
        Add-Status $group $state $detail $true
    }
    if($TextureRoot){
        try{$root=Resolve-TextureFolder $TextureRoot;Assert-NoLinks $root
            $dds=@(Get-ChildItem -LiteralPath $root -File -Recurse -Filter '*.dds')
            Add-Status 'HD textures' $(if($dds.Count){'Found'}else{'Missing'}) "$($dds.Count) DDS files; optional pack selection, not an author-version authenticity check." $true
        }catch{Add-Status 'HD textures' 'Error' $_.Exception.Message $true}
    }else{Add-Status 'HD textures' 'Optional' 'None selected; base textures work. Updates retain existing textures.' $false}
    foreach($dep in @(@('VC++ 2010 x86',@('msvcp100.dll','msvcr100.dll')),@('Legacy DirectX',@('d3dx11_43.dll')))){
        $missing=@($dep[1] | Where-Object {!(Test-Path -LiteralPath (Join-Path $env:WINDIR ('SysWOW64\'+$_)))})
        Add-Status $dep[0] $(if($missing.Count){'Missing'}else{'Found'}) $(if($missing.Count){'Use the official download button; install before playing.'}else{'32-bit library files present; not a full runtime health check.'}) $false
    }
    $steam='';foreach($key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam')){
        $value=Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        if($value){foreach($prop in @('SteamPath','InstallPath')){if($value.PSObject.Properties[$prop]){$steam=$value.$prop;break}}}
        if($steam){break}
    }
    $steamVR=$steam -and (Test-Path -LiteralPath (Join-Path $steam 'steamapps\common\SteamVR\bin\win64\vrserver.exe'))
    Add-Status 'SteamVR' $(if($steamVR){'Found'}else{'Check manually'}) 'Default Steam library detection only; connect headset and verify SteamVR before playing.' $false
    $state='Found';$detail='New destination; existing folders are never overwritten.'
    try{
        if(!$Destination){throw 'Choose an installation destination.'}
        $full=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
        if(Test-Path -LiteralPath $full){$state='Exists';$detail='Choose a new folder. Updates are side-by-side.'}
        elseif($GameRoot){
            $g=[IO.Path]::GetFullPath($GameRoot).TrimEnd('\')
            if($full -eq $g -or $g.StartsWith($full+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Destination overlaps the source game.'}
            foreach($folder in @('System','Maps','Textures','Sounds','Music','help')){
                $data=Join-Path $g $folder
                if($full -eq $data -or $full.StartsWith($data+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Destination is inside source game data.'}
            }
        }
        if($SourceInstallation){$old=[IO.Path]::GetFullPath($SourceInstallation).TrimEnd('\');if($full -eq $old -or $full.StartsWith($old+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Choose a destination outside the old installation.'}}
    }catch{$state='Error';$detail=$_.Exception.Message}
    Add-Status 'Destination' $state $detail $true
    $ready=@($items | Where-Object {$_.Required -and $_.State -ne 'Found'}).Count -eq 0
    return [pscustomobject]@{Ready=$ready;Items=@($items.ToArray())}
}
