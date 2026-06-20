# klogin-win

**Project guide:** [AGENTS.md](AGENTS.md) · Platform overview: [klogin-server AGENTS.md](https://github.com/KLoginAgent/klogin-server/blob/main/AGENTS.md)

Windows login agent for KLogin. Replaces the lock-screen sign-in experience with KLogin username/password, then logs the user into a mapped local Windows account without re-entering the Windows password.

## Architecture

```
Lock screen (Credential Provider DLL)
        │ named pipe JSON
        ▼
KLogin Agent (Windows Service)
        │ HTTPS /api/auth/*
        ▼
KLogin backend (Python/FastAPI)
```

### Login flow

1. User enters **KLogin** username/password on the lock screen.
2. Credential Provider sends a `login` request to the Agent via `\\.\pipe\klogin-agent`.
3. Agent calls `POST /api/auth/agent-login`.
4. If the user has **one** mapped Windows account (e.g. waylon → kiosk), Agent auto-selects and returns Windows credentials.
5. If the user has **multiple** mappings (e.g. justin → kiosk, admin), CP shows a picker and sends `select` with the chosen mapping id.
6. Agent calls `POST /api/auth/agent-select` and returns the Windows username/password for LSA logon.

## Projects

| Project | Purpose |
|---------|---------|
| `KLogin.Shared` | API client, models, login orchestration, pipe client |
| `KLogin.Agent` | Windows Service + named pipe server |
| `KLogin.Console` | Dev/test CLI (direct backend or via pipe) |
| `KLogin.CredentialProvider` | Native CP DLL scaffold (C++/COM) |

## Build (Windows)

Requirements: .NET 8 SDK, Visual Studio 2022 with C++ workload (for Credential Provider).

```powershell
cd klogin-win
dotnet build KLogin.sln -c Release
```

Build the credential provider in Visual Studio (x64 Release).

## Configure

Edit `src/KLogin.Agent/appsettings.json`:

```json
{
  "KLogin": {
    "BackendBaseUrl": "https://klogin.kumpe.app/api",
    "PipeName": "klogin-agent"
  }
}
```

## Run locally for development

Terminal 1 — backend (from klogin-server root):

```bash
docker compose up -d
```

Terminal 2 — agent (Windows):

```powershell
dotnet run --project src/KLogin.Agent
```

Terminal 3 — test console:

```powershell
dotnet run --project src/KLogin.Console
```

Use mode `1` for direct backend testing, mode `2` to exercise the named pipe like the Credential Provider will.

### Seed users

| KLogin user | Password | Windows accounts |
|-------------|----------|------------------|
| justin | justin123 | kiosk, administrator (picker) |
| waylon | waylon123 | kiosk (auto) |

## CI (GitHub Actions)

Workflow: `.github/workflows/publish-windows-agent.yml`

| Trigger | Installer tags |
|---------|----------------|
| Push to `main` | `latest`, `stage` |
| Release published | `latest`, `stable`, release tag (e.g. `v1.0.0`, `1.0.0`) |
| Manual dispatch | `stable`, `stage`, or `dev` (+ branch selection) |

Set repository variable `KLOGIN_BACKEND_URL` (default: `https://klogin.kumpe.app/api`) to override the MSI server URL pre-fill.

## Build MSI and Setup EXE (Windows)

Requirements: Windows 10/11, .NET 8 SDK, Visual Studio 2022 (C++ workload), WiX Toolset 5.

```powershell
cd klogin-win
./installer/build.ps1 -Configuration Release -BackendUrl "https://klogin.kumpe.app/api"
```

Artifacts:
- `dist/agent/` — self-contained KLogin Agent publish output
- `installer/bin/Release/en-US/KLoginAgent.msi` — per-machine installer (service + credential provider)
- `installer/bin/Release/en-US/KLoginAgentSetup.exe` — bootstrapper EXE wrapping the MSI

The MSI:
- Prompts for **KLogin backend server URL** during setup (default `https://klogin.kumpe.app/api`)
- Installs the agent to `C:\Program Files\KLogin\Agent` (includes `verify-install.ps1`)
- Registers and starts the `KLoginAgent` Windows service
- Copies `KLoginCredentialProvider.dll` to `System32` and registers the credential provider
- Writes the chosen URL to `appsettings.json` → `KLogin:BackendBaseUrl`

Silent install with a explicit URL:

```powershell
msiexec /i KLoginAgent.msi KLOGIN_BACKEND_URL="https://klogin.kumpe.app/api" /qn
```

## Install (manual alternative)

```powershell
./installer/install.ps1 -BackendUrl "http://your-server:8000"
./installer/verify-install.ps1
```

### Lock screen not showing KLogin?

**Seeing two user tiles (e.g. kiosk, Administrator)?** That is normal. Those are **local Windows accounts** created on the PC. KLogin does not replace that list — it adds a **KLogin sign-in tile** (or an entry under **Sign-in options**). After KLogin auth, the agent logs you into the mapped Windows account automatically.

1. **Reboot** after install (sign-out alone often is not enough).
2. On the lock screen, look for **Sign-in options** (bottom-left shield icon) → **KLogin**.
3. Run `C:\Program Files\KLogin\Agent\verify-install.ps1` as Administrator.
4. After a reboot + sign-in attempt, check `C:\ProgramData\KLogin\credential-provider.log` for load errors.
5. If Windows password login disappeared, restore it while troubleshooting:
   ```powershell
   New-ItemProperty HKLM:\SOFTWARE\KLoginAgent -Name ShowAllCredentialProviders -Value 1 -PropertyType DWord -Force
   # Reboot
   ```
6. Install [Microsoft VC++ 2015-2022 x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) if `verify-install.ps1` reports LoadLibrary failure.

**Kiosk / single-user setups:** for a cleaner lock screen, use one local Windows account per machine; KLogin mappings decide which account each user lands in.

## Pipe protocol

One JSON object per line over `\\.\pipe\klogin-agent`.

**Login**

```json
{"action":"login","username":"justin","password":"justin123"}
```

**Select account** (after multi-mapping login)

```json
{"action":"select","token":"<jwt>","mappingId":2}
```

**Success response**

```json
{"success":true,"data":{"status":"success","credentials":{"windowsUsername":"kiosk","displayName":"Kiosk User","domain":".","windowsPassword":"..."}}}
```

**Select response**

```json
{"success":true,"data":{"status":"select","token":"...","mappings":[{"mappingId":1,"windowsUsername":"kiosk",...}]}}
```

## Credential Provider next steps

The CP scaffold includes a native pipe client. Complete implementation should follow Microsoft's [Credential Provider sample](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/CredentialProvider):

- Implement `ICredentialProvider` / `ICredentialProviderCredential`
- Register GUID `{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}`
- Use returned credentials with `CredPackAuthenticationBuffer` / LSA logon APIs

Test only on a VM snapshot — a broken CP can lock you out of Windows.
