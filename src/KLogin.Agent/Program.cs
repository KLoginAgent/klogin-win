using KLogin.Agent;
using KLogin.Agent.Pipe;
using KLogin.Shared.Services;
using Microsoft.Win32;

var builder = Host.CreateApplicationBuilder(args);

var backendFromRegistry = Registry.LocalMachine
    .OpenSubKey(@"SOFTWARE\KLoginAgent")
    ?.GetValue("BackendBaseUrl") as string;
if (!string.IsNullOrWhiteSpace(backendFromRegistry))
{
    builder.Configuration.AddInMemoryCollection(new Dictionary<string, string?>
    {
        [$"{KLoginOptions.SectionName}:BackendBaseUrl"] = backendFromRegistry,
    });
}

builder.Services
    .AddWindowsService(options => options.ServiceName = "KLoginAgent")
    .Configure<KLoginOptions>(builder.Configuration.GetSection(KLoginOptions.SectionName))
    .AddSingleton(sp =>
    {
        var options = sp.GetRequiredService<Microsoft.Extensions.Options.IOptions<KLoginOptions>>().Value;
        return new BackendClient(options.BackendBaseUrl);
    })
    .AddSingleton<LoginOrchestrator>()
    .AddSingleton(sp =>
    {
        var options = sp.GetRequiredService<Microsoft.Extensions.Options.IOptions<KLoginOptions>>().Value;
        return new AgentPipeServer(
            sp.GetRequiredService<LoginOrchestrator>(),
            sp.GetRequiredService<ILogger<AgentPipeServer>>(),
            options.PipeName);
    })
    .AddHostedService<AgentWorker>();

var host = builder.Build();
host.Run();
