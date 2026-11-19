<#
.SYNOPSIS
    L4G AIMS - Build-time Payload Encryption
.DESCRIPTION
    Encrypts the compiled panel executable for the PowerShell installer.
    Everything here runs under PowerShell 5.1 / .NET Framework, so the cipher is
    AES-256-CBC + PKCS7 (CipherMode.GCM does not exist on .NET Framework, which
    is why the previous build produced an undecryptable payload).

    Output format:  Base64( Salt(16) || IV(16) || AES-256-CBC(PKCS7) )
    KDF:            PBKDF2-HMAC-SHA1, 100000 iterations, 32-byte key
.NOTES
    powershell -ExecutionPolicy Bypass -File encrypt_payload.ps1
#>

param(
    [string]$InputPath  = "$PSScriptRoot\..\..\output\L4G_Panel.exe",
    [string]$OutputPath = "$PSScriptRoot\payload.dat",
    [string]$MasterKey  = "L4G-Master-Key-32-Chars-Long!!"
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $InputPath)) {
    Write-Host "[L4G] Input file not found: $InputPath" -ForegroundColor Red
    exit 1
}

$plaintext = [IO.File]::ReadAllBytes($InputPath)
if ($plaintext.Length -lt 2) {
    Write-Host "[L4G] Input file is empty: $InputPath" -ForegroundColor Red
    exit 1
}

$rng = [Security.Cryptography.RandomNumberGenerator]::Create()
$salt = New-Object byte[] 16
$iv   = New-Object byte[] 16
$rng.GetBytes($salt)
$rng.GetBytes($iv)

$kdf    = New-Object Security.Cryptography.Rfc2898DeriveBytes($MasterKey, $salt, 100000)
$aesKey = $kdf.GetBytes(32)

$aes          = [Security.Cryptography.Aes]::Create()
$aes.Key      = $aesKey
$aes.IV       = $iv
$aes.Mode     = [Security.Cryptography.CipherMode]::CBC
$aes.Padding  = [Security.Cryptography.PaddingMode]::PKCS7
$ciphertext   = $aes.CreateEncryptor().TransformFinalBlock($plaintext, 0, $plaintext.Length)

$blob = New-Object byte[] ($salt.Length + $iv.Length + $ciphertext.Length)
[Array]::Copy($salt,       0, $blob, 0,                 $salt.Length)
[Array]::Copy($iv,         0, $blob, $salt.Length,      $iv.Length)
[Array]::Copy($ciphertext, 0, $blob, $salt.Length + $iv.Length, $ciphertext.Length)

[IO.File]::WriteAllText($OutputPath, [Convert]::ToBase64String($blob))

# Round-trip check: decrypt what we just wrote and compare hashes. A payload the
# installer cannot decrypt is worse than no payload at all.
$check = [Convert]::FromBase64String([IO.File]::ReadAllText($OutputPath))
$kdf2  = New-Object Security.Cryptography.Rfc2898DeriveBytes($MasterKey, $check[0..15], 100000)
$aes2  = [Security.Cryptography.Aes]::Create()
$aes2.Key     = $kdf2.GetBytes(32)
$aes2.IV      = $check[16..31]
$aes2.Mode    = [Security.Cryptography.CipherMode]::CBC
$aes2.Padding = [Security.Cryptography.PaddingMode]::PKCS7
$roundtrip = $aes2.CreateDecryptor().TransformFinalBlock($check, 32, $check.Length - 32)

$sha = [Security.Cryptography.SHA256]::Create()
$h1  = [BitConverter]::ToString($sha.ComputeHash($plaintext))
$h2  = [BitConverter]::ToString($sha.ComputeHash($roundtrip))

Write-Host "[L4G] Payload encrypted: $OutputPath" -ForegroundColor Green
Write-Host "      plaintext : $($plaintext.Length) bytes  $h1"
Write-Host "      encrypted : $($blob.Length) bytes (base64 $((Get-Item $OutputPath).Length) bytes)"

if ($h1 -ne $h2) {
    Write-Host "[L4G] ROUND-TRIP FAILED - installer would not decrypt correctly" -ForegroundColor Red
    exit 1
}
Write-Host "      round-trip: OK" -ForegroundColor Green
