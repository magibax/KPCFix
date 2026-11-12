param([switch]$Uninstall)
$ErrorActionPreference='Stop'
try {
 Set-Location -LiteralPath $PSScriptRoot
 if(Get-Process -Name @('dedicated','DEDICA~1','client') -ErrorAction SilentlyContinue){throw 'Close the game and server for this one-time setup.'}
 $builds=Get-Content (Join-Path $PSScriptRoot 'SERVER_TOOLS_SUPPORTED_BUILDS.json') -Raw | ConvertFrom-Json
 $meta=Get-Content (Join-Path $PSScriptRoot 'SERVER_TOOLS_BUILD.json') -Raw | ConvertFrom-Json
 $exe=@('dedicated.exe','DEDICA~1.EXE') | Where-Object {Test-Path -LiteralPath (Join-Path $PSScriptRoot $_)} | Select-Object -First 1
 if(-not $exe){throw 'Dedicated executable missing. Extract beside Psycho.exe and dedicated.exe.'}
 $exeHash=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $exe) -Algorithm SHA256).Hash
 $profile=$builds.servers | Where-Object {$_.'dedicated.exe'.sha256 -eq $exeHash} | Select-Object -First 1
 if(-not $profile){throw 'Unsupported dedicated executable; official 1.13 is required.'}
 $server=Join-Path $PSScriptRoot 'server.dll'
 $backend=Join-Path $PSScriptRoot 'server.original.dll'
 $proxy=Join-Path $PSScriptRoot 'KPCServerTools.dll'
 $originalHash=$profile.'server.dll'.sha256
 # Explicitly approved experimental proxy hashes allow a safe return to stable R3.
 # Never trust an arbitrary proxy merely because the official backup exists.
 $approvedExperimentalProxies=@(
  '7112b5ed97f94f8b7cd84bc80f7fdc945f0fb02e1a1cc6ad8f574b9b70d6d373',
  '23dd5de7f099c04706d2a59603bb88763d21bbde2cd9053cf6ebade935598b08',
  '7ea0dfa44dc7870579de6b4e6ffea9018903b7525c3aa7d1b8c9d91013d3fc0d'
 )
 $knownProxies=@($meta.proxy_sha256)+@($meta.previous_proxy_hashes)+$approvedExperimentalProxies
 if(-not $Uninstall -and (Get-FileHash -LiteralPath $proxy -Algorithm SHA256).Hash -ne $meta.proxy_sha256){throw 'Proxy DLL does not match its build metadata.'}
 if(Test-Path -LiteralPath $backend){
  if((Get-FileHash -LiteralPath $backend -Algorithm SHA256).Hash -ne $originalHash){throw 'server.original.dll does not match this dedicated executable.'}
 }elseif($Uninstall){throw 'Original server DLL backup is missing.'}
 $currentHash=if(Test-Path $server){(Get-FileHash -LiteralPath $server -Algorithm SHA256).Hash}else{''}
 if($currentHash -and $currentHash -ne $originalHash -and $currentHash -notin $knownProxies){
  if((Test-Path -LiteralPath $backend) -and -not $Uninstall){throw 'Unrecognized active server.dll. The original server.original.dll is verified. Move server.dll to a backup folder, keep server.original.dll in place, then rerun setup.'}
  throw 'Unrecognized server.dll; it has not been replaced.'
 }
 if($Uninstall){
  if(Test-Path $server){
   $previous=Join-Path $PSScriptRoot ('server.proxy-'+[Guid]::NewGuid().ToString('N')+'.previous')
   [IO.File]::Replace($backend,$server,$previous)
   Remove-Item -LiteralPath $previous -Force -ErrorAction SilentlyContinue
  }else{[IO.File]::Move($backend,$server)}
  Write-Host '[KPCFix Server Tools] Original server.dll restored. object.lto remains stock.'
  exit 0
 }
 if(-not (Test-Path $backend) -and $currentHash -ne $originalHash){throw 'Matching original server.dll is missing.'}
 # Fully validate migration before renaming the engine or deploying the proxy.
 & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Restore-LegacyObject.ps1')
 if($LASTEXITCODE -ne 0){throw 'Could not restore the original object module; proxy was not deployed.'}
 if(-not (Test-Path $backend)){
  if($currentHash -ne $originalHash){throw 'Matching original server.dll is missing.'}
  [IO.File]::Move($server,$backend)
 }
 $temp=Join-Path $PSScriptRoot ('server.proxy-'+[Guid]::NewGuid().ToString('N')+'.tmp')
 try {
  [IO.File]::Copy($proxy,$temp,$false)
  if(Test-Path $server){
   $previous=$temp+'.previous'
   [IO.File]::Replace($temp,$server,$previous)
   Remove-Item -LiteralPath $previous -Force -ErrorAction SilentlyContinue
  }else{[IO.File]::Move($temp,$server)}
 }finally{if(Test-Path $temp){Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue}}
 if((Get-FileHash -LiteralPath $server -Algorithm SHA256).Hash -ne $meta.proxy_sha256){throw 'Proxy installation verification failed; original engine is preserved as server.original.dll.'}
 $ini=Join-Path $PSScriptRoot 'KPCServerTools.ini'
 if(-not (Test-Path $ini)){Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'KPCServerTools-default.ini') -Destination $ini}
 # Preserve obsolete on-disk patch installers away from the current entry points.
 foreach($name in @('Set-ServerCheats.ps1','Enable-ServerCheats.cmd','Disable-ServerCheats.cmd','patch.json')){
  $old=Join-Path $PSScriptRoot $name
  if(Test-Path -LiteralPath $old){
   $archive=Join-Path $PSScriptRoot 'KPCServerTools-Legacy'
   [void](New-Item -ItemType Directory -Path $archive -Force)
   $destination=Join-Path $archive $name
   if(Test-Path $destination){$destination=Join-Path $archive ([Guid]::NewGuid().ToString('N')+'-'+$name)}
   Move-Item -LiteralPath $old -Destination $destination
  }
 }
 Write-Host '[KPCFix Server Tools] DLL installed. Original engine preserved; object.lto is stock.'
 Write-Host '[KPCFix Server Tools] Normal server starts and live commands no longer rewrite game binaries.'
}catch{Write-Host ('[KPCFix Server Tools] '+$_.Exception.Message) -ForegroundColor Red;exit 1}
