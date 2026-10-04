<#
.SYNOPSIS
    Reproducible release pipeline: one Canonical Deployment -> installer,
    portable ZIP and Qt IFW update repository.

.DESCRIPTION
    Stages run in order and fail fast with a clear marker:
        Build -> Deploy -> Verify -> PackageInstaller -> PackagePortable -> GenerateRepository
    Use -Stage to run a single stage (its prerequisites must already exist).
    All product identity, including the version, comes from
    packaging/product.json; there is deliberately no override so that the
    artifact names, the IFW package version and the compiled executable
    can never disagree.

.PARAMETER Stage
    All (default) or one of Build, Deploy, Verify, PackageInstaller,
    PackagePortable, GenerateRepository.

.PARAMETER Theme
    Installer wizard theme: light (default) or dark. Per-user automatic
    switching is not provided by Qt IFW; this selects the built-in look.

.PARAMETER QtIfwRoot
    Qt Installer Framework root (contains bin/binarycreator.exe, bin/repogen.exe).
    Defaults to $env:QTIFW_ROOT or C:\Qt\Tools\QtInstallerFramework\4.10.

.PARAMETER QtPrefix
    Qt kit prefix used for the build/deploy stages.

.NOTES
    Requires MSVC (cl.exe) on PATH for the Build stage; if missing, the script
    imports the Visual Studio 2022 environment automatically.
#>
[CmdletBinding()]
param(
    [ValidateSet('All','Build','Deploy','Verify','PackageInstaller','PackagePortable','GenerateRepository')]
    [string] $Stage = 'All',
    [ValidateSet('light','dark')]
    [string] $Theme = 'light',
    [string] $QtIfwRoot = $(if ($env:QTIFW_ROOT) { $env:QTIFW_ROOT } else { 'C:\Qt\Tools\QtInstallerFramework\4.10' }),
    [string] $QtPrefix = 'C:/Qt/6.11.1/msvc2022_64',
    [string] $Generator = 'Ninja',
    [string] $CMake = 'C:/Qt/Tools/CMake_64/bin/cmake.exe',
    [string] $Ninja = 'C:/Qt/Tools/Ninja/ninja.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ---------------------------------------------------------------------------
# Paths and metadata
# ---------------------------------------------------------------------------
$Root        = Split-Path $PSScriptRoot -Parent          # project/
$ProductJson = Join-Path $PSScriptRoot 'product.json'
if (-not (Test-Path $ProductJson)) { throw "product.json not found at $ProductJson" }
$Meta = Get-Content $ProductJson -Raw | ConvertFrom-Json

# The version is read from product.json only (the same value CMake compiles
# into the executable); it is validated here so a typo fails before building.
$Version = [string] $Meta.version
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "product.json version '$Version' is not MAJOR.MINOR.PATCH" }

$ArtifactBase = $Meta.artifactBase
$ExeName      = $Meta.exeName
$ComponentId  = $Meta.componentId

$BuildDir     = Join-Path $Root 'build/release'
$ReleaseDir   = Join-Path $Root 'release'
$DeployName   = "$ArtifactBase-$Version"
$DeployDir    = Join-Path $ReleaseDir $DeployName
$InstallerSrc = Join-Path $Root 'installer'
$WorkDir      = Join-Path $ReleaseDir '_installer_work'
$SetupExe     = Join-Path $ReleaseDir "$ArtifactBase-$Version-Setup.exe"
$ZipFile      = Join-Path $ReleaseDir "$ArtifactBase-$Version-win64.zip"
$RepoDir      = Join-Path $ReleaseDir 'repository'

# Token map for @Token@ substitution in the IFW templates.
$Tokens = @{
    ProductName         = $Meta.displayName
    Version             = $Version
    Publisher           = $Meta.publisher
    Homepage            = $Meta.homepage
    InstallDirName      = $Meta.installDirName
    MaintenanceToolName = $Meta.maintenanceToolName
    ComponentId         = $ComponentId
    ExeName             = $ExeName
    StableChannelUrl    = $Meta.update.channels.stable
    ReleaseDate         = (Get-Date -Format 'yyyy-MM-dd')
}

