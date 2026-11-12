<#
.SYNOPSIS
    Measures ReqDeck: cold start, idle memory, a workspace of 3,000 requests,
    a 20 MB JSON response with a filter, pretty-printing, and memory over 100
    requests.

.DESCRIPTION
    The application runs as a portable copy of a deployment in a temporary
    folder, so the settings and data of the user stay untouched. The
    workspace, JSON, and request numbers come from reqdeck_bench of the
    Release build, which works without a window. Every number is printed as
    a Markdown table row; nothing is uploaded.

    Cold start is the time from starting the process until its main window
    exists, the best of -Runs starts after one warm-up start. Idle memory is
    the private memory five seconds after that.

.PARAMETER AppDir
    Deployed application folder with ReqDeck.exe, for example the one
    release.ps1 writes to release/ReqDeck-<version>. Defaults to the newest
    such folder.

.PARAMETER BenchExe
    reqdeck_bench.exe of the Release build.

.PARAMETER QtPrefix
    Qt kit prefix whose bin folder reqdeck_bench needs.

.PARAMETER Runs
    Number of measured cold starts.
#>
[CmdletBinding()]
param(
    [string] $AppDir = '',
    [string] $BenchExe = '',
    [string] $QtPrefix = 'C:/Qt/6.11.1/msvc2022_64',
    [int] $Runs = 5
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = Split-Path $PSScriptRoot -Parent # project/
if (-not $AppDir) {
    $candidate = Get-ChildItem (Join-Path $Root 'release') -Directory -Filter 'ReqDeck-*' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $candidate) { throw 'No deployment found; run packaging/release.ps1 first or pass -AppDir.' }
    $AppDir = $candidate.FullName
}
if (-not (Test-Path (Join-Path $AppDir 'ReqDeck.exe'))) { throw "ReqDeck.exe not found in $AppDir" }
if (-not $BenchExe) { $BenchExe = Join-Path $Root 'build/windows-msvc2022-release/Release/reqdeck_bench.exe' }
if (-not (Test-Path $BenchExe)) { throw "reqdeck_bench.exe not found at $BenchExe; build the Release preset first." }

# A portable copy: all its data stays inside the temporary folder.
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("ReqDeck-measure-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$app = Join-Path $work 'app'
Copy-Item $AppDir $app -Recurse
Copy-Item (Join-Path $PSScriptRoot 'portable.ini') (Join-Path $app 'portable.ini') -Force
$exe = Join-Path $app 'ReqDeck.exe'

function Measure-Start([string[]] $Arguments) {
    # Returns the milliseconds until the main window exists and the private memory 5 s later.
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = if ($Arguments) { Start-Process $exe -ArgumentList $Arguments -PassThru } else { Start-Process $exe -PassThru }
    try {
        while ($process.MainWindowHandle -eq 0) {
            if ($watch.ElapsedMilliseconds -gt 30000) { throw 'The main window did not appear within 30 s.' }
            Start-Sleep -Milliseconds 10
            $process.Refresh()
        }
        $startMs = $watch.ElapsedMilliseconds
        Start-Sleep -Seconds 5
        $process.Refresh()
        return [pscustomobject]@{ StartMs = $startMs; PrivateMB = [math]::Round($process.PrivateMemorySize64 / 1MB, 1) }
    } finally {
        if (-not $process.HasExited) {
            $null = $process.CloseMainWindow()
            if (-not $process.WaitForExit(5000)) { $process.Kill() }
        }
    }
}

try {
    $rows = New-Object System.Collections.Generic.List[string]
    $rows.Add('| Measurement | Value |')
    $rows.Add('|---|---|')

    $null = Measure-Start @()
    $starts = 1..$Runs | ForEach-Object { Measure-Start @() }
    $best = $starts | Sort-Object StartMs | Select-Object -First 1
    $rows.Add("| Cold start to main window (best of $Runs) | $($best.StartMs) ms |")
    $rows.Add("| Private memory, idle after start | $($best.PrivateMB) MB |")

    # reqdeck_bench also writes the 3,000-request workspace that the application then opens.
    $workspace = Join-Path $work 'bench.reqdeck'
    $env:PATH = (Join-Path $QtPrefix 'bin') + ';' + $env:PATH
    $bench = & $BenchExe $workspace
    if ($LASTEXITCODE -ne 0) { throw "reqdeck_bench failed with exit code $LASTEXITCODE" }
    $values = @{}
    foreach ($line in $bench) {
        if ($line -match '^BENCH (\S+) (\S+) (\S+)$') { $values[$Matches[1]] = "$($Matches[2]) $($Matches[3])" }
    }
    $rows.Add("| Workspace of 3,000 requests: save / load | $($values['workspace3000.save']) / $($values['workspace3000.load']) |")
    $opened = Measure-Start @("`"$workspace`"")
    $rows.Add("| Start with the 3,000-request workspace | $($opened.StartMs) ms, $($opened.PrivateMB) MB |")
    $rows.Add("| 20 MB JSON response: show / JSONPath filter `$[*].id | $($values['json20mb.show']) / $($values['json20mb.filter']) |")
    $rows.Add("| 0.9 MB JSON response: pretty-print | $($values['json1mb.pretty']) |")
    $rows.Add("| 100 requests to a local server | $($values['requests100.time']), memory growth $($values['requests100.memoryGrowth']) |")

    $cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name.Trim()
    $ramGB = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB)
    Write-Output "Measured $(Get-Date -Format 'yyyy-MM-dd') on $cpu, $ramGB GB RAM, $((Get-CimInstance Win32_OperatingSystem).Caption)."
    Write-Output ''
    $rows | ForEach-Object { Write-Output $_ }
} finally {
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}
