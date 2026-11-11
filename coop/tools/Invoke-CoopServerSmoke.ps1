<#
.SYNOPSIS
 Native Windows smoke test of the experimental co-op Server Tools DLL without clients.

.DESCRIPTION
 Run this ONLY in a disposable copy of an installed game folder where
 Setup-KPCServerTools.cmd has already been run. It drives the real launcher
 worker (Start-KPC-Selected-Server.ps1), so the real version checks and INI
 rewrite run, then types console commands into the dedicated server's own
 console (nextlevel, quit) and checks KPCServerTools.log for the expected
 markers. Results go to a JSON report. The script never writes game binaries.
 It writes kpc-gui-server.cfg and kpc-gui-maps.txt (as the launcher does), and
 the worker updates KPCServerTools.ini. PassThrough restores the INI afterwards.

 The dedicated server opens its normal game port (server-template.cfg). It is
 started with -nogamespy, so it is not announced to the GameSpy master list.

 It must run in its own console window (the worker and dedicated.exe share it):
   Start-Process powershell.exe -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File Invoke-CoopServerSmoke.ps1 -GameDir C:\KPCTest\Psycho -Mode Coop -Report C:\KPCTest\coop.json'

 Modes:
  Coop        co-op MVP start on R1M2B, then console nextlevel twice
              (R1M2C, then R1M3A), then quit.
  Normal      PM01 deathmatch without the co-op box: CoopSession must be 0.
  PassThrough [ServerTools] Enabled=0: the proxy must forward to the original.

 Hold (Coop only, for client tests): with -CommandFile, the scripted
 nextlevels are skipped. The server keeps running and every new line appended
 to the command file is typed into the server console (for example
 "say hello", "nextlevel", "list_ids"). "quit" ends the run; -HoldSeconds is
 the upper bound. -MinPlayers temporarily sets [Coop] CoopMinPlayers (1-3) so
 one client can drive the whole route; the INI is restored afterwards.
#>
param(
 [Parameter(Mandatory=$true)][string]$GameDir,
 [ValidateSet('Coop','Normal','PassThrough')][string]$Mode='Coop',
 [Parameter(Mandatory=$true)][string]$Report,
 [int]$TimeoutSeconds=90,
 [string]$CommandFile,
 [int]$HoldSeconds=900,
 [ValidateRange(0,3)][int]$MinPlayers=0
)
$ErrorActionPreference='Stop'
$GameDir=(Resolve-Path -LiteralPath $GameDir).Path
foreach($required in 'dedicated.exe','server.dll','server.original.dll','KPCServerTools.dll','KPCServerTools.ini','server-template.cfg','Start-KPC-Selected-Server.ps1'){
 if(-not (Test-Path -LiteralPath (Join-Path $GameDir $required))){throw "Missing $required in $GameDir; install Server Tools in a disposable copy first."}
}
if(Get-Process -Name @('dedicated','DEDICA~1') -ErrorAction SilentlyContinue){throw 'A dedicated server is already running.'}

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
$template=[regex]::Replace($template,'(?m)^"hostname".*$','"hostname" "KPCFix co-op smoke test"')
if($Mode -eq 'Coop'){
 $map='worlds\realm1\R1M2B'
 foreach($pair in @(@('maxclients','3'),@('fraglimit','0'),@('timelimit','0'),@('keepitems','1'))){
  $template=[regex]::Replace($template,'(?m)^"'+$pair[0]+'".*$',('"'+$pair[0]+'" "'+$pair[1]+'"'))
 }
 $maps=@('worlds\realm1\R1M2B','worlds\realm1\R1M2C','worlds\realm1\R1M3A')
}else{
 $map='worlds\deathmatch\PM01'
 $maps=@($map)
}
Set-Content -LiteralPath (Join-Path $GameDir 'kpc-gui-server.cfg') $template -Encoding ASCII
Set-Content -LiteralPath (Join-Path $GameDir 'kpc-gui-maps.txt') $maps -Encoding ASCII
if($CommandFile -and $Mode -ne 'Coop'){throw '-CommandFile is only supported with -Mode Coop.'}
if($MinPlayers -gt 0){
 $text=[IO.File]::ReadAllText($ini)
 if($text -notmatch '(?im)^[ \t]*\[Coop\][ \t]*\r?\n'){throw 'KPCServerTools.ini has no [Coop] section.'}
 $text=[regex]::Replace($text,'(?im)^[ \t]*CoopMinPlayers[ \t]*=[^\r\n]*(\r?\n)?','')
 # Insert directly under [Coop], reusing that line's own newline sequence.
 $text=[regex]::Replace($text,'(?im)^([ \t]*\[Coop\][ \t]*)(\r?\n)',('$1$2CoopMinPlayers='+$MinPlayers+'$2'))
 [IO.File]::WriteAllText($ini,$text)
}
if($Mode -eq 'PassThrough'){
 $text=[IO.File]::ReadAllText($ini)
 if($text -notmatch '(?im)^[ \t]*Enabled[ \t]*='){throw 'KPCServerTools.ini has no Enabled key.'}
 [IO.File]::WriteAllText($ini,[regex]::Replace($text,'(?im)^[ \t]*Enabled[ \t]*=[^\r\n]*','Enabled=0'))
}