function Write-Stage([string] $name) {
    Write-Host ''
    Write-Host "== STAGE ${name}: START ==" -ForegroundColor Cyan
}
function Complete-Stage([string] $name) {
    Write-Host "== STAGE ${name}: OK ==" -ForegroundColor Green
}
function Fail-Stage([string] $name, [string] $message) {
    Write-Host "== STAGE ${name}: FAILED ==" -ForegroundColor Red
    throw $message
}

function Expand-Tokens([string] $text) {
    foreach ($key in $Tokens.Keys) {
        $text = $text.Replace("@$key@", [string]$Tokens[$key])
    }
    return $text
}

function Copy-WithTokens([string] $src, [string] $dst) {
    $content = Get-Content $src -Raw
    $content = Expand-Tokens $content
    $dstDir = Split-Path $dst -Parent
    if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
    Set-Content -Path $dst -Value $content -Encoding UTF8 -NoNewline
}

function Import-VsDevEnv {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
    $vcvars = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $vcvars) { throw 'cl.exe not found and no vcvars64.bat located; run from a VS 2022 developer prompt.' }
    Write-Host "Importing MSVC environment from: $vcvars"
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') { Set-Item -Path "Env:$($matches[1])" -Value $matches[2] }
    }
}

# ---------------------------------------------------------------------------
# Stages
# ---------------------------------------------------------------------------
function Invoke-Build {
    Write-Stage 'Build'
    Import-VsDevEnv
    & $CMake -S $Root -B $BuildDir -G $Generator `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_CXX_COMPILER=cl `
        "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        "-DCMAKE_PREFIX_PATH=$QtPrefix" `
        -DBUILD_TESTING=OFF `
        '-DCMAKE_INSTALL_BINDIR=.'
    if ($LASTEXITCODE -ne 0) { Fail-Stage 'Build' 'CMake configure failed.' }
    & $CMake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { Fail-Stage 'Build' 'CMake build failed.' }
    Complete-Stage 'Build'
}

function Invoke-Deploy {
    Write-Stage 'Deploy'
    if (-not (Test-Path $BuildDir)) { Fail-Stage 'Deploy' "Build directory missing: $BuildDir (run Build first)." }
    if (Test-Path $DeployDir) { Remove-Item -Recurse -Force $DeployDir }
    & $CMake --install $BuildDir --config Release --prefix $DeployDir
    if ($LASTEXITCODE -ne 0) { Fail-Stage 'Deploy' 'cmake --install failed.' }
    Complete-Stage 'Deploy'
}

function Invoke-Verify {
    Write-Stage 'Verify'
    if (-not (Test-Path $DeployDir)) { Fail-Stage 'Verify' "Deployment missing: $DeployDir (run Deploy first)." }
    & (Join-Path $PSScriptRoot 'verify-deployment.ps1') -DeploymentDir $DeployDir -ExeName $ExeName
    if ($LASTEXITCODE -ne 0) { Fail-Stage 'Verify' 'Deployment verification failed.' }
    Complete-Stage 'Verify'
}

function Build-InstallerWorkTree {
    # Assemble a token-substituted IFW config + packages tree whose single
    # component's data/ is the canonical deployment. Shared by installer and
    # repository generation.
    if (-not (Test-Path $DeployDir)) { throw "Deployment missing: $DeployDir (run Deploy first)." }
    if (Test-Path $WorkDir) { Remove-Item -Recurse -Force $WorkDir }

    $workConfig = Join-Path $WorkDir 'config'
    $workPkgMeta = Join-Path $WorkDir "packages/$ComponentId/meta"
    $workPkgData = Join-Path $WorkDir "packages/$ComponentId/data"
    New-Item -ItemType Directory -Path $workConfig, $workPkgMeta, $workPkgData -Force | Out-Null

    # config.xml + branding + chosen theme stylesheet.
    Copy-WithTokens (Join-Path $InstallerSrc 'config/config.xml.in') (Join-Path $workConfig 'config.xml')
    # installer.ico becomes the icon of Setup.exe and of the Maintenance Tool
    # (IFW writes the Maintenance Tool from the installer binary), so it uses a
    # dedicated icon that is distinct from the application icon.
    Copy-Item (Join-Path $Root 'resources/images/app/MaintenanceTool.ico') (Join-Path $workConfig 'installer.ico') -Force
    Copy-Item (Join-Path $InstallerSrc "styles/$Theme.qss")                (Join-Path $workConfig 'style.qss') -Force
    # Generate the compact header logo and flat name banner (keeps the Modern
    # wizard header small so the page list stays fully visible).
    & (Join-Path $PSScriptRoot 'make-installer-images.ps1') `
        -OutDir $workConfig `
        -DisplayName $Meta.displayName `
        -IconSource (Join-Path $Root 'resources/images/app/ReqDeckLogo.png')

    # package meta: package.xml + scripts + UI + license texts.
    Copy-WithTokens (Join-Path $InstallerSrc "packages/$ComponentId/meta/package.xml.in")   (Join-Path $workPkgMeta 'package.xml')
    Copy-WithTokens (Join-Path $InstallerSrc "packages/$ComponentId/meta/installscript.qs") (Join-Path $workPkgMeta 'installscript.qs')
    Copy-Item (Join-Path $InstallerSrc "packages/$ComponentId/meta/desktopcheckbox.ui")     (Join-Path $workPkgMeta 'desktopcheckbox.ui') -Force
    Copy-Item (Join-Path $Root 'LICENSE')                (Join-Path $workPkgMeta 'license-gpl.txt') -Force
    Copy-Item (Join-Path $Root 'THIRD_PARTY_NOTICES.md') (Join-Path $workPkgMeta 'third-party.txt') -Force

    # component data = canonical deployment.
    Copy-Item (Join-Path $DeployDir '*') $workPkgData -Recurse -Force

    return @{ Config = $workConfig; Packages = (Join-Path $WorkDir 'packages') }
}

