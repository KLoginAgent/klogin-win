using KLogin.Shared;

namespace KLogin.Agent;

public sealed class KLoginOptions
{
    public const string SectionName = "KLogin";

    public string BackendBaseUrl { get; set; } = BackendDefaults.ProductionUrl;
    public string PipeName { get; set; } = "klogin-agent";
}
