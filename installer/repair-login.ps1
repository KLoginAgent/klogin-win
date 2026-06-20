#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Restore local user tiles and Windows password/PIN sign-in after KLogin filter issues.
    Does not remove KLogin — only disables the credential provider filter.
#>
$ErrorActionPreference = 'Continue'

$CpFilterClsid = '{8f3e2a10-4b5c-4d6e-9f01-23456789abce}'
$AgentReg = 'HKLM:\SOFTWARE\KLoginAgent'

Write-Host "`nKLogin lock screen repair`n" -ForegroundColor Cyan
Write-Host "This restores local user tiles and password/PIN sign-in." -ForegroundColor Yellow
Write-Host "KLogin agent files are left installed.`n"

if (-not (Test-Path $AgentReg)) {
    New-Item -Path $AgentReg -Force | Out-Null
}
New-ItemProperty -Path $AgentReg -Name ShowAllCredentialProviders -Value 1 -PropertyType DWord -Force | Out-Null
Remove-ItemProperty -Path $AgentReg -Name HideWindowsCredentialProviders -ErrorAction SilentlyContinue

$filterPaths = @(
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters\$CpFilterClsid",
    "HKLM:\SOFTWARE\Classes\CLSID\$CpFilterClsid"
)
foreach ($path in $filterPaths) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
        Write-Host "Removed: $path"
    }
}

Write-Host @"

Done. REBOOT this PC now.

After reboot you should see:
  - Local user tiles again (kiosk, Administrator, etc.)
  - Microsoft account with PIN/password as before
  - KLogin under Sign-in options (after clicking a user) once CP fixes are installed

To make KLogin the only sign-in method later (after it works), set:
  New-ItemProperty HKLM:\SOFTWARE\KLoginAgent -Name HideWindowsCredentialProviders -Value 1 -PropertyType DWord -Force
  then reinstall the MSI so the filter is registered again.
"@ -ForegroundColor Green
