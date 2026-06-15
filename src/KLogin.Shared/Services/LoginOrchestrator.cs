using KLogin.Shared.Models;
using KLogin.Shared.Services;

namespace KLogin.Shared.Services;

public sealed class LoginOrchestrator
{
    private readonly BackendClient _backend;

    public LoginOrchestrator(BackendClient backend)
    {
        _backend = backend;
    }

    public async Task<LoginFlowResult> LoginAsync(string username, string password, CancellationToken ct = default)
    {
        try
        {
            var login = await _backend.AgentLoginAsync(username, password, ct);

            if (login.AutoSelect && login.SelectedMappingId is int mappingId)
            {
                return await CompleteSelectionAsync(login.AccessToken, mappingId, ct);
            }

            if (login.Mappings.Count == 1)
            {
                return await CompleteSelectionAsync(login.AccessToken, login.Mappings[0].MappingId, ct);
            }

            return new LoginFlowResult(
                LoginStatus.SelectAccount,
                login.AccessToken,
                login.Mappings,
                null,
                null);
        }
        catch (BackendRequestException ex)
        {
            return new LoginFlowResult(LoginStatus.Failed, null, null, null, ex.Message);
        }
        catch (Exception ex)
        {
            return new LoginFlowResult(LoginStatus.Failed, null, null, null, ex.Message);
        }
    }

    public async Task<LoginFlowResult> SelectAsync(string accessToken, int mappingId, CancellationToken ct = default)
    {
        try
        {
            return await CompleteSelectionAsync(accessToken, mappingId, ct);
        }
        catch (BackendRequestException ex)
        {
            return new LoginFlowResult(LoginStatus.Failed, null, null, null, ex.Message);
        }
        catch (Exception ex)
        {
            return new LoginFlowResult(LoginStatus.Failed, null, null, null, ex.Message);
        }
    }

    private async Task<LoginFlowResult> CompleteSelectionAsync(
        string accessToken,
        int mappingId,
        CancellationToken ct)
    {
        var selected = await _backend.AgentSelectAsync(accessToken, mappingId, ct);
        var credentials = new WindowsLogonCredentials(
            selected.WindowsUsername,
            selected.DisplayName,
            selected.Domain,
            selected.WindowsPassword);

        return new LoginFlowResult(LoginStatus.Success, accessToken, null, credentials, null);
    }
}
