param([string]$RunnerRoot = 'C:\workspace\kingdom-3ds-runner')
$ErrorActionPreference = 'Stop'
$env:PATHEXT = [Environment]::GetEnvironmentVariable('PATHEXT', 'Machine')
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
$repo = 'Reqrefusion/kingdom-3ds'
if (-not (Test-Path -LiteralPath (Join-Path $RunnerRoot 'config.cmd'))) { throw 'Extract the official Windows x64 Actions runner into RunnerRoot first.' }
gh auth status --hostname github.com
if ($LASTEXITCODE -ne 0) { throw 'Run gh auth login --hostname github.com --git-protocol https --web --scopes repo,workflow first.' }
$token = $null
if (-not (Test-Path -LiteralPath (Join-Path $RunnerRoot '.runner'))) {
    $token = gh api --hostname github.com --method POST "repos/$repo/actions/runners/registration-token" --jq .token
    if ($LASTEXITCODE -ne 0 -or -not $token) { throw 'Cannot obtain runner registration token; repository admin access is required.' }
}
Push-Location -LiteralPath $RunnerRoot
try {
    if (-not (Test-Path -LiteralPath '.runner')) {
        try {
            & .\config.cmd --unattended --url "https://github.com/$repo" --token $token --name "${env:COMPUTERNAME}-kingdom-3ds" --labels kingdom-3ds-local --work _work
            if ($LASTEXITCODE -ne 0) { throw 'Runner registration failed.' }
        } finally { $token = $null }
    } else {
        $config = Get-Content -LiteralPath '.runner' -Raw | ConvertFrom-Json
        if ($config.gitHubUrl.TrimEnd('/') -ine "https://github.com/$repo") { throw 'Existing runner belongs to another repository.' }
    }
} finally { Pop-Location }
# Start under the current Windows user so Python, FFmpeg and SDK paths are available.
$launcher = Join-Path $RunnerRoot 'start-runner.ps1'
@'
$ErrorActionPreference = 'Stop'
$env:PATHEXT = [Environment]::GetEnvironmentVariable('PATHEXT', 'Machine')
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
$mutex = New-Object System.Threading.Mutex($false, 'kingdom-3ds-local-runner')
if (-not $mutex.WaitOne(0)) { $mutex.Dispose(); exit 0 }
try {
    Set-Location -LiteralPath $PSScriptRoot
    & .\run.cmd *> (Join-Path $PSScriptRoot 'listener.log')
} finally { $mutex.ReleaseMutex(); $mutex.Dispose() }
'@ | Set-Content -LiteralPath $launcher -Encoding UTF8
$powerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$arguments = '-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "' + $launcher + '"'
New-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name 'Kingdom3DSRunner' -Value ('"' + $powerShell + '" ' + $arguments) -PropertyType String -Force | Out-Null
Start-Process -FilePath $powerShell -ArgumentList $arguments -WindowStyle Hidden
Write-Output 'Runner registered and started. It will start again when this Windows user signs in.'
