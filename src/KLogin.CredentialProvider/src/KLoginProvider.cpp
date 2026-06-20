#include "KLoginProvider.h"

#include "KLoginCredential.h"
#include "helpers.h"

#include <new>
#include <shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "shlwapi.lib")

namespace {

const GUID kGuidNull{};

static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR s_fields[] = {
    { KLoginCredential::FID_TILE, L"KLogin", CPFT_TILE_IMAGE, CPFG_CREDENTIAL_PROVIDER_LOGO },
    { KLoginCredential::FID_LABEL, L"Sign in with KLogin", CPFT_LARGE_TEXT, kGuidNull },
    { KLoginCredential::FID_USERNAME, L"KLogin username", CPFT_EDIT_TEXT, kGuidNull },
    { KLoginCredential::FID_PASSWORD, L"KLogin password", CPFT_PASSWORD_TEXT, kGuidNull },
    { KLoginCredential::FID_MAPPING, L"Windows account", CPFT_COMBOBOX, kGuidNull },
    { KLoginCredential::FID_SUBMIT, L"Sign in", CPFT_SUBMIT_BUTTON, kGuidNull },
    { KLoginCredential::FID_EMERGENCY_LINK, L"Emergency local administrator sign-in", CPFT_COMMAND_LINK, kGuidNull },
    { KLoginCredential::FID_LOCAL_USER, L"Local username", CPFT_EDIT_TEXT, kGuidNull },
    { KLoginCredential::FID_LOCAL_PASS, L"Local password", CPFT_PASSWORD_TEXT, kGuidNull },
};

static const DWORD s_fieldCount = KLoginCredential::FID_COUNT;

}  // namespace

KLoginProvider::KLoginProvider() = default;

KLoginProvider::~KLoginProvider() {
    if (_pCredential) {
        _pCredential->Release();
    }
    if (_pUserArray) {
        _pUserArray->Release();
    }
}

IFACEMETHODIMP KLoginProvider::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(KLoginProvider, ICredentialProvider),
        QITABENT(KLoginProvider, ICredentialProviderSetUserArray),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) KLoginProvider::AddRef() { return InterlockedIncrement(&_cRef); }

IFACEMETHODIMP_(ULONG) KLoginProvider::Release() {
    const LONG cRef = InterlockedDecrement(&_cRef);
    if (!cRef) {
        delete this;
    }
    return cRef;
}

void KLoginProvider::SyncTargetUserSid() {
    _userSid.clear();
    if (!_pUserArray) {
        KLogin::LogCp(L"SetUserArray: no user array");
        if (_pCredential) {
            _pCredential->SetTargetUserSid(_userSid);
        }
        return;
    }

    DWORD count = 0;
    if (FAILED(_pUserArray->GetUserCount(&count))) {
        KLogin::LogCp(L"SetUserArray: GetUserCount failed");
        if (_pCredential) {
            _pCredential->SetTargetUserSid(_userSid);
        }
        return;
    }

    wchar_t countLine[64]{};
    StringCchPrintfW(countLine, _countof(countLine), L"SetUserArray: %u user(s)", count);
    KLogin::LogCp(countLine);

    if (count == 0) {
        if (_pCredential) {
            _pCredential->SetTargetUserSid(_userSid);
        }
        return;
    }

    ICredentialProviderUser* pUser = nullptr;
    if (FAILED(_pUserArray->GetAt(0, &pUser)) || !pUser) {
        KLogin::LogCp(L"SetUserArray: GetAt(0) failed");
        if (_pCredential) {
            _pCredential->SetTargetUserSid(_userSid);
        }
        return;
    }

    LPWSTR sid = nullptr;
    if (SUCCEEDED(pUser->GetSid(&sid)) && sid) {
        _userSid = sid;
        CoTaskMemFree(sid);
        KLogin::LogCp(L"SetUserArray: cached target user SID");
    } else {
        KLogin::LogCp(L"SetUserArray: GetSid failed");
    }
    pUser->Release();

    if (_pCredential) {
        _pCredential->SetTargetUserSid(_userSid);
    }
}

