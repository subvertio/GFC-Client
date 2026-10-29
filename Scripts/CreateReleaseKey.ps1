# Creates GFC's Android release signing key. Run it once, yourself, in a terminal: it asks for the
# password, which never goes into the project. Every release of the app must be signed with this key,
# so back it up (the file and the password, kept apart) somewhere safe. Lose it and installed apps can't
# be updated; Google Play can't replace it either unless you enrol in Play App Signing.
# Usage:  .\Scripts\CreateReleaseKey.ps1                     (D:\GFCKeys\gfc-release.keystore, alias gfc)
#         .\Scripts\CreateReleaseKey.ps1 -KeyStore E:\keys\gfc.keystore
# Avoid " and \ in the password: Unreal's settings files can't hold them.
param(
    [string]$KeyStore = 'D:\GFCKeys\gfc-release.keystore',
    [string]$Alias = 'gfc',
    [string]$JavaHome = ''
)
$ErrorActionPreference = 'Stop'
if (-not $JavaHome) {
    $JavaHome = Get-ChildItem 'D:\Java' -Directory -Filter 'jdk-21*' -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        Where-Object { Test-Path (Join-Path $_.FullName 'bin\keytool.exe') } | Select-Object -First 1 -ExpandProperty FullName
}
$keytool = Join-Path $JavaHome 'bin\keytool.exe'
if (-not (Test-Path $keytool)) { throw 'keytool not found: pass -JavaHome (a JDK 17 or 21 folder).' }
if (Test-Path $KeyStore) { throw "$KeyStore already exists. Keep using it: a new key can't update apps signed with the old one." }
New-Item -ItemType Directory -Force (Split-Path $KeyStore -Parent) | Out-Null

# PowerShell asks for the password (keytool's own prompt reads nothing in some terminals) and hands it
# to keytool through an environment variable of this process only, never on a command line.
function Read-Secret($prompt) {
    $secure = Read-Host -AsSecureString $prompt
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try { [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr) } finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
}
Write-Host 'Creating the release key. Choose a password: 12+ characters, no " or \.' -ForegroundColor Cyan
$password = Read-Secret 'Release key password'
if ($password.Length -lt 12 -or $password -match '["\\]') { throw 'The password needs 12+ characters and no " or \.' }
if ((Read-Secret 'Type it again') -ne $password) { throw 'The two passwords differ. Nothing was created.' }
$env:GFC_NEW_KEY_PASSWORD = $password
$password = $null
try {
    & $keytool -genkeypair -keystore $KeyStore -storetype PKCS12 -alias $Alias -keyalg RSA -keysize 4096 -validity 10000 `
        -dname 'CN=GFC, O=Galactic Federation Coin' -storepass:env GFC_NEW_KEY_PASSWORD -keypass:env GFC_NEW_KEY_PASSWORD
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $KeyStore)) { throw 'keytool did not create the key.' }
    $listing = & $keytool -list -v -keystore $KeyStore -alias $Alias -storepass:env GFC_NEW_KEY_PASSWORD | Out-String
} finally {
    Remove-Item Env:GFC_NEW_KEY_PASSWORD -ErrorAction SilentlyContinue
}
$fingerprint = [regex]::Match($listing, 'SHA256:\s*([0-9A-F:]{95})').Groups[1].Value
if (-not $fingerprint) { throw 'Could not read the fingerprint (wrong password?). The key was created; run keytool -list -v to see it.' }

Write-Host "`nRelease key created: $KeyStore (alias $Alias)" -ForegroundColor Green
Write-Host "SHA-256 fingerprint: $fingerprint"
Write-Host @"

Next:
1. Back up $KeyStore now, e.g. to F:\GFC\, and keep the password apart from it.
2. Let GFC links open the app (App Links): add the fingerprint on the node,
     ssh -t gfc-node gfc config
   and set   GFC_ANDROID_PACKAGE=tech.galacticfederation.gfc
             GFC_ANDROID_CERT_SHA256=$fingerprint
3. Build the signed release:   .\Scripts\PackageAndroid.ps1 -Release
"@
