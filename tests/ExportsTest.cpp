// Loads the built plugin DLL the way a script extender does and calls its
// real exports.
//
//   ExportsTest.exe <Sep2026LoadingCrashFix.dll>
//
// Checks: the six Query/Load exports exist; the export table stays under
// xNVSE's 50-name limit (PluginChecker.cpp skips DLLs above it); every Query
// fills PluginInfo; every editor Load does nothing; a game Load blocks the AC3
// decoder and says so in the log.

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	if (!condition) {
		++g_failures;
		std::printf("FAIL: %s\n", what);
	}
}

struct Head { UINT32 extenderVersion, runtimeVersion, editorVersion, isEditor; };
struct Info { UINT32 infoVersion; const char* name; UINT32 version; };
using QueryFn = bool (*)(const Head*, Info*);
using LoadFn = bool (*)(const Head*);

DWORD ExportedNameCount(HMODULE module) {
	auto* base = reinterpret_cast<const BYTE*>(module);
	auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
	return exports->NumberOfNames;
}

std::string ReadLog() {
	std::ifstream file("Sep2026LoadingCrashFix.log");
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

}  // namespace

int main(int argc, char** argv) {
	if (argc != 2) {
		std::printf("usage: ExportsTest.exe <plugin.dll>\n");
		return 2;
	}
	// The plugin writes its log into the current directory, as in the game.
	char temp[MAX_PATH];
	GetTempPathA(MAX_PATH, temp);
	std::string dir = std::string(temp) + "Sep2026LoadingCrashFixExportsTest";
	CreateDirectoryA(dir.c_str(), nullptr);
	char dll[MAX_PATH];
	GetFullPathNameA(argv[1], MAX_PATH, dll, nullptr);
	SetCurrentDirectoryA(dir.c_str());

	HMODULE module = LoadLibraryA(dll);
	Check(module != nullptr, "DLL loads");
	if (module == nullptr) {
		return 1;
	}
	Check(ExportedNameCount(module) == 6, "exactly six exports (xNVSE skips DLLs over 50)");

	const struct { const char* prefix; UINT32 infoVersion; } extenders[] = {
		{"OBSE", 3}, {"FOSE", 1}, {"NVSE", 1}};
	for (const auto& extender : extenders) {
		const std::string query = std::string(extender.prefix) + "Plugin_Query";
		const std::string load = std::string(extender.prefix) + "Plugin_Load";
		auto queryFn = reinterpret_cast<QueryFn>(GetProcAddress(module, query.c_str()));
		auto loadFn = reinterpret_cast<LoadFn>(GetProcAddress(module, load.c_str()));
		Check(queryFn != nullptr && loadFn != nullptr, "Query and Load exported");
		if (queryFn == nullptr || loadFn == nullptr) {
			continue;
		}
		Info info{};
		const Head editor{1, 0, 1, 1};
		Check(queryFn(&editor, &info), "Query returns true");
		Check(info.infoVersion == extender.infoVersion && info.name != nullptr &&
		          std::strcmp(info.name, "Sep2026LoadingCrashFix") == 0 && info.version == 2,
		      "Query fills PluginInfo");
		Check(loadFn(&editor), "editor Load returns true");
		Check(ReadLog().find("Editor: nothing to do") != std::string::npos, "editor Load logged");
		std::printf("%s exports ok\n", extender.prefix);
	}

	// One real game load: the registration thread reports into the log.
	auto queryFn = reinterpret_cast<QueryFn>(GetProcAddress(module, "NVSEPlugin_Query"));
	auto loadFn = reinterpret_cast<LoadFn>(GetProcAddress(module, "NVSEPlugin_Load"));
	Info info{};
	const Head game{6, 0x040020D0, 0, 0};
	queryFn(&game, &info);
	Check(loadFn(&game), "game Load returns true");
	bool blocked = false;
	for (int i = 0; i < 100 && !blocked; ++i) {
		Sleep(50);
		blocked = ReadLog().find("AC3 decoder blocked for this process") != std::string::npos;
	}
	Check(blocked, "game Load blocks the AC3 decoder within 5 s");
	std::printf("log:\n%s", ReadLog().c_str());

	std::printf(g_failures == 0 ? "ExportsTest: all passed\n" : "ExportsTest: %d failed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
