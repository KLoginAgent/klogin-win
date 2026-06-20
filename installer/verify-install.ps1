#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Verify KLogin Agent + Credential Provider installation on this machine.
#>
$ErrorActionPreference = 'Continue'

$CpClsid = '{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}'
$CpFilterClsid = '{8f3e2a10-4b5c-4d6e-9f01-23456789abce}'
$SystemDll = "$env:Windir\System32\KLoginCredentialProvider.dll"
$AgentDir = "${env:ProgramFiles}\KLogin\Agent"
$CpLog = "$env:ProgramData\KLogin\credential-provider.log"

function Write-Check([bool]$Ok, [string]$Message) {
    $symbol = if ($Ok) { '[OK]' } else { '[!!]' }
    $color = if ($Ok) { 'Green' } else { 'Yellow' }
    Write-Host "$symbol $Message" -ForegroundColor $color
}

Write-Host "`nKLogin install verification`n" -ForegroundColor Cyan

$installSignals = @()

$service = Get-Service -Name KLoginAgent -ErrorAction SilentlyContinue
if ($service) {
    $installSignals += 'service'
    Write-Check ($service.Status -eq 'Running') "KLoginAgent service: $($service.Status)"
} else {
    Write-Check $false 'KLoginAgent service not found'
}

$dllExists = Test-Path $SystemDll
if ($dllExists) { $installSignals += 'dll' }
Write-Check $dllExists "Credential Provider DLL: $SystemDll"

if ($dllExists) {
    $dll = Get-Item $SystemDll
    Write-Host "    Size: $($dll.Length) bytes, Modified: $($dll.LastWriteTime)"
}

$cpReg = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$CpClsid"
$cpRegOk = Test-Path $cpReg
if ($cpRegOk) { $installSignals += 'cp-reg' }
Write-Check $cpRegOk 'Credential Provider registry key'

$filterReg = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters\$CpFilterClsid"
$filterRegOk = Test-Path $filterReg
if ($filterRegOk) { $installSignals += 'filter-reg' }
Write-Check $filterRegOk 'Credential Provider Filter registry key'

$clsidReg = "HKLM:\SOFTWARE\Classes\CLSID\$CpClsid\InprocServer32"
$clsidOk = Test-Path $clsidReg
Write-Check $clsidOk 'CLSID InprocServer32 registration'
if ($clsidOk) {
    $dllPath = (Get-ItemProperty $clsidReg).'(default)'
    Write-Host "    DLL path: $dllPath"
    Write-Check ($dllPath -eq $SystemDll) 'Registry DLL path matches System32 copy'
}

$filterClsidReg = "HKLM:\SOFTWARE\Classes\CLSID\$CpFilterClsid\InprocServer32"
$filterClsidOk = Test-Path $filterClsidReg
Write-Check $filterClsidOk 'Filter CLSID InprocServer32 registration'

Write-Host "`nRegistered credential providers:" -ForegroundColor Cyan
Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers' |
    ForEach-Object { Write-Host "  $($_.PSChildName) = $((Get-ItemProperty $_.PSPath).'(default)')" }

Write-Host "`nRegistered credential provider filters:" -ForegroundColor Cyan
Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters' -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host "  $($_.PSChildName) = $((Get-ItemProperty $_.PSPath).'(default)')" }

