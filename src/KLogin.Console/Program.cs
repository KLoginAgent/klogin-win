using KLogin.Shared;
using KLogin.Shared.Models;
using KLogin.Shared.Pipe;
using KLogin.Shared.Services;

const string defaultBackend = BackendDefaults.LocalUrl;
const string defaultPipe = "klogin-agent";

Console.WriteLine("KLogin Windows Agent Console");
Console.WriteLine("1) Direct backend  2) Via agent pipe");
Console.Write("Mode [1]: ");
var mode = Console.ReadLine()?.Trim();
var usePipe = mode == "2";

Console.Write($"Backend URL [{defaultBackend}]: ");
var backendUrl = Console.ReadLine()?.Trim();
if (string.IsNullOrWhiteSpace(backendUrl))
{
    backendUrl = defaultBackend;
}

Console.Write("Username: ");
var username = Console.ReadLine()?.Trim() ?? string.Empty;
Console.Write("Password: ");
var password = ReadPassword();

LoginFlowResult result;
if (usePipe)
{
    Console.Write($"Pipe name [{defaultPipe}]: ");
    var pipeName = Console.ReadLine()?.Trim();
    if (string.IsNullOrWhiteSpace(pipeName))
    {
        pipeName = defaultPipe;
    }

    result = await LoginViaPipeAsync(new AgentPipeClient(pipeName), username, password);
}
else
{
    using var backend = new BackendClient(backendUrl);
    var orchestrator = new LoginOrchestrator(backend);
    result = await orchestrator.LoginAsync(username, password);
}

await HandleResultAsync(result, usePipe, defaultPipe, backendUrl);

static async Task<LoginFlowResult> LoginViaPipeAsync(AgentPipeClient client, string username, string password)
{
    var response = await client.LoginAsync(username, password);
    if (!response.Success)
    {
        return new LoginFlowResult(LoginStatus.Failed, null, null, null, response.Error);
    }

    return ParsePipeLoginResponse(response.Data);
}

static async Task HandleResultAsync(LoginFlowResult result, bool usePipe, string pipeName, string backendUrl)
{
    while (result.Status == LoginStatus.SelectAccount)
    {
        Console.WriteLine();
        Console.WriteLine("Select Windows account:");
        for (var i = 0; i < result.Mappings!.Count; i++)
        {
            var mapping = result.Mappings[i];
            var label = string.IsNullOrWhiteSpace(mapping.Label) ? mapping.DisplayName : mapping.Label;
            Console.WriteLine($"  [{i + 1}] {label} ({mapping.WindowsUsername}@{mapping.Domain})");
        }

        Console.Write("Choice: ");
        if (!int.TryParse(Console.ReadLine(), out var choice) || choice < 1 || choice > result.Mappings.Count)
        {
            Console.WriteLine("Invalid choice.");
            return;
        }

        var selected = result.Mappings[choice - 1];
        if (usePipe)
        {
            var client = new AgentPipeClient(pipeName);
            var response = await client.SelectAsync(result.AccessToken!, selected.MappingId);
            if (!response.Success)
            {
                Console.WriteLine($"Select failed: {response.Error}");
                return;
            }

            result = ParsePipeSelectResponse(response.Data);
        }
        else
        {
            using var backend = new BackendClient(backendUrl);
            var orchestrator = new LoginOrchestrator(backend);
            result = await orchestrator.SelectAsync(result.AccessToken!, selected.MappingId);
        }
    }

    switch (result.Status)
    {
        case LoginStatus.Success:
            Console.WriteLine();
            Console.WriteLine("Windows logon ready:");
            Console.WriteLine($"  User   : {result.Credentials!.WindowsUsername}");
            Console.WriteLine($"  Domain : {result.Credentials.Domain}");
            Console.WriteLine($"  Name   : {result.Credentials.DisplayName}");
            Console.WriteLine("  Password retrieved for Windows logon (not shown).");
            break;
        case LoginStatus.Failed:
            Console.WriteLine($"Login failed: {result.Error}");
            break;
    }
}

static LoginFlowResult ParsePipeLoginResponse(JsonElement? data)
{
    if (data is null)
    {
        return new LoginFlowResult(LoginStatus.Failed, null, null, null, "Missing response data");
    }

    var status = data.Value.GetProperty("status").GetString();
    if (status == "success")
    {
        var creds = data.Value.GetProperty("credentials");
        return new LoginFlowResult(
            LoginStatus.Success,
            null,
            null,
            new WindowsLogonCredentials(
                creds.GetProperty("windowsUsername").GetString()!,
                creds.GetProperty("displayName").GetString()!,
                creds.GetProperty("domain").GetString()!,
                creds.GetProperty("windowsPassword").GetString()!),
            null);
    }

    if (status == "select")
    {
        var token = data.Value.GetProperty("token").GetString()!;
        var mappings = data.Value.GetProperty("mappings").EnumerateArray()
            .Select(item => new AgentMappingOption(
                item.GetProperty("mappingId").GetInt32(),
                item.GetProperty("windowsUsername").GetString()!,
                item.GetProperty("displayName").GetString()!,
                item.GetProperty("domain").GetString()!,
                item.TryGetProperty("label", out var label) ? label.GetString() : null))
            .ToList();

        return new LoginFlowResult(LoginStatus.SelectAccount, token, mappings, null, null);
    }

    return new LoginFlowResult(LoginStatus.Failed, null, null, null, "Unexpected pipe response");
}

static LoginFlowResult ParsePipeSelectResponse(JsonElement? data) =>
    ParsePipeLoginResponse(data);

static string ReadPassword()
{
    var chars = new List<char>();
    while (true)
    {
        var key = Console.ReadKey(intercept: true);
        if (key.Key == ConsoleKey.Enter)
        {
            Console.WriteLine();
            break;
        }

        if (key.Key == ConsoleKey.Backspace && chars.Count > 0)
        {
            chars.RemoveAt(chars.Count - 1);
            continue;
        }

        if (!char.IsControl(key.KeyChar))
        {
            chars.Add(key.KeyChar);
        }
    }

    return new string(chars.ToArray());
}
