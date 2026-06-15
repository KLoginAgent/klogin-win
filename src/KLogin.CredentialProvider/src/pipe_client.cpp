#include "pipe_client.h"

#include "helpers.h"

#include <string>

namespace KLogin {

PipeResponse SendPipeRequest(const std::string& requestJson, const std::wstring& pipeName) {
    PipeResponse response{};

    const std::wstring path = L"\\\\.\\pipe\\" + pipeName;
    HANDLE pipe = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (pipe == INVALID_HANDLE_VALUE) {
        response.error = L"Could not connect to KLogin Agent";
        return response;
    }

    std::string payload = requestJson;
    payload.push_back('\n');
    DWORD written = 0;
    if (!WriteFile(pipe, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr)) {
        CloseHandle(pipe);
        response.error = L"Failed to write pipe request";
        return response;
    }

    char buffer[16384]{};
    DWORD read = 0;
    if (!ReadFile(pipe, buffer, sizeof(buffer) - 1, &read, nullptr)) {
        CloseHandle(pipe);
        response.error = L"Failed to read pipe response";
        return response;
    }

    CloseHandle(pipe);
    buffer[read] = '\0';
    response.success = true;
    response.dataJson.assign(buffer, read);
    return response;
}

static std::wstring BuildLoginJson(const std::wstring& username, const std::wstring& password) {
    return L"{\"action\":\"login\",\"username\":\"" + username + L"\",\"password\":\"" + password + L"\"}";
}

static std::wstring BuildSelectJson(const std::wstring& token, int mappingId) {
    return L"{\"action\":\"select\",\"token\":\"" + token + L"\",\"mappingId\":" + std::to_wstring(mappingId) + L"}";
}

static std::wstring ExtractObject(const std::wstring& json, const std::wstring& key) {
    const size_t keyPos = json.find(L"\"" + key + L"\"");
    if (keyPos == std::wstring::npos) {
        return json;
    }
    const size_t start = json.find(L'{', keyPos);
    if (start == std::wstring::npos) {
        return json;
    }
    int depth = 0;
    for (size_t i = start; i < json.size(); ++i) {
        if (json[i] == L'{') {
            ++depth;
        } else if (json[i] == L'}') {
            --depth;
            if (depth == 0) {
                return json.substr(start, i - start + 1);
            }
        }
    }
    return json;
}

static void ApplyCredentials(const std::wstring& json, LoginPipeResult& result) {
    const std::wstring credentials = ExtractObject(json, L"credentials");
    result.windowsUsername = JsonGetString(credentials, L"windowsUsername");
    result.displayName = JsonGetString(credentials, L"displayName");
    result.domain = JsonGetString(credentials, L"domain");
    result.windowsPassword = JsonGetString(credentials, L"windowsPassword");
}

LoginPipeResult SendLoginRequest(const std::wstring& username, const std::wstring& password) {
    LoginPipeResult result{};
    const PipeResponse response = SendPipeRequest(WideToUtf8(BuildLoginJson(username, password)));
    if (!response.success) {
        result.error = response.error.empty() ? L"Agent communication failed" : response.error;
        return result;
    }

    const std::wstring json = Utf8ToWide(response.dataJson);
    if (!JsonGetBool(json, L"success")) {
        result.error = JsonGetString(json, L"error");
        if (result.error.empty()) {
            result.error = L"Login failed";
        }
        return result;
    }

    const std::wstring data = ExtractObject(json, L"data");
    const std::wstring status = JsonGetString(data, L"status");
    if (status == L"success") {
        result.status = LoginPipeStatus::Success;
        ApplyCredentials(data, result);
        return result;
    }

    if (status == L"select") {
        result.status = LoginPipeStatus::SelectAccount;
        result.token = JsonGetString(data, L"token");
        size_t pos = 0;
        while ((pos = data.find(L"\"mappingId\"", pos)) != std::wstring::npos) {
            MappingOption option{};
            const auto slice = data.substr(pos, 400);
            option.mappingId = JsonGetInt(slice, L"mappingId");
            option.windowsUsername = JsonGetString(slice, L"windowsUsername");
            option.displayName = JsonGetString(slice, L"displayName");
            option.label = JsonGetString(slice, L"label");
            result.options.push_back(option);
            pos += 12;
        }
        return result;
    }

    result.error = L"Unexpected agent response";
    return result;
}

LoginPipeResult SendSelectRequest(const std::wstring& token, int mappingId) {
    LoginPipeResult result{};
    const PipeResponse response = SendPipeRequest(WideToUtf8(BuildSelectJson(token, mappingId)));
    if (!response.success) {
        result.error = response.error.empty() ? L"Agent communication failed" : response.error;
        return result;
    }

    const std::wstring json = Utf8ToWide(response.dataJson);
    if (!JsonGetBool(json, L"success")) {
        result.error = JsonGetString(json, L"error");
        if (result.error.empty()) {
            result.error = L"Select failed";
        }
        return result;
    }

    const std::wstring data = ExtractObject(json, L"data");
    result.status = LoginPipeStatus::Success;
    ApplyCredentials(data, result);
    return result;
}

}  // namespace KLogin
