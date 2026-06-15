using KLogin.Agent.Pipe;
using KLogin.Shared.Services;
using Microsoft.Extensions.Options;

namespace KLogin.Agent;

public sealed class AgentWorker : BackgroundService
{
    private readonly AgentPipeServer _pipeServer;
    private readonly BackendClient _backendClient;
    private readonly ILogger<AgentWorker> _logger;
    private readonly KLoginOptions _options;

    public AgentWorker(
        AgentPipeServer pipeServer,
        BackendClient backendClient,
        IOptions<KLoginOptions> options,
        ILogger<AgentWorker> logger)
    {
        _pipeServer = pipeServer;
        _backendClient = backendClient;
        _options = options.Value;
        _logger = logger;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation(
            "KLogin Agent starting. Backend={BackendUrl}, Pipe={PipeName}",
            _options.BackendBaseUrl,
            _options.PipeName);

        try
        {
            if (await _backendClient.HealthAsync(stoppingToken))
            {
                _logger.LogInformation("Backend health check passed");
            }
            else
            {
                _logger.LogWarning("Backend health check failed; agent will retry on login requests");
            }
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Backend unreachable at startup");
        }

        await _pipeServer.RunAsync(stoppingToken);
    }
}
