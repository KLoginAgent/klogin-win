#include "KLoginProvider.h"

#include "KLoginCredential.h"

#include <new>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR s_fields[] = {
    { KLoginCredential::FID_LABEL, CPFT_LARGE_TEXT, L"Sign in with KLogin" },
    { KLoginCredential::FID_USERNAME, CPFT_EDIT_TEXT, L"KLogin username" },
    { KLoginCredential::FID_PASSWORD, CPFT_PASSWORD_TEXT, L"KLogin password" },
    { KLoginCredential::FID_MAPPING, CPFT_COMBOBOX, L"Windows account" },
    { KLoginCredential::FID_SUBMIT, CPFT_SUBMIT_BUTTON, L"Sign in" },
};

static const DWORD s_fieldCount = KLoginCredential::FID_COUNT;

KLoginProvider::KLoginProvider() = default;

KLoginProvider::~KLoginProvider() {
    if (_pCredential) {
        _pCredential->Release();
    }
}

IFACEMETHODIMP KLoginProvider::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(KLoginProvider, ICredentialProvider),
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

IFACEMETHODIMP KLoginProvider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD) {
    if (cpus != CPUS_LOGON && cpus != CPUS_UNLOCK_WORKSTATION) {
        return E_NOTIMPL;
    }
    _cpus = cpus;
    if (!_pCredential) {
        _pCredential = new (std::nothrow) KLoginCredential(_cpus);
        if (!_pCredential) {
            return E_OUTOFMEMORY;
        }
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

    copy->dwFieldID = s_fields[dwIndex].dwFieldID;
    copy->cpft = s_fields[dwIndex].cpft;
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
    return S_OK;
}

IFACEMETHODIMP KLoginProvider::GetCredentialAt(DWORD dwIndex, ICredentialProviderCredential** ppcpc) {
    if (dwIndex != 0 || !ppcpc || !_pCredential) {
        return E_INVALIDARG;
    }
    _pCredential->AddRef();
    *ppcpc = _pCredential;
    return S_OK;
}