function Invoke-PackageInstaller {
    Write-Stage 'PackageInstaller'
    $binarycreator = Join-Path $QtIfwRoot 'bin/binarycreator.exe'
    $devtool       = Join-Path $QtIfwRoot 'bin/devtool.exe'
    if (-not (Test-Path $binarycreator)) { Fail-Stage 'PackageInstaller' "binarycreator not found: $binarycreator (set -QtIfwRoot)." }
    if (-not (Test-Path $devtool))       { Fail-Stage 'PackageInstaller' "devtool not found: $devtool (set -QtIfwRoot)." }
    $tree = Build-InstallerWorkTree
    if (Test-Path $SetupExe) { Remove-Item -Force $SetupExe }
    # Hybrid installer (Qt IFW 4.x): the full payload is embedded for an offline
    # first install, and the <RemoteRepositories> from config.xml are kept so the
    # Maintenance Tool can later fetch updates from the published repository.
    & $binarycreator --hybrid -c (Join-Path $tree.Config 'config.xml') -p $tree.Packages $SetupExe
    if ($LASTEXITCODE -ne 0) { Fail-Stage 'PackageInstaller' 'binarycreator failed.' }
    if (-not (Test-Path $SetupExe)) { Fail-Stage 'PackageInstaller' 'Setup.exe was not produced.' }
    Test-HybridInstaller -Devtool $devtool
    Write-Host "Installer: $SetupExe"
    Complete-Stage 'PackageInstaller'
}

