# 按窗口标题片段截图任意窗口：shotwin.ps1 -Find '设置' -Out x.png [-Delay 600]
param(
    [Parameter(Mandatory=$true)][string]$Find,
    [Parameter(Mandatory=$true)][string]$Out,
    [int]$Delay = 600
)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class FW2 {
  public delegate bool Proc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(Proc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x,int y,int cx,int cy,uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  public static IntPtr Find(string needle) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h,l) => {
      if (!IsWindowVisible(h)) return true;
      var sb = new StringBuilder(300); GetWindowTextW(h, sb, 300);
      if (sb.ToString().Contains(needle)) { found = h; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
"@
Start-Sleep -Milliseconds $Delay
$h = [FW2]::Find($Find)
if ($h -eq [IntPtr]::Zero) { Write-Host "找不到含 '$Find' 的窗口" -ForegroundColor Yellow; exit 1 }
[FW2]::SetWindowPos($h, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0001 -bor 0x0040) | Out-Null
[FW2]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 600
$r = New-Object FW2+RECT
[FW2]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L; $hh = $r.B - $r.T
if ($w -le 0 -or $hh -le 0) { Write-Host "窗口尺寸异常"; exit 1 }
$bmp = New-Object System.Drawing.Bitmap($w, $hh)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size($w, $hh)))
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "saved $Out ($w x $hh)"
