param([switch]$ValidateLayout)
$ErrorActionPreference='Stop'
trap{
    $startupLog=Join-Path ([IO.Path]::GetTempPath()) ('HP2VR-Setup-GUI-error-'+[Guid]::NewGuid().ToString('N')+'.txt')
    ($_ | Format-List * -Force | Out-String)+"`r`n"+$_.ScriptStackTrace | Set-Content -LiteralPath $startupLog
    Write-Host "Setup interface failed. Details saved to $startupLog"
    try{[void][Windows.Forms.MessageBox]::Show("Setup interface error: $($_.Exception.Message)`r`nLog: $startupLog")}catch{}
    exit 1
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$package=$PSScriptRoot
$form=New-Object Windows.Forms.Form
$form.Text='HP2VR guided setup'
$form.Size=New-Object Drawing.Size(1000,790)
$form.MinimumSize=$form.Size
$form.StartPosition='CenterScreen'
$form.AutoScroll=$true
$fields=@{}
$y=15
$intro=New-Object Windows.Forms.Label
$intro.Text='Install to a NEW folder. Your original game stays untouched. Update: select an existing HP2VR install below.'
$intro.SetBounds(15,$y,950,30);$form.Controls.Add($intro);$y+=35
foreach($row in @(@('GameRoot','HP2 game folder'),@('DependencyRoot','Renderer folder (blank = game)'),@('OpenVRDll','OpenVR SDK folder or DLL'),@('TextureRoot','Optional HD texture folder'),@('SourceInstallation','Update from (blank = fresh)'),@('Destination','New installation folder'))){
    $label=New-Object Windows.Forms.Label;$label.Text=$row[1];$label.SetBounds(15,$y,230,25);$form.Controls.Add($label)
    $box=New-Object Windows.Forms.TextBox;$box.SetBounds(245,$y,620,25);$form.Controls.Add($box);$fields[$row[0]]=$box
    $button=New-Object Windows.Forms.Button;$button.Text='Browse';$button.Tag=$row[0];$button.SetBounds(875,$y,85,25);$form.Controls.Add($button)
    $button.Add_Click({
        $picker=New-Object Windows.Forms.FolderBrowserDialog
        $picker.Description='Select folder. For OpenVR choose the extracted SDK root containing bin. Destination must be a new folder: select its parent, then edit the path.'
        try{if($picker.ShowDialog() -eq 'OK'){$fields[$this.Tag].Text=if($this.Tag -eq 'Destination'){Join-Path $picker.SelectedPath 'HP2VR'}else{$picker.SelectedPath}}}finally{$picker.Dispose()}
    })
    $box.Add_TextChanged({if($install){$install.Enabled=$false};if($open){$open.Enabled=$false}})
    $y+=34
}
$fields.Destination.Text=Join-Path (Split-Path $package) 'HP2VR-Installed-New'
$label=New-Object Windows.Forms.Label;$label.Text='Official/source downloads (extract archives yourself; Microsoft installers run separately):';$label.SetBounds(15,$y,940,25);$form.Controls.Add($label);$y+=26
$x=15
foreach($link in @(
    @('Renderer 1.6.2','https://www.gamepressure.com/download/harry-potter-and-the-chamber-of-secrets-directx11-renderer-v162/zb12b06'),
    @('OpenVR 2.15.6','https://github.com/ValveSoftware/openvr/releases/tag/v2.15.6'),
    @('HD textures','https://www.moddb.com/mods/harry-potter-and-the-chamber-of-secrets-esrgan-upscale-pack/addons/harry-potter-and-the-chamber-of-secrets-esrgan-upscale-pack'),
    @('VC++ 2010 x86','https://www.microsoft.com/en-us/download/details.aspx?id=26999'),
    @('Legacy DirectX','https://www.microsoft.com/en-us/download/details.aspx?id=8109'),
    @('SteamVR','https://store.steampowered.com/app/250820/SteamVR/')
)){
    $button=New-Object Windows.Forms.Button;$button.Text=$link[0];$button.Tag=$link[1];$button.SetBounds($x,$y,150,30);$form.Controls.Add($button)
    $button.Add_Click({Start-Process $this.Tag});$x+=158
}
$y+=40
$status=New-Object Windows.Forms.ListView;$status.View='Details';$status.FullRowSelect=$true;$status.SetBounds(15,$y,945,205)
[void]$status.Columns.Add('Component',155);[void]$status.Columns.Add('Status',105);[void]$status.Columns.Add('Details',660);$form.Controls.Add($status);$y+=215
$check=New-Object Windows.Forms.Button;$check.Text='1. Check requirements';$check.SetBounds(15,$y,180,32);$form.Controls.Add($check)
$install=New-Object Windows.Forms.Button;$install.Text='2. Install / update';$install.SetBounds(205,$y,180,32);$install.Enabled=$false;$form.Controls.Add($install)
$guide=New-Object Windows.Forms.Button;$guide.Text='First-run guide';$guide.SetBounds(395,$y,160,32);$form.Controls.Add($guide)
$guide.Add_Click({Start-Process (Join-Path $package 'installer\FIRST-RUN.html')})
$open=New-Object Windows.Forms.Button;$open.Text='Open installed folder';$open.SetBounds(565,$y,180,32);$open.Enabled=$false;$form.Controls.Add($open)
$open.Add_Click({Start-Process explorer.exe -ArgumentList ('"'+$fields.Destination.Text+'"')})
$logs=New-Object Windows.Forms.Button;$logs.Text='Open diagnostic logs';$logs.SetBounds(755,$y,205,32);$logs.Enabled=$false;$form.Controls.Add($logs)
$logs.Add_Click({if($script:runFolder){Start-Process explorer.exe -ArgumentList ('"'+$script:runFolder+'"')}})
$y+=42
$log=New-Object Windows.Forms.TextBox;$log.Multiline=$true;$log.ReadOnly=$true;$log.ScrollBars='Vertical';$log.SetBounds(15,$y,945,135);$form.Controls.Add($log)
$script:job=$null
$timer=New-Object Windows.Forms.Timer;$timer.Interval=400
function Begin-SetupJob([bool]$checking){
    $script:checking=$checking
    $script:runFolder=Join-Path ([IO.Path]::GetTempPath()) ('HP2VR-Setup-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $script:runFolder | Out-Null
    $script:prefix=Join-Path $script:runFolder 'worker'
    $argsMap=@{}
    foreach($key in $fields.Keys){if($fields[$key].Text.Trim()){$argsMap[$key]=$fields[$key].Text.Trim()}}
    if($checking){$argsMap.StatusFile=$script:prefix+'.json'}
    $request=Join-Path $script:runFolder 'request.json'
    @{Package=$package;Arguments=$argsMap;Mode=$(if($checking){'Check'}else{'Install'})} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $request -Encoding UTF8
    try{
        $worker=Join-Path $package 'installer\Run-SetupJob.ps1'
        $script:job=Start-Process powershell.exe -ArgumentList @('-NoProfile','-NonInteractive','-STA','-OutputFormat','Text','-File',('"'+$worker+'"'),'-RequestFile',('"'+$request+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput ($script:prefix+'.out') -RedirectStandardError ($script:prefix+'.err')
        $null=$script:job.Handle
    }catch{
        $detail="Could not launch setup worker: $($_.Exception.Message)`r`n$($_.InvocationInfo.PositionMessage)`r`n$($_.ScriptStackTrace)"
        $detail | Set-Content -LiteralPath (Join-Path $script:runFolder 'launch-error.txt')
        $script:job=$null;$logs.Enabled=$true;$log.Text=$detail+"`r`nLogs: $script:runFolder";return
    }
    foreach($control in $form.Controls){$control.Enabled=$false};$log.Enabled=$true;$status.Enabled=$true
    $log.Text="Working. Logs: $script:runFolder`r`nLarge copies can take several minutes. Logs contain local paths; review before sharing."
    $timer.Start()
}
$check.Add_Click({try{Begin-SetupJob $true}catch{[void][Windows.Forms.MessageBox]::Show($_.Exception.Message)}})
$install.Add_Click({try{Begin-SetupJob $false}catch{[void][Windows.Forms.MessageBox]::Show($_.Exception.Message)}})
$timer.Add_Tick({
    if(!$script:job){return}
    if(!$script:job.HasExited){
        $tail=Get-Content -LiteralPath ($script:prefix+'.out') -Tail 16 -ErrorAction SilentlyContinue
        $log.Text="Working. Logs: $script:runFolder`r`n"+($tail -join "`r`n");return
    }
    $timer.Stop();$script:job.WaitForExit();$code=$script:job.ExitCode;$script:job.Dispose();$script:job=$null
    "Worker exit status: $code" | Set-Content -LiteralPath (Join-Path $script:runFolder 'process-exit.txt')
    foreach($control in $form.Controls){$control.Enabled=$true};$install.Enabled=$false;$open.Enabled=$false
    $log.Text=((Get-Content -LiteralPath ($script:prefix+'.out') -Raw -ErrorAction SilentlyContinue)+(Get-Content -LiteralPath ($script:prefix+'.err') -Raw -ErrorAction SilentlyContinue))
    $logs.Enabled=$true
    $completion=$null
    try{$completion=Get-Content -LiteralPath (Join-Path $script:runFolder 'result.json') -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop}catch{
        $log.AppendText("`r`nNO VALID COMPLETION REPORT. Worker exit status: $code. It may have been blocked, terminated, or failed before logging initialized. Inspect worker.err and process-exit.txt. No success is assumed.")
    }
    $succeeded=$completion -and $completion.Success -and ($null -eq $code -or $code -eq 0)
    if($succeeded -and $script:checking){
        try{
            $result=Get-Content -LiteralPath ($script:prefix+'.json') -Raw | ConvertFrom-Json
            $status.Items.Clear()
            foreach($entry in $result.Items){$item=New-Object Windows.Forms.ListViewItem($entry.Name);[void]$item.SubItems.Add($entry.State);[void]$item.SubItems.Add($entry.Detail);[void]$status.Items.Add($item)}
            $install.Enabled=$result.Ready
            $log.Text='Checks finished. '+$(if($result.Ready){'Required files match. Review optional/prerequisite warnings before installing.'}else{'Not ready: see the component table for missing or mismatched requirements.'})
        }catch{$log.Text='Worker finished but its requirement report could not be read: '+$_.Exception.Message}
    }elseif($succeeded){$open.Enabled=$true;$log.AppendText("`r`nReady. Open the installed folder, read FIRST-RUN.html, then use Start-HP2VR.cmd.")}
    elseif($completion -and !$completion.Success){$log.AppendText("`r`nFAILED PHASE: $($completion.Phase)`r`n$($completion.Error.Message)`r`n$($completion.Hint)")}
    elseif($completion){$log.AppendText("`r`nWorker report and exit status disagree. Installation not confirmed. Exit status: $code")}
    $log.AppendText("`r`nDiagnostic folder: $script:runFolder`r`nKeep any INSTALL-INCOMPLETE folder. Logs include local paths/usernames; review before sharing.")
})
$form.Add_FormClosing({if($script:job){$_.Cancel=$true;[void][Windows.Forms.MessageBox]::Show('Setup is still working. Wait for it to finish.')}})
if($ValidateLayout){Write-Output ('Layout constructed: '+$form.Controls.Count+' controls');$form.Dispose();$timer.Dispose();return}
try{[void]$form.ShowDialog()}finally{$timer.Dispose();$form.Dispose()}