function Test-HybridInstaller([string] $Devtool) {
    # binarycreator records the installer kind in metadata/config/config-internal.ini
    # inside the binary (offline-only: offlineOnly=true, hybridInstaller=false;
    # hybrid: offlineOnly=true, hybridInstaller=true). Dump it and require the
    # hybrid flag so an accidental offline-only or online-only build cannot ship.
    $dumpDir = Join-Path $WorkDir '_dump'
    if (Test-Path $dumpDir) { Remove-Item -Recurse -Force $dumpDir }
    & $Devtool dump $SetupExe $dumpDir
    if ($LASTEXITCODE -ne 0) { Fail-Stage 'PackageInstaller' 'devtool dump failed; cannot verify the installer kind.' }
    $ini = Join-Path $dumpDir 'metadata/config/config-internal.ini'
    if (-not (Test-Path $ini)) { Fail-Stage 'PackageInstaller' "config-internal.ini not found in the installer dump ($ini)." }
    $flags = @{}
    foreach ($line in Get-Content $ini) {
        if ($line -match '^\s*([A-Za-z]+)\s*=\s*(.+?)\s*$') { $flags[$matches[1]] = $matches[2] }
    }
    if ($flags['hybridInstaller'] -ne 'true') {
        Fail-Stage 'PackageInstaller' "Installer is not hybrid (hybridInstaller=$($flags['hybridInstaller']), offlineOnly=$($flags['offlineOnly']))."
    }
    Write-Host "Installer kind: hybrid (offline payload + remote repository $($Tokens.StableChannelUrl))"
    Remove-Item -Recurse -Force $dumpDir
}

function Invoke-PackagePortable {
    Write-Stage 'PackagePortable'
    if (-not (Test-Path $DeployDir)) { Fail-Stage 'PackagePortable' "Deployment missing: $DeployDir (run Deploy first)." }
    $portableStage = Join-Path $ReleaseDir "_portable/$DeployName"
    if (Test-Path (Join-Path $ReleaseDir '_portable')) { Remove-Item -Recurse -Force (Join-Path $ReleaseDir '_portable') }
    New-Item -ItemType Directory -Path $portableStage -Force | Out-Null
    Copy-Item (Join-Path $DeployDir '*') $portableStage -Recurse -Force
    # Portable marker (only the ZIP carries it; the installer never does).
    Copy-Item (Join-Path $PSScriptRoot 'portable.ini') (Join-Path $portableStage 'portable.ini') -Force
    if (Test-Path $ZipFile) { Remove-Item -Force $ZipFile }
    Compress-Archive -Path $portableStage -DestinationPath $ZipFile -Force
    Remove-Item -Recurse -Force (Join-Path $ReleaseDir '_portable')
    if (-not (Test-Path $ZipFile)) { Fail-Stage 'PackagePortable' 'Portable ZIP was not produced.' }
    Write-Host "Portable ZIP: $ZipFile"
    Complete-Stage 'PackagePortable'
}

function Invoke-GenerateRepository {
    Write-Stage 'GenerateRepository'
    $repogen = Join-Path $QtIfwRoot 'bin/repogen.exe'
    if (-not (Test-Path $repogen)) { Fail-Stage 'GenerateRepository' "repogen not found: $repogen (set -QtIfwRoot)." }
    $tree = Build-InstallerWorkTree
    if (Test-Path $RepoDir) { Remove-Item -Recurse -Force $RepoDir }
    & $repogen -p $tree.Packages $RepoDir
    if ($LASTEXITCODE -ne 0) { Fail-Stage 'GenerateRepository' 'repogen failed.' }
    if (-not (Test-Path (Join-Path $RepoDir 'Updates.xml'))) { Fail-Stage 'GenerateRepository' 'Updates.xml was not produced.' }
    Write-Host "Repository: $RepoDir"
    Complete-Stage 'GenerateRepository'
}

# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------
New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null
Write-Host "Product : $($Meta.displayName)  v$Version"
Write-Host "Root    : $Root"
Write-Host "Qt IFW  : $QtIfwRoot"
Write-Host "Theme   : $Theme"

switch ($Stage) {
    'All' {
        Invoke-Build
        Invoke-Deploy
        Invoke-Verify
        Invoke-PackageInstaller
        Invoke-PackagePortable
        Invoke-GenerateRepository
    }
    'Build'              { Invoke-Build }
    'Deploy'             { Invoke-Deploy }
    'Verify'             { Invoke-Verify }
    'PackageInstaller'   { Invoke-PackageInstaller }
    'PackagePortable'    { Invoke-PackagePortable }
    'GenerateRepository' { Invoke-GenerateRepository }
}

Write-Host ''
Write-Host 'Release pipeline finished.' -ForegroundColor Green
