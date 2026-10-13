[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$RequestFile)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$run=Split-Path -Parent $RequestFile
$result=[ordered]@{Success=$false;StartedUtc=[DateTime]::UtcNow.ToString('o');FinishedUtc=$null;Phase='Worker startup';Error=$null;Hint=$null}
$transcribing=$false
$exitCode=1
try{
    Start-Transcript -LiteralPath (Join-Path $run 'transcript.txt') -Force | Out-Null
    $transcribing=$true
    $request=Get-Content -LiteralPath $RequestFile -Raw | ConvertFrom-Json
    $argsMap=@{};foreach($property in $request.Arguments.PSObject.Properties){$argsMap[$property.Name]=$property.Value}
    Write-Host ('Started: '+[DateTime]::Now.ToString('o'))
    Write-Host "PowerShell: $($PSVersionTable.PSVersion); 64-bit process: $([Environment]::Is64BitProcess); OS: $([Environment]::OSVersion.VersionString)"
    Write-Host "Package: $($request.Package)"
    Write-Host "Diagnostic folder: $run"
    foreach($key in $argsMap.Keys){Write-Host "$key = $($argsMap[$key])"}
    $script:HP2SetupPhase='Load installer'
    & (Join-Path $request.Package 'Install-HP2VR.ps1') @argsMap
    $result.Success=$true;$result.Phase='Completed';$exitCode=0
}catch{
    $err=$_
    if(Get-Variable HP2SetupPhase -Scope Global -ErrorAction SilentlyContinue){$result.Phase=$global:HP2SetupPhase}
    $result.Error=[ordered]@{Message=$err.Exception.Message;ExceptionType=$err.Exception.GetType().FullName;HResult=$err.Exception.HResult;ErrorId=$err.FullyQualifiedErrorId;Category=[string]$err.CategoryInfo;Target=[string]$err.TargetObject;Position=$err.InvocationInfo.PositionMessage;Stack=$err.ScriptStackTrace;Exception=$err.Exception.ToString()}
    $result.Hint=switch -Regex ($err.Exception.Message){
        'denied|Unauthorized|blocked' {'Check folder permissions and Windows protection history. Do not disable antivirus. Try a writable destination.';break}
        'hash|modified|unsupported|version|component' {'Check the named file and exact supported version. Re-extract the official mod/dependency download; do not substitute DLLs from unrelated sites.';break}
        'space|disk full' {'Free space on the destination drive and retry with a new destination.';break}
        'exists|overlap|inside|contain|replace' {'Choose a new destination outside source data folders. Do not overwrite an existing installation.';break}
        'Close HP2' {'Close the game and companion, then retry.';break}
        default {'Review the named phase, file and exception below. Keep any incomplete folder; use a new destination for retry.'}
    }
    Write-Host "FAILED during: $($result.Phase)"
    Write-Host ($result.Error | Format-List | Out-String)
    Write-Host "Next step: $($result.Hint)"
}finally{
    $result.FinishedUtc=[DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding UTF8
    if($transcribing){Stop-Transcript | Out-Null}
}
exit $exitCode
