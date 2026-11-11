$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
Set-Location -LiteralPath $PSScriptRoot
$parsedMaps=Get-Content (Join-Path $PSScriptRoot 'maps.json') -Raw | ConvertFrom-Json
$maps=New-Object 'System.Collections.Generic.List[object]'
foreach($entry in $parsedMaps) {
 if($entry -is [System.Array]) { foreach($single in $entry) { $maps.Add($single) } }
 else { $maps.Add($entry) }
}
if($maps.Count -ne 59 -or $maps[0].Name -ne 'PM01') { throw 'Map catalog did not load correctly. Expected 59 individual maps.' }
$form=New-Object Windows.Forms.Form
$form.Text='KPCFix Server Tools'
$form.ClientSize=New-Object Drawing.Size(620,440)
$form.StartPosition='CenterScreen'
$form.FormBorderStyle='FixedDialog'
$form.MaximizeBox=$false
function Label($text,$x,$y) {
 $c=New-Object Windows.Forms.Label; $c.Text=$text; $c.Location=New-Object Drawing.Point($x,$y); $c.Size=New-Object Drawing.Size(560,22); $form.Controls.Add($c)
}
Label 'Campaign maps are experimental and may be buggy in multiplayer.' 16 12
Label 'Starting map' 16 40
$map=New-Object System.Windows.Forms.ComboBox
$map.DropDownStyle=[System.Windows.Forms.ComboBoxStyle]::DropDownList
$map.Location=New-Object System.Drawing.Point(16,62)
$map.Width=588
[void]$form.Controls.Add($map)
Label 'Server name' 16 96
$name=New-Object Windows.Forms.TextBox; $name.Location=New-Object Drawing.Point(16,118); $name.Width=588; $name.Text='KPC Server'; $form.Controls.Add($name)
function NumberField($text,$x,$width,$minimum,$maximum,$value) {
 $label=New-Object Windows.Forms.Label
 $label.Text=$text; $label.Location=New-Object Drawing.Point($x,154); $label.Size=New-Object Drawing.Size($width,20)
 [void]$form.Controls.Add($label)
 $control=New-Object Windows.Forms.NumericUpDown
 $control.Location=New-Object Drawing.Point($x,176); $control.Width=$width
 $control.Minimum=$minimum; $control.Maximum=$maximum; $control.Value=$value
 [void]$form.Controls.Add($control)
 return $control
}
$port=NumberField 'UDP port' 16 125 1 65535 42972
$players=NumberField 'Maximum players' 157 125 1 16 8
$frags=NumberField 'Frag limit (0 = none)' 298 145 0 10000 20
$minutes=NumberField 'Minutes (0 = none)' 459 145 0 1440 30
$settings=@{}
$choices=@(
 @('teamplay','Teamplay',0),@('keepitems','Keep items',0),@('fovchange','Allow FOV changes',1),
 @('startgauntlets','Start with gauntlets',1),@('startbelt','Start with belt',1),@('startboots','Start with boots',0),
 @('startshoulderplates','Start with shoulders',0),@('startbreastplate','Start with breastplate',0),@('startmask','Start with mask',0),
 @('startscourge','Start with Scourge',0),@('startzero','Start with Zero Cannon',0),@('startmagma','Start with Magma Cannon',0),
 @('startwindblade','Start with Windblade',0),@('startultimate','Start with ultimate',0)
)
$i=0
foreach($choice in $choices) {
 $box=New-Object Windows.Forms.CheckBox
 $box.Text=[string]$choice[1]; $box.Checked=[bool]$choice[2]
 $box.Location=New-Object Drawing.Point((16+($i%3)*198),(214+[int][math]::Floor($i/3)*27))
 $box.Size=New-Object Drawing.Size(194,27)
 $form.Controls.Add($box); $settings[[string]$choice[0]]=$box; $i++
}
$cheats=New-Object Windows.Forms.CheckBox
$cheats.Text='Allow cheats on startup (noclip and invulnerability)'
$cheats.Checked=$false
$cheats.Location=New-Object Drawing.Point(16,351)
$cheats.Size=New-Object Drawing.Size(370,26)
$form.Controls.Add($cheats)
# EXPERIMENTAL co-op MVP: fixed R1M2B -> R1M2C route, max 3 players, chat lobby.
$coop=New-Object Windows.Forms.CheckBox
$coop.Text='EXPERIMENTAL co-op session: R1M2B -> R1M2C, max 3, chat lobby (!ready / !start)'
$coop.Checked=$false
$coop.Location=New-Object Drawing.Point(16,408)
$coop.Size=New-Object Drawing.Size(588,26)
$form.Controls.Add($coop)
$coopIndex=-1
for($k=0;$k -lt $maps.Count;$k++){ if($maps[$k].Path -eq 'worlds\realm1\R1M2B'){ $coopIndex=$k } }
if($coopIndex -lt 0){ throw 'Map catalog lacks worlds\realm1\R1M2B.' }
$coop.add_CheckedChanged({
 if($coop.Checked){ $map.SelectedIndex=$coopIndex; $map.Enabled=$false; if($players.Value -gt 3){ $players.Value=3 } }
 else { $map.Enabled=$true }
})
$hint=New-Object Windows.Forms.Label; $hint.Text='Live console: kpc_cheats 0 / 1'; $hint.Location=New-Object Drawing.Point(16,382); $hint.Size=New-Object Drawing.Size(330,20); $form.Controls.Add($hint)
$button=New-Object Windows.Forms.Button; $button.Text='Start selected map'; $button.Location=New-Object Drawing.Point(404,360); $button.Size=New-Object Drawing.Size(200,34); $form.Controls.Add($button)
$script:visible=$maps.ToArray()
foreach($entry in $script:visible) {
 $suffix=if($entry.Intro){' (intro)'}else{''}
 [void]$map.Items.Add([string]($entry.Group+' / '+$entry.Name+$suffix))
}
if($map.Items.Count){$map.SelectedIndex=0}
$button.add_Click({
 try {
  if($map.SelectedIndex -lt 0){throw 'Select a map.'}
  if($name.Text -match '["\r\n]' -or [string]::IsNullOrWhiteSpace($name.Text)){throw 'Enter a server name without quotes or newlines.'}
  $running=Get-Process -Name @('dedicated','DEDICA~1') -ErrorAction SilentlyContinue
  if($running){throw 'A dedicated server is already running. Close it before starting another server with this launcher.'}
  $chosen=$script:visible[$map.SelectedIndex]
  if($coop.Checked -and $chosen.Path -ne 'worlds\realm1\R1M2B'){throw 'Co-op starts at Realm 1 / R1M2B.'}
  $config=Get-Content (Join-Path $PSScriptRoot 'server-template.cfg') -Raw
  $config=[regex]::Replace($config,'(?m)^"hostname".*$',{param($m) '"hostname" "'+$name.Text+'"'})
  $config=[regex]::Replace($config,'(?m)^"port".*$',('"port" "'+$port.Value+'"'))
  $config=[regex]::Replace($config,'(?m)^"maxclients".*$',('"maxclients" "'+$players.Value+'"'))
  $config=[regex]::Replace($config,'(?m)^"fraglimit".*$',('"fraglimit" "'+$frags.Value+'"'))
  $config=[regex]::Replace($config,'(?m)^"timelimit".*$',('"timelimit" "'+$minutes.Value+'"'))
  foreach($key in $settings.Keys) {
   $value=if($settings[$key].Checked){'1'}else{'0'}
   $pattern='(?m)^"'+[regex]::Escape($key)+'".*$'
   $config=[regex]::Replace($config,$pattern,('"'+$key+'" "'+$value+'"'))
  }
  $mapLines=@($chosen.Path)
  if($coop.Checked){
   # Server-side route; the co-op DLL only ever advances along these entries.
   $limit=[Math]::Min([int]$players.Value,3)
   $config=[regex]::Replace($config,'(?m)^"maxclients".*$',('"maxclients" "'+$limit+'"'))
   $config=[regex]::Replace($config,'(?m)^"fraglimit".*$','"fraglimit" "0"')
   $config=[regex]::Replace($config,'(?m)^"timelimit".*$','"timelimit" "0"')
   $config=[regex]::Replace($config,'(?m)^"keepitems".*$','"keepitems" "1"')
   $mapLines=@('worlds\realm1\R1M2B','worlds\realm1\R1M2C')
  }
  Set-Content (Join-Path $PSScriptRoot 'kpc-gui-server.cfg') $config -Encoding ASCII
  Set-Content (Join-Path $PSScriptRoot 'kpc-gui-maps.txt') $mapLines -Encoding ASCII
  $worker=Join-Path $PSScriptRoot 'Start-KPC-Selected-Server.ps1'
  $cheatMode=if($cheats.Checked){'Enabled'}else{'Disabled'}
  $coopArg=if($coop.Checked){' -Coop'}else{''}
  Start-Process powershell.exe -WorkingDirectory $PSScriptRoot -ArgumentList ('-NoLogo -NoProfile -NoExit -ExecutionPolicy Bypass -File "'+$worker+'" -Map "'+$chosen.Path+'" -Cheats '+$cheatMode+$coopArg)
 } catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message,'KPCFix server launcher') | Out-Null }
})
[Windows.Forms.Application]::Run($form)
