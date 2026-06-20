#Requires -RunAsAdministrator

$CpClsid = "{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}"
$CpFilterClsid = "{8f3e2a10-4b5c-4d6e-9f01-23456789abce}"
$SystemDll = "C:\Windows\System32\KLoginCredentialProvider.dll"

Write-Host "Stopping KLogin Agent..."
sc.exe stop KLoginAgent 2>$null
sc.exe delete KLoginAgent 2>$null

$cpReg = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$CpClsid"
if (Test-Path $cpReg) { Remove-Item $cpReg -Recurse -Force }

$filterReg = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters\$CpFilterClsid"
if (Test-Path $filterReg) { Remove-Item $filterReg -Recurse -Force }

$clsidReg = "HKLM:\SOFTWARE\Classes\CLSID\$CpClsid"
if (Test-Path $clsidReg) { Remove-Item $clsidReg -Recurse -Force }

$filterClsidReg = "HKLM:\SOFTWARE\Classes\CLSID\$CpFilterClsid"
if (Test-Path $filterClsidReg) { Remove-Item $filterClsidReg -Recurse -Force }

if (Test-Path $SystemDll) { Remove-Item $SystemDll -Force }

$agentReg = "HKLM:\SOFTWARE\KLoginAgent"
if (Test-Path $agentReg) { Remove-Item $agentReg -Recurse -Force }

Write-Host "KLogin uninstalled."
