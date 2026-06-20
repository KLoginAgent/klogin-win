#include "helpers.h"

#include <credentialprovider.h>
#include <ntsecapi.h>
#include <strsafe.h>
#include <cwctype>
#include <vector>

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "gdi32.lib")

namespace KLogin {

void LogCp(const wchar_t* message) {
    if (!message) {
        return;
    }

    CreateDirectoryW(L"C:\\ProgramData\\KLogin", nullptr);

    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t line[512]{};
    StringCchPrintfW(
        line,
        _countof(line),
        L"[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        message);

    const HANDLE file = CreateFileW(
        L"C:\\ProgramData\\KLogin\\credential-provider.log",
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    const DWORD byteLen = static_cast<DWORD>(wcslen(line) * sizeof(wchar_t));
    WriteFile(file, line, byteLen, &written, nullptr);
    CloseHandle(file);
}

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
    if (colon == std::wstring::npos || startQuote == std::wstring::npos) {
        return L"";
    }

    std::wstring result;
    for (size_t i = startQuote + 1; i < json.size(); ++i) {
        const wchar_t ch = json[i];
        if (ch == L'"') {
            break;
        }
        if (ch == L'\\' && i + 1 < json.size()) {
            const wchar_t next = json[++i];
            switch (next) {
                case L'"':
                case L'\\':
                case L'/':
                    result += next;
                    break;
                case L'b':
                    result += L'\b';
                    break;
                case L'f':
                    result += L'\f';
                    break;
                case L'n':
                    result += L'\n';
                    break;
                case L'r':
                    result += L'\r';
                    break;
                case L't':
                    result += L'\t';
                    break;
                case L'u':
                    if (i + 4 < json.size()) {
                        wchar_t hex[5] = {
                            json[i + 1],
                            json[i + 2],
                            json[i + 3],
                            json[i + 4],
                            L'\0',
                        };
                        result += static_cast<wchar_t>(wcstol(hex, nullptr, 16));
                        i += 4;
                    }
                    break;
                default:
                    result += next;
                    break;
            }
            continue;
        }
        result += ch;
    }
    return result;
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

std::wstring NormalizeAccountName(const std::wstring& value) {
    if (value.empty()) {
        return L"";
    }
    const size_t slash = value.find_last_of(L"\\/");
    std::wstring name = slash == std::wstring::npos ? value : value.substr(slash + 1);
    for (auto& ch : name) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return name;
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

static HRESULT RetrieveMsv1AuthPackage(ULONG* pulAuthPackage) {
    HANDLE hLsa = nullptr;
    NTSTATUS status = LsaConnectUntrusted(&hLsa);
    if (FAILED(HRESULT_FROM_NT(status))) {
        return HRESULT_FROM_NT(status);
    }

    LSA_STRING packageName{};
    const char msv[] = MSV1_0_PACKAGE_NAME;
    packageName.Buffer = const_cast<PCHAR>(msv);
    packageName.Length = static_cast<USHORT>(strlen(msv));
    packageName.MaximumLength = packageName.Length + 1;

    status = LsaLookupAuthenticationPackage(hLsa, &packageName, pulAuthPackage);
    LsaDeregisterLogonProcess(hLsa);
    return FAILED(HRESULT_FROM_NT(status)) ? HRESULT_FROM_NT(status) : S_OK;
}

static bool IsLocalLogonDomain(const std::wstring& domain) {
    if (domain.empty() || domain == L".") {
        return true;
    }

    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(computerName, &size)) {
        return false;
    }
    return CompareStringOrdinal(domain.c_str(), -1, computerName, -1, TRUE) == CSTR_EQUAL;
}

static void FreeUnicodeString(UNICODE_STRING& value) {
    if (value.Buffer) {
        HeapFree(GetProcessHeap(), 0, value.Buffer);
        value.Buffer = nullptr;
    }
}

static HRESULT PackThreeStringLogon(
    DWORD cbHeader,
    const UNICODE_STRING& domain,
    const UNICODE_STRING& user,
    const UNICODE_STRING& password,
    void (*writeHeader)(BYTE* buffer, const UNICODE_STRING& domain, const UNICODE_STRING& user, const UNICODE_STRING& password),
    ULONG authPackage,
    BYTE** rgbSerialized,
    DWORD* cbSerialized) {
    const DWORD cbDomain = domain.MaximumLength;
    const DWORD cbUser = user.MaximumLength;
    const DWORD cbPassword = password.MaximumLength;
    const DWORD cbTotal = cbHeader + cbDomain + cbUser + cbPassword;

    auto* buffer = static_cast<BYTE*>(CoTaskMemAlloc(cbTotal));
    if (!buffer) {
        return E_OUTOFMEMORY;
    }
    ZeroMemory(buffer, cbTotal);

    writeHeader(buffer, domain, user, password);

    BYTE* cursor = buffer + cbHeader;
    CopyMemory(cursor, domain.Buffer, cbDomain);
    cursor += cbDomain;
    CopyMemory(cursor, user.Buffer, cbUser);
    cursor += cbUser;
    CopyMemory(cursor, password.Buffer, cbPassword);

    *rgbSerialized = buffer;
    *cbSerialized = cbTotal;
    return S_OK;
}

static void WriteKerbHeader(
    BYTE* buffer,
    const UNICODE_STRING& domain,
    const UNICODE_STRING& user,
    const UNICODE_STRING& password) {
    auto* packed = reinterpret_cast<KERB_INTERACTIVE_UNLOCK_LOGON*>(buffer);
    const DWORD cbHeader = sizeof(KERB_INTERACTIVE_UNLOCK_LOGON);

    BYTE* domainPtr = buffer + cbHeader;
    packed->Logon.LogonDomainName.Buffer = reinterpret_cast<PWSTR>(domainPtr - buffer);
    packed->Logon.LogonDomainName.Length = domain.Length;
    packed->Logon.LogonDomainName.MaximumLength = domain.MaximumLength;

    BYTE* userPtr = domainPtr + domain.MaximumLength;
    packed->Logon.UserName.Buffer = reinterpret_cast<PWSTR>(userPtr - buffer);
    packed->Logon.UserName.Length = user.Length;
    packed->Logon.UserName.MaximumLength = user.MaximumLength;

    BYTE* passwordPtr = userPtr + user.MaximumLength;
    packed->Logon.Password.Buffer = reinterpret_cast<PWSTR>(passwordPtr - buffer);
    packed->Logon.Password.Length = password.Length;
    packed->Logon.Password.MaximumLength = password.MaximumLength;
}

static void WriteMsv1Header(
    BYTE* buffer,
    const UNICODE_STRING& domain,
    const UNICODE_STRING& user,
    const UNICODE_STRING& password) {
    auto* packed = reinterpret_cast<MSV1_0_INTERACTIVE_LOGON*>(buffer);
    packed->MessageType = MsV1_0InteractiveLogon;
    const DWORD cbHeader = sizeof(MSV1_0_INTERACTIVE_LOGON);

    BYTE* domainPtr = buffer + cbHeader;
    packed->LogonDomainName.Buffer = reinterpret_cast<PWSTR>(domainPtr - buffer);
    packed->LogonDomainName.Length = domain.Length;
    packed->LogonDomainName.MaximumLength = domain.MaximumLength;

    BYTE* userPtr = domainPtr + domain.MaximumLength;
    packed->UserName.Buffer = reinterpret_cast<PWSTR>(userPtr - buffer);
    packed->UserName.Length = user.Length;
    packed->UserName.MaximumLength = user.MaximumLength;

    BYTE* passwordPtr = userPtr + user.MaximumLength;
    packed->Password.Buffer = reinterpret_cast<PWSTR>(passwordPtr - buffer);
    packed->Password.Length = password.Length;
    packed->Password.MaximumLength = password.MaximumLength;
}

static HRESULT PackKerbPasswordLogon(
    const std::wstring& domain,
    const std::wstring& username,
    const std::wstring& password,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD* pulAuthPackage,
    BYTE** rgbSerialized,
    DWORD* cbSerialized) {
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

    {
        ULONG authPackage = 0;
        hr = RetrieveKerbAuthPackage(&authPackage);
        if (FAILED(hr)) {
            goto cleanupStrings;
        }

        hr = PackThreeStringLogon(
            sizeof(KERB_INTERACTIVE_UNLOCK_LOGON),
            kiul.Logon.LogonDomainName,
            kiul.Logon.UserName,
            kiul.Logon.Password,
            WriteKerbHeader,
            authPackage,
            rgbSerialized,
            cbSerialized);
        if (SUCCEEDED(hr)) {
            auto* packed = reinterpret_cast<KERB_INTERACTIVE_UNLOCK_LOGON*>(*rgbSerialized);
            packed->Logon.MessageType = kiul.Logon.MessageType;
            *pulAuthPackage = authPackage;
            LogCp(cpus == CPUS_UNLOCK_WORKSTATION ? L"PackPasswordLogon: Kerberos unlock" : L"PackPasswordLogon: Kerberos interactive");
        }
    }

cleanupStrings:
    FreeUnicodeString(kiul.Logon.LogonDomainName);
    FreeUnicodeString(kiul.Logon.UserName);
    FreeUnicodeString(kiul.Logon.Password);
    return hr;
}

static HRESULT PackMsv1PasswordLogon(
    const std::wstring& domain,
    const std::wstring& username,
    const std::wstring& password,
    DWORD* pulAuthPackage,
    BYTE** rgbSerialized,
    DWORD* cbSerialized) {
    MSV1_0_INTERACTIVE_LOGON logon{};
    logon.MessageType = MsV1_0InteractiveLogon;

    const std::wstring packDomain = IsLocalLogonDomain(domain) ? L"" : domain;
    HRESULT hr = CopyUnicodeString(logon.LogonDomainName, packDomain.c_str());
    if (FAILED(hr)) {
        return hr;
    }
    hr = CopyUnicodeString(logon.UserName, username.c_str());
    if (FAILED(hr)) {
        goto cleanupStrings;
    }
    hr = CopyUnicodeString(logon.Password, password.c_str());
    if (FAILED(hr)) {
        goto cleanupStrings;
    }

    {
        ULONG authPackage = 0;
        hr = RetrieveMsv1AuthPackage(&authPackage);
        if (FAILED(hr)) {
            goto cleanupStrings;
        }

        hr = PackThreeStringLogon(
            sizeof(MSV1_0_INTERACTIVE_LOGON),
            logon.LogonDomainName,
            logon.UserName,
            logon.Password,
            WriteMsv1Header,
            authPackage,
            rgbSerialized,
            cbSerialized);
        if (SUCCEEDED(hr)) {
            *pulAuthPackage = authPackage;
            LogCp(L"PackPasswordLogon: MSV1_0 interactive (local account)");
        }
    }

cleanupStrings:
    FreeUnicodeString(logon.LogonDomainName);
    FreeUnicodeString(logon.UserName);
    FreeUnicodeString(logon.Password);
    return hr;
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
    if (username.empty() || password.empty()) {
        LogCp(L"PackPasswordLogon: missing Windows username or password");
        return E_INVALIDARG;
    }

    if (cpus != CPUS_UNLOCK_WORKSTATION && IsLocalLogonDomain(domain)) {
        return PackMsv1PasswordLogon(domain, username, password, pulAuthPackage, rgbSerialized, cbSerialized);
    }

    return PackKerbPasswordLogon(domain, username, password, cpus, pulAuthPackage, rgbSerialized, cbSerialized);
}

HBITMAP CreateTileBitmap() {
    constexpr int kSize = 72;
    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) {
        return nullptr;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HBITMAP hbmp = CreateCompatibleBitmap(hdcScreen, kSize, kSize);
    if (!hbmp) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HGDIOBJ oldBitmap = SelectObject(hdcMem, hbmp);
    const RECT rect = {0, 0, kSize, kSize};
    HBRUSH fill = CreateSolidBrush(RGB(0, 103, 192));
    if (fill) {
        FillRect(hdcMem, &rect, fill);
        DeleteObject(fill);
    }

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 255));
    HFONT font = CreateFontW(
        36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (font) {
        HGDIOBJ oldFont = SelectObject(hdcMem, font);
        DrawTextW(hdcMem, L"K", 1, const_cast<LPRECT>(&rect), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdcMem, oldFont);
        DeleteObject(font);
    }

    SelectObject(hdcMem, oldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return hbmp;
}

}  // namespace KLogin
