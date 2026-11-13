param(
 [Parameter(Mandatory=$true)][string]$Map,
 [ValidateSet('Enabled','Disabled')][string]$Cheats='Disabled',
 # EXPERIMENTAL co-op (feature branch DLL only). With -Coop, -Map random picks the
 # lobby from the shipped deathmatch maps (PM01-PM10); an explicit world is used as is.
 [switch]$Coop,
 # Co-op only. Campaign (default): the 48-world campaign the host picks with !campaign
 # (writes CoopCampaign=1 and CoopIntro=1). Route: the fixed test route
 # R1M2B > R1M2C > R1M3A (CoopCampaign=0; used by the smoke test).
 [ValidateSet('Campaign','Route')][string]$CoopMode='Campaign',
 # Keep: leave TraceScripts/TraceTiming as they are. On/Off: write both (test logging).
 [ValidateSet('Keep','On','Off')][string]$TestLogging='Keep'
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
if ($Coop -and $Map -eq 'random') {
 # Co-op lobby: a random shipped deathmatch map; players use its own multiplayer starts.
 $Map = 'worlds\deathmatch\PM{0:D2}' -f (Get-Random -Minimum 1 -Maximum 11)
 Write-Host "[KPCFix Co-op] Lobby map (random deathmatch): $Map"
}
if ($Map -notmatch '^worlds\\(deathmatch\\PM(0[1-9]|10)|realm[0-5]\\(R[1-5]M[0-4][ABC]?|TUTORIAL))$') { throw 'Invalid map path.' }
$Exe = @('dedicated.exe','DEDICA~1.EXE') | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $Exe) { throw 'Dedicated EXE missing: put these files in the installed game folder.' }
. (Join-Path $PSScriptRoot 'Check-KPCServerVersion.ps1')
$GameProfile = Assert-KPCGameVersion
Assert-KPCServerVersion $Exe $GameProfile
foreach ($Required in @('kpc-gui-server.cfg','kpc-gui-maps.txt','KPCServerTools.ini')) {
 if (-not (Test-Path -LiteralPath $Required)) { throw "Missing $Required; run Setup-KPCServerTools.cmd first." }
}
# Initial policy is configuration only; game binaries are never rewritten at launch.
$iniPath=Join-Path $PSScriptRoot 'KPCServerTools.ini'
# Keep existing INI encoding and newline layout. Older installs can lack
# AllowCheats; insert the missing key under [ServerTools] rather than failing.
$reader=[IO.StreamReader]::new($iniPath,([Text.UTF8Encoding]::new($false)),$true)
try {
 $ini=$reader.ReadToEnd()
 $iniEncoding=$reader.CurrentEncoding
}finally {$reader.Dispose()}
# The DLL reads this file with GetPrivateProfileIntW, which does not skip a
# UTF-8 BOM: a BOM hides a first-line [ServerTools] section. Write UTF-8 without one.
if($iniEncoding -is [Text.UTF8Encoding]){$iniEncoding=[Text.UTF8Encoding]::new($false)}
$nl=if($ini.Contains("`r`n")){"`r`n"}elseif($ini.Contains("`r")){"`r"}else{"`n"}
# Set-IniKey <text> <section> <key> <value> [-IfPresent] returns the INI text with the key set.
# An existing key line is rewritten in place (commented ';Key=' lines are left alone); a
# missing key goes directly under its section header, and a missing section is appended
# (except [ServerTools], which must exist). -IfPresent only rewrites an existing key.
function Set-IniKey([string]$Text,[string]$Section,[string]$Key,[string]$Value,[switch]$IfPresent) {
 $line=$Key+'='+$Value
 $pattern='(?im)^[ \t]*'+[regex]::Escape($Key)+'[ \t]*=[^\r\n]*'
 if($Text -match $pattern){return [regex]::Replace($Text,$pattern,{param($m) $line})}
 if($IfPresent){return $Text}
 $header=[regex]::Match($Text,'(?im)^[ \t]*\['+[regex]::Escape($Section)+'\][ \t]*(?:\r\n|\n|\r|$)')
 if($header.Success){
  $separator=if($header.Value.EndsWith("`n") -or $header.Value.EndsWith("`r")){''}else{$nl}
  return $Text.Insert($header.Index+$header.Length,$separator+$line+$nl)
 }
 if($Section -eq 'ServerTools'){throw 'Missing [ServerTools] section in KPCServerTools.ini.'}
 $tail=if($Text.Length -gt 0 -and -not ($Text.EndsWith("`n") -or $Text.EndsWith("`r"))){$nl}else{''}
 return $Text+$tail+'['+$Section+']'+$nl+$line+$nl
}
$ini=Set-IniKey $ini 'ServerTools' 'AllowCheats' $(if($Cheats -eq 'Enabled'){'1'}else{'0'})
if($Coop){
 # A co-op start writes every switch its mode needs, so a fresh default INI works.
 # Campaign: !campaign needs CoopCampaign=1, and the campaign needs CoopIntro=1 (each
 # world's intro script also loads its other scripts, e.g. the R1M1A lever's).
 # (Missing keys go directly under [Coop], so they are set last to first.)
 # The lobby world (after completion or !abort) is the map this run starts on.
 $ini=Set-IniKey $ini 'Coop' 'CoopLobbyWorld' $Map
 if($CoopMode -eq 'Campaign'){$ini=Set-IniKey $ini 'Coop' 'CoopIntro' '1'}
 $ini=Set-IniKey $ini 'Coop' 'CoopCampaign' $(if($CoopMode -eq 'Campaign'){'1'}else{'0'})
 $ini=Set-IniKey $ini 'Coop' 'CoopSession' '1'
}else{
 # A normal launch never inherits a previous co-op session; only an existing key changes.
 $ini=Set-IniKey $ini 'Coop' 'CoopSession' '0' -IfPresent
}
if($TestLogging -ne 'Keep'){
 $trace=if($TestLogging -eq 'On'){'1'}else{'0'}
 foreach($key in 'TraceScripts','TraceTiming'){$ini=Set-IniKey $ini 'Coop' $key $trace}
}
[IO.File]::WriteAllText($iniPath,$ini,$iniEncoding)
# Write to this console's input buffer, not the active desktop window.
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class KPCConsoleStart {
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
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 static extern IntPtr CreateFile(string name,uint access,uint share,IntPtr security,uint disposition,uint flags,IntPtr template);
 [DllImport("kernel32.dll", EntryPoint="WriteConsoleInputW", SetLastError=true)]
 static extern bool WriteConsoleInput(IntPtr h,[In] Record[] records,uint length,out uint written);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 public static void Submit(string text) {
  IntPtr h=CreateFile("CONIN$",0xC0000000,3,IntPtr.Zero,3,0,IntPtr.Zero);
  if(h==new IntPtr(-1)) throw new Win32Exception(Marshal.GetLastWin32Error());
  try {
   Record[] r=new Record[text.Length*2];
   for(int i=0;i<text.Length;i++) {
    r[i*2]=new Record { Type=1, Down=1, Repeat=1, Character=text[i], VirtualKey=(ushort)(text[i]=='\r'?13:0) };
    r[i*2+1]=r[i*2];r[i*2+1].Down=0;
   }
   uint n;
   if(!WriteConsoleInput(h,r,(uint)r.Length,out n)||n!=(uint)r.Length)
    throw new Win32Exception(Marshal.GetLastWin32Error());
  } finally { CloseHandle(h); }
 }
}
'@
Write-Host "[KPCFix Server Tools] Starting $Map; GameSpy reporting disabled."
if ($Coop -and $CoopMode -eq 'Campaign') {
 Write-Host '[KPCFix Co-op] EXPERIMENTAL co-op campaign. In chat the host types !campaign full, or !campaign water / earth / air / fire / nightmare; !abort returns everyone to the lobby.' -ForegroundColor Yellow
 Write-Host '[KPCFix Co-op] Server console: kpc_cheats 0/1, changelevel <world>.' -ForegroundColor Yellow
} elseif ($Coop) {
 Write-Host '[KPCFix Co-op] EXPERIMENTAL co-op test route R1M2B > R1M2C > R1M3A: everyone types !ready, then the host !start.' -ForegroundColor Yellow
}
$ServerCfg = 'kpc-gui-server.cfg'
if ($Coop) {
 # Co-op: no start armour, so the realm armour pickups (R1M1A gauntlets, belt,
 # ...) exist. In multiplayer an armour item hides itself when the server
 # already hands that piece out at spawn (object.lto 0xc8520). The GUI's own
 # config is left as it is; a co-op copy with the armour cvars appended (later
 # lines win) is executed instead.
 # The co-op map list is written here too: the GUI's kpc-gui-maps.txt holds
 # whatever was last picked for deathmatch, while the MVP route needs R1M2B,
 # R1M2C and R1M3A in the native maplist (campaign worlds missing from it are
 # written into the next slot in memory by the server DLL).
 $coopMaps=@($Map)+@(@('worlds\realm1\R1M2B','worlds\realm1\R1M2C','worlds\realm1\R1M3A') | Where-Object { $_ -ne $Map })
 Set-Content -LiteralPath 'kpc-coop-maps.txt' -Value $coopMaps -Encoding ASCII
 $coopCfg = @(Get-Content -LiteralPath $ServerCfg)
 $coopCfg += '// KPCFix co-op: lobby/MVP route map list'
 $coopCfg += '"maplist" "kpc-coop-maps.txt"'
 $coopCfg += '// KPCFix co-op: no start armour (realm armour pickups stay in the world)'
 foreach ($piece in 'startgauntlets','startbelt','startboots','startshoulderplates','startbreastplate','startmask') { $coopCfg += ('"{0}" "0"' -f $piece) }
 Set-Content -LiteralPath 'kpc-coop-server.cfg' -Value $coopCfg -Encoding ASCII
 $ServerCfg = 'kpc-coop-server.cfg'
 Write-Host '[KPCFix Co-op] Co-op map list (kpc-coop-maps.txt) and no start armour (kpc-coop-server.cfg).'
}
$Server = Start-Process -FilePath (Join-Path $PSScriptRoot $Exe) -ArgumentList @('-nogamespy','-exec',$ServerCfg) -NoNewWindow -PassThru
# Queue console input after startup; this does not require the server window to have focus.
Start-Sleep -Seconds 3
$Server.Refresh()
if ($Server.HasExited) { throw "Dedicated server exited before map startup (code $($Server.ExitCode))." }
[KPCConsoleStart]::Submit("start $Map`r")
$Server.WaitForExit()
exit $Server.ExitCode
