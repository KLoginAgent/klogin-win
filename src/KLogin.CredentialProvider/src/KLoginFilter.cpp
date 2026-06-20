#include "KLoginFilter.h"

#include "guid.h"

#include <initguid.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace {

// Microsoft Password Credential Provider
// {60b78e88-ead8-445c-9cfd-0b87f74ea6cd}
static const GUID CLSID_PasswordProvider = {
    0x60b78e88, 0xead8, 0x445c, {0x9c, 0xfd, 0x0b, 0x87, 0xf7, 0x4e, 0xa6, 0xcd}};

// PIN Logon / Windows Hello PIN
// {918E0FFA-C4C4-4D4D-BD58-FA4A0E2DD0A6}
static const GUID CLSID_PinLogon = {
    0x918e0ffa, 0xc4c4, 0x4d4d, {0xbd, 0x58, 0xfa, 0x4a, 0x0e, 0x2d, 0xd0, 0xa6}};

// Windows Hello (NGC)
// {C27B358A-41A2-4718-AC20-FC995E902D98}
static const GUID CLSID_NgcProvider = {
    0xc27b358a, 0x41a2, 0x4718, {0xac, 0x20, 0xfc, 0x99, 0x5e, 0x90, 0x2d, 0x98}};

// Facial / biometric recognition provider
// {FB259757-F8AC-4E63-AF05-8C0D90F0E9E7}
static const GUID CLSID_BioProvider = {
    0xfb259757, 0xf8ac, 0x4e63, {0xaf, 0x05, 0x8c, 0x0d, 0x90, 0xf0, 0xe9, 0xe7}};

bool IsKnownSystemProvider(const GUID& clsid) {
    return IsEqualGUID(clsid, CLSID_PasswordProvider) ||
           IsEqualGUID(clsid, CLSID_PinLogon) ||
           IsEqualGUID(clsid, CLSID_NgcProvider) ||
           IsEqualGUID(clsid, CLSID_BioProvider);
}

bool ShowAllCredentialProviders() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\KLoginAgent",
        L"ShowAllCredentialProviders",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return status == ERROR_SUCCESS && value != 0;
}

}  // namespace

KLoginFilter::KLoginFilter() = default;
KLoginFilter::~KLoginFilter() = default;

IFACEMETHODIMP KLoginFilter::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(KLoginFilter, ICredentialProviderFilter),
        {0},
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) KLoginFilter::AddRef() { return InterlockedIncrement(&_cRef); }

IFACEMETHODIMP_(ULONG) KLoginFilter::Release() {
    const LONG cRef = InterlockedDecrement(&_cRef);
    if (!cRef) {
        delete this;
    }
    return cRef;
}

IFACEMETHODIMP KLoginFilter::Filter(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD dwFlags,
    GUID* rgclsidProviders,
    BOOL* rgbAllow,
    DWORD cProviders) {
    UNREFERENCED_PARAMETER(dwFlags);

    if (!rgclsidProviders || !rgbAllow) {
        return E_INVALIDARG;
    }

    if (cpus != CPUS_LOGON && cpus != CPUS_UNLOCK_WORKSTATION) {
        return S_OK;
    }

    if (ShowAllCredentialProviders()) {
        return S_OK;
    }

    for (DWORD i = 0; i < cProviders; ++i) {
        if (IsEqualGUID(rgclsidProviders[i], CLSID_KLoginCredentialProvider)) {
            rgbAllow[i] = TRUE;
            continue;
        }
        if (IsKnownSystemProvider(rgclsidProviders[i])) {
            rgbAllow[i] = FALSE;
        }
    }

    return S_OK;
}

IFACEMETHODIMP KLoginFilter::UpdateRemoteCredential(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcsIn,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcsOut) {
    UNREFERENCED_PARAMETER(pcpcsIn);
    UNREFERENCED_PARAMETER(pcpcsOut);
    return E_NOTIMPL;
}
