#pragma once

#include <string>
#include <vector>

namespace KLogin {

enum class LoginPipeStatus {
    Success,
    SelectAccount,
    Failed,
};

struct MappingOption {
    int mappingId = 0;
    std::wstring windowsUsername;
    std::wstring displayName;
    std::wstring label;
};

struct LoginPipeResult {
    LoginPipeStatus status = LoginPipeStatus::Failed;
    std::wstring error;
    std::wstring token;
    std::vector<MappingOption> options;
    std::wstring windowsUsername;
    std::wstring displayName;
    std::wstring domain;
    std::wstring windowsPassword;
};

struct PipeResponse {
    bool success = false;
    std::wstring error;
    std::string dataJson;
};

PipeResponse SendPipeRequest(const std::string& requestJson, const std::wstring& pipeName = L"klogin-agent");
LoginPipeResult SendLoginRequest(const std::wstring& username, const std::wstring& password);
LoginPipeResult SendSelectRequest(const std::wstring& token, int mappingId);

}  // namespace KLogin
