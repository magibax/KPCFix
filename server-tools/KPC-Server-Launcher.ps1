# KPCFix Server Tools launcher. Pick deathmatch or the experimental co-op campaign, then
# start the dedicated server through Start-KPC-Selected-Server.ps1 in its own console.
# The choices are remembered in kpc-gui-settings.json next to this script.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
Set-Location -LiteralPath $PSScriptRoot
$iniPath=Join-Path $PSScriptRoot 'KPCServerTools.ini'
$logPath=Join-Path $PSScriptRoot 'KPCServerTools.log'
$settingsPath=Join-Path $PSScriptRoot 'kpc-gui-settings.json'

# Map catalogue: maps.json groups may be nested arrays; flatten to 59 single maps.
$parsedMaps=Get-Content (Join-Path $PSScriptRoot 'maps.json') -Raw | ConvertFrom-Json
$maps=New-Object 'System.Collections.Generic.List[object]'
foreach($entry in $parsedMaps) {
 if($entry -is [System.Array]) { foreach($single in $entry) { $maps.Add($single) } }
 else { $maps.Add($entry) }
}
if($maps.Count -ne 59 -or $maps[0].Name -ne 'PM01') { throw 'Map catalog did not load correctly. Expected 59 individual maps.' }

# Last used choices (ignored if missing or unreadable), else the INI, else defaults.
$saved=$null
if(Test-Path -LiteralPath $settingsPath) { try { $saved=Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json } catch { $saved=$null } }
function Saved([string]$Name,$Default) {
 if($saved -and ($saved.PSObject.Properties.Name -contains $Name)) { return $saved.$Name }
 return $Default
}
function Get-IniValue([string]$Key,[string]$Default) {
 if(Test-Path -LiteralPath $iniPath) {
  $m=[regex]::Match([IO.File]::ReadAllText($iniPath),'(?im)^[ \t]*'+[regex]::Escape($Key)+'[ \t]*=[ \t]*([^\r\n;]*)')
  if($m.Success) { return $m.Groups[1].Value.Trim() }
 }
 return $Default
}

# ---- Controls -------------------------------------------------------------------------
$form=New-Object Windows.Forms.Form
$form.SuspendLayout()
$form.AutoScaleDimensions=New-Object Drawing.SizeF(96,96)
$form.AutoScaleMode=[Windows.Forms.AutoScaleMode]::Dpi
$form.Font=New-Object Drawing.Font('Segoe UI',9)
$form.Text='KPCFix Server Tools'
$form.ClientSize=New-Object Drawing.Size(640,604)
$form.StartPosition='CenterScreen'
$form.FormBorderStyle='FixedDialog'
$form.MaximizeBox=$false

function Add-Group([string]$Text,[int]$Y,[int]$Height) {
 $g=New-Object Windows.Forms.GroupBox
 $g.Text=$Text; $g.Location=New-Object Drawing.Point(12,$Y); $g.Size=New-Object Drawing.Size(616,$Height)
 $form.Controls.Add($g); return $g
}
function Add-Label($Parent,[string]$Text,[int]$X,[int]$Y,[int]$Width,[int]$Height=20) {
 $c=New-Object Windows.Forms.Label
 $c.Text=$Text; $c.Location=New-Object Drawing.Point($X,$Y); $c.Size=New-Object Drawing.Size($Width,$Height)
 $Parent.Controls.Add($c); return $c
}
function Add-Number($Parent,[string]$Text,[int]$X,[int]$Y,[int]$Width,[int]$Minimum,[int]$Maximum,$Value) {
 [void](Add-Label $Parent $Text $X $Y ($Width+60))
 $c=New-Object Windows.Forms.NumericUpDown
 $c.Location=New-Object Drawing.Point($X,($Y+20)); $c.Width=$Width
 $c.Minimum=$Minimum; $c.Maximum=$Maximum
 $c.Value=[Math]::Min($Maximum,[Math]::Max($Minimum,[int]$Value))
 $Parent.Controls.Add($c); return $c
}
function Add-Check($Parent,[string]$Text,[int]$X,[int]$Y,[int]$Width,[bool]$Checked) {
 $c=New-Object Windows.Forms.CheckBox
 $c.Text=$Text; $c.Checked=$Checked; $c.Location=New-Object Drawing.Point($X,$Y); $c.Size=New-Object Drawing.Size($Width,22)
 $Parent.Controls.Add($c); return $c
}
function Add-Button([string]$Text,[int]$X,[int]$Width) {
 $c=New-Object Windows.Forms.Button
 $c.Text=$Text; $c.Location=New-Object Drawing.Point($X,560); $c.Size=New-Object Drawing.Size($Width,32)
 $form.Controls.Add($c); return $c
}

