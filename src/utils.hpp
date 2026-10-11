#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "sha256.hpp"

namespace fs = std::filesystem;
using std::wstring;

inline wstring launcherName;

[[noreturn]] inline void failf(const wstring &msg) {
	MessageBoxW(nullptr, msg.c_str(), launcherName.c_str(), MB_ICONERROR);
	ExitProcess(1);
}

inline wstring trimSpace(const wstring &s) {
	const wchar_t *ws = L" \t\r\n\v\f";
	size_t b = s.find_first_not_of(ws);
	if (b == wstring::npos)
		return L"";
	return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

inline wstring trimQuotes(const wstring &s) {
	size_t b = s.find_first_not_of(L'"');
	if (b == wstring::npos)
		return L"";
	return s.substr(b, s.find_last_not_of(L'"') - b + 1);
}

inline wstring utf8ToWide(const std::string &s) {
	if (s.empty())
		return L"";
	int n =
	    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

inline wstring errnoText() { return _wcserror(errno); }

inline wstring winErrorText(DWORD code) {
	wchar_t *p = nullptr;
	DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	                             FORMAT_MESSAGE_FROM_SYSTEM |
	                             FORMAT_MESSAGE_IGNORE_INSERTS,
	                         nullptr, code, 0, (LPWSTR)&p, 0, nullptr);
	wstring s =
	    n ? trimSpace(wstring(p, n)) : L"error " + std::to_wstring(code);
	LocalFree(p);
	return s;
}

inline wstring resolve(const wstring &dir, const wstring &path) {
	fs::path joined = fs::path(dir) / path;
	std::error_code ec;
	fs::path abs = fs::absolute(joined, ec);
	return (ec ? joined : abs.lexically_normal()).wstring();
}

struct FileCloser {
	void operator()(FILE *f) const { fclose(f); }
};
using File = std::unique_ptr<FILE, FileCloser>;

inline File openFile(const wstring &path) {
	File f(_wfopen(path.c_str(), L"rb"));
	if (!f)
		throw L"could not open:\n" + path + L"\n\n" + errnoText();
	return f;
}

inline std::vector<wstring> readLines(const wstring &path) {
	File f = openFile(path);

	std::string data;
	char chunk[4096];
	size_t r;
	while ((r = fread(chunk, 1, sizeof chunk, f.get())) > 0)
		data.append(chunk, r);
	if (ferror(f.get()))
		throw L"could not read:\n" + path + L"\n\n" + errnoText();

	if (data.compare(0, 3, "\xEF\xBB\xBF") == 0)
		data.erase(0, 3);

	std::vector<wstring> lines;
	for (size_t pos = 0; pos < data.size();) {
		size_t nl = data.find('\n', pos);
		if (nl == std::string::npos)
			nl = data.size();
		size_t end = nl;
		if (end > pos && data[end - 1] == '\r')
			end--;
		lines.push_back(utf8ToWide(data.substr(pos, end - pos)));
		pos = nl + 1;
	}
	return lines;
}

inline wstring hashFile(const wstring &path) {
	File f = openFile(path);

	Sha256 sha;
	uint8_t buf[1 << 16];
	size_t r;
	while ((r = fread(buf, 1, sizeof buf, f.get())) > 0)
		sha.update(buf, r);
	if (ferror(f.get()))
		throw L"could not read:\n" + path + L"\n\n" + errnoText();
	return sha.hex();
}

inline void splitCommand(const wstring &raw, wstring &exe, wstring &args) {
	wstring line = trimSpace(raw);

	if (!line.empty() && line[0] == L'"') {
		size_t end = line.find(L'"', 1);
		if (end == wstring::npos)
			throw wstring(L"unterminated quote");
		exe = line.substr(1, end - 1);
		args = trimSpace(line.substr(end + 1));
		return;
	}

	wstring lower = line;
	std::transform(lower.begin(), lower.end(), lower.begin(),
	               [](wchar_t c) { return (wchar_t)std::towlower(c); });
	size_t split = lower.find(L".exe");
	split = split != wstring::npos
	            ? split + 4
	            : std::min(line.find_first_of(L" \t"), line.size());

	exe = line.substr(0, split);
	args = trimSpace(line.substr(split));
}

inline void verifyLine(const wstring &dir, const wstring &line) {
	size_t i = line.find_first_of(L" \t");
	if (i == wstring::npos)
		throw wstring(L"expected \"<sha256> <path>\"");

	wstring want = line.substr(0, i);
	std::transform(want.begin(), want.end(), want.begin(),
	               [](wchar_t c) { return (wchar_t)std::towlower(c); });
	if (want.size() != 64 ||
	    want.find_first_not_of(L"0123456789abcdef") != wstring::npos)
		throw wstring(L"invalid sha256 (expected 64 hex chars)");

	wstring rel = trimQuotes(trimSpace(line.substr(i)));
	if (rel.empty())
		throw wstring(L"missing file path");

	wstring path = resolve(dir, rel);
	wstring got = hashFile(path);
	if (got != want)
		throw L"hash mismatch:\n" + path + L"\n\nexpected: " + want +
		    L"\nactual:   " + got;
}
