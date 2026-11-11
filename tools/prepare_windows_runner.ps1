$ErrorActionPreference = 'Stop'
$env:PATHEXT = [Environment]::GetEnvironmentVariable('PATHEXT', 'Machine')
# Read the current user's installed tool paths, including changes since runner startup.
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $env:Path
if (-not $env:DEVKITPRO) { $env:DEVKITPRO = 'C:\devkitPro' }
$paths = @((Join-Path $env:DEVKITPRO 'devkitARM\bin'), (Join-Path $env:DEVKITPRO 'tools\bin'))
$env:Path = ($paths -join ';') + ';' + $env:Path
foreach ($tool in @('python', 'ffmpeg', 'git', 'gh')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Missing tool on PATH: $tool" }
}
foreach ($relative in @('devkitARM\bin\arm-none-eabi-gcc.exe', 'tools\bin\smdhtool.exe', 'tools\bin\3dsxtool.exe', 'libctru\lib\libctru.a', 'portlibs\3ds\lib\libcitro2d.a', 'portlibs\3ds\lib\libcitro3d.a')) {
    if (-not (Test-Path -LiteralPath (Join-Path $env:DEVKITPRO $relative))) { throw "Missing devkitPro component: $relative" }
}
python -c "import sys,PIL; assert sys.version_info >= (3,9); print('Python', sys.version.split()[0], 'Pillow', PIL.__version__)"
if ($LASTEXITCODE -ne 0) { throw 'Python 3.9+ and Pillow are required' }
if ($env:GITHUB_ENV) { "DEVKITPRO=$env:DEVKITPRO" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append }
if ($env:GITHUB_PATH) { $env:Path -split ';' | Where-Object { $_ } | Select-Object -Unique | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append }
Write-Output "Windows build tools ready; DEVKITPRO=$env:DEVKITPRO"
