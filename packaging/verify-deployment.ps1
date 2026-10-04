<#
.SYNOPSIS
    Verifies a canonical deployment directory before it is packaged.

.DESCRIPTION
    Checks that the deployment produced by `cmake --install` is self-contained
    and free of development artifacts, so the same tree can be turned into the
    installer and the portable ZIP with confidence. Every check is reported;
    the script exits non-zero if any required check fails.

.PARAMETER DeploymentDir
    Path to the canonical deployment directory (the folder containing the
    application executable).

.PARAMETER ExeName
    Base name of the main executable, without the .exe extension.
    Defaults to the exeName field of packaging/product.json.

.PARAMETER RequireSignature
    When set, the main executable must carry a valid Authenticode signature.
    Off by default: local builds are unsigned.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $DeploymentDir,
    [string] $ExeName,
    [switch] $RequireSignature
)

$ErrorActionPreference = 'Stop'

if (-not $ExeName) {
    $productJson = Join-Path $PSScriptRoot 'product.json'
    if (Test-Path $productJson) {
        $ExeName = (Get-Content $productJson -Raw | ConvertFrom-Json).exeName
    }
}
if (-not $ExeName) { $ExeName = 'ReqDeck' }

$errors = New-Object System.Collections.Generic.List[string]
$checks = New-Object System.Collections.Generic.List[string]

function Test-Item {
    param([string] $RelPath, [string] $Label)
    $full = Join-Path $DeploymentDir $RelPath
    if (Test-Path $full) {
        $checks.Add("OK   : $Label ($RelPath)")
    } else {
        $errors.Add("FAIL : $Label is missing ($RelPath)")
    }
}

# Qt plugins live under plugins/ when windeployqt writes a qt.conf, or flat at
# the deployment root otherwise. Resolve a plugin path against both.
$pluginRoots = @($DeploymentDir, (Join-Path $DeploymentDir 'plugins')) |
    Where-Object { Test-Path $_ }

