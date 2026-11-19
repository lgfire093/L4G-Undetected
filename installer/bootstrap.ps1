<#
.SYNOPSIS
    L4G AIMS - Stage 0 Bootstrap
.DESCRIPTION
    Fetches the main installer and runs it. This is the target of the one-liner:

        irm https://raw.githubusercontent.com/lgfire093/L4G-Undetected/master/installer/bootstrap.ps1 | iex

    The installer URL is resolved against a mirror list because raw.githubusercontent
    is the first source to disappear when a repo moves or a CDN edge caches a 404.

.NOTES
    There is deliberately no AMSI bypass in this file. AMSI scans the entire script
    block before the first statement runs, so a bypass living inside the script it
    is meant to disarm never executes - it only gifts the scanner a known signature
    and the whole file is blocked. Measured: Defender -> Trojan:PowerShell/PsAttack.A.
#>

[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
$ErrorActionPreference = 'SilentlyContinue'

$SOURCES = @(
    "https://raw.githubusercontent.com/lgfire093/L4G-Undetected/master/installer",
    "https://raw.githubusercontent.com/lgfire093/L4G-Undetected/main/installer",
    "https://cdn.jsdelivr.net/gh/lgfire093/L4G-Undetected@master/installer",
    "https://cdn.jsdelivr.net/gh/lgfire093/L4G-Undetected@main/installer"
)

$installer = $null
foreach ($base in $SOURCES) {
    try {
        $response = Invoke-WebRequest -Uri "$base/install.ps1" -UseBasicParsing -TimeoutSec 20
        if ($response.StatusCode -eq 200 -and $response.Content.Length -gt 512) {
            $installer = $response.Content
            break
        }
    } catch {}
}

if (-not $installer) {
    Write-Host "[L4G] Bootstrap failed: install.ps1 unreachable from all sources." -ForegroundColor Red
    foreach ($base in $SOURCES) { Write-Host "      $base/install.ps1" -ForegroundColor DarkGray }
    exit 1
}

Invoke-Expression $installer
