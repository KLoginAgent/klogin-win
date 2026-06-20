using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;
using KLogin.Shared.Models;

namespace KLogin.Shared.Services;

public sealed class BackendClient : IDisposable
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        PropertyNameCaseInsensitive = true,
    };

    private readonly HttpClient _http;
    private readonly string _apiPrefix;

    public BackendClient(string baseUrl, HttpMessageHandler? handler = null)
    {
        _http = handler is null ? new HttpClient() : new HttpClient(handler);
        _http.BaseAddress = NormalizeBaseAddress(baseUrl);
        _http.Timeout = TimeSpan.FromSeconds(30);
        _apiPrefix = BaseUrlIncludesApiPath(baseUrl) ? string.Empty : "api/";
    }

    public async Task<AgentLoginResponse> AgentLoginAsync(string username, string password, CancellationToken ct = default)
    {
        using var response = await _http.PostAsJsonAsync(
            $"{_apiPrefix}auth/agent-login",
            new LoginRequest(username, password),
            JsonOptions,
            ct);

        await EnsureSuccessAsync(response, ct);
        var payload = await response.Content.ReadFromJsonAsync<AgentLoginPayload>(JsonOptions, ct)
            ?? throw new InvalidOperationException("Empty agent-login response");

        return new AgentLoginResponse(
            payload.AccessToken,
            payload.TokenType,
            payload.User.Username,
            payload.User.DisplayName,
            payload.Mappings.Select(MapOption).ToList(),
            payload.AutoSelect,
            payload.SelectedMappingId);
    }

    public async Task<AgentSelectResponse> AgentSelectAsync(string accessToken, int mappingId, CancellationToken ct = default)
    {
        using var request = new HttpRequestMessage(HttpMethod.Post, $"{_apiPrefix}auth/agent-select")
        {
            Content = JsonContent.Create(new AgentSelectRequest(mappingId), options: JsonOptions),
        };
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", accessToken);

        using var response = await _http.SendAsync(request, ct);
        await EnsureSuccessAsync(response, ct);
        var payload = await response.Content.ReadFromJsonAsync<AgentSelectPayload>(JsonOptions, ct)
            ?? throw new InvalidOperationException("Empty agent-select response");

        return new AgentSelectResponse(
            payload.MappingId,
            payload.WindowsUsername,
            payload.DisplayName,
            payload.Domain,
            payload.WindowsPassword);
    }

    public async Task<bool> HealthAsync(CancellationToken ct = default)
    {
        using var response = await _http.GetAsync($"{_apiPrefix}health", ct);
        return response.IsSuccessStatusCode;
    }

    public void Dispose() => _http.Dispose();

    private static Uri NormalizeBaseAddress(string baseUrl)
    {
        var trimmed = baseUrl.TrimEnd('/');
        return new Uri(trimmed + "/", UriKind.Absolute);
    }

    private static bool BaseUrlIncludesApiPath(string baseUrl)
    {
        var trimmed = baseUrl.TrimEnd('/');
        return trimmed.EndsWith("/api", StringComparison.OrdinalIgnoreCase);
    }

    private static AgentMappingOption MapOption(AgentMappingPayload payload) =>
        new(payload.MappingId, payload.WindowsUsername, payload.DisplayName, payload.Domain, payload.Label);

    private static async Task EnsureSuccessAsync(HttpResponseMessage response, CancellationToken ct)
    {
        if (response.IsSuccessStatusCode)
        {
            return;
        }

        var body = await response.Content.ReadAsStringAsync(ct);
        throw new BackendRequestException((int)response.StatusCode, ExtractDetail(body));
    }

    private static string ExtractDetail(string body)
    {
        try
        {
            using var doc = JsonDocument.Parse(body);
            if (doc.RootElement.TryGetProperty("detail", out var detail))
            {
                return detail.GetString() ?? body;
            }
        }
        catch (JsonException)
        {
            // fall through
        }

        return string.IsNullOrWhiteSpace(body) ? "Request failed" : body;
    }

    private sealed record AgentLoginPayload(
        string AccessToken,
        string TokenType,
        UserPayload User,
        List<AgentMappingPayload> Mappings,
        bool AutoSelect,
        int? SelectedMappingId);

    private sealed record UserPayload(string Username, string DisplayName);
    private sealed record AgentMappingPayload(
        int MappingId,
        string WindowsUsername,
        string DisplayName,
        string Domain,
        string? Label);

    private sealed record AgentSelectPayload(
        int MappingId,
        string WindowsUsername,
        string DisplayName,
        string Domain,
        string WindowsPassword);
}

public sealed class BackendRequestException : Exception
{
    public BackendRequestException(int statusCode, string detail)
        : base(detail)
    {
        StatusCode = statusCode;
    }

    public int StatusCode { get; }
}
