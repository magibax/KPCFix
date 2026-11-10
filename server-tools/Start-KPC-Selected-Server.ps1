param([Parameter(Mandatory=$true)][string]$Map)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
if ($Map -notmatch '^worlds\\(deathmatch\\PM(0[1-9]|10)|realm[0-5]\\(R[1-5]M[0-4][ABC]?|TUTORIAL))$') { throw 'Invalid map path.' }
$Exe = @('dedicated.exe','DEDICA~1.EXE') | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $Exe) { throw 'Dedicated EXE missing: put these files in the installed game folder.' }
. (Join-Path $PSScriptRoot 'Check-KPCVersion.ps1')
$GameProfile = Assert-KPCGameVersion
Assert-KPCServerVersion $Exe $GameProfile
foreach ($Required in @('kpc-gui-server.cfg','kpc-gui-maps.txt')) {
    if (-not (Test-Path -LiteralPath $Required)) { throw "Missing $Required; keep the starter configuration beside this launcher." }
}
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
$Server = Start-Process -FilePath (Join-Path $PSScriptRoot $Exe) -ArgumentList @('-nogamespy','-exec','kpc-gui-server.cfg') -NoNewWindow -PassThru
# Queue console input after startup; this does not require the server window to have focus.
Start-Sleep -Seconds 3
$Server.Refresh()
if ($Server.HasExited) { throw "Dedicated server exited before map startup (code $($Server.ExitCode))." }
[KPCConsoleStart]::Submit("start $Map`r")
$Server.WaitForExit()
exit $Server.ExitCode
