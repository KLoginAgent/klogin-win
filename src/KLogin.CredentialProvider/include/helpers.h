#pragma once

#include <credentialprovider.h>
#include <windows.h>
#include <string>

namespace KLogin {

std::wstring Utf8ToWide(const std::string& value);
std::string WideToUtf8(const std::wstring& value);
std::wstring JsonGetString(const std::wstring& json, const std::wstring& key);
int JsonGetInt(const std::wstring& json, const std::wstring& key);
bool JsonGetBool(const std::wstring& json, const std::wstring& key);

HRESULT PackPasswordLogon(
    const std::wstring& domain,
    const std::wstring& username,
    const std::wstring& password,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD* pulAuthPackage,
    BYTE** rgbSerialized,
    DWORD* cbSerialized);

void SecureZeroWide(std::wstring& value);
void LogCp(const wchar_t* message);

}  // namespace KLogin
