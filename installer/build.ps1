#Requires -Version 5.1
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$BackendUrl = 'http://localhost:8000',
    [string]$ProductVersion = '0.1.0.0',
    [string[]]$VersionTags = @('dev'),
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
$Dist = Join-Path $Root 'dist'
$AgentPublish = Join-Path $Dist 'agent'
$InstallerOutput = Join-Path $Dist 'installers'
$CpProject = Join-Path $Root 'src\KLogin.CredentialProvider\KLogin.CredentialProvider.vcxproj'
$CpDll = Join-Path $Root "src\KLogin.CredentialProvider\x64\$Configuration\KLoginCredentialProvider.dll"
$InstallerDir = $PSScriptRoot

Write-Host "==> Restore .NET projects"
dotnet restore (Join-Path $Root 'KLogin.sln')

Write-Host "==> Build .NET solution ($Configuration)"
dotnet build (Join-Path $Root 'KLogin.sln') -c $Configuration --no-restore

if (-not $SkipTests) {
    Write-Host "==> Build console test harness"
    dotnet build (Join-Path $Root 'src\KLogin.Console\KLogin.Console.csproj') -c $Configuration --no-restore
}

Write-Host "==> Publish KLogin Agent (win-x64 self-contained)"
if (Test-Path $AgentPublish) {
    Remove-Item $AgentPublish -Recurse -Force
}
dotnet publish (Join-Path $Root 'src\KLogin.Agent\KLogin.Agent.csproj') `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    -p:PublishSingleFile=false `
    -o $AgentPublish

Write-Host "==> Patch default backend URL in published appsettings"
$AppSettings = Join-Path $AgentPublish 'appsettings.json'
if (Test-Path $AppSettings) {
    $json = Get-Content $AppSettings -Raw | ConvertFrom-Json
    $json.KLogin.BackendBaseUrl = $BackendUrl
    $json | ConvertTo-Json -Depth 5 | Set-Content $AppSettings
}

Write-Host "==> Build Credential Provider ($Configuration|x64)"
$Msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe |
    Select-Object -First 1
if (-not $Msbuild) {
    throw 'MSBuild not found. Install Visual Studio 2022 with Desktop development with C++.'
}
& $Msbuild $CpProject /p:Configuration=$Configuration /p:Platform=x64 /m
if (-not (Test-Path $CpDll)) {
    throw "Credential Provider DLL not found at $CpDll"
}

Write-Host "==> Install WiX Toolset (if needed)"
dotnet tool restore --tool-manifest (Join-Path $InstallerDir '.config\dotnet-tools.json') 2>$null
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    dotnet tool install --global wix --version 5.0.2
}

Write-Host "==> Build MSI (ProductVersion=$ProductVersion)"
Push-Location $InstallerDir
try {
    dotnet build .\KLogin.Installer.wixproj -c $Configuration `
        -p:AgentPublishDir="$AgentPublish\\" `
        -p:CredentialProviderDll="$CpDll" `
        -p:BackendUrl="$BackendUrl" `
        -p:ProductVersion="$ProductVersion"

    Write-Host "==> Build bootstrapper EXE"
    dotnet build .\KLogin.Bundle.wixproj -c $Configuration `
        -p:ProductVersion="$ProductVersion"
}
finally {
    Pop-Location
}

$MsiPath = Join-Path $InstallerDir "bin\$Configuration\en-US\KLoginAgent.msi"
$ExePath = Join-Path $InstallerDir "bin\$Configuration\en-US\KLoginAgentSetup.exe"

if (-not (Test-Path $MsiPath) -or -not (Test-Path $ExePath)) {
    throw 'Installer outputs were not produced.'
}

if (Test-Path $InstallerOutput) {
    Remove-Item $InstallerOutput -Recurse -Force
}
New-Item -ItemType Directory -Path $InstallerOutput | Out-Null

foreach ($tag in $VersionTags) {
    $safeTag = $tag -replace '[^A-Za-z0-9._-]', '-'
    Copy-Item $MsiPath (Join-Path $InstallerOutput "KLoginAgent-$safeTag.msi") -Force
    Copy-Item $ExePath (Join-Path $InstallerOutput "KLoginAgentSetup-$safeTag.exe") -Force
}

Write-Host ""
Write-Host "Build complete:"
Write-Host "  Agent publish : $AgentPublish"
Write-Host "  CP DLL        : $CpDll"
Write-Host "  ProductVersion: $ProductVersion"
Write-Host "  Version tags  : $($VersionTags -join ', ')"
Write-Host "  Installers    : $InstallerOutput"
