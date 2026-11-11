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
 # Preserve UTF-8 BOM and Unicode comments when inserting.
 $bom=([Text.UTF8Encoding]::new($true))
 [IO.File]::WriteAllText($iniPath,"[ServerTools]`r`n; café`r`nEnabled=1`r`n",$bom)
 $Cheats='Disabled'
 & $apply
 $bytes=[IO.File]::ReadAllBytes($iniPath)
 if($bytes.Length -lt 3 -or $bytes[0] -ne 239 -or $bytes[1] -ne 187 -or $bytes[2] -ne 191){throw 'UTF-8 BOM not preserved'}
 $actual=[IO.File]::ReadAllText($iniPath)
 if(-not $actual.Contains('; café') -or -not $actual.Contains("AllowCheats=0`r`n")){throw 'Unicode text or CRLF not preserved'}
 # Without [ServerTools], throw rather than insert into the wrong section.
 [IO.File]::WriteAllText($iniPath,"[Coop]`nTraceCampaign=0`n",([Text.UTF8Encoding]::new($false)))
 $failed=$false
 try {$Cheats='Disabled'; & $apply} catch {$failed=$true}
 if(-not $failed){throw 'Missing [ServerTools] should have failed'}
 if([IO.File]::ReadAllText($iniPath) -ne "[Coop]`nTraceCampaign=0`n"){throw 'Invalid INI unexpectedly rewritten'}
 Write-Host 'PASS: Windows INI insertion, CRLF, BOM/Unicode and fail-closed missing section'
}finally {if([IO.Directory]::Exists($testDir)){[IO.Directory]::Delete($testDir,$true)}}