if ($dllExists) {
    Write-Host "`nNative DLL load test:" -ForegroundColor Cyan
    $loadCode = @'
using System;
using System.Runtime.InteropServices;
public static class NativeLoad {
  [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern IntPtr LoadLibraryW(string path);
  [DllImport("kernel32", SetLastError=true)]
  public static extern bool FreeLibrary(IntPtr h);
}
'@
    try {
        Add-Type -TypeDefinition $loadCode -ErrorAction Stop | Out-Null
        $h = [NativeLoad]::LoadLibraryW($SystemDll)
        if ($h -eq [IntPtr]::Zero) {
            $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            Write-Check $false "LoadLibrary failed (Win32 error $err). Install Microsoft VC++ 2015-2022 x64 Redistributable."
        } else {
            Write-Check $true 'LoadLibrary succeeded'
            [NativeLoad]::FreeLibrary($h) | Out-Null
        }
    } catch {
        Write-Check $false "LoadLibrary test failed: $($_.Exception.Message)"
    }
}

$agentDirExists = Test-Path $AgentDir
if ($agentDirExists) { $installSignals += 'agent-dir' }
if ($agentDirExists) {
    Write-Check $true "Agent directory: $AgentDir"
} else {
    Write-Check $false "Agent directory: $AgentDir"
}

$agentReg = Get-ItemProperty -Path 'HKLM:\SOFTWARE\KLoginAgent' -ErrorAction SilentlyContinue
if ($agentReg) {
    $installSignals += 'agent-reg'
    Write-Check $true "Backend URL (registry): $($agentReg.BackendBaseUrl)"
    if ($agentReg.ShowAllCredentialProviders -eq 1) {
        Write-Host "    ShowAllCredentialProviders=1 (Windows password/PIN providers are NOT hidden)" -ForegroundColor Yellow
    }
} else {
    Write-Check $false 'Backend URL not set in HKLM\SOFTWARE\KLoginAgent'
}

$appSettings = Join-Path $AgentDir 'appsettings.json'
if (Test-Path $appSettings) {
    $config = Get-Content $appSettings -Raw | ConvertFrom-Json
    Write-Host "    appsettings BackendBaseUrl: $($config.KLogin.BackendBaseUrl)"
}

Write-Host "`nLocal Windows accounts (these are the user tiles on the lock screen):" -ForegroundColor Cyan
Get-LocalUser | Where-Object Enabled | ForEach-Object { Write-Host "  $($_.Name)" }

Write-Host "`nLock screen notes:" -ForegroundColor Cyan
Write-Host @"
- The user tiles you see (e.g. kiosk, Administrator) are LOCAL WINDOWS ACCOUNTS — not KLogin.
  KLogin maps your KLogin username to one of those accounts after you authenticate.
- KLogin appears as its own sign-in tile, or under 'Sign-in options' (shield icon, bottom-left).
- After install or upgrade, REBOOT — sign-out is usually not enough.
- If password sign-in disappeared or local users vanished from the lock screen, run **repair-login.ps1** as Administrator, then **reboot**:
  ```powershell
  & "C:\Program Files\KLogin\Agent\repair-login.ps1"
  ```
  Or manually:
  ```powershell
  New-ItemProperty HKLM:\SOFTWARE\KLoginAgent -Name ShowAllCredentialProviders -Value 1 -PropertyType DWord -Force
  Remove-Item "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters\{8f3e2a10-4b5c-4d6e-9f01-23456789abce}" -Recurse -Force -ErrorAction SilentlyContinue
  # Reboot
  ```
- After reboot + failed sign-in, check: $CpLog
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

if (Test-Path $CpLog) {
    Write-Host "`nRecent credential provider log:" -ForegroundColor Cyan
    Get-Content $CpLog -Tail 10 | ForEach-Object { Write-Host "  $_" }
}

if ($installSignals.Count -eq 0) {
    Write-Host "`nKLogin is NOT installed (no service, DLL, registry, or agent directory found)." -ForegroundColor Green
    exit 0
}

if (-not $dllExists -or -not $cpRegOk) {
    Write-Host "`nKLogin is partially installed or broken. Re-run the latest MSI or run uninstall.ps1 to clean up." -ForegroundColor Red
    exit 1
}

if (-not $filterRegOk) {
    Write-Host "`nCredential Provider Filter is not registered. Reinstall with the latest MSI." -ForegroundColor Yellow
}

if ($service -and $service.Status -ne 'Running') {
    Write-Host "`nAgent service exists but is not running. Try: sc.exe start KLoginAgent" -ForegroundColor Yellow
    exit 1
}

Write-Host "`nInstall looks complete. Reboot, then look for KLogin under Sign-in options." -ForegroundColor Green
