#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Test COM instantiation of the KLogin credential provider outside the lock screen.
#>
$ErrorActionPreference = 'Stop'

$CpClsid = [Guid]'{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}'
$FilterClsid = [Guid]'{8f3e2a10-4b5c-4d6e-9f01-23456789abce}'
$SystemDll = "$env:Windir\System32\KLoginCredentialProvider.dll"
$CpLog = "$env:ProgramData\KLogin\credential-provider.log"

Write-Host "KLogin credential provider COM test`n" -ForegroundColor Cyan

if (-not (Test-Path $SystemDll)) {
    throw "Missing DLL: $SystemDll"
}

foreach ($pair in @(
        @{ Name = 'Credential Provider'; Clsid = $CpClsid },
        @{ Name = 'Credential Provider Filter'; Clsid = $FilterClsid }
    )) {
    Write-Host "Creating $($pair.Name)..."
    try {
        $obj = [Activator]::CreateInstance($pair.Clsid)
        if ($null -eq $obj) {
            throw 'CreateInstance returned null'
        }
        Write-Host "  OK: $($obj.GetType().FullName)" -ForegroundColor Green
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($obj)
    } catch {
        Write-Host "  FAILED: $($_.Exception.Message)" -ForegroundColor Red
    }
}

if (Test-Path $CpLog) {
    Write-Host "`nCredential provider log:" -ForegroundColor Cyan
    Get-Content $CpLog -Tail 20 | ForEach-Object { Write-Host "  $_" }
} else {
    Write-Host "`nNo log yet at $CpLog" -ForegroundColor Yellow
}

Write-Host "`nIf COM creation succeeds but KLogin is missing on the lock screen, reboot after reinstall." -ForegroundColor Yellow
