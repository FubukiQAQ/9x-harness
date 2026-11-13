# 用 SendInput 在屏幕坐标处双击（跨进程真实鼠标事件，开发验证用）
#   click.ps1 -X 100 -Y 200 [-Double] [-Button left|right]
param(
    [Parameter(Mandatory=$true)][int]$X,
    [Parameter(Mandatory=$true)][int]$Y,
    [switch]$Double,
    [string]$Button = 'left'
)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Mi {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  public const uint DOWN_L = 0x0002, UP_L = 0x0004, DOWN_R = 0x0008, UP_R = 0x0010;
}
"@
[Mi]::SetCursorPos($X, $Y) | Out-Null
Start-Sleep -Milliseconds 120
$down = if ($Button -eq 'right') { [Mi]::DOWN_R } else { [Mi]::DOWN_L }
$up   = if ($Button -eq 'right') { [Mi]::UP_R }   else { [Mi]::UP_L }
[Mi]::mouse_event($down, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 60
[Mi]::mouse_event($up, 0, 0, 0, [IntPtr]::Zero)
if ($Double) {
    Start-Sleep -Milliseconds 90
    [Mi]::mouse_event($down, 0, 0, 0, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [Mi]::mouse_event($up, 0, 0, 0, [IntPtr]::Zero)
}
Write-Host "clicked $X,$Y $(if($Double){'(double)'})"
