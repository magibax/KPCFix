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
$form.ClientSize=New-Object Drawing.Size(620,400)
$form.StartPosition='CenterScreen'
$form.FormBorderStyle='FixedDialog'
$form.MaximizeBox=$false
function Label($text,$x,$y) {
 $c=New-Object Windows.Forms.Label; $c.Text=$text; $c.Location=New-Object Drawing.Point($x,$y); $c.Size=New-Object Drawing.Size(560,22); $form.Controls.Add($c)
}
Label 'Map category' 20 15
$group=New-Object System.Windows.Forms.ComboBox
$group.DropDownStyle=[System.Windows.Forms.ComboBoxStyle]::DropDownList
$group.Location=New-Object System.Drawing.Point(20,40)
$group.Width=580
[void]$form.Controls.Add($group)
Label 'Starting map (M0 entries are scripted realm introductions)' 20 80
$map=New-Object System.Windows.Forms.ComboBox
$map.DropDownStyle=[System.Windows.Forms.ComboBoxStyle]::DropDownList
$map.Location=New-Object System.Drawing.Point(20,105)
$map.Width=580
[void]$form.Controls.Add($map)
Label 'Server name' 20 145
$name=New-Object Windows.Forms.TextBox; $name.Location=New-Object Drawing.Point(20,170); $name.Width=580; $name.Text='KPC LAN Server'; $form.Controls.Add($name)
Label 'UDP port                                      Maximum players' 20 210
$port=New-Object Windows.Forms.NumericUpDown; $port.Location=New-Object Drawing.Point(20,235); $port.Minimum=1; $port.Maximum=65535; $port.Value=42972; $form.Controls.Add($port)
$players=New-Object Windows.Forms.NumericUpDown; $players.Location=New-Object Drawing.Point(280,235); $players.Minimum=1; $players.Maximum=16; $players.Value=4; $form.Controls.Add($players)
Label 'Campaign maps are experimental multiplayer maps. This does not add co-op.' 20 280
$button=New-Object Windows.Forms.Button; $button.Text='Start selected map'; $button.Location=New-Object Drawing.Point(390,325); $button.Size=New-Object Drawing.Size(210,40); $form.Controls.Add($button)
$group.Items.Add('All maps') | Out-Null
$categories=New-Object 'System.Collections.Generic.List[string]'
foreach($entry in $maps) {
 $category=[string]$entry.Group
 if(-not $categories.Contains($category)) { $categories.Add($category); [void]$group.Items.Add($category) }
}
$group.add_SelectedIndexChanged({
 $map.Items.Clear()
 $script:visible=@($maps | Where-Object { $group.SelectedItem -eq 'All maps' -or $_.Group -eq $group.SelectedItem })
 foreach($entry in $script:visible) { $suffix=if($entry.Intro){' (intro)'}else{''}; [void]$map.Items.Add([string]($entry.Group+' / '+$entry.Name+$suffix)) }
 if($map.Items.Count){$map.SelectedIndex=0}
})
$group.SelectedIndex=1
$button.add_Click({
 try {
  if($map.SelectedIndex -lt 0){throw 'Select a map.'}
  if($name.Text -match '["\r\n]' -or [string]::IsNullOrWhiteSpace($name.Text)){throw 'Enter a server name without quotes or newlines.'}
  $running=Get-Process -Name dedicated,'DEDICA~1' -ErrorAction SilentlyContinue
  if($running){throw 'A dedicated server is already running. Close it before starting another server with this launcher.'}
  $chosen=$script:visible[$map.SelectedIndex]
  $config=Get-Content (Join-Path $PSScriptRoot 'server-template.cfg') -Raw
  $config=[regex]::Replace($config,'(?m)^"hostname".*$',{param($m) '"hostname" "'+$name.Text+'"'})
  $config=[regex]::Replace($config,'(?m)^"port".*$',('"port" "'+$port.Value+'"'))
  $config=[regex]::Replace($config,'(?m)^"maxclients".*$',('"maxclients" "'+$players.Value+'"'))
  Set-Content (Join-Path $PSScriptRoot 'kpc-gui-server.cfg') $config -Encoding ASCII
  Set-Content (Join-Path $PSScriptRoot 'kpc-gui-maps.txt') $chosen.Path -Encoding ASCII
  $worker=Join-Path $PSScriptRoot 'Start-KPC-Selected-Server.ps1'
  Start-Process powershell.exe -WorkingDirectory $PSScriptRoot -ArgumentList ('-NoProfile -NoExit -ExecutionPolicy Bypass -File "'+$worker+'" -Map "'+$chosen.Path+'"')
 } catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message,'KPCFix server launcher') | Out-Null }
})
[Windows.Forms.Application]::Run($form)
