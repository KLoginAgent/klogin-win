namespace KLogin.Agent;

public sealed class KLoginOptions
{
    public const string SectionName = "KLogin";

    public string BackendBaseUrl { get; set; } = "http://localhost:8000";
    public string PipeName { get; set; } = "klogin-agent";
}
