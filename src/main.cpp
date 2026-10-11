#include "utils.hpp"

static const wchar_t *kConfigName = L"launch_data.of";

int main() {
	wchar_t selfBuf[32768];
	DWORD len = GetModuleFileNameW(nullptr, selfBuf, 32768);
	if (!len)
		failf(L"Could not determine launcher location");

	fs::path selfPath(wstring(selfBuf, len));
	launcherName = selfPath.filename().wstring();
	wstring dir = selfPath.parent_path().wstring();

	std::vector<wstring> lines;
	try {
		lines = readLines(dir + L"\\" + kConfigName);
	} catch (const wstring &e) {
		failf(wstring(L"Could not read ") + kConfigName + L":\n" + e);
	}
	if (lines.empty())
		failf(wstring(kConfigName) + L" is empty");

	for (size_t i = 1; i < lines.size(); i++) {
		wstring line = trimSpace(lines[i]);
		if (line.empty())
			continue;
		try {
			verifyLine(dir, line);
		} catch (const wstring &e) {
			failf(wstring(kConfigName) + L" line " + std::to_wstring(i + 1) +
			      L": " + e);
		}
	}

	wstring exe, args;
	try {
		splitCommand(lines[0], exe, args);
	} catch (const wstring &e) {
		failf(wstring(kConfigName) + L" line 1: " + e);
	}

	exe = resolve(dir, exe);
	if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
		failf(L"Executable not found:\n" + exe);

	wstring cmdline = L"\"" + exe + L"\"";
	if (!args.empty())
		cmdline += L" " + args;
	wstring exeDir = fs::path(exe).parent_path().wstring();

	std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
	cl.push_back(L'\0');

	STARTUPINFOW si = {};
	si.cb = sizeof si;
	PROCESS_INFORMATION pi = {};

	if (!CreateProcessW(exe.c_str(), cl.data(), nullptr, nullptr, FALSE, 0,
	                    nullptr, exeDir.c_str(), &si, &pi))
		failf(L"Failed to launch:\n" + cmdline + L"\n\n" +
		      winErrorText(GetLastError()));

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return 0;
}
