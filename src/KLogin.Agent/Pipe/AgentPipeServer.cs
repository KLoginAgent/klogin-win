using System.IO.Pipes;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using KLogin.Shared.Models;
using KLogin.Shared.Services;

namespace KLogin.Agent.Pipe;

public sealed class AgentPipeServer
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
        WriteIndented = false,
    };

    private readonly LoginOrchestrator _orchestrator;
    private readonly ILogger<AgentPipeServer> _logger;
    private readonly string _pipeName;

    public AgentPipeServer(LoginOrchestrator orchestrator, ILogger<AgentPipeServer> logger, string pipeName)
    {
        _orchestrator = orchestrator;
        _logger = logger;
        _pipeName = pipeName;
    }

    public async Task RunAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("Named pipe server listening on \\\\.\\pipe\\{PipeName}", _pipeName);

        while (!stoppingToken.IsCancellationRequested)
        {
            await using var server = CreatePipe();
            try
            {
                await server.WaitForConnectionAsync(stoppingToken);
                await HandleClientAsync(server, stoppingToken);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                break;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Pipe server error");
            }
        }
    }

    private NamedPipeServerStream CreatePipe()
    {
        var security = new PipeSecurity();
        security.AddAccessRule(new PipeAccessRule(
            new SecurityIdentifier(WellKnownSidType.LocalSystemSid, null),
            PipeAccessRights.FullControl,
            AccessControlType.Allow));
        security.AddAccessRule(new PipeAccessRule(
            new SecurityIdentifier(WellKnownSidType.BuiltinAdministratorsSid, null),
            PipeAccessRights.FullControl,
            AccessControlType.Allow));
        security.AddAccessRule(new PipeAccessRule(
            new SecurityIdentifier(WellKnownSidType.WorldSid, null),
            PipeAccessRights.ReadWrite,
            AccessControlType.Allow));

        return NamedPipeServerStreamAcl.Create(
            _pipeName,
            PipeDirection.InOut,
            NamedPipeServerStream.MaxAllowedServerInstances,
            PipeTransmissionMode.Byte,
            PipeOptions.Asynchronous,
            inBufferSize: 4096,
            outBufferSize: 4096,
            security);
    }

    private async Task HandleClientAsync(NamedPipeServerStream server, CancellationToken ct)
    {
        using var reader = new StreamReader(server, Encoding.UTF8, leaveOpen: true);
        using var writer = new StreamWriter(server, Encoding.UTF8, leaveOpen: true) { AutoFlush = true };

        var line = await reader.ReadLineAsync(ct);
        if (string.IsNullOrWhiteSpace(line))
        {
            await WriteResponseAsync(writer, PipeResponse.Fail("Empty request"), ct);
            return;
        }

        PipeRequest? request;
        try
        {
            request = JsonSerializer.Deserialize<PipeRequest>(line, JsonOptions);
        }
        catch (JsonException ex)
        {
            await WriteResponseAsync(writer, PipeResponse.Fail($"Invalid JSON: {ex.Message}"), ct);
            return;
        }

        if (request is null || string.IsNullOrWhiteSpace(request.Action))
        {
            await WriteResponseAsync(writer, PipeResponse.Fail("Missing action"), ct);
            return;
        }

        var response = request.Action.ToLowerInvariant() switch
        {
            "health" => await HandleHealthAsync(ct),
            "login" => await HandleLoginAsync(request, ct),
            "select" => await HandleSelectAsync(request, ct),
            _ => PipeResponse.Fail($"Unknown action: {request.Action}"),
        };

        await WriteResponseAsync(writer, response, ct);
    }

    private async Task<PipeResponse> HandleHealthAsync(CancellationToken ct)
    {
        // Health is checked via orchestrator's backend on login; keep pipe responsive.
        await Task.Yield();
        return PipeResponse.Ok(new { status = "ready" });
    }

    private async Task<PipeResponse> HandleLoginAsync(PipeRequest request, CancellationToken ct)
    {
        if (string.IsNullOrWhiteSpace(request.Username) || string.IsNullOrWhiteSpace(request.Password))
        {
            return PipeResponse.Fail("Username and password are required");
        }

        var result = await _orchestrator.LoginAsync(request.Username, request.Password, ct);
        return ToPipeResponse(result);
    }

    private async Task<PipeResponse> HandleSelectAsync(PipeRequest request, CancellationToken ct)
    {
        if (string.IsNullOrWhiteSpace(request.Token) || request.MappingId is null)
        {
            return PipeResponse.Fail("Token and mappingId are required");
        }

        var result = await _orchestrator.SelectAsync(request.Token, request.MappingId.Value, ct);
        return ToPipeResponse(result);
    }

    private static PipeResponse ToPipeResponse(LoginFlowResult result) =>
        result.Status switch
        {
            LoginStatus.Success => PipeResponse.Ok(new
            {
                status = "success",
                credentials = new
                {
                    result.Credentials!.WindowsUsername,
                    result.Credentials.DisplayName,
                    result.Credentials.Domain,
                    result.Credentials.WindowsPassword,
                },
            }),
            LoginStatus.SelectAccount => PipeResponse.Ok(new
            {
                status = "select",
                token = result.AccessToken,
                mappings = result.Mappings!.Select(m => new
                {
                    m.MappingId,
                    m.WindowsUsername,
                    m.DisplayName,
                    m.Domain,
                    m.Label,
                }),
            }),
            LoginStatus.Failed => PipeResponse.Fail(result.Error ?? "Login failed"),
            _ => PipeResponse.Fail("Unexpected login state"),
        };

    private static Task WriteResponseAsync(StreamWriter writer, PipeResponse response, CancellationToken ct) =>
        writer.WriteLineAsync(JsonSerializer.Serialize(response, JsonOptions).AsMemory(), ct);

    private sealed record PipeRequest(
        string Action,
        string? Username,
        string? Password,
        string? Token,
        int? MappingId);

    private sealed record PipeResponse(bool Success, string? Error, object? Data)
    {
        public static PipeResponse Ok(object data) => new(true, null, data);
        public static PipeResponse Fail(string error) => new(false, error, null);
    }
}
