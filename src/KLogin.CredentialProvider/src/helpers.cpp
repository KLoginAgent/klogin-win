#include "helpers.h"

#include <credentialprovider.h>
#include <ntsecapi.h>
#include <vector>

#pragma comment(lib, "secur32.lib")

namespace KLogin {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], size);
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

static size_t FindKey(const std::wstring& json, const std::wstring& key) {
    return json.find(L"\"" + key + L"\"");
}

std::wstring JsonGetString(const std::wstring& json, const std::wstring& key) {
    const size_t keyPos = FindKey(json, key);
    if (keyPos == std::wstring::npos) {
        return L"";
    }
    const size_t colon = json.find(L':', keyPos);
    const size_t startQuote = json.find(L'"', colon);
    const size_t endQuote = json.find(L'"', startQuote + 1);
    if (colon == std::wstring::npos || startQuote == std::wstring::npos || endQuote == std::wstring::npos) {
        return L"";
    }
    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

int JsonGetInt(const std::wstring& json, const std::wstring& key) {
    const size_t keyPos = FindKey(json, key);
    if (keyPos == std::wstring::npos) {
        return 0;
    }
    const size_t colon = json.find(L':', keyPos);
    if (colon == std::wstring::npos) {
        return 0;
    }
    size_t start = colon + 1;
    while (start < json.size() && iswspace(json[start])) {
        ++start;
    }
    return _wtoi(json.c_str() + start);
}

bool JsonGetBool(const std::wstring& json, const std::wstring& key) {
    const size_t keyPos = FindKey(json, key);
    if (keyPos == std::wstring::npos) {
        return false;
    }
    const size_t colon = json.find(L':', keyPos);
    return colon != std::wstring::npos && json.find(L"true", colon) != std::wstring::npos;
}

void SecureZeroWide(std::wstring& value) {
    if (!value.empty()) {
        SecureZeroMemory(&value[0], value.size() * sizeof(wchar_t));
        value.clear();
    }
}

static HRESULT CopyUnicodeString(UNICODE_STRING& dest, PCWSTR source) {
    const USHORT byteLen = static_cast<USHORT>(wcslen(source) * sizeof(wchar_t));
    dest.Length = byteLen;
    dest.MaximumLength = byteLen + sizeof(wchar_t);
    dest.Buffer = static_cast<PWSTR>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dest.MaximumLength));
    if (!dest.Buffer) {
        return E_OUTOFMEMORY;
    }
    CopyMemory(dest.Buffer, source, byteLen);
    return S_OK;
}

static HRESULT RetrieveKerbAuthPackage(ULONG* pulAuthPackage) {
    HANDLE hLsa = nullptr;
    NTSTATUS status = LsaConnectUntrusted(&hLsa);
    if (FAILED(HRESULT_FROM_NT(status))) {
        return HRESULT_FROM_NT(status);
    }

    LSA_STRING packageName{};
    const char kerb[] = "Kerberos";
    packageName.Buffer = const_cast<PCHAR>(kerb);
    packageName.Length = static_cast<USHORT>(strlen(kerb));
    packageName.MaximumLength = packageName.Length + 1;

    status = LsaLookupAuthenticationPackage(hLsa, &packageName, pulAuthPackage);
    LsaDeregisterLogonProcess(hLsa);
    return FAILED(HRESULT_FROM_NT(status)) ? HRESULT_FROM_NT(status) : S_OK;
}

HRESULT PackPasswordLogon(
    const std::wstring& domain,
    const std::wstring& username,
    const std::wstring& password,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD* pulAuthPackage,
    BYTE** rgbSerialized,
    DWORD* cbSerialized) {
    if (!rgbSerialized || !cbSerialized || !pulAuthPackage) {
        return E_INVALIDARG;
    }

    KERB_INTERACTIVE_UNLOCK_LOGON kiul{};
    kiul.Logon.MessageType = (cpus == CPUS_UNLOCK_WORKSTATION) ? KerbWorkstationUnlockLogon : KerbInteractiveLogon;

    HRESULT hr = CopyUnicodeString(kiul.Logon.LogonDomainName, domain.c_str());
    if (FAILED(hr)) {
        return hr;
    }
    hr = CopyUnicodeString(kiul.Logon.UserName, username.c_str());
    if (FAILED(hr)) {
        goto cleanupStrings;
    }
    hr = CopyUnicodeString(kiul.Logon.Password, password.c_str());
    if (FAILED(hr)) {
        goto cleanupStrings;
    }

    const DWORD cbHeader = sizeof(KERB_INTERACTIVE_UNLOCK_LOGON);
    const DWORD cbDomain = kiul.Logon.LogonDomainName.MaximumLength;
    const DWORD cbUser = kiul.Logon.UserName.MaximumLength;
    const DWORD cbPassword = kiul.Logon.Password.MaximumLength;
    const DWORD cbTotal = cbHeader + cbDomain + cbUser + cbPassword;

    auto* buffer = static_cast<BYTE*>(CoTaskMemAlloc(cbTotal));
    if (!buffer) {
        hr = E_OUTOFMEMORY;
        goto cleanupStrings;
    }
    ZeroMemory(buffer, cbTotal);

    auto* packed = reinterpret_cast<KERB_INTERACTIVE_UNLOCK_LOGON*>(buffer);
    packed->Logon.MessageType = kiul.Logon.MessageType;

    BYTE* cursor = buffer + cbHeader;
    CopyMemory(cursor, kiul.Logon.LogonDomainName.Buffer, cbDomain);
    packed->Logon.LogonDomainName.Buffer = reinterpret_cast<PWSTR>(cursor - buffer);
    packed->Logon.LogonDomainName.Length = kiul.Logon.LogonDomainName.Length;
    packed->Logon.LogonDomainName.MaximumLength = kiul.Logon.LogonDomainName.MaximumLength;
    cursor += cbDomain;

    CopyMemory(cursor, kiul.Logon.UserName.Buffer, cbUser);
    packed->Logon.UserName.Buffer = reinterpret_cast<PWSTR>(cursor - buffer);
    packed->Logon.UserName.Length = kiul.Logon.UserName.Length;
    packed->Logon.UserName.MaximumLength = kiul.Logon.UserName.MaximumLength;
    cursor += cbUser;

    CopyMemory(cursor, kiul.Logon.Password.Buffer, cbPassword);
    packed->Logon.Password.Buffer = reinterpret_cast<PWSTR>(cursor - buffer);
    packed->Logon.Password.Length = kiul.Logon.Password.Length;
    packed->Logon.Password.MaximumLength = kiul.Logon.Password.MaximumLength;

    ULONG authPackage = 0;
    hr = RetrieveKerbAuthPackage(&authPackage);
    if (FAILED(hr)) {
        CoTaskMemFree(buffer);
        goto cleanupStrings;
    }

    *rgbSerialized = buffer;
    *cbSerialized = cbTotal;
    *pulAuthPackage = authPackage;
    hr = S_OK;

cleanupStrings:
    if (kiul.Logon.LogonDomainName.Buffer) {
        HeapFree(GetProcessHeap(), 0, kiul.Logon.LogonDomainName.Buffer);
    }
    if (kiul.Logon.UserName.Buffer) {
        HeapFree(GetProcessHeap(), 0, kiul.Logon.UserName.Buffer);
    }
    if (kiul.Logon.Password.Buffer) {
        HeapFree(GetProcessHeap(), 0, kiul.Logon.Password.Buffer);
    }
    return hr;
}

}  // namespace KLogin
