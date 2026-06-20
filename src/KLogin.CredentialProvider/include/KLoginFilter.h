#pragma once

#include <credentialprovider.h>
#include <windows.h>

class KLoginFilter : public ICredentialProviderFilter {
public:
    KLoginFilter();
    virtual ~KLoginFilter();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Filter(
        CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
        DWORD dwFlags,
        GUID* rgclsidProviders,
        BOOL* rgbAllow,
        DWORD cProviders) override;

    IFACEMETHODIMP UpdateRemoteCredential(
        const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcsIn,
        CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcsOut) override;

private:
    long _cRef = 1;
};
