<#
.SYNOPSIS
 Native Windows smoke test of Server Tools on the real dedicated.exe, without clients.

.DESCRIPTION
 Run this ONLY in a disposable copy of an installed game folder where
 Setup-KPCServerTools.cmd has already been run (Server Tools as winmm.dll).
 It drives the real launcher worker (Start-KPC-Selected-Server.ps1), so the
 real version check and INI rewrite run, starts PM01, types "quit" into the
 server's own console and checks KPCServerTools.log. Results go to a JSON
 report. It never writes game binaries; it writes kpc-gui-server.cfg and
 kpc-gui-maps.txt (as the launcher does), and the worker sets AllowCheats.
 PassThrough restores KPCServerTools.ini afterwards.

 The server opens the game port from server-template.cfg; -nogamespy keeps it
 off the GameSpy master list.

 It must run in its own console window (the worker and dedicated.exe share it):
   Start-Process powershell.exe -Wait -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File tools\Invoke-ServerSmoke.ps1 -GameDir C:\KPCTest\smoke-master\Psycho -Mode Normal -Report C:\KPCTest\normal.json'

 Modes:
  Normal      hooks installed on the stock object.lto, loaded as winmm.dll,
              server frame installed (Europe-family server.dll) or reported
              as not supported (USA server.dll).
  PassThrough [ServerTools] Enabled=0: the original engine runs unhooked.
