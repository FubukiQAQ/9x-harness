# 截屏工具（开发验证用）：shot.ps1 <输出png> [-Title <窗口标题片段>] [-Pid2 <pid>] [-Delay 毫秒]
param(
    [Parameter(Mandatory=$true)][string]$Out,
    [string]$Title = '',
    [int]$Delay = 1200,
    [int]$Pid2 = 0
)

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class SmWin {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

Start-Sleep -Milliseconds $Delay

$proc = $null
if ($Pid2 -ne 0) {
    $proc = Get-Process -Id $Pid2 -ErrorAction SilentlyContinue
}
if (-not $proc -and $Title -ne '') {
    $proc = Get-Process | Where-Object { $_.MainWindowTitle -like "*$Title*" } | Select-Object -First 1
}
if ($proc) {
    $hw = $proc.MainWindowHandle
    if ($hw -ne 0) {
        [SmWin]::ShowWindow($hw, 9) | Out-Null
        [SmWin]::SetWindowPos($hw, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
        [SmWin]::BringWindowToTop($hw) | Out-Null
        [SmWin]::SetForegroundWindow($hw) | Out-Null
        Start-Sleep -Milliseconds 500
    }
} else {
    Write-Host "找不到窗口 ($Title / $Pid2)" -ForegroundColor Yellow
}

if ($proc -and $proc.MainWindowHandle -ne 0) {
    $r = New-Object SmWin+RECT
    [SmWin]::GetWindowRect($proc.MainWindowHandle, [ref]$r) | Out-Null
    $x = $r.Left; $y = $r.Top
    $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) { $x = 0; $y = 0; $w = 1024; $h = 768 }
} else {
    $x = 0; $y = 0
    $w = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Width
    $h = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Height
}

$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "saved $Out ($w x $h)"
