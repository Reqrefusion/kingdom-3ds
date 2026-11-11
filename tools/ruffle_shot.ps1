# Runs the original Kingdom SWF in Ruffle, sends scripted keys/clicks and saves window screenshots.
#
#   powershell -File tools/ruffle_shot.ps1 -OutDir ruffle_out -Plan "wait 6;click;wait 3;shot a;key RIGHT 8;shot b"
#
# Plan steps (separated by ';'):  wait S | click | key NAME SECONDS (hold) | tap NAME | shot NAME
# Keys: LEFT RIGHT DOWN UP. The window is brought to the foreground (SendInput needs focus);
# do not touch the mouse/keyboard while it runs. The SWF is the original Newgrounds build
# from the Flashpoint data pack (work/original_swf, SHA-256 87795a69...).
param(
	[string]$Plan = "wait 6;shot a",
	[string]$OutDir = "ruffle_out",
	[int]$Width = 864,
	[int]$Height = 480,
	[string]$Swf = "",
	[string]$Ruffle = "C:\Flashpoint\Data\Ruffle\standalone\latest\ruffle.exe"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($Swf -eq "") { $Swf = (Resolve-Path "$root\..\work\original_swf\pack\content\uploads.ungrounded.net\626000\626899_main.swf").Path }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public class RfWin {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr e);
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
'@
Get-Process ruffle -ErrorAction SilentlyContinue | Stop-Process -Force
$p = Start-Process $Ruffle -ArgumentList "--width $Width --height $Height `"$Swf`"" -PassThru
Start-Sleep 4
$p.Refresh()
$h = $p.MainWindowHandle
$vk = @{ LEFT = 0x25; UP = 0x26; RIGHT = 0x27; DOWN = 0x28 }
function Client-Rect {
	$r = New-Object RfWin+RECT; [RfWin]::GetClientRect($h, [ref]$r) | Out-Null
	$pt = New-Object RfWin+POINT; [RfWin]::ClientToScreen($h, [ref]$pt) | Out-Null
	return @{ X = $pt.X; Y = $pt.Y; W = $r.R; H = $r.B }
}
try {
	foreach ($step in $Plan.Split(";")) {
		$a = $step.Trim().Split(" ")
		switch ($a[0]) {
			"wait" { Start-Sleep -Milliseconds ([int]([double]$a[1] * 1000)) }
			"click" {
				[RfWin]::SetForegroundWindow($h) | Out-Null
				$c = Client-Rect
				[RfWin]::SetCursorPos($c.X + [int]($c.W / 2), $c.Y + [int]($c.H / 2)) | Out-Null
				[RfWin]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
				[RfWin]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
			}
			"key" {
				[RfWin]::SetForegroundWindow($h) | Out-Null
				[RfWin]::keybd_event([byte]$vk[$a[1]], 0, 0, [UIntPtr]::Zero)
				Start-Sleep -Milliseconds ([int]([double]$a[2] * 1000))
				[RfWin]::keybd_event([byte]$vk[$a[1]], 0, 2, [UIntPtr]::Zero)
			}
			"tap" {
				[RfWin]::SetForegroundWindow($h) | Out-Null
				[RfWin]::keybd_event([byte]$vk[$a[1]], 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
				[RfWin]::keybd_event([byte]$vk[$a[1]], 0, 2, [UIntPtr]::Zero)
			}
			"shot" {
				[RfWin]::SetForegroundWindow($h) | Out-Null
				Start-Sleep -Milliseconds 300
				$c = Client-Rect
				$bmp = New-Object System.Drawing.Bitmap $c.W, $c.H
				$g = [System.Drawing.Graphics]::FromImage($bmp)
				$g.CopyFromScreen($c.X, $c.Y, 0, 0, $bmp.Size)
				$bmp.Save("$OutDir\$($a[1]).png")
				"saved $OutDir\$($a[1]).png ($($c.W)x$($c.H))"
			}
		}
	}
} finally {
	Get-Process ruffle -ErrorAction SilentlyContinue | Stop-Process -Force
}