# Server
$serverGroup=Add-Group 'Server' 8 104
[void](Add-Label $serverGroup 'Server name' 12 22 200)
$name=New-Object Windows.Forms.TextBox
$name.Location=New-Object Drawing.Point(12,42); $name.Width=300; $name.Text=[string](Saved 'Name' 'KPC Server')
$serverGroup.Controls.Add($name)
$port=Add-Number $serverGroup 'UDP port' 330 22 100 1 65535 (Saved 'Port' 42972)
$players=Add-Number $serverGroup 'Maximum players' 450 22 100 1 16 (Saved 'Players' 8)
$cheats=Add-Check $serverGroup 'Allow cheats at start (noclip, invulnerability, cheat keys). Host console: kpc_cheats 0 / 1' 12 72 590 ([bool](Saved 'Cheats' ((Get-IniValue 'AllowCheats' '0') -eq '1')))

# Game type
$modeGroup=Add-Group 'Game' 118 52
$deathmatch=New-Object Windows.Forms.RadioButton
$deathmatch.Text='Deathmatch'; $deathmatch.Location=New-Object Drawing.Point(12,20); $deathmatch.Size=New-Object Drawing.Size(150,24)
$coop=New-Object Windows.Forms.RadioButton
$coop.Text='Co-op campaign (EXPERIMENTAL, up to 3 players)'; $coop.Location=New-Object Drawing.Point(180,20); $coop.Size=New-Object Drawing.Size(420,24)
$modeGroup.Controls.Add($deathmatch); $modeGroup.Controls.Add($coop)

# Deathmatch
$dmGroup=Add-Group 'Deathmatch' 176 244
[void](Add-Label $dmGroup 'Map (campaign worlds are experimental in deathmatch and may be buggy)' 12 22 590)
$map=New-Object Windows.Forms.ComboBox
$map.DropDownStyle=[Windows.Forms.ComboBoxStyle]::DropDownList
$map.Location=New-Object Drawing.Point(12,42); $map.Width=590; $map.MaxDropDownItems=20
$dmGroup.Controls.Add($map)
foreach($entry in $maps) {
 $suffix=if($entry.Intro){' (intro)'}else{''}
 [void]$map.Items.Add([string]($entry.Group+' / '+$entry.Name+$suffix))
}
$savedMap=[string](Saved 'Map' '')
$map.SelectedIndex=[Math]::Max(0,[Array]::FindIndex($maps.ToArray(),[Predicate[object]]{param($m) $m.Path -eq $savedMap}))
$frags=Add-Number $dmGroup 'Frag limit (0 = none)' 12 74 120 0 10000 (Saved 'Frags' 20)
$minutes=Add-Number $dmGroup 'Time limit in minutes (0 = none)' 200 74 120 0 1440 (Saved 'Minutes' 30)
# server-template.cfg cvar, label, default
$choices=@(
 @('teamplay','Teamplay',0),@('keepitems','Keep items',0),@('fovchange','Allow FOV changes',1),
 @('startgauntlets','Start with gauntlets',1),@('startbelt','Start with belt',1),@('startboots','Start with boots',0),
 @('startshoulderplates','Start with shoulders',0),@('startbreastplate','Start with breastplate',0),@('startmask','Start with mask',0),
 @('startscourge','Start with Scourge',0),@('startzero','Start with Zero Cannon',0),@('startmagma','Start with Magma Cannon',0),
 @('startwindblade','Start with Windblade',0),@('startultimate','Start with ultimate weapon',0)
)
$savedCvars=Saved 'Cvars' $null
$cvars=[ordered]@{}
$i=0
foreach($choice in $choices) {
 $key=[string]$choice[0]
 $checked=[bool]$choice[2]
 if($savedCvars -and ($savedCvars.PSObject.Properties.Name -contains $key)) { $checked=[bool]$savedCvars.$key }
 $cvars[$key]=Add-Check $dmGroup ([string]$choice[1]) (12+($i%3)*198) (126+[int][Math]::Floor($i/3)*22) 194 $checked
 $i++
}

# Co-op
$coopGroup=Add-Group 'Co-op campaign' 426 116
[void](Add-Label $coopGroup ("The lobby is a random deathmatch map. Players start without armour; the realm pickups give it.`n"+
 "To start, the host types in chat:  !campaign full   or   !campaign water / earth / air / fire / nightmare`n"+
 "!abort returns everyone to the lobby. Server console: kpc_cheats 0 / 1, changelevel <world>") 12 22 596 54)
$testLogging=Add-Check $coopGroup 'Test logging: script starts and timings in KPCServerTools.log (TraceScripts, TraceTiming)' 12 80 590 ([bool](Saved 'TestLogging' ((Get-IniValue 'TraceScripts' '0') -eq '1')))

