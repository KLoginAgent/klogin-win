#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Test COM registration of the KLogin credential provider outside the lock screen.
#>
$ErrorActionPreference = 'Continue'

$CpClsid = [Guid]'{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}'
$FilterClsid = [Guid]'{8f3e2a10-4b5c-4d6e-9f01-23456789abce}'
$SystemDll = Join-Path $env:Windir 'System32\KLoginCredentialProvider.dll'
$CpLog = Join-Path $env:ProgramData 'KLogin\credential-provider.log'

Write-Host ''
Write-Host 'KLogin credential provider COM test' -ForegroundColor Cyan
Write-Host ''

if (-not (Test-Path $SystemDll)) {
    throw "Missing DLL: $SystemDll"
}

foreach ($pair in @(
        @{ Name = 'Credential Provider'; Clsid = $CpClsid },
        @{ Name = 'Credential Provider Filter'; Clsid = $FilterClsid }
    )) {
    Write-Host "Creating $($pair.Name)..."
    try {
        $type = [Type]::GetTypeFromCLSID($pair.Clsid)
        if ($null -eq $type) {
            throw 'GetTypeFromCLSID returned null (registry missing or wrong CLSID path)'
        }
        $obj = [Activator]::CreateInstance($type)
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
    Write-Host ''
    Write-Host 'Credential provider log (best lock-screen diagnostic):' -ForegroundColor Cyan
    Get-Content $CpLog -Tail 20 | ForEach-Object { Write-Host "  $_" }
    $hasLogon = Select-String -Path $CpLog -Pattern 'SetUsageScenario: CPUS_LOGON' -Quiet
    $hasTile = Select-String -Path $CpLog -Pattern 'GetCredentialCount: returning 1 tile' -Quiet
    if ($hasLogon -and $hasTile) {
        Write-Host ''
        Write-Host '  => Winlogon IS loading KLogin on the lock screen.' -ForegroundColor Green
        Write-Host '     Click a user tile, then Sign-in options. Look for Sign in with KLogin.' -ForegroundColor Green
    }
} else {
    Write-Host ''
    Write-Host "No log yet at $CpLog" -ForegroundColor Yellow
    Write-Host 'Visit the lock screen once, then re-run this script.' -ForegroundColor Yellow
}

Write-Host ''
Write-Host 'Note: COM test failure here does not mean the CP is broken if the log shows CPUS_LOGON.' -ForegroundColor Yellow
