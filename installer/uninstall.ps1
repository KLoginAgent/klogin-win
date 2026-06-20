#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Fully remove KLogin Agent, credential provider, and related registry keys.
    Use after MSI uninstall if verify-install still reports components present,
    or instead of MSI when cleaning up a manual install.ps1 deployment.
#>
param(
    [switch]$KeepProgramData
)

$ErrorActionPreference = 'Continue'

$CpClsid = '{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}'
$CpFilterClsid = '{8f3e2a10-4b5c-4d6e-9f01-23456789abce}'
$SystemDll = "$env:Windir\System32\KLoginCredentialProvider.dll"
$AgentDir = "${env:ProgramFiles}\KLogin\Agent"
$InstallDir = "${env:ProgramFiles}\KLogin"
$ProgramDataDir = "$env:ProgramData\KLogin"
$ServiceName = 'KLoginAgent'

function Remove-RegTree([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $Path) {
            Write-Warning "Could not remove registry key: $Path"
            return $false
        }
        Write-Host "  Removed registry: $Path"
        return $true
    }
    return $true
}

function Remove-FileWithRetry([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return $true
    }
    try {
        Remove-Item -LiteralPath $Path -Force -ErrorAction Stop
        Write-Host "  Removed file: $Path"
        return $true
    } catch {
        Write-Warning "Could not remove $Path ($($_.Exception.Message)). Reboot, then run this script again."
        return $false
    }
}

function Remove-DirWithRetry([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return $true
    }
    try {
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
        Write-Host "  Removed directory: $Path"
        return $true
    } catch {
        Write-Warning "Could not remove $Path ($($_.Exception.Message)). Stop KLoginAgent, reboot, then retry."
        return $false
    }
}

Write-Host "`nKLogin uninstall`n" -ForegroundColor Cyan

Write-Host "Stopping and removing Windows service..."
$service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($service) {
    if ($service.Status -eq 'Running') {
        Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
    }
    sc.exe stop $ServiceName 2>$null | Out-Null
    Start-Sleep -Seconds 1
}
sc.exe delete $ServiceName 2>$null | Out-Null
if (Get-Service -Name $ServiceName -ErrorAction SilentlyContinue) {
    Write-Warning "Service $ServiceName still present. Reboot and run uninstall again."
} else {
    Write-Host "  Service removed: $ServiceName"
}

Write-Host "Removing credential provider registration..."
$regPaths = @(
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$CpClsid",
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters\$CpFilterClsid",
    "HKLM:\SOFTWARE\Classes\CLSID\$CpClsid",
    "HKLM:\SOFTWARE\Classes\CLSID\$CpFilterClsid",
    "HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID\$CpClsid",
    "HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID\$CpFilterClsid",
    'HKLM:\SOFTWARE\KLoginAgent'
)
foreach ($path in $regPaths) {
    Remove-RegTree $path | Out-Null
}

Write-Host "Removing credential provider DLL..."
Remove-FileWithRetry $SystemDll | Out-Null

Write-Host "Removing agent files..."
Remove-DirWithRetry $AgentDir | Out-Null
if ((Test-Path -LiteralPath $InstallDir) -and -not (Get-ChildItem -LiteralPath $InstallDir -ErrorAction SilentlyContinue)) {
    Remove-DirWithRetry $InstallDir | Out-Null
} elseif (Test-Path -LiteralPath $InstallDir) {
    Remove-DirWithRetry $InstallDir | Out-Null
}

if (-not $KeepProgramData) {
    Write-Host "Removing program data..."
    Remove-DirWithRetry $ProgramDataDir | Out-Null
}

Write-Host "`nUninstall summary:" -ForegroundColor Cyan
$remaining = @()
if (Get-Service -Name $ServiceName -ErrorAction SilentlyContinue) { $remaining += "Service: $ServiceName" }
if (Test-Path -LiteralPath $SystemDll) { $remaining += "DLL: $SystemDll" }
if (Test-Path -LiteralPath "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$CpClsid") {
    $remaining += "Credential Provider registry"
}
if (Test-Path -LiteralPath "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters\$CpFilterClsid") {
    $remaining += "Credential Provider Filter registry"
}
if (Test-Path -LiteralPath 'HKLM:\SOFTWARE\KLoginAgent') { $remaining += 'HKLM\SOFTWARE\KLoginAgent' }
if (Test-Path -LiteralPath $AgentDir) { $remaining += "Agent directory: $AgentDir" }

if ($remaining.Count -eq 0) {
    Write-Host "KLogin fully removed. Reboot to unload any in-memory credential provider." -ForegroundColor Green
} else {
    Write-Host "Some components remain (often because files are in use):" -ForegroundColor Yellow
    $remaining | ForEach-Object { Write-Host "  - $_" }
    Write-Host "`nReboot, then run uninstall.ps1 again or verify with verify-install.ps1." -ForegroundColor Yellow
}
