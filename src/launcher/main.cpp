#include <windows.h>
#include <shellapi.h>

#include <string>

namespace {
std::wstring quoteArg(const std::wstring& arg)
{
    if (arg.empty()) {
        return L"\"\"";
    }

    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }

    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }

        if (ch == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
            continue;
        }

        if (backslashes > 0) {
            result.append(backslashes, L'\\');
            backslashes = 0;
        }
        result.push_back(ch);
    }

    if (backslashes > 0) {
        result.append(backslashes * 2, L'\\');
    }
    result.push_back(L'"');
    return result;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t launcherPathBuffer[MAX_PATH] = {0};
    const DWORD launcherPathLen = GetModuleFileNameW(nullptr, launcherPathBuffer, MAX_PATH);
    if (launcherPathLen == 0 || launcherPathLen >= MAX_PATH) {
        MessageBoxW(nullptr, L"Failed to resolve launcher path.", L"FLiNG Downloader", MB_ICONERROR);
        return 1;
    }

    std::wstring launcherPath(launcherPathBuffer);
    const size_t lastSeparator = launcherPath.find_last_of(L"\\/");
    const std::wstring baseDir = (lastSeparator == std::wstring::npos) ? L"." : launcherPath.substr(0, lastSeparator);
    const std::wstring appDir = baseDir + L"\\app";
    const std::wstring appExe = appDir + L"\\FLiNG Downloader.exe";

    if (GetFileAttributesW(appExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(nullptr, L"Cannot find app\\FLiNG Downloader.exe", L"FLiNG Downloader", MB_ICONERROR);
        return 1;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        MessageBoxW(nullptr, L"Failed to parse command-line arguments.", L"FLiNG Downloader", MB_ICONERROR);
        return 1;
    }

    std::wstring commandLine = quoteArg(appExe);
    for (int i = 1; i < argc; ++i) {
        commandLine += L" ";
        commandLine += quoteArg(argv[i]);
    }
    LocalFree(argv);

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};

    const BOOL ok = CreateProcessW(
        appExe.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        appDir.c_str(),
        &startupInfo,
        &processInfo);

    if (!ok) {
        MessageBoxW(nullptr, L"Failed to start app\\FLiNG Downloader.exe", L"FLiNG Downloader", MB_ICONERROR);
        return 1;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
        exitCode = 1;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return static_cast<int>(exitCode);
}