function Resolve-Plugin {
    param([string] $RelPath)
    foreach ($root in $pluginRoots) {
        $candidate = Join-Path $root $RelPath
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

function Test-Plugin {
    param([string] $RelPath, [string] $Label)
    if (Resolve-Plugin $RelPath) {
        $checks.Add("OK   : $Label ($RelPath)")
    } else {
        $errors.Add("FAIL : $Label is missing ($RelPath)")
    }
}

if (-not (Test-Path $DeploymentDir)) {
    Write-Error "Deployment directory not found: $DeploymentDir"
    exit 2
}

# 1. Main executable: present, with the version metadata a code-signing service
#    requires (ProductName and ProductVersion), and, when requested, signed. Qt
#    tools and the VC++ redistributable are third-party and signed by their
#    vendors.
$exe = Join-Path $DeploymentDir "$ExeName.exe"
Test-Item "$ExeName.exe" 'Main executable'
if (Test-Path $exe) {
    $info = (Get-Item $exe).VersionInfo
    if ($info.ProductName -and $info.ProductVersion) {
        $checks.Add("OK   : version metadata ($ExeName.exe : $($info.ProductName) $($info.ProductVersion))")
    } else {
        $errors.Add("FAIL : ProductName/ProductVersion metadata missing ($ExeName.exe)")
    }
    if ($RequireSignature) {
        $sig = Get-AuthenticodeSignature -FilePath $exe
        if ($sig.Status -eq 'Valid') {
            $checks.Add("OK   : Authenticode signature ($ExeName.exe : $($sig.SignerCertificate.Subject))")
        } else {
            $errors.Add("FAIL : Authenticode signature $($sig.Status) ($ExeName.exe)")
        }
    }
}

# 2. No debug Qt DLLs (a release deployment must not ship the debug runtime).
$debugDlls = Get-ChildItem -Path $DeploymentDir -Recurse -File -Filter '*d.dll' |
    Where-Object { $_.Name -match '^Qt6.*d\.dll$' }
if ($debugDlls) {
    foreach ($d in $debugDlls) { $errors.Add("FAIL : debug DLL present ($($d.Name))") }
} else {
    $checks.Add('OK   : no debug Qt DLLs')
}

# 3. Required Qt plugins (under plugins/ or flat at the root).
Test-Plugin 'platforms/qwindows.dll' 'Windows platform plugin'

$imageFormatsDir = Resolve-Plugin 'imageformats'
$imageFormats = @()
if ($imageFormatsDir) {
    $imageFormats = Get-ChildItem -Path $imageFormatsDir -File -Filter '*.dll'
}
if ($imageFormats.Count -gt 0) {
    $checks.Add("OK   : image format plugins ($($imageFormats.Count) file(s))")
} else {
    $errors.Add('FAIL : no image format plugins under (plugins/)imageformats/')
}
Test-Plugin 'imageformats/qsvg.dll' 'SVG image format plugin (menu and toolbar icons)'

# HTTPS needs a TLS backend; on Windows windeployqt ships the Schannel backend,
# which uses the operating system's TLS stack.
$tlsDir = Resolve-Plugin 'tls'
$tlsBackends = @()
if ($tlsDir) { $tlsBackends = Get-ChildItem -Path $tlsDir -File -Filter '*.dll' }
if ($tlsBackends.Count -gt 0) {
    $checks.Add("OK   : TLS backend plugin(s) ($(($tlsBackends | ForEach-Object Name) -join ', '))")
} else {
    $errors.Add('FAIL : no TLS backend under (plugins/)tls/')
}

# 4. OpenSSL must not be shipped silently: THIRD_PARTY_NOTICES.md states that it
#    is not bundled, so its runtime DLLs in the deployment mean the notices (and
#    LICENSES/) have to be updated first.
$sslPresent    = Get-ChildItem -Path $DeploymentDir -Recurse -File -Filter 'libssl-3*.dll'    -ErrorAction SilentlyContinue
$cryptoPresent = Get-ChildItem -Path $DeploymentDir -Recurse -File -Filter 'libcrypto-3*.dll' -ErrorAction SilentlyContinue
if ($sslPresent -or $cryptoPresent) {
    $errors.Add('FAIL : OpenSSL runtime DLLs present, but THIRD_PARTY_NOTICES.md declares OpenSSL as not bundled')
} else {
    $checks.Add('OK   : no OpenSSL runtime DLLs (matches THIRD_PARTY_NOTICES.md)')
}

# 5. Qt Quick runtime: the QML modules the UI imports.
foreach ($qmlModule in 'QtQuick/Controls/Material', 'QtQuick/Layouts', 'QtQuick/Dialogs') {
    $found = @($DeploymentDir, (Join-Path $DeploymentDir 'qml')) |
        ForEach-Object { Join-Path $_ $qmlModule } |
        Where-Object { Test-Path (Join-Path $_ 'qmldir') } | Select-Object -First 1
    if ($found) {
        $checks.Add("OK   : QML module ($qmlModule)")
    } else {
        $errors.Add("FAIL : QML module missing ($qmlModule)")
    }
}

# 6. Licensing materials.
Test-Item 'LICENSE'                 'Project license'
Test-Item 'NOTICE'                  'Notice file'
Test-Item 'THIRD_PARTY_NOTICES.md'  'Third-party notices'
$licensesDir = Join-Path $DeploymentDir 'licenses'
if ((Test-Path $licensesDir) -and (Get-ChildItem $licensesDir -File)) {
    $checks.Add('OK   : bundled license texts (licenses/)')
} else {
    $errors.Add('FAIL : licenses/ directory missing or empty')
}

# 7. No development artifacts leaking into the deployment.
$devArtifacts = Get-ChildItem -Path $DeploymentDir -Recurse -File |
    Where-Object { $_.Extension -in '.obj', '.ilk', '.pdb', '.exp', '.lib' -or
                   $_.Name -in 'CMakeCache.txt', 'build.ninja' }
if ($devArtifacts) {
    foreach ($a in $devArtifacts) { $errors.Add("FAIL : development artifact present ($($a.Name))") }
} else {
    $checks.Add('OK   : no development artifacts')
}

Write-Host ''
Write-Host "Deployment verification: $DeploymentDir"
foreach ($c in $checks) { Write-Host "  $c" }
if ($errors.Count -gt 0) {
    Write-Host ''
    foreach ($e in $errors) { Write-Host "  $e" -ForegroundColor Red }
    Write-Host ''
    Write-Error "Deployment verification FAILED with $($errors.Count) error(s)."
    exit 1
}

Write-Host ''
Write-Host 'Deployment verification passed.' -ForegroundColor Green
exit 0
