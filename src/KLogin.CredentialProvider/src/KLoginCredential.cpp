#include "KLoginCredential.h"

#include "guid.h"
#include "helpers.h"

#include <new>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

KLoginCredential::KLoginCredential(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) : _cpus(cpus) {}

KLoginCredential::~KLoginCredential() {
    if (_pEvents) {
        _pEvents->Release();
    }
    KLogin::SecureZeroWide(_password);
    KLogin::SecureZeroWide(_winPassword);
    KLogin::SecureZeroWide(_localPassword);
}

IFACEMETHODIMP KLoginCredential::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(KLoginCredential, ICredentialProviderCredential),
        QITABENT(KLoginCredential, ICredentialProviderCredential2),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) KLoginCredential::AddRef() { return InterlockedIncrement(&_cRef); }

IFACEMETHODIMP_(ULONG) KLoginCredential::Release() {
    const LONG cRef = InterlockedDecrement(&_cRef);
    if (!cRef) {
        delete this;
    }
    return cRef;
}

IFACEMETHODIMP KLoginCredential::Advise(ICredentialProviderCredentialEvents* pcpce) {
    if (_pEvents) {
        _pEvents->Release();
    }
    _pEvents = pcpce;
    if (_pEvents) {
        _pEvents->AddRef();
    }
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::UnAdvise() {
    if (_pEvents) {
        _pEvents->Release();
        _pEvents = nullptr;
    }
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::SetSelected(BOOL* pbAutoLogon) {
    *pbAutoLogon = FALSE;
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::SetDeselected() {
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::GetFieldState(DWORD dwFieldID, CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs, CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) {
    if (dwFieldID >= FID_COUNT || !pcpfs || !pcpfis) {
        return E_INVALIDARG;
    }

    *pcpfis = CPFIS_NONE;
    switch (dwFieldID) {
        case FID_LABEL:
            *pcpfs = CPFS_DISPLAY_IN_BOTH;
            break;
        case FID_USERNAME:
            *pcpfs = _stage == Stage::Login ? CPFS_DISPLAY_IN_BOTH : CPFS_HIDDEN;
            break;
        case FID_PASSWORD:
            *pcpfs = _stage == Stage::Login ? CPFS_DISPLAY_IN_BOTH : CPFS_HIDDEN;
            break;
        case FID_MAPPING:
            *pcpfs = _stage == Stage::Select ? CPFS_DISPLAY_IN_BOTH : CPFS_HIDDEN;
            break;
        case FID_SUBMIT:
            *pcpfs = (_stage == Stage::Login || _stage == Stage::Select || _stage == Stage::Emergency)
                ? CPFS_DISPLAY_IN_BOTH
                : CPFS_HIDDEN;
            break;
        case FID_EMERGENCY_LINK:
            *pcpfs = _stage == Stage::Login ? CPFS_DISPLAY_IN_BOTH : CPFS_HIDDEN;
            break;
        case FID_LOCAL_USER:
        case FID_LOCAL_PASS:
            *pcpfs = _stage == Stage::Emergency ? CPFS_DISPLAY_IN_BOTH : CPFS_HIDDEN;
            break;
        default:
            return E_INVALIDARG;
    }
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::GetStringValue(DWORD dwFieldID, LPWSTR* ppwsz) {
    if (!ppwsz) {
        return E_INVALIDARG;
    }
    switch (dwFieldID) {
        case FID_LABEL:
            if (_stage == Stage::Emergency) {
                return SHStrDupW(L"Emergency local sign-in", ppwsz);
            }
            if (_stage == Stage::Select) {
                return SHStrDupW(L"Choose Windows account", ppwsz);
            }
            return SHStrDupW(L"Sign in with KLogin", ppwsz);
        case FID_USERNAME:
            return SHStrDupW(_username.c_str(), ppwsz);
        case FID_LOCAL_USER:
            return SHStrDupW(_localUser.c_str(), ppwsz);
        case FID_EMERGENCY_LINK:
            return SHStrDupW(L"Emergency local administrator sign-in", ppwsz);
        default:
            return E_NOTIMPL;
    }
}

IFACEMETHODIMP KLoginCredential::GetBitmapValue(DWORD, HBITMAP*) { return E_NOTIMPL; }
IFACEMETHODIMP KLoginCredential::GetCheckboxValue(DWORD, BOOL*, LPWSTR*) { return E_NOTIMPL; }

IFACEMETHODIMP KLoginCredential::GetComboBoxValueCount(DWORD dwFieldID, DWORD* pcItems, DWORD* pdwSelectedItem) {
    if (dwFieldID != FID_MAPPING || !pcItems || !pdwSelectedItem) {
        return E_INVALIDARG;
    }
    // Winlogon rejects combobox fields with zero items during enumeration.
    const DWORD count = _options.empty() ? 1 : static_cast<DWORD>(_options.size());
    *pcItems = count;
    *pdwSelectedItem = _selectedMapping < count ? _selectedMapping : 0;
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, LPWSTR* ppwszItem) {
    if (dwFieldID != FID_MAPPING || !ppwszItem) {
        return E_INVALIDARG;
    }
    if (_options.empty()) {
        if (dwItem != 0) {
            return E_INVALIDARG;
        }
        return SHStrDupW(L"", ppwszItem);
    }
    if (dwItem >= _options.size()) {
        return E_INVALIDARG;
    }
    const auto& option = _options[dwItem];
    const std::wstring label = option.label.empty() ? option.displayName : option.label;
    return SHStrDupW(label.c_str(), ppwszItem);
}

IFACEMETHODIMP KLoginCredential::GetSubmitButtonValue(DWORD dwFieldID, DWORD* pdwAdjacentTo) {
    if (dwFieldID != FID_SUBMIT || !pdwAdjacentTo) {
        return E_INVALIDARG;
    }
    if (_stage == Stage::Emergency) {
        *pdwAdjacentTo = FID_LOCAL_PASS;
    } else if (_stage == Stage::Select) {
        *pdwAdjacentTo = FID_MAPPING;
    } else {
        *pdwAdjacentTo = FID_PASSWORD;
    }
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::SetStringValue(DWORD dwFieldID, LPCWSTR pwz) {
    if (!pwz) {
        return E_INVALIDARG;
    }
    if (dwFieldID == FID_USERNAME) {
        _username = pwz;
        return S_OK;
    }
    if (dwFieldID == FID_PASSWORD) {
        _password = pwz;
        return S_OK;
    }
    if (dwFieldID == FID_LOCAL_USER) {
        _localUser = pwz;
        return S_OK;
    }
    if (dwFieldID == FID_LOCAL_PASS) {
        _localPassword = pwz;
        return S_OK;
    }
    return E_INVALIDARG;
}

IFACEMETHODIMP KLoginCredential::SetCheckboxValue(DWORD, BOOL) { return E_NOTIMPL; }

IFACEMETHODIMP KLoginCredential::SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem) {
    if (dwFieldID != FID_MAPPING) {
        return E_INVALIDARG;
    }
    if (_options.empty()) {
        if (dwSelectedItem == 0) {
            _selectedMapping = 0;
            return S_OK;
        }
        return E_INVALIDARG;
    }
    if (dwSelectedItem >= _options.size()) {
        return E_INVALIDARG;
    }
    _selectedMapping = dwSelectedItem;
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::CommandLinkClicked(DWORD dwFieldID) {
    if (dwFieldID == FID_EMERGENCY_LINK) {
        _stage = Stage::Emergency;
        return UpdateFields();
    }
    return E_NOTIMPL;
}

HRESULT KLoginCredential::UpdateFields() {
    if (!_pEvents) {
        return S_OK;
    }
    for (DWORD i = 0; i < FID_COUNT; ++i) {
        CREDENTIAL_PROVIDER_FIELD_STATE cpfs{};
        CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis{};
        if (SUCCEEDED(GetFieldState(i, &cpfs, &cpfis))) {
            _pEvents->SetFieldState(this, i, cpfs);
            _pEvents->SetFieldInteractiveState(this, i, cpfis);
        }
    }
    return S_OK;
}

HRESULT KLoginCredential::PerformLogin() {
    const auto result = KLogin::SendLoginRequest(_username, _password);
    if (result.status == KLogin::LoginPipeStatus::Failed) {
        _statusText = result.error;
        return E_FAIL;
    }

    if (result.status == KLogin::LoginPipeStatus::SelectAccount) {
        _stage = Stage::Select;
        _token = result.token;
        _options = result.options;
        _selectedMapping = 0;
        return UpdateFields();
    }

    _winUser = result.windowsUsername;
    _winDomain = result.domain.empty() ? L"." : result.domain;
    _winPassword = result.windowsPassword;
    _stage = Stage::Ready;
    return S_OK;
}

HRESULT KLoginCredential::PerformSelect() {
    if (_options.empty()) {
        return E_FAIL;
    }
    const int mappingId = _options[_selectedMapping].mappingId;
    const auto result = KLogin::SendSelectRequest(_token, mappingId);
    if (result.status != KLogin::LoginPipeStatus::Success) {
        _statusText = result.error;
        return E_FAIL;
    }
    _winUser = result.windowsUsername;
    _winDomain = result.domain.empty() ? L"." : result.domain;
    _winPassword = result.windowsPassword;
    _stage = Stage::Ready;
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::GetSerialization(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
    LPWSTR* ppwszOptionalStatusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) {
    if (!pcpgsr || !pcpcs) {
        return E_INVALIDARG;
    }

    *pcpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;

    if (_stage == Stage::Emergency) {
        if (_localUser.empty() || _localPassword.empty()) {
            if (ppwszOptionalStatusText) {
                SHStrDupW(L"Enter local username and password", ppwszOptionalStatusText);
            }
            if (pcpsiOptionalStatusIcon) {
                *pcpsiOptionalStatusIcon = CPSI_WARNING;
            }
            return S_OK;
        }
        _winUser = _localUser;
        _winDomain = L".";
        _winPassword = _localPassword;
        _stage = Stage::Ready;
    } else if (_stage == Stage::Login) {
        if (FAILED(PerformLogin())) {
            if (ppwszOptionalStatusText) {
                SHStrDupW(_statusText.c_str(), ppwszOptionalStatusText);
            }
            if (pcpsiOptionalStatusIcon) {
                *pcpsiOptionalStatusIcon = CPSI_ERROR;
            }
            return S_OK;
        }
        if (_stage == Stage::Select) {
            return S_OK;
        }
    }

    if (_stage == Stage::Select) {
        if (FAILED(PerformSelect())) {
            if (ppwszOptionalStatusText) {
                SHStrDupW(_statusText.c_str(), ppwszOptionalStatusText);
            }
            if (pcpsiOptionalStatusIcon) {
                *pcpsiOptionalStatusIcon = CPSI_ERROR;
            }
            return S_OK;
        }
    }

    if (_stage != Stage::Ready) {
        return S_OK;
    }

    DWORD authPackage = 0;
    BYTE* rgb = nullptr;
    DWORD cb = 0;
    const HRESULT hr = KLogin::PackPasswordLogon(_winDomain, _winUser, _winPassword, _cpus, &authPackage, &rgb, &cb);
    if (FAILED(hr)) {
        return hr;
    }

    pcpcs->rgbSerialization = rgb;
    pcpcs->cbSerialization = cb;
    pcpcs->ulAuthenticationPackage = authPackage;
    pcpcs->clsidCredentialProvider = CLSID_KLoginCredentialProvider;
    *pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
    KLogin::SecureZeroWide(_winPassword);
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::ReportResult(NTSTATUS, NTSTATUS, LPWSTR* ppwszOptionalStatusText, CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) {
    if (ppwszOptionalStatusText) {
        *ppwszOptionalStatusText = nullptr;
    }
    if (pcpsiOptionalStatusIcon) {
        *pcpsiOptionalStatusIcon = CPSI_NONE;
    }
    return S_OK;
}

IFACEMETHODIMP KLoginCredential::GetUserSid(LPWSTR* ppwszSid) {
    if (!ppwszSid) {
        return E_INVALIDARG;
    }
    // Not tied to a specific Windows user — show in Sign-in options for any selected account.
    *ppwszSid = nullptr;
    return S_FALSE;
}
