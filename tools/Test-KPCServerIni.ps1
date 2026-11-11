# Exercise the exact INI-editing block embedded in the production launcher.
# Uses PowerShell/.NET on a Windows GitHub runner; game files are not required.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$launcher=[IO.File]::ReadAllText((Join-Path $root 'server-tools/Start-KPC-Selected-Server.ps1'))
$start=$launcher.IndexOf('$iniPath=Join-Path $PSScriptRoot')
$end=$launcher.IndexOf('# Write to this console', $start)
if($start -lt 0 -or $end -le $start){throw 'Launcher INI block not found'}
$logic=$launcher.Substring($start,$end-$start)
$testDir=Join-Path ([IO.Path]::GetTempPath()) ('kpc-ini-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($testDir)
$logic=$logic.Replace('Join-Path $PSScriptRoot','Join-Path $testDir')
$apply=[scriptblock]::Create($logic)
$iniPath=Join-Path $testDir 'KPCServerTools.ini'
try {
 # Missing key: insertion stays within [ServerTools] and LF stays LF.
 [IO.File]::WriteAllText($iniPath,"[ServerTools]`nEnabled=1`n[Coop]`nTraceCampaign=0`n",([Text.UTF8Encoding]::new($false)))
 $Cheats='Disabled'
 & $apply
 $actual=[IO.File]::ReadAllText($iniPath)
 if($actual -ne "[ServerTools]`nAllowCheats=0`nEnabled=1`n[Coop]`nTraceCampaign=0`n"){throw "LF insertion failed: $actual"}
 # Existing key: no duplicate, all line endings remain CRLF.
 [IO.File]::WriteAllText($iniPath,"[ServerTools]`r`nEnabled=1`r`nAllowCheats=0`r`n",([Text.UTF8Encoding]::new($false)))
 $Cheats='Enabled'
 & $apply
 $actual=[IO.File]::ReadAllText($iniPath)
 if($actual -ne "[ServerTools]`r`nEnabled=1`r`nAllowCheats=1`r`n"){throw "CRLF update failed: $actual"}
 # A UTF-8 BOM is dropped; Unicode comments and CRLF survive the rewrite.
 $accent=[string][char]0xE9
 $bom=([Text.UTF8Encoding]::new($true))
 [IO.File]::WriteAllText($iniPath,"[ServerTools]`r`n; caf$accent`r`nEnabled=1`r`n",$bom)
 $Cheats='Enabled'
 & $apply
 $bytes=[IO.File]::ReadAllBytes($iniPath)
 if($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191){throw 'UTF-8 BOM was not removed'}
 $actual=[IO.File]::ReadAllText($iniPath)
 if($actual -ne "[ServerTools]`r`nAllowCheats=1`r`n; caf$accent`r`nEnabled=1`r`n"){throw "BOM rewrite changed other content: $actual"}
 # On real Windows, read back through the proxy DLL's own API, GetPrivateProfileIntW.
 # 1 = section found, 7 = fallback (section missed).
 if($env:OS -eq 'Windows_NT'){
  Add-Type -Namespace KPCTest -Name Ini -MemberDefinition '[DllImport("kernel32.dll",CharSet=CharSet.Unicode,ExactSpelling=true)] public static extern uint GetPrivateProfileIntW(string section,string key,int fallback,string file);'
  $rewritten=[KPCTest.Ini]::GetPrivateProfileIntW('ServerTools','AllowCheats',7,$iniPath)
  if($rewritten -ne 1){throw "GetPrivateProfileIntW could not read the rewritten INI: $rewritten"}
  # Reference observation, not an assertion: why the BOM is dropped.
  $control=Join-Path $testDir 'bom-control.ini'
  [IO.File]::WriteAllText($control,"[ServerTools]`r`nAllowCheats=1`r`n",$bom)
  $withBom=[KPCTest.Ini]::GetPrivateProfileIntW('ServerTools','AllowCheats',7,$control)
  $plain=Join-Path $testDir 'plain-control.ini'
  [IO.File]::WriteAllText($plain,"[ServerTools]`r`nAllowCheats=1`r`n",([Text.UTF8Encoding]::new($false)))
  $withoutBom=[KPCTest.Ini]::GetPrivateProfileIntW('ServerTools','AllowCheats',7,$plain)
  Write-Host "INFO: GetPrivateProfileIntW first-line [ServerTools] AllowCheats: with UTF-8 BOM=$withBom, without BOM=$withoutBom"
 }
 # Without [ServerTools], throw rather than insert into the wrong section.
 [IO.File]::WriteAllText($iniPath,"[Coop]`nTraceCampaign=0`n",([Text.UTF8Encoding]::new($false)))
 $failed=$false
 try {$Cheats='Disabled'; & $apply} catch {$failed=$true}
 if(-not $failed){throw 'Missing [ServerTools] should have failed'}
 if([IO.File]::ReadAllText($iniPath) -ne "[Coop]`nTraceCampaign=0`n"){throw 'Invalid INI unexpectedly rewritten'}
 Write-Host 'PASS: Windows INI insertion, CRLF, BOM removal/Unicode and fail-closed missing section'
}finally {if([IO.Directory]::Exists($testDir)){[IO.Directory]::Delete($testDir,$true)}}
