# Runs the installed kingdom_translit.3dsx in Azahar with an autotest script,
# waits, captures the Azahar window (PrintWindow: no focus change) and quits.
#
#   powershell -File tools/azahar_autotest.ps1 -Script "seed 0.5`nfreeze 900`ncpu 0" -Wait 45 -Out shot.png
#   powershell -File tools/azahar_autotest.ps1 -ScriptFile tests/baseline/input.txt -Freeze 900 -Out shot.png
#
# autotest.txt lines (platform/3ds/main.c): "seed S", "freeze F", "cpu 0|1",
# "progress <PlayState.savedProgress text>", and host script lines
# "<frame> key NAME down|up" / "<frame> mouse down|up".
# The script temporarily switches Azahar to the plain top/bottom layout
# (render_3d=0, layout_option=0) and restores qt-config.ini afterwards.
# autotest.txt is deleted at the end so normal play is unaffected.
# CPU mode (cpu 1) runs ~3.7 fps: 600 frames need about 230 s.
param(
	[string]$Script = "",
	[string]$ScriptFile = "",
	[int]$Freeze = 0,
	[int]$Wait = 45,
	[switch]$Stereo, # side-by-side stereoscopic top screen (render_3d=1, 3D slider = factor_3d): both eyes in the shot
	[string]$Out = "azahar_shot.png",
	[string]$Product = "kingdom_translit",   # base name of the .3dsx installed in sdmc/3ds (port.json "product")
	[string]$DataDir = "",                    # sdmc/3ds/<DataDir> holds saves/log/autotest.txt (default = Product)
	[string]$Azahar = ""                      # path of azahar.exe (port.json "azahar_exe"); empty = search PATH and Downloads
)
$ErrorActionPreference = "Stop"
if ($Azahar -eq "") {
	$cmd = Get-Command azahar.exe -ErrorAction SilentlyContinue
	if ($cmd) { $Azahar = $cmd.Source }
	else {
		$found = Get-ChildItem "$env:USERPROFILE\Downloads" -Recurse -Depth 2 -Filter azahar.exe -ErrorAction SilentlyContinue | Select-Object -First 1
		if ($found) { $Azahar = $found.FullName }
	}
}
if (-not (Test-Path $Azahar)) { throw "azahar.exe not found: pass -Azahar <path> or set azahar_exe in port.json" }
if ($DataDir -eq "") { $DataDir = $Product }
$data = "$env:APPDATA\Azahar\sdmc\3ds\$DataDir"
New-Item -ItemType Directory -Force $data | Out-Null   # the game creates it on its first run; the script may come first
$cfg = "$env:APPDATA\Azahar\config\qt-config.ini"
$bak = "$env:TEMP\${Product}_qt-config.ini.bak"

$text = $Script
if ($Freeze -gt 0) { $text = "seed 0.5`nfreeze $Freeze`n" + $text }
if ($ScriptFile -ne "") { $text = $text + "`n" + (Get-Content $ScriptFile -Raw) }
# autotest.txt is written only after the checks below and removed in the finally block: a leftover file would put
# the game into frozen autotest mode on its next normal start.

# Only touch Azahar processes of this tool (their command line names kingdom_translit). Another piece of work may
# be using the same emulator and SD card: then stop instead of killing it or capturing its window.
$mine = @(Get-CimInstance Win32_Process -Filter "Name='azahar.exe'" | Where-Object { $_.CommandLine -like "*$Product.3dsx*" })
$others = @(Get-CimInstance Win32_Process -Filter "Name='azahar.exe'" | Where-Object { $_.CommandLine -notlike "*$Product.3dsx*" })
if ($others.Count -gt 0) { throw "Another Azahar is running (pid $($others[0].ProcessId)): not touching it. Close it or wait, then retry." }
$mine | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep 1
Copy-Item $cfg $bak -Force
try {
	Set-Content "$data\autotest.txt" $text -Encoding ascii
	$mode3d = if ($Stereo) { 'render_3d=1' } else { 'render_3d=0' }
	(Get-Content $bak) -replace '^render_3d=\d$', $mode3d -replace '^layout_option=4$', 'layout_option=0' |
		Set-Content $cfg -Encoding utf8
	$proc = Start-Process $Azahar -ArgumentList "`"$env:APPDATA\Azahar\sdmc\3ds\$Product.3dsx`"" -PassThru
	Start-Sleep $Wait
	Add-Type -AssemblyName System.Drawing
	Add-Type @'
using System; using System.Runtime.InteropServices;
public class AzWin {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  public struct RECT { public int L, T, R, B; } }
'@
	$p = Get-Process -Id $proc.Id
	$r = New-Object AzWin+RECT
	[AzWin]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
	$bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	$hdc = $g.GetHdc(); [AzWin]::PrintWindow($p.MainWindowHandle, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc)
	$bmp.Save($Out)
} finally {
	if ($proc) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
	Start-Sleep 1
	Copy-Item $bak $cfg -Force
	Remove-Item "$data\autotest.txt" -ErrorAction SilentlyContinue
}
Get-Content "$data\runtime.log" | Select-String 'autotest|trace'
"saved $Out"
