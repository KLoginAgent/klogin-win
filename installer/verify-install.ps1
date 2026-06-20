#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Verify KLogin Agent + Credential Provider installation on this machine.
#>
$ErrorActionPreference = 'Continue'

$CpClsid = '{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}'
$SystemDll = "$env:Windir\System32\KLoginCredentialProvider.dll"
$AgentDir = "${env:ProgramFiles}\KLogin\Agent"

function Write-Check([bool]$Ok, [string]$Message) {
    $symbol = if ($Ok) { '[OK]' } else { '[!!]' }
    $color = if ($Ok) { 'Green' } else { 'Yellow' }
    Write-Host "$symbol $Message" -ForegroundColor $color
}

Write-Host "`nKLogin install verification`n" -ForegroundColor Cyan

$service = Get-Service -Name KLoginAgent -ErrorAction SilentlyContinue
if ($service) {
    Write-Check ($service.Status -eq 'Running') "KLoginAgent service: $($service.Status)"
} else {
    Write-Check $false 'KLoginAgent service not found'
}

$dllExists = Test-Path $SystemDll
Write-Check $dllExists "Credential Provider DLL: $SystemDll"

if ($dllExists) {
    $dll = Get-Item $SystemDll
    Write-Host "    Size: $($dll.Length) bytes, Modified: $($dll.LastWriteTime)"
}

$cpReg = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$CpClsid"
$cpRegOk = Test-Path $cpReg
Write-Check $cpRegOk "Credential Provider registry key"

$clsidReg = "HKLM:\SOFTWARE\Classes\CLSID\$CpClsid\InprocServer32"
$clsidOk = Test-Path $clsidReg
Write-Check $clsidOk 'CLSID InprocServer32 registration'
if ($clsidOk) {
    $dllPath = (Get-ItemProperty $clsidReg).'(default)'
    Write-Host "    DLL path: $dllPath"
    Write-Check ($dllPath -eq $SystemDll) 'Registry DLL path matches System32 copy'
}

$backendUrl = (Get-ItemProperty -Path 'HKLM:\SOFTWARE\KLoginAgent' -ErrorAction SilentlyContinue).BackendBaseUrl
if ($backendUrl) {
    Write-Check $true "Backend URL (registry): $backendUrl"
} else {
    Write-Check $false 'Backend URL not set in HKLM\SOFTWARE\KLoginAgent'
}

$appSettings = Join-Path $AgentDir 'appsettings.json'
if (Test-Path $appSettings) {
    $config = Get-Content $appSettings -Raw | ConvertFrom-Json
    Write-Host "    appsettings BackendBaseUrl: $($config.KLogin.BackendBaseUrl)"
}

Write-Host "`nLock screen notes:" -ForegroundColor Cyan
Write-Host @"
- KLogin does NOT remove the default Windows password/PIN screen.
- After install, REBOOT (sign-out alone is often not enough).
- On the lock screen, click 'Sign-in options' and look for 'KLogin' / 'Sign in with KLogin'.
- If KLogin never appears in sign-in options, check Event Viewer:
  Windows Logs > Application, filter for 'Credential' or source 'Microsoft-Windows-Winlogon'.
"@

Write-Host "`nQuick pipe test (agent must be running):" -ForegroundColor Cyan
Write-Host @'
  $client = New-Object System.IO.Pipes.NamedPipeClientStream(".", "klogin-agent", [System.IO.Pipes.PipeDirection]::InOut)
  $client.Connect(3000)
  $writer = New-Object System.IO.StreamWriter($client); $writer.AutoFlush = $true
  $reader = New-Object System.IO.StreamReader($client)
  $writer.WriteLine('{"action":"health"}')
  $reader.ReadLine()
  $client.Close()
'@

if (-not $dllExists -or -not $cpRegOk) {
    Write-Host "`nCredential Provider is missing or not registered. Re-run the MSI or install.ps1." -ForegroundColor Red
    exit 1
}

if ($service -and $service.Status -ne 'Running') {
    Write-Host "`nAgent service exists but is not running. Try: sc.exe start KLoginAgent" -ForegroundColor Yellow
    exit 1
}

Write-Host "`nInstall looks complete. Reboot, then use Sign-in options on the lock screen." -ForegroundColor Green
