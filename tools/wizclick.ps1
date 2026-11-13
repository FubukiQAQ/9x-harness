# 按“客户区坐标”点击正在运行的窗口（默认 SETUP.EXE）。
# 用法: tools\wizclick.ps1 -X 200 -Y 108 [-Pid2 1234] [-Double]
param(
    [int]$X = 0,
    [int]$Y = 0,
    [int]$Pid2 = 0,
    [switch]$Double,
    [switch]$NoFocus
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WC {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
}
"@
if ($Pid2 -ne 0) {
    $p = Get-Process -Id $Pid2 -ErrorAction Stop
} else {
    $p = Get-Process SETUP -ErrorAction Stop | Select-Object -First 1
}
if (-not $NoFocus) { [WC]::SetForegroundWindow($p.MainWindowHandle) | Out-Null; Start-Sleep -Milliseconds 250 }
$pt = New-Object WC+POINT
$pt.X = 0; $pt.Y = 0
[WC]::ClientToScreen($p.MainWindowHandle, [ref]$pt) | Out-Null
$sx = $pt.X + $X
$sy = $pt.Y + $Y
[WC]::SetCursorPos($sx, $sy) | Out-Null
Start-Sleep -Milliseconds 120
[WC]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 40
[WC]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
if ($Double) {
    Start-Sleep -Milliseconds 60
    [WC]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 40
    [WC]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
}
"screenc:{0},{1} pid:{2}" -f $sx, $sy, $p.Id
