using KLogin.Agent;
using KLogin.Agent.Pipe;
using KLogin.Shared.Services;

var builder = Host.CreateApplicationBuilder(args);

builder.Services
    .AddWindowsService(options => options.ServiceName = "KLogin Agent")
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
