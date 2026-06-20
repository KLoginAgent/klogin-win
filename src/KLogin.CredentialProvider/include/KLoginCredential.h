#pragma once

#include <credentialprovider.h>
#include <windows.h>
#include <strsafe.h>

#include <string>
#include <vector>

#include "pipe_client.h"

class KLoginCredential : public ICredentialProviderCredential {
public:
    enum FieldId : DWORD {
        FID_LABEL = 0,
        FID_USERNAME,
        FID_PASSWORD,
        FID_MAPPING,
        FID_SUBMIT,
        FID_EMERGENCY_LINK,
        FID_LOCAL_USER,
        FID_LOCAL_PASS,
        FID_COUNT,
    };

    KLoginCredential(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);
    virtual ~KLoginCredential();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* pcpce) override;
    IFACEMETHODIMP UnAdvise() override;
    IFACEMETHODIMP SetSelected(BOOL* pbAutoLogon) override;
    IFACEMETHODIMP SetDeselected() override;
    IFACEMETHODIMP GetFieldState(DWORD dwFieldID, CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs, CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) override;
    IFACEMETHODIMP GetStringValue(DWORD dwFieldID, LPWSTR* ppwsz) override;
    IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp) override;
    IFACEMETHODIMP GetCheckboxValue(DWORD dwFieldID, BOOL* pbChecked, LPWSTR* ppwszLabel) override;
    IFACEMETHODIMP GetComboBoxValueCount(DWORD dwFieldID, DWORD* pcItems, DWORD* pdwSelectedItem) override;
    IFACEMETHODIMP GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, LPWSTR* ppwszItem) override;
    IFACEMETHODIMP GetSubmitButtonValue(DWORD dwFieldID, DWORD* pdwAdjacentTo) override;
    IFACEMETHODIMP SetStringValue(DWORD dwFieldID, LPCWSTR pwz) override;
    IFACEMETHODIMP SetCheckboxValue(DWORD dwFieldID, BOOL bChecked) override;
    IFACEMETHODIMP SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem) override;
    IFACEMETHODIMP CommandLinkClicked(DWORD dwFieldID) override;
    IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr, CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs, LPWSTR* ppwszOptionalStatusText, CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) override;
    IFACEMETHODIMP ReportResult(NTSTATUS ntsStatus, NTSTATUS ntsSubstatus, LPWSTR* ppwszOptionalStatusText, CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) override;

private:
    enum class Stage { Login, Select, Emergency, Ready };

    long _cRef = 1;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO _cpus;
    ICredentialProviderCredentialEvents* _pEvents = nullptr;
    Stage _stage = Stage::Login;
    std::wstring _username;
    std::wstring _password;
    std::wstring _token;
    std::vector<KLogin::MappingOption> _options;
    DWORD _selectedMapping = 0;
    std::wstring _winUser;
    std::wstring _winDomain;
    std::wstring _winPassword;
    std::wstring _localUser;
    std::wstring _localPassword;
    std::wstring _statusText;

    HRESULT UpdateFields();
    HRESULT PerformLogin();
    HRESULT PerformSelect();
};