# Buttons
$editIni=Add-Button 'Edit settings (INI)' 12 140
$openLog=Add-Button 'Open server log' 160 140
$start=Add-Button 'Start server' 468 160
$form.AcceptButton=$start

# ---- Behaviour ------------------------------------------------------------------------
function Update-Mode {
 $isCoop=$coop.Checked
 $dmGroup.Enabled=-not $isCoop
 $coopGroup.Enabled=$isCoop
 # Co-op sessions hold at most 3 players (CoopMaxPlayers); lowering Maximum clamps Value.
 $players.Maximum=if($isCoop){3}else{16}
}
$deathmatch.add_CheckedChanged({ Update-Mode })
$coop.add_CheckedChanged({ Update-Mode })
if([string](Saved 'Mode' 'Deathmatch') -eq 'Coop') { $coop.Checked=$true } else { $deathmatch.Checked=$true }
Update-Mode

$editIni.add_Click({
 if(Test-Path -LiteralPath $iniPath) { Start-Process notepad.exe -ArgumentList ('"'+$iniPath+'"') }
 else { [void][Windows.Forms.MessageBox]::Show('KPCServerTools.ini is missing. Run Setup-KPCServerTools.cmd first.','KPCFix Server Tools') }
})
$openLog.add_Click({
 if(Test-Path -LiteralPath $logPath) { Start-Process notepad.exe -ArgumentList ('"'+$logPath+'"') }
 else { [void][Windows.Forms.MessageBox]::Show('There is no KPCServerTools.log yet. It is written when the server starts.','KPCFix Server Tools') }
})

$start.add_Click({
 try {
  if([string]::IsNullOrWhiteSpace($name.Text) -or $name.Text -match '["\r\n]') { throw 'Enter a server name without quotes or line breaks.' }
  if(Get-Process -Name @('dedicated','DEDICA~1') -ErrorAction SilentlyContinue) { throw 'A dedicated server is already running. Close it before starting another one.' }
  $isCoop=$coop.Checked
  $chosen=$maps[$map.SelectedIndex]
  # Server config from the template. Co-op: no frag or time limit, items kept; the
  # worker adds the co-op map list and turns the start armour off.
  $values=[ordered]@{
   hostname=$name.Text; port=[string]$port.Value; maxclients=[string]$players.Value
   fraglimit=$(if($isCoop){'0'}else{[string]$frags.Value}); timelimit=$(if($isCoop){'0'}else{[string]$minutes.Value})
  }
  foreach($key in $cvars.Keys) { $values[$key]=if($cvars[$key].Checked){'1'}else{'0'} }
  if($isCoop) { $values['keepitems']='1' }
  $config=Get-Content (Join-Path $PSScriptRoot 'server-template.cfg') -Raw
  foreach($key in $values.Keys) {
   $line='"'+$key+'" "'+$values[$key]+'"'
   $config=[regex]::Replace($config,'(?m)^"'+[regex]::Escape($key)+'".*$',{param($m) $line})
  }
  Set-Content (Join-Path $PSScriptRoot 'kpc-gui-server.cfg') $config -Encoding ASCII
  Set-Content (Join-Path $PSScriptRoot 'kpc-gui-maps.txt') @($chosen.Path) -Encoding ASCII

  $cvarState=[ordered]@{}; foreach($key in $cvars.Keys) { $cvarState[$key]=$cvars[$key].Checked }
  $state=[ordered]@{
   Mode=$(if($isCoop){'Coop'}else{'Deathmatch'}); Name=$name.Text; Port=[int]$port.Value; Players=[int]$players.Value
   Cheats=$cheats.Checked; Map=$chosen.Path; Frags=[int]$frags.Value; Minutes=[int]$minutes.Value
   Cvars=$cvarState; TestLogging=$testLogging.Checked
  }
  try { Set-Content -LiteralPath $settingsPath -Value ($state | ConvertTo-Json -Depth 3) -Encoding ASCII } catch { }

  $arguments='-NoLogo -NoProfile -NoExit -ExecutionPolicy Bypass -File "'+(Join-Path $PSScriptRoot 'Start-KPC-Selected-Server.ps1')+'"'
  $arguments+=' -Cheats '+$(if($cheats.Checked){'Enabled'}else{'Disabled'})
  if($isCoop) { $arguments+=' -Map random -Coop -TestLogging '+$(if($testLogging.Checked){'On'}else{'Off'}) }
  else { $arguments+=' -Map "'+$chosen.Path+'"' }
  Start-Process powershell.exe -WorkingDirectory $PSScriptRoot -ArgumentList $arguments
 } catch { [void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'KPCFix Server Tools') }
})

$form.ResumeLayout($false)
[Windows.Forms.Application]::Run($form)
