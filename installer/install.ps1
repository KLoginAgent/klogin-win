#Requires -RunAsAdministrator
param(
    [string]$AgentPath = (Join-Path $PSScriptRoot "..\src\KLogin.Agent\bin\Release\net8.0-windows\KLogin.Agent.exe"),
    [string]$CredentialProviderDll = (Join-Path $PSScriptRoot "..\src\KLogin.CredentialProvider\x64\Release\KLoginCredentialProvider.dll"),
    [string]$BackendUrl = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BackendUrl)) {
    $defaultUrl = "http://localhost:8000"
    $BackendUrl = Read-Host "Enter KLogin server URL [$defaultUrl]"
    if ([string]::IsNullOrWhiteSpace($BackendUrl)) {
        $BackendUrl = $defaultUrl
    }
}

$CpClsid = "{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}"
$SystemDll = "C:\Windows\System32\KLoginCredentialProvider.dll"

Write-Host "Installing KLogin Agent service..."
sc.exe create KLoginAgent binPath= "`"$AgentPath`"" start= auto DisplayName= "KLogin Agent"
sc.exe description KLoginAgent "KLogin backend bridge for Windows logon"
sc.exe failure KLoginAgent reset= 86400 actions= restart/60000/restart/60000/restart/60000

$configPath = Join-Path (Split-Path $AgentPath -Parent) "appsettings.json"
if (Test-Path $configPath) {
    $config = Get-Content $configPath -Raw | ConvertFrom-Json
    $config.KLogin.BackendBaseUrl = $BackendUrl
    $config | ConvertTo-Json -Depth 5 | Set-Content $configPath
}

$agentReg = "HKLM:\SOFTWARE\KLoginAgent"
New-Item -Path $agentReg -Force | Out-Null
Set-ItemProperty -Path $agentReg -Name "BackendBaseUrl" -Value $BackendUrl

sc.exe start KLoginAgent

Write-Host "Installing Credential Provider..."
Copy-Item $CredentialProviderDll -Destination $SystemDll -Force

$cpReg = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$CpClsid"
New-Item -Path $cpReg -Force | Out-Null
Set-ItemProperty -Path $cpReg -Name "(default)" -Value "KLogin Credential Provider"

$clsidReg = "HKLM:\SOFTWARE\Classes\CLSID\$CpClsid"
New-Item -Path $clsidReg -Force | Out-Null
Set-ItemProperty -Path $clsidReg -Name "(default)" -Value "KLogin Credential Provider"
New-Item -Path "$clsidReg\InprocServer32" -Force | Out-Null
Set-ItemProperty -Path "$clsidReg\InprocServer32" -Name "(default)" -Value $SystemDll
Set-ItemProperty -Path "$clsidReg\InprocServer32" -Name "ThreadingModel" -Value "Apartment"

Write-Host "KLogin installed."
Write-Host ""
Write-Host "IMPORTANT — lock screen behavior:"
Write-Host "  • KLogin adds a sign-in option; it does NOT remove the default Windows password/PIN screen."
Write-Host "  • REBOOT this PC (sign-out alone is often not enough for the credential provider to load)."
Write-Host "  • On the lock screen, click 'Sign-in options' and choose the KLogin tile."
Write-Host ""
Write-Host "Run .\verify-install.ps1 to confirm the service, DLL, and registry keys."
