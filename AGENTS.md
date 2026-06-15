# klogin-win — guide for developers and AI assistants

Windows client for [KLogin](https://github.com/KLoginAgent/klogin-server). Read `../AGENTS.md` in the monorepo checkout (or klogin-server `AGENTS.md`) for the full platform picture.

## This repo

| Project | Type | Purpose |
|---------|------|---------|
| `KLogin.Shared` | .NET 8 class lib | Backend API client, models, `LoginOrchestrator`, `AgentPipeClient` |
| `KLogin.Agent` | .NET 8 Windows Service | Named pipe server; talks to Python backend |
| `KLogin.Console` | .NET 8 console | Dev CLI to test login flow |
| `KLogin.CredentialProvider` | C++ x64 DLL | Lock-screen credential provider (COM) |

## Build requirements

- .NET 8 SDK (`global.json`)
- Visual Studio 2022 with **Desktop development with C++** (for CP DLL)
- WiX Toolset 5 (installed by `build.ps1` / CI)

Build **only on Windows**. Cannot compile CP or service host on macOS/Linux.

## Build commands

```powershell
# Full MSI + Setup EXE
./installer/build.ps1 -Configuration Release `
  -BackendUrl "http://your-server:8000" `
  -ProductVersion "1.0.0.0" `
  -VersionTags stable,latest

# Dev: run agent + test console
dotnet run --project src/KLogin.Agent
dotnet run --project src/KLogin.Console
```

Outputs:
- `dist/agent/` — published service binaries
- `dist/installers/KLoginAgent-<tag>.msi`
- `dist/installers/KLoginAgentSetup-<tag>.exe`

## CI

Workflow: **`.github/workflows/publish-windows-agent.yml`** (lives in this repo, not klogin-server)

| Trigger | Installer tags |
|---------|----------------|
| Push to `main` | `latest`, `stage` |
| Release | `latest`, `stable`, release tag |
| Manual | `stable`, `stage`, or `dev` + branch |

Set repo variable `KLOGIN_BACKEND_URL` (default value pre-filled in the installer's server URL prompt).

## Installer behavior

- **MSI / Setup EXE:** custom dialog prompts for backend server URL (default from build).
- **Silent MSI:** `msiexec /i KLoginAgent.msi KLOGIN_BACKEND_URL="https://host:8000" /qn`
- **`install.ps1`:** prompts interactively if `-BackendUrl` is omitted.

## Login flow (implementation map)

1. **CP** (`KLoginCredential.cpp`) — UI fields, calls pipe
2. **Agent** (`AgentPipeServer.cs`) — JSON pipe → `LoginOrchestrator`
3. **Shared** (`BackendClient.cs`) — `POST /api/auth/agent-login`, `agent-select`
4. **Backend** (klogin-server) — validates user, returns mappings + Windows creds
5. **CP** — `PackPasswordLogon` → Windows logon

## Credential Provider GUID

`{8f3e2a10-4b5c-4d6e-9f01-23456789abcd}`

## When editing

- Pipe protocol changes require updating **both** `AgentPipeServer.cs` and `pipe_client.cpp`.
- Backend URL default: `src/KLogin.Agent/appsettings.json` → `KLogin:BackendBaseUrl`.
- Test CP changes on a **VM snapshot** — a broken CP can lock you out of Windows.
- Push to **this repo**; klogin-server gitignores `klogin-win/`.

## Related docs

- Platform overview: klogin-server `AGENTS.md`
- User-facing readme: `README.md`
