#include "config.hpp"
#include "inject.hpp"

static const wchar_t *kConfigName = L"launch_data.of";

int main() {
	wchar_t selfBuf[32768];
	DWORD len = GetModuleFileNameW(nullptr, selfBuf, 32768);
	if (!len)
		failf(L"Could not determine launcher location");

	wstring selfPath(selfBuf, len);
	launcherName = fileNameOf(selfPath);
	wstring dir = parentDirOf(selfPath);

	std::vector<wstring> lines;
	try {
		lines = readLines(dir + L"\\" + kConfigName);
	} catch (const wstring &e) {
		failf(wstring(L"Could not read ") + kConfigName + L":\n" + e);
	}
	if (lines.empty())
		failf(wstring(kConfigName) + L" is empty");

	LaunchConfig cfg;
	try {
		cfg = parseConfig(lines);
	} catch (const wstring &e) {
		failf(wstring(kConfigName) + L" " + e);
	}

	for (const ConfigEntry &entry : cfg.hashes) {
		try {
			verifyLine(dir, entry.value);
		} catch (const wstring &e) {
			failf(wstring(kConfigName) + L" line " +
			      std::to_wstring(entry.line) + L": " + e);
		}
	}

	wstring exe, args;
	try {
		splitCommand(cfg.command.value, exe, args);
	} catch (const wstring &e) {
		failf(wstring(kConfigName) + L" line 1: " + e);
	}

	exe = resolve(dir, exe);
	if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
		failf(L"Executable not found:\n" + exe);

	wstring cmdline = L"\"" + exe + L"\"";
	if (!args.empty())
		cmdline += L" " + args;
	wstring exeDir = parentDirOf(exe);

	std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
	cl.push_back(L'\0');

	STARTUPINFOW si = {};
	si.cb = sizeof si;
	PROCESS_INFORMATION pi = {};

	bool suspended = !cfg.injects.empty();
	DWORD flags = suspended ? CREATE_SUSPENDED : 0;

	if (!CreateProcessW(exe.c_str(), cl.data(), nullptr, nullptr, FALSE, flags,
	                    nullptr, exeDir.c_str(), &si, &pi))
		failf(L"Failed to launch:\n" + cmdline + L"\n\n" +
		      winErrorText(GetLastError()));

	for (const ConfigEntry &entry : cfg.injects) {
		wstring dll = resolve(dir, entry.value);
		try {
			injectDll(pi.hProcess, dll);
		} catch (const wstring &e) {
			if (cfg.abortOnInjectFailure) {
				TerminateProcess(pi.hProcess, 1);
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				failf(L"Injection failed, aborting launch.\n\n" + e);
			}
			warnf(L"Injection failed:\n\n" + e);
		}
	}

	if (suspended && ResumeThread(pi.hThread) == (DWORD)-1) {
		wstring err = winErrorText(GetLastError());
		TerminateProcess(pi.hProcess, 1);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		failf(L"Failed to resume the process:\n" + err);
	}

	if (cfg.wait)
		WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return 0;
}
