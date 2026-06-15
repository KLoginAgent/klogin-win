using System.IO.Pipes;
using System.Text;
using System.Text.Json;

namespace KLogin.Shared.Pipe;

public sealed class AgentPipeClient
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
    };

    private readonly string _pipeName;

    public AgentPipeClient(string pipeName = "klogin-agent")
    {
        _pipeName = pipeName;
    }

    public async Task<PipeClientResponse> LoginAsync(string username, string password, CancellationToken ct = default)
    {
        return await SendAsync(new
        {
            action = "login",
            username,
            password,
        }, ct);
    }

    public async Task<PipeClientResponse> SelectAsync(string token, int mappingId, CancellationToken ct = default)
    {
        return await SendAsync(new
        {
            action = "select",
            token,
            mappingId,
        }, ct);
    }

    public async Task<PipeClientResponse> HealthAsync(CancellationToken ct = default)
    {
        return await SendAsync(new { action = "health" }, ct);
    }

    private async Task<PipeClientResponse> SendAsync(object payload, CancellationToken ct)
    {
        await using var client = new NamedPipeClientStream(
            ".",
            _pipeName,
            PipeDirection.InOut,
            PipeOptions.Asynchronous);

        await client.ConnectAsync(5000, ct);

        using var writer = new StreamWriter(client, Encoding.UTF8, leaveOpen: true) { AutoFlush = true };
        using var reader = new StreamReader(client, Encoding.UTF8, leaveOpen: true);

        var json = JsonSerializer.Serialize(payload, JsonOptions);
        await writer.WriteLineAsync(json.AsMemory(), ct);

        var line = await reader.ReadLineAsync(ct);
        if (string.IsNullOrWhiteSpace(line))
        {
            return new PipeClientResponse(false, "Empty response from agent", null);
        }

        using var doc = JsonDocument.Parse(line);
        var root = doc.RootElement;
        var success = root.GetProperty("success").GetBoolean();
        var error = root.TryGetProperty("error", out var errorProp) ? errorProp.GetString() : null;
        JsonElement? data = root.TryGetProperty("data", out var dataProp) ? dataProp : null;
        return new PipeClientResponse(success, error, data);
    }
}

public sealed record PipeClientResponse(bool Success, string? Error, JsonElement? Data);
