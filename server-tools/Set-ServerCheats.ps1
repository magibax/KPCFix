param(
 [ValidateSet('Enabled','Disabled','Stock')][string]$Mode,
 [switch]$Enable,
 [switch]$Disable
)
$ErrorActionPreference='Stop'
try {
 if($Mode -and ($Enable -or $Disable)){throw 'Choose -Mode or the legacy -Enable/-Disable switches, not both.'}
 if(-not $Mode){
  if($Enable -eq $Disable){throw 'Choose -Mode Enabled, -Mode Disabled, or -Mode Stock.'}
  $Mode=if($Enable){'Enabled'}else{'Stock'}
 }
 if(Get-Process -Name @('dedicated','DEDICA~1','client') -ErrorAction SilentlyContinue){throw 'Close the game and server first.'}
 $m=Get-Content (Join-Path $PSScriptRoot 'patch.json') -Raw | ConvertFrom-Json
 $file=Join-Path $PSScriptRoot 'object.lto'
 $b=[IO.File]::ReadAllBytes($file)
 $hash=(Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()
 $known=@($m.original,$m.invulnerability,$m.patched,$m.disabled,
  'cd28762a9fd9eda2d61de4b300e678cd1db77407ca4065c07a7d504042eda6d0',
  'cf68a1312078693a4e932de02485ef95f3af7b87087b48eff2daa0c370ae3326',
  'f35d386377f0bc0485504071cac87ee853a75ff393bb9716ba42f610d3be1051',
  '1d7fe7a6a8e19f62de73e65270447ecdb9735cedc7395584142a9c62da303596',
  'c666b3f12202d9704ef9e5aa66c449f4409fe482867da2c66f52cbc44a748e96',
  '6a1d6a7804b7223e846a4b826983afee559e5305f13b1bf2dc350bd1a79eb5ad',
  '2ed6fb337901939d9aa641a6c0ddeb75656365bac47811bf033850b352a70a12',
  'c1cc5b94e336185e4c211c9909da6e7f43e78a970e228c334e6b3a901a47fb48',
  'a7a266fbb3066b579b1e4e8dabf5ac497ac569b915b5dcaf98c5a58ec22d40ab',
  '147bc86f35759995d89fcbc9c679b7879822ab5c667808a4815590bda268c25d')
 if($hash -notin $known){throw 'Unsupported object.lto. Nothing changed.'}
 $stock=[byte[]]$b.Clone()
 foreach($c in $m.changes){$stock[[int]$c.offset]=[byte]$c.stock}
 $sha=[Security.Cryptography.SHA256]::Create()
 try {
  $check=([BitConverter]::ToString($sha.ComputeHash($stock))).Replace('-','').ToLowerInvariant()
  if($check -ne $m.original){throw 'Normalization failed. Nothing changed.'}
  $backup=Join-Path $PSScriptRoot 'object.lto.before-noclip-test'
  if(Test-Path $backup){
   if((Get-FileHash $backup -Algorithm SHA256).Hash.ToLowerInvariant() -ne $m.original){throw 'Backup mismatch. Nothing changed.'}
  } else {[IO.File]::WriteAllBytes($backup,$stock)}
  $result=[byte[]]$stock.Clone()
  switch($Mode){
   'Enabled' {
    foreach($c in $m.changes){$result[[int]$c.offset]=[byte]$c.patched}
    $expected=$m.patched
   }
   'Disabled' {
    foreach($c in $m.changes){$result[[int]$c.offset]=[byte]$c.disabled}
    $expected=$m.disabled
   }
   'Stock' {$expected=$m.original}
  }
  $check=([BitConverter]::ToString($sha.ComputeHash($result))).Replace('-','').ToLowerInvariant()
  if($check -ne $expected){throw 'Output verification failed. Nothing changed.'}
  if($hash -ne $expected){
   $transaction=Join-Path $PSScriptRoot ('object.lto.kpc-'+[Guid]::NewGuid().ToString('N'))
   $temp=$transaction+'.tmp'
   $previous=$transaction+'.previous'
   $failed=$transaction+'.failed'
   try {
    [IO.File]::WriteAllBytes($temp,$result)
    # Windows PowerShell 5.1 can bind $null as an empty string for this API.
    # Supply a real same-directory rollback path instead of a null backup path.
    [IO.File]::Replace($temp,$file,$previous)
    if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected){
     # Keep both versions if rollback itself fails; never delete the old module.
     [IO.File]::Replace($previous,$file,$failed)
     throw 'Installed-file verification failed; previous object.lto restored.'
    }
    Remove-Item -LiteralPath $previous -Force -ErrorAction SilentlyContinue
    if(Test-Path -LiteralPath $failed){Remove-Item -LiteralPath $failed -Force -ErrorAction SilentlyContinue}
   } finally {
    if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue}
   }
  }
 } finally {$sha.Dispose()}
 switch($Mode){
  'Enabled' {Write-Host '[KPCFix Server Tools] Initial cheats enabled; live console control and chat logging active.'}
  'Disabled' {Write-Host '[KPCFix Server Tools] Initial cheats disabled; live console control and chat logging active.'}
  'Stock' {Write-Host '[KPCFix Server Tools] Original object.lto restored; all server patches removed.'}
 }
} catch {Write-Host ('[KPCFix Server Tools] '+$_.Exception.Message) -ForegroundColor Red;exit 1}