#>
param(
 [Parameter(Mandatory=$true)][string]$GameDir,
 [ValidateSet('Normal','PassThrough')][string]$Mode='Normal',
 [Parameter(Mandatory=$true)][string]$Report,
 [int]$TimeoutSeconds=90
)
$ErrorActionPreference='Stop'
$GameDir=(Resolve-Path -LiteralPath $GameDir).Path
foreach($required in 'dedicated.exe','server.dll','winmm.dll','KPCServerTools.dll','KPCServerTools.ini','server-template.cfg','Start-KPC-Selected-Server.ps1'){
 if(-not (Test-Path -LiteralPath (Join-Path $GameDir $required))){throw "Missing $required in $GameDir; install Server Tools in a disposable copy first."}
}
# Another server (for example a player's test server in a different copy) may
# keep running: refuse only a server from this folder or an occupied game port.
function Get-OwnServers{
 @(Get-Process -Name @('dedicated','DEDICA~1') -ErrorAction SilentlyContinue | Where-Object {
  $_.Path -and $_.Path.StartsWith($GameDir+'\',[StringComparison]::OrdinalIgnoreCase) })
}
if(@(Get-OwnServers).Count){throw "A dedicated server from $GameDir is already running."}
$portMatch=[regex]::Match((Get-Content -LiteralPath (Join-Path $GameDir 'server-template.cfg') -Raw),'(?m)^"port"\s+"(\d+)"')
if(-not $portMatch.Success){throw 'server-template.cfg has no "port" line.'}
$gamePort=[int]$portMatch.Groups[1].Value
if(Get-NetUDPEndpoint -LocalPort $gamePort -ErrorAction SilentlyContinue){throw "UDP port $gamePort is already in use; use a copy with a free port in server-template.cfg."}

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
public static class KPCSmokeConsole {
 [StructLayout(LayoutKind.Explicit, Size=20)]
 public struct Record {
  [FieldOffset(0)] public ushort Type;
  [FieldOffset(4)] public int Down;
  [FieldOffset(8)] public ushort Repeat;
  [FieldOffset(10)] public ushort VirtualKey;
  [FieldOffset(12)] public ushort Scan;
  [FieldOffset(14)] public char Character;
  [FieldOffset(16)] public uint Control;
 }
 [StructLayout(LayoutKind.Sequential)] struct Coord { public short X, Y; }
 [StructLayout(LayoutKind.Sequential)] struct Info { public Coord Size, Cursor; public ushort Attr; public short L,T,R,B; public Coord Max; }
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 static extern IntPtr CreateFile(string name,uint access,uint share,IntPtr security,uint disposition,uint flags,IntPtr template);
 [DllImport("kernel32.dll", EntryPoint="WriteConsoleInputW", SetLastError=true)]
 static extern bool WriteConsoleInput(IntPtr h,[In] Record[] records,uint length,out uint written);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetConsoleScreenBufferInfo(IntPtr h,out Info i);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, EntryPoint="ReadConsoleOutputCharacterW", SetLastError=true)]
 static extern bool ReadChars(IntPtr h,StringBuilder b,uint n,Coord at,out uint read);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 static IntPtr Open(string name){
  IntPtr h=CreateFile(name,0xC0000000,3,IntPtr.Zero,3,0,IntPtr.Zero);
  if(h==new IntPtr(-1)) throw new Win32Exception(Marshal.GetLastWin32Error());
  return h;
 }
 public static void Submit(string text) {
  IntPtr h=Open("CONIN$");
  try {
   Record[] r=new Record[text.Length*2];
   for(int i=0;i<text.Length;i++) {
    r[i*2]=new Record { Type=1, Down=1, Repeat=1, Character=text[i], VirtualKey=(ushort)(text[i]=='\r'?13:0) };
    r[i*2+1]=r[i*2];r[i*2+1].Down=0;
   }
   uint n;
   if(!WriteConsoleInput(h,r,(uint)r.Length,out n)||n!=(uint)r.Length) throw new Win32Exception(Marshal.GetLastWin32Error());
  } finally { CloseHandle(h); }
 }
 // Text of the console screen buffer up to the cursor row (evidence only).
 public static string Screen() {
  IntPtr h=Open("CONOUT$");
  try {
   Info i; if(!GetConsoleScreenBufferInfo(h,out i)) return "";
   StringBuilder all=new StringBuilder();
   for(short y=0;y<=i.Cursor.Y;y++){
    StringBuilder b=new StringBuilder(i.Size.X+1); uint read; Coord c; c.X=0; c.Y=y;
    if(ReadChars(h,b,(uint)i.Size.X,c,out read)) all.AppendLine(b.ToString(0,(int)read).TrimEnd());
   }
   return all.ToString();
  } finally { CloseHandle(h); }
 }
}
'@

$log=Join-Path $GameDir 'KPCServerTools.log'
$ini=Join-Path $GameDir 'KPCServerTools.ini'
$iniBytes=[IO.File]::ReadAllBytes($ini)
$start=if(Test-Path -LiteralPath $log){(Get-Item -LiteralPath $log).Length}else{0}
function Read-NewLog {
 if(-not (Test-Path -LiteralPath $log)){return @()}
 $fs=[IO.File]::Open($log,'Open','Read','ReadWrite')
 try{ [void]$fs.Seek($start,'Begin'); $text=(New-Object IO.StreamReader($fs)).ReadToEnd() }finally{$fs.Dispose()}
 return @($text -split "`r?`n" | Where-Object { $_ })
}
function Wait-Log([string]$pattern,[int]$seconds){
 $deadline=(Get-Date).AddSeconds($seconds)
 while((Get-Date) -lt $deadline){
  if(@(Read-NewLog | Where-Object { $_ -match $pattern }).Count){return $true}
  if($script:worker.HasExited){return $false}
  Start-Sleep -Milliseconds 500
 }
 return $false
}
$checks=[ordered]@{}
function Check([string]$name,[bool]$ok){$script:checks[$name]=if($ok){'PASS'}else{'FAIL'}}

$template=Get-Content -LiteralPath (Join-Path $GameDir 'server-template.cfg') -Raw
$template=[regex]::Replace($template,'(?m)^"hostname".*$','"hostname" "KPCFix smoke test"')
$map='worlds\deathmatch\PM01'
Set-Content -LiteralPath (Join-Path $GameDir 'kpc-gui-server.cfg') $template -Encoding ASCII
Set-Content -LiteralPath (Join-Path $GameDir 'kpc-gui-maps.txt') @($map) -Encoding ASCII
if($Mode -eq 'PassThrough'){
 $text=[IO.File]::ReadAllText($ini)
 if($text -notmatch '(?im)^[ \t]*Enabled[ \t]*='){throw 'KPCServerTools.ini has no Enabled key.'}
 [IO.File]::WriteAllText($ini,[regex]::Replace($text,'(?im)^[ \t]*Enabled[ \t]*=[^\r\n]*','Enabled=0'))
}

$hash=(Get-FileHash -LiteralPath (Join-Path $GameDir 'KPCServerTools.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
$serverHash=(Get-FileHash -LiteralPath (Join-Path $GameDir 'server.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
$loaderHash=(Get-FileHash -LiteralPath (Join-Path $GameDir 'winmm.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
# Europe/Germany/Italy server.dll; the server frame hook supports only this one.
$europeServer=$serverHash -eq '83b7bce809948b6f70ed3f35a77a771b6087f1104d90d06a62ac3dc0d4bd37d5'
# Windows PowerShell 5.1 does not quote array arguments; quote paths explicitly.
$workerArgs='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $GameDir 'Start-KPC-Selected-Server.ps1')+'" -Map "'+$map+'" -Cheats Disabled'
$events=New-Object System.Collections.Generic.List[string]
$screen=''
$exitCode=$null
try{
 $script:worker=Start-Process -FilePath powershell.exe -ArgumentList $workerArgs -WorkingDirectory $GameDir -NoNewWindow -PassThru
 $null=$worker.Handle   # keep the handle so ExitCode is available
 $events.Add("worker started pid=$($worker.Id)")
 Check 'proxy active' (Wait-Log ($(if($Mode -eq 'PassThrough'){'Original engine pass-through'}else{'Dedicated DLL active'})) $TimeoutSeconds)
 if($Mode -ne 'PassThrough'){
  Check 'object hooks installed on stock object.lto' (Wait-Log 'Runtime object hooks installed; file unchanged' $TimeoutSeconds)
 }
 Start-Sleep -Seconds 11
 $screen=[KPCSmokeConsole]::Screen()
 if(-not $worker.HasExited){[KPCSmokeConsole]::Submit("quit`r");$events.Add('typed quit')}
 if(-not $worker.WaitForExit(30000)){
  $events.Add('quit timed out; stopping dedicated server')
  Get-OwnServers | Stop-Process -Force   # never a server from another folder
  [void]$worker.WaitForExit(10000)
 }
 $worker.Refresh(); if($worker.HasExited){$exitCode=$worker.ExitCode}
}finally{
 if($Mode -eq 'PassThrough'){[IO.File]::WriteAllBytes($ini,$iniBytes);$events.Add('restored KPCServerTools.ini')}
}

$lines=@(Read-NewLog)
Check 'winmm.dll is the installed Server Tools' ($hash -eq $loaderHash)
Check 'server.dll is not a Server Tools build' ($hash -ne $serverHash)
Check 'loaded as winmm.dll' (@($lines | Where-Object { $_ -match 'Loaded as winmm\.dll; original server\.dll unchanged' }).Count -ge 1)
if($Mode -eq 'PassThrough'){
 Check 'no hook lines in pass-through' (-not @($lines | Where-Object { $_ -match 'hooks installed|server frame installed' }).Count)
}elseif($europeServer){
 Check 'server frame installed' (@($lines | Where-Object { $_ -match 'Minimum server frame installed, ServerFrameMs=' }).Count -ge 1)
}else{
 Check 'server frame reported as not supported (USA server.dll)' (@($lines | Where-Object { $_ -match 'ServerFrameMs is not supported for this server\.dll' }).Count -ge 1)
}
$failed=@($checks.Values | Where-Object { $_ -ne 'PASS' }).Count
$result=[ordered]@{
 mode=$Mode; map=$map; date=(Get-Date).ToString('o'); os=[Environment]::OSVersion.VersionString
 result=$(if($failed){'FAIL'}else{'PASS'}); checks=$checks; events=$events; worker_exit=$exitCode
 kpcservertools_dll_sha256=$hash; server_dll_sha256=$serverHash; winmm_dll_sha256=$loaderHash
 object_lto_sha256=(Get-FileHash -LiteralPath (Join-Path $GameDir 'object.lto') -Algorithm SHA256).Hash.ToLowerInvariant()
 dedicated_exe_sha256=(Get-FileHash -LiteralPath (Join-Path $GameDir 'dedicated.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
 log=$lines; console=$screen
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Report -Encoding UTF8
Write-Host "[KPCFix Server Tools smoke] $Mode $($result.result); report: $Report"
if($failed){exit 1}
exit 0