IFACEMETHODIMP KLoginProvider::SetUserArray(ICredentialProviderUserArray* users) {
    if (_pUserArray) {
        _pUserArray->Release();
        _pUserArray = nullptr;
    }

    if (users) {
        users->AddRef();
        _pUserArray = users;
    }

    SyncTargetUserSid();
    return S_OK;
}

IFACEMETHODIMP KLoginProvider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD) {
    if (cpus != CPUS_LOGON && cpus != CPUS_UNLOCK_WORKSTATION) {
        KLogin::LogCp(L"SetUsageScenario: unsupported scenario");
        return E_NOTIMPL;
    }
    _cpus = cpus;
    KLogin::LogCp(cpus == CPUS_LOGON ? L"SetUsageScenario: CPUS_LOGON" : L"SetUsageScenario: CPUS_UNLOCK_WORKSTATION");
    if (!_pCredential) {
        _pCredential = new (std::nothrow) KLoginCredential(_cpus);
        if (!_pCredential) {
            KLogin::LogCp(L"SetUsageScenario: failed to allocate credential");
            return E_OUTOFMEMORY;
        }
        _pCredential->SetTargetUserSid(_userSid);
    }
    return S_OK;
}

IFACEMETHODIMP KLoginProvider::SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION*) {
    return E_NOTIMPL;
}

IFACEMETHODIMP KLoginProvider::Advise(ICredentialProviderEvents*, UINT_PTR) { return S_OK; }
IFACEMETHODIMP KLoginProvider::UnAdvise() { return S_OK; }

IFACEMETHODIMP KLoginProvider::GetFieldDescriptorCount(DWORD* pdwCount) {
    if (!pdwCount) {
        return E_INVALIDARG;
    }
    *pdwCount = s_fieldCount;
    return S_OK;
}

IFACEMETHODIMP KLoginProvider::GetFieldDescriptorAt(DWORD dwIndex, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd) {
    if (!ppcpfd || dwIndex >= s_fieldCount) {
        return E_INVALIDARG;
    }

    auto* copy = static_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(
        CoTaskMemAlloc(sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR)));
    if (!copy) {
        return E_OUTOFMEMORY;
    }

    ZeroMemory(copy, sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR));
    copy->dwFieldID = s_fields[dwIndex].dwFieldID;
    copy->cpft = s_fields[dwIndex].cpft;
    copy->guidFieldType = s_fields[dwIndex].guidFieldType;
    if (FAILED(SHStrDupW(s_fields[dwIndex].pszLabel, &copy->pszLabel))) {
        CoTaskMemFree(copy);
        return E_OUTOFMEMORY;
    }

    *ppcpfd = copy;
    return S_OK;
}

IFACEMETHODIMP KLoginProvider::GetCredentialCount(DWORD* pdwCount, DWORD* pdwDefault, BOOL* pbAutoLogonWithDefault) {
    if (!pdwCount || !pdwDefault || !pbAutoLogonWithDefault) {
        return E_INVALIDARG;
    }
    *pdwCount = 1;
    *pdwDefault = 0;
    *pbAutoLogonWithDefault = FALSE;
    KLogin::LogCp(L"GetCredentialCount: returning 1 tile");
    return S_OK;
}

IFACEMETHODIMP KLoginProvider::GetCredentialAt(DWORD dwIndex, ICredentialProviderCredential** ppcpc) {
    if (dwIndex != 0 || !ppcpc) {
        return E_INVALIDARG;
    }
    if (!_pCredential) {
        const auto scenario = _cpus != CPUS_INVALID ? _cpus : CPUS_LOGON;
        _pCredential = new (std::nothrow) KLoginCredential(scenario);
        if (!_pCredential) {
            KLogin::LogCp(L"GetCredentialAt: failed to allocate credential");
            return E_OUTOFMEMORY;
        }
        _pCredential->SetTargetUserSid(_userSid);
    }
    _pCredential->AddRef();
    *ppcpc = _pCredential;
    KLogin::LogCp(L"GetCredentialAt: returning credential tile");
    return S_OK;
}