$hash=(Get-FileHash -LiteralPath (Join-Path $GameDir 'KPCServerTools.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
$serverHash=(Get-FileHash -LiteralPath (Join-Path $GameDir 'server.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
# Windows PowerShell 5.1 does not quote array arguments; quote paths explicitly.
$workerArgs='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $GameDir 'Start-KPC-Selected-Server.ps1')+'" -Map "'+$map+'" -Cheats Disabled'
if($Mode -eq 'Coop'){$workerArgs+=' -Coop'}
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
 if($Mode -eq 'Coop'){
  Check 'session armed' (Wait-Log 'armed session armed kpc-coop/1 route=R1M2B>R1M2C>R1M3A' 10)
  Start-Sleep -Seconds 8   # let the first world finish loading before nextlevel
 }
 if($Mode -eq 'Coop' -and $CommandFile){
  if(-not (Test-Path -LiteralPath $CommandFile)){New-Item -ItemType File -Path $CommandFile | Out-Null}
  $done=@(Get-Content -LiteralPath $CommandFile).Count
  $events.Add("holding; forwarding new lines of $CommandFile")
  $deadline=(Get-Date).AddSeconds($HoldSeconds)
  while(-not $worker.HasExited -and (Get-Date) -lt $deadline){
   $all=@(Get-Content -LiteralPath $CommandFile)
   for(;$done -lt $all.Count;$done++){
    $command=$all[$done].Trim()
    if(-not $command -or $command -match '[\x00-\x1f]'){continue}
    $events.Add("$((Get-Date).ToString('HH:mm:ss')) typed $command")
    [KPCSmokeConsole]::Submit("$command`r")
   }
   Start-Sleep -Milliseconds 500
  }
 }elseif($Mode -eq 'Coop'){
  # Native "start <map>" prepends that map to the maplist file, so the list is
  # R1M2B, R1M2B, R1M2C, R1M3A: the first nextlevel reloads R1M2B.
  $expect=@('R1M2B','R1M2C','R1M3A')
  for($i=0;$i -lt $expect.Count;$i++){
   $epoch=$i+1
   [KPCSmokeConsole]::Submit("nextlevel`r"); $events.Add("typed nextlevel (expect $($expect[$i]))")
   Check "nextlevel $epoch entered hooked NextLevel (epoch $epoch)" (Wait-Log ("e=$epoch seq=\d+ LOADING world-loading native NextLevel") $TimeoutSeconds)
   Check "nextlevel $epoch loaded $($expect[$i]); not started, so phase stays LOBBY" (Wait-Log ("e=$epoch seq=\d+ LOBBY world-loaded "+'worlds\\realm1\\'+$expect[$i]+'$') $TimeoutSeconds)
   Start-Sleep -Seconds 5
  }
 }else{
  Start-Sleep -Seconds 8
 }
 Start-Sleep -Seconds 3
 $screen=[KPCSmokeConsole]::Screen()
 if(-not $worker.HasExited){[KPCSmokeConsole]::Submit("quit`r");$events.Add('typed quit')}
 if(-not $worker.WaitForExit(30000)){
  $events.Add('quit timed out; stopping dedicated server')
  Get-Process -Name @('dedicated','DEDICA~1') -ErrorAction SilentlyContinue | Stop-Process -Force
  [void]$worker.WaitForExit(10000)
 }
 $worker.Refresh(); if($worker.HasExited){$exitCode=$worker.ExitCode}
}finally{
 if($Mode -eq 'PassThrough' -or $MinPlayers -gt 0){[IO.File]::WriteAllBytes($ini,$iniBytes);$events.Add('restored KPCServerTools.ini')}
}

$lines=@(Read-NewLog)
$buildLine=@($lines | Where-Object { $_ -match 'build=coop-session-v[0-9]+ ' }) | Select-Object -First 1
if($Mode -eq 'PassThrough'){
 Check 'no co-op or hook lines in pass-through' (-not @($lines | Where-Object { $_ -match 'Co-op\]|hooks installed' }).Count)
}else{
 Check 'server.dll is the installed proxy' ($hash -eq $serverHash)
 if($Mode -eq 'Coop'){
  Check 'build line names the loaded DLL hash' ([bool]$buildLine -and $buildLine.EndsWith("sha256=$hash"))
  Check 'effective CoopSession=1' ([bool]@($lines | Where-Object { $_ -eq '[KPCFix Co-op] effective CoopSession=1' }).Count)
  if(-not $CommandFile){Check 'no player events on an empty server' (-not @($lines | Where-Object { $_ -match '^\[KPCFix Co-op\] g=\d+ e=\d+ seq=\d+ \S+ (spawn|say|change|end-level|exit|exit-seen|presence|change-failed) ' }).Count)}
  Check 'session world names match native Loaded world output' (@($lines | Where-Object { $_ -match 'world-loaded \(unknown\)' }).Count -eq 0)
  Check 'object module loaded once in this server run' (@($lines | Where-Object { $_ -match 'Runtime object hooks installed' }).Count -eq 1)
 }else{
  Check 'co-op disabled for a normal start' (-not @($lines | Where-Object { $_ -match 'effective CoopSession=1|session armed' }).Count)
 }
}
$failed=@($checks.Values | Where-Object { $_ -ne 'PASS' }).Count
$result=[ordered]@{
 mode=$Mode; map=$map; date=(Get-Date).ToString('o'); os=[Environment]::OSVersion.VersionString
 result=$(if($failed){'FAIL'}else{'PASS'}); checks=$checks; events=$events; worker_exit=$exitCode
 kpcservertools_dll_sha256=$hash; server_dll_sha256=$serverHash
 server_original_dll_sha256=(Get-FileHash -LiteralPath (Join-Path $GameDir 'server.original.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
 object_lto_sha256=(Get-FileHash -LiteralPath (Join-Path $GameDir 'object.lto') -Algorithm SHA256).Hash.ToLowerInvariant()
 dedicated_exe_sha256=(Get-FileHash -LiteralPath (Join-Path $GameDir 'dedicated.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
 log=$lines; console=$screen
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Report -Encoding UTF8
Write-Host "[KPCFix Co-op smoke] $Mode $($result.result); report: $Report"
if($failed){exit 1}
exit 0
