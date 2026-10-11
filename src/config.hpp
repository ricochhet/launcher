#pragma once

#include "utils.hpp"

struct ConfigEntry {
	size_t line;
	wstring value;
};

struct LaunchConfig {
	ConfigEntry command;
	std::vector<ConfigEntry> hashes;
	std::vector<ConfigEntry> injects;
	bool wait = false;
	bool abortOnInjectFailure = false;
};

inline wstring splitKeyword(const wstring &line, wstring &rest) {
	size_t i = line.find_first_of(L" \t");
	if (i == wstring::npos) {
		rest.clear();
		return toLower(line);
	}
	rest = trimSpace(line.substr(i));
	return toLower(line.substr(0, i));
}

inline LaunchConfig parseConfig(const std::vector<wstring> &lines) {
	LaunchConfig cfg;
	cfg.command = {1, lines.at(0)};

	for (size_t i = 1; i < lines.size(); i++) {
		size_t lineNo = i + 1;
		wstring line = trimSpace(lines[i]);
		if (line.empty() || line[0] == L'#')
			continue;

		wstring rest;
		wstring keyword = splitKeyword(line, rest);
		auto fail = [&](const wstring &msg) {
			return L"line " + std::to_wstring(lineNo) + L": " + msg;
		};

		if (keyword == L"inject") {
			wstring path = trimQuotes(rest);
			if (path.empty())
				throw fail(L"inject: missing DLL path");
			cfg.injects.push_back({lineNo, path});
		} else if (keyword == L"wait") {
			if (!rest.empty())
				throw fail(L"wait: unexpected argument");
			cfg.wait = true;
		} else if (keyword == L"on_inject_failure") {
			wstring mode = toLower(rest);
			if (mode == L"abort")
				cfg.abortOnInjectFailure = true;
			else if (mode == L"warn")
				cfg.abortOnInjectFailure = false;
			else
				throw fail(L"on_inject_failure: expected \"warn\" or "
				           L"\"abort\"");
		} else {
			try {
				wstring want, rel;
				parseHashLine(line, want, rel);
			} catch (const wstring &e) {
				throw fail(e);
			}
			cfg.hashes.push_back({lineNo, line});
		}
	}
	return cfg;
}
