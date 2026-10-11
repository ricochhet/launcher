#pragma once

#include "utils.hpp"

#ifndef LIST_MODULES_ALL
#define LIST_MODULES_ALL 0x03
#endif

namespace inject_detail {

using EnumModulesExFn = BOOL(WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD,
                                       DWORD);
using EnumModulesFn = BOOL(WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD);
using GetBaseNameFn = DWORD(WINAPI *)(HANDLE, HMODULE, LPWSTR, DWORD);

inline wstring fileName(const wstring &path) { return fileNameOf(path); }

inline bool moduleLoaded(HANDLE process, const wstring &name) {
	HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
	auto enumEx =
	    (EnumModulesExFn)(void *)GetProcAddress(k32, "K32EnumProcessModulesEx");
	auto enumPlain =
	    (EnumModulesFn)(void *)GetProcAddress(k32, "K32EnumProcessModules");
	auto baseName =
	    (GetBaseNameFn)(void *)GetProcAddress(k32, "K32GetModuleBaseNameW");
	if (!baseName || (!enumEx && !enumPlain))
		return false;

	std::vector<HMODULE> mods(64);
	for (int attempt = 0; attempt < 4; attempt++) {
		DWORD needed = 0;
		DWORD bytes = (DWORD)(mods.size() * sizeof(HMODULE));
		BOOL ok = enumEx ? enumEx(process, mods.data(), bytes, &needed,
		                          LIST_MODULES_ALL)
		                 : enumPlain(process, mods.data(), bytes, &needed);
		if (!ok)
			return false;
		if (needed > bytes) {
			mods.resize(needed / sizeof(HMODULE));
			continue;
		}

		wchar_t buf[MAX_PATH];
		for (DWORD i = 0; i < needed / sizeof(HMODULE); i++) {
			DWORD n = baseName(process, mods[i], buf, MAX_PATH);
			if (n && _wcsicmp(buf, name.c_str()) == 0)
				return true;
		}
		return false;
	}
	return false;
}

} // namespace inject_detail

inline void injectDll(HANDLE process, const wstring &dllPath) {
	if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
		throw L"DLL not found:\n" + dllPath;

	HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
	LPVOID loadLibrary = (LPVOID)GetProcAddress(k32, "LoadLibraryW");
	if (!loadLibrary)
		throw wstring(L"could not locate LoadLibraryW");

	SIZE_T size = (dllPath.size() + 1) * sizeof(wchar_t);
	LPVOID remote = VirtualAllocEx(process, nullptr, size,
	                               MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!remote)
		throw L"Failed to allocate memory in the target process:\n" +
		    winErrorText(GetLastError());

	wstring error;
	if (!WriteProcessMemory(process, remote, dllPath.c_str(), size, nullptr)) {
		error = L"Failed to write to the target process:\n" +
		        winErrorText(GetLastError());
	} else {
		HANDLE thread = CreateRemoteThread(process, nullptr, 0,
		                                   (LPTHREAD_START_ROUTINE)loadLibrary,
		                                   remote, 0, nullptr);
		if (!thread) {
			error = L"Failed to CreateRemoteThread(), make sure the "
			        L"architecture of this launcher matches the target "
			        L"executable.\n\n" +
			        winErrorText(GetLastError());
		} else {
			WaitForSingleObject(thread, INFINITE);
			CloseHandle(thread);
			if (!inject_detail::moduleLoaded(process,
			                                 inject_detail::fileName(dllPath)))
				error = L"Failed to inject DLL. Is it valid and are all of "
				        L"its dependencies present?\n" +
				        dllPath;
		}
	}

	VirtualFreeEx(process, remote, 0, MEM_RELEASE);
	if (!error.empty())
		throw error;
}
