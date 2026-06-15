namespace KLogin.Shared.Models;

public sealed record LoginRequest(string Username, string Password);

public sealed record AgentMappingOption(
    int MappingId,
    string WindowsUsername,
    string DisplayName,
    string Domain,
    string? Label);

public sealed record AgentLoginResponse(
    string AccessToken,
    string TokenType,
    string Username,
    string DisplayName,
    IReadOnlyList<AgentMappingOption> Mappings,
    bool AutoSelect,
    int? SelectedMappingId);

public sealed record AgentSelectRequest(int MappingId);

public sealed record AgentSelectResponse(
    int MappingId,
    string WindowsUsername,
    string DisplayName,
    string Domain,
    string WindowsPassword);

public sealed record WindowsLogonCredentials(
    string WindowsUsername,
    string DisplayName,
    string Domain,
    string WindowsPassword);

public enum LoginStatus
{
    Success,
    SelectAccount,
    Failed,
}

public sealed record LoginFlowResult(
    LoginStatus Status,
    string? AccessToken,
    IReadOnlyList<AgentMappingOption>? Mappings,
    WindowsLogonCredentials? Credentials,
    string? Error);
