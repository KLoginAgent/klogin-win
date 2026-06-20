namespace KLogin.Shared;

public static class BackendDefaults
{
    /// <summary>Production API base (includes /api path segment).</summary>
    public const string ProductionUrl = "https://klogin.kumpe.app/api";

    /// <summary>Local docker-compose backend (server root; /api added by client).</summary>
    public const string LocalUrl = "http://localhost:8000";
}
