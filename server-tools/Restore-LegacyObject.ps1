# Internal setup migration: restore only exact known object patches to stock.
param([ValidateSet('Stock')][string]$Mode='Stock')
$ErrorActionPreference='Stop'
try {
 if(Get-Process -Name @('dedicated','DEDICA~1','client') -ErrorAction SilentlyContinue){throw 'Close the game and server before setup.'}
 $m=Get-Content (Join-Path $PSScriptRoot 'legacy-object-patches.json') -Raw | ConvertFrom-Json
 $file=Join-Path $PSScriptRoot 'object.lto'
 $hash=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
 if($hash -eq $m.original){exit 0}
 $known=@($m.patched,$m.disabled,'cd28762a9fd9eda2d61de4b300e678cd1db77407ca4065c07a7d504042eda6d0','cf68a1312078693a4e932de02485ef95f3af7b87087b48eff2daa0c370ae3326','f35d386377f0bc0485504071cac87ee853a75ff393bb9716ba42f610d3be1051','1d7fe7a6a8e19f62de73e65270447ecdb9735cedc7395584142a9c62da303596','c666b3f12202d9704ef9e5aa66c449f4409fe482867da2c66f52cbc44a748e96','6a1d6a7804b7223e846a4b826983afee559e5305f13b1bf2dc350bd1a79eb5ad','2ed6fb337901939d9aa641a6c0ddeb75656365bac47811bf033850b352a70a12','c1cc5b94e336185e4c211c9909da6e7f43e78a970e228c334e6b3a901a47fb48','a7a266fbb3066b579b1e4e8dabf5ac497ac569b915b5dcaf98c5a58ec22d40ab','147bc86f35759995d89fcbc9c679b7879822ab5c667808a4815590bda268c25d','de5cc80287084508b56de27e9a535de7d389022327771c4f96107a327fc4adc8')
 if($hash -notin $known){throw 'Unsupported object.lto; nothing changed.'}
 $stock=[IO.File]::ReadAllBytes($file)
 foreach($c in $m.changes){$stock[[int]$c.offset]=[byte]$c.stock}
 $sha=[Security.Cryptography.SHA256]::Create()
 try {$check=([BitConverter]::ToString($sha.ComputeHash($stock))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
 if($check -ne $m.original){throw 'Original object reconstruction failed; nothing changed.'}
 $transaction=$file+'.restore-'+[Guid]::NewGuid().ToString('N')
 $temp=$transaction+'.tmp';$previous=$transaction+'.previous'
 try {
  [IO.File]::WriteAllBytes($temp,$stock)
  [IO.File]::Replace($temp,$file,$previous)
  if((Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $m.original){
   [IO.File]::Replace($previous,$file,$transaction+'.failed')
   throw 'Verification failed; previous module restored.'
  }
  Remove-Item -LiteralPath $previous -Force -ErrorAction SilentlyContinue
 }finally{if(Test-Path $temp){Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue}}
 Write-Host '[KPCFix Server Tools] Original 1.13 object.lto restored for runtime patching.'
}catch{Write-Host ('[KPCFix Server Tools] '+$_.Exception.Message) -ForegroundColor Red;exit 1}
