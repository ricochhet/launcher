#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), len);
    return w;
}

static std::wstring Trim(const std::wstring& s) {
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b])) ++b;
    while (e > b && iswspace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

static void Fail(const std::wstring& msg) {
    MessageBoxW(nullptr, msg.c_str(), L"Launcher", MB_OK | MB_ICONERROR);
}

int main() {
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
    fs::path baseDir = fs::path(selfPath).parent_path();
    fs::path dataFile = baseDir / L"launch_data.of";

    std::ifstream in(dataFile, std::ios::binary);
    if (!in) {
        Fail(L"Could not open:\n" + dataFile.wstring());
        return 1;
    }

    std::wstring line;
    std::string raw;
    while (std::getline(in, raw)) {
        if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
            raw.erase(0, 3);
        line = Trim(Utf8ToWide(raw));
        if (!line.empty()) break;
    }
    if (line.empty()) {
        Fail(L"launch_data.of is empty.");
        return 1;
    }

    std::wstring exeStr, args;
    if (line[0] == L'"') {
        size_t close = line.find(L'"', 1);
        if (close == std::wstring::npos) {
            Fail(L"Unterminated quote in launch_data.of.");
            return 1;
        }
        exeStr = line.substr(1, close - 1);
        args = Trim(line.substr(close + 1));
    } else {
        std::wstring lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
        size_t split = lower.find(L".exe");
        split = (split != std::wstring::npos) ? split + 4 : line.find_first_of(L" \t");
        if (split == std::wstring::npos) split = line.size();
        exeStr = line.substr(0, split);
        args = Trim(line.substr(split));
    }

    fs::path exe = fs::path(exeStr);
    if (exe.is_relative()) exe = baseDir / exe;
    exe = exe.lexically_normal();

    if (!fs::exists(exe)) {
        Fail(L"Executable not found:\n" + exe.wstring());
        return 1;
    }

    std::wstring cmd = L"\"" + exe.wstring() + L"\"";
    if (!args.empty()) cmd += L" " + args;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring workDir = exe.parent_path().wstring();

    BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, workDir.c_str(), &si, &pi);
    if (!ok) {
        Fail(L"Failed to launch (error " + std::to_wstring(GetLastError()) + L"):\n" + cmd);
        return 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
