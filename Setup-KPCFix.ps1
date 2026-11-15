# Apply the display preset once and the Developer flag before each launch.
[CmdletBinding()]
param([ValidateRange(640,7680)][int]$Width=1920,
      [ValidateRange(480,4320)][int]$Height=1080,
      [switch]$Restore, [switch]$OnLaunch)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$Manifest=Join-Path $PSScriptRoot 'KPCFix-Display-Backups.json'
$Marker=Join-Path $PSScriptRoot 'KPCFix-Display-Preset.applied'
$CRLF=([string][char]13)+[char]10
$LF=[string][char]10
$LineBreaks=[char[]]@([char]13,[char]10)
if ($Restore) {
  if (-not (Test-Path -LiteralPath $Manifest)) { throw 'No setup backup manifest found.' }
  $Files=Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
  foreach ($Entry in $Files) {
    if ($Entry.Existed -and !(Test-Path -LiteralPath (Join-Path $PSScriptRoot ($Entry.Path+'.kpcfix-display.bak')) -PathType Leaf)) {
      throw "Missing display backup for $($Entry.Path)."
    }
  }
  foreach ($Entry in $Files) {
    $Target=Join-Path $PSScriptRoot $Entry.Path
    if ($Entry.Existed) { Copy-Item -LiteralPath ($Target+'.kpcfix-display.bak') -Destination $Target -Force }
    # Do not delete a profile created by the game after KPCFix was installed.
    elseif ($Entry.Path -eq 'autoexec.cfg' -and (Test-Path -LiteralPath $Target)) { Remove-Item -LiteralPath $Target }
  }
  Remove-Item -LiteralPath $Manifest
  Remove-Item -LiteralPath $Marker -ErrorAction SilentlyContinue
  foreach ($Entry in $Files) {
    if ($Entry.Existed) { Remove-Item -LiteralPath (Join-Path $PSScriptRoot ($Entry.Path+'.kpcfix-display.bak')) }
  }
  Write-Host 'Restored game configuration from the initial KPCFix setup backup.'
  exit 0
}
. (Join-Path $PSScriptRoot 'Check-KPCVersion.ps1')
$null = Assert-KPCGameVersion
$FirstDisplayApply = -not ($OnLaunch -and (Test-Path -LiteralPath $Marker))
$IniPath=Join-Path $PSScriptRoot 'KPCFix.ini'
$IniText=if(Test-Path -LiteralPath $IniPath){Get-Content -LiteralPath $IniPath -Raw}else{''}
function Get-KPCFixOption([string]$Name,[int]$Default) {
  $section=[regex]::Match($IniText,'(?ims)^\[KPCFix\][^\[]*')
  if(!$section.Success){return $Default}
  $match=[regex]::Match($section.Value,'(?im)^\s*'+[regex]::Escape($Name)+'\s*=\s*([01])\s*(?:;.*)?$')
  if(!$match.Success){return $Default}
  return [int]$match.Groups[1].Value
}
$ForceDeveloper=(Get-KPCFixOption 'ForceDeveloper' 1) -ne 0
$Encoding=[Text.Encoding]::GetEncoding(1252)
function Set-KPCDeveloperFlag([string]$RelativePath) {
  if(!$ForceDeveloper){return $false}
  $Path=Join-Path $PSScriptRoot $RelativePath
  $Exists=Test-Path -LiteralPath $Path -PathType Leaf
  if($Exists){$Text=$Encoding.GetString([IO.File]::ReadAllBytes($Path))}else{$Text=''}
  if($Text.Contains([char]0)){throw "Unsupported text encoding in $RelativePath; skipped. Close the game before setup."}
  $Newline=if($Text.Contains($CRLF)){$CRLF}else{$LF}
  $Before=$Text
  $Pattern='(?im)^[\t ]*"?Developer"?[\t ]+[^\r\n]*'
  $Line='"Developer" "1"'
  if([regex]::IsMatch($Text,$Pattern)){$Text=[regex]::Replace($Text,$Pattern,$Line)}
  else{$Text=$Text.TrimEnd($LineBreaks)+$Newline+$Line+$Newline}
  if($Text -ceq $Before){return $false}
  if(!$Exists){
    $Parent=Split-Path -Parent $Path
    if(!(Test-Path -LiteralPath $Parent)){New-Item -ItemType Directory -Path $Parent -Force|Out-Null}
  }
  $RecordsLocal=@($script:Records | Where-Object {$_.Path -eq $RelativePath})
  if($RecordsLocal.Count -eq 0){
    if($Exists){
      if(Test-Path -LiteralPath ($Path+'.kpcfix-display.bak')){throw "Untracked backup already exists for $RelativePath; retain it and resolve before setup."}
      Copy-Item -LiteralPath $Path -Destination ($Path+'.kpcfix-display.bak')
    }
    $script:Records+=@{Path=$RelativePath;Existed=[bool]$Exists}
    [IO.File]::WriteAllText($Manifest,(ConvertTo-Json -InputObject @($script:Records)),[Text.Encoding]::UTF8)
  }
  $Temp=$Path+'.kpcfix-config.tmp'
  [IO.File]::WriteAllBytes($Temp,$Encoding.GetBytes($Text))
  Move-Item -LiteralPath $Temp -Destination $Path -Force
  Write-Host "Set Developer=1 in $RelativePath."
  return $true
}
$Paths=@('autoexec.cfg')
if (Test-Path -LiteralPath 'Profiles') {
  $Paths+=@(Get-ChildItem -LiteralPath 'Profiles' -Filter '*.cfg' -File | ForEach-Object {'Profiles\'+$_.Name})
}
$script:Records=@()
if (Test-Path -LiteralPath $Manifest) { $script:Records=@(Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json) }
foreach ($Relative in $Paths) {
  $Path=Join-Path $PSScriptRoot $Relative
  $Exists=Test-Path -LiteralPath $Path
  if($FirstDisplayApply){
    if($Exists){$Text=$Encoding.GetString([IO.File]::ReadAllBytes($Path))}else{$Text=''}
    if($Text.Contains([char]0)){throw "Unsupported text encoding in $Relative; skipped. Close the game before setup."}
    $Before=$Text
    $Newline=if($Text.Contains($CRLF)){$CRLF}else{$LF}
    foreach($Setting in @(@('ScreenWidth',"$Width"),@('ScreenHeight',"$Height"))){
      $Key=$Setting[0];$Value=$Setting[1];$Line='"'+$Key+'" "'+$Value+'"'
      $Pattern='(?im)^[\t ]*"?'+[regex]::Escape($Key)+'"?[\t ]+[^\r\n]*'
      if([regex]::IsMatch($Text,$Pattern)){$Text=[regex]::Replace($Text,$Pattern,$Line)}
      else{$Text=$Text.TrimEnd($LineBreaks)+$Newline+$Line+$Newline}
    }
    if($Text -cne $Before){
      $RecordsLocal=@($script:Records | Where-Object {$_.Path -eq $Relative})
      if($RecordsLocal.Count -eq 0){
        if($Exists){
          if(Test-Path -LiteralPath ($Path+'.kpcfix-display.bak')){throw "Untracked backup already exists for $Relative; retain it and resolve before setup."}
          Copy-Item -LiteralPath $Path -Destination ($Path+'.kpcfix-display.bak')
        }
        $script:Records+=@{Path=$Relative;Existed=[bool]$Exists}
        [IO.File]::WriteAllText($Manifest,(ConvertTo-Json -InputObject @($script:Records)),[Text.Encoding]::UTF8)
      }
      if(!$Exists){$Parent=Split-Path -Parent $Path;if(!(Test-Path -LiteralPath $Parent)){New-Item -ItemType Directory -Path $Parent -Force|Out-Null}}
      $Temp=$Path+'.kpcfix-display.tmp';[IO.File]::WriteAllBytes($Temp,$Encoding.GetBytes($Text));Move-Item -LiteralPath $Temp -Destination $Path -Force
    }
  }
}
if($ForceDeveloper){
  foreach($Relative in @('default.cfg')+$Paths){Set-KPCDeveloperFlag $Relative | Out-Null}
}else{Write-Host 'ForceDeveloper=0; stored Developer values were left unchanged.'}
if($FirstDisplayApply){
  Write-Host ('Applied '+$Width+'x'+$Height+' display preset. Later game resolution changes are retained.')
  [IO.File]::WriteAllText($Marker,($Width.ToString()+'x'+$Height.ToString()),[Text.Encoding]::ASCII)
}else{Write-Host 'Display resolution left as selected in game.'}
