param([switch]$Enable,[switch]$Disable)
$ErrorActionPreference='Stop'
try {
 if($Enable -eq $Disable){throw 'Choose -Enable or -Disable.'}
 if(Get-Process dedicated,'DEDICA~1',client -ErrorAction SilentlyContinue){throw 'Close the game and server first.'}
 $m=Get-Content (Join-Path $PSScriptRoot 'patch.json') -Raw | ConvertFrom-Json
 $file=Join-Path $PSScriptRoot 'object.lto'
 $b=[IO.File]::ReadAllBytes($file)
 $hash=(Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()
 if($hash -notin @($m.original,$m.invulnerability,$m.patched,'c1cc5b94e336185e4c211c9909da6e7f43e78a970e228c334e6b3a901a47fb48','a7a266fbb3066b579b1e4e8dabf5ac497ac569b915b5dcaf98c5a58ec22d40ab','147bc86f35759995d89fcbc9c679b7879822ab5c667808a4815590bda268c25d')){throw 'Unsupported object.lto. Nothing changed.'}
 $stock=[byte[]]$b.Clone()
 foreach($c in $m.changes){$stock[[int]$c.offset]=[byte]$c.stock}
 $sha=[Security.Cryptography.SHA256]::Create()
 $check=([BitConverter]::ToString($sha.ComputeHash($stock))).Replace('-','').ToLowerInvariant()
 if($check -ne $m.original){throw 'Normalization failed. Nothing changed.'}
 $backup=Join-Path $PSScriptRoot 'object.lto.before-noclip-test'
 if(Test-Path $backup){if((Get-FileHash $backup -Algorithm SHA256).Hash.ToLowerInvariant() -ne $m.original){throw 'Backup mismatch. Nothing changed.'}}
 else{[IO.File]::WriteAllBytes($backup,$stock)}
 $result=$stock
 if($Enable){foreach($c in $m.changes){$result[[int]$c.offset]=[byte]$c.patched}}
 $expected=if($Enable){$m.patched}else{$m.original}
 $check=([BitConverter]::ToString($sha.ComputeHash($result))).Replace('-','').ToLowerInvariant()
 if($check -ne $expected){throw 'Output verification failed. Nothing changed.'}
 [IO.File]::WriteAllBytes($file,$result)
 Write-Host $(if($Enable){'[KPCFix Server Tools] Noclip and invulnerability ENABLED.'}else{'[KPCFix Server Tools] Original object.lto restored; cheats disabled.'})
} catch {Write-Host $_.Exception.Message -ForegroundColor Red;exit 1}
