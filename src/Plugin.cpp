#include "DecoderBlock.h"

#include <cstdarg>
#include <cstdio>

// Minimal replica of the xOBSE plugin interface; only the layout of these two
// structs matters, and it must stay binary compatible with obse/PluginAPI.h.
namespace obse {

struct Interface {
	UINT32 obseVersion;
	UINT32 oblivionVersion;
	UINT32 editorVersion;
	UINT32 isEditor;
};

struct Info {
	enum { kInfoVersion = 3 };  // xOBSE rejects infoVersion < 2

	UINT32 infoVersion;
	const char* name;
	UINT32 version;
};

}  // namespace obse

namespace {

constexpr const char* kPluginName = "Sep2026LoadingCrashFix";
constexpr UINT32 kPluginVersion = 1;
constexpr const char* kLogFile = "Sep2026LoadingCrashFix.log";

CRITICAL_SECTION g_logLock;

void Log(const char* format, ...) {
	EnterCriticalSection(&g_logLock);
	if (FILE* file = std::fopen(kLogFile, "a")) {
		va_list args;
		va_start(args, format);
		std::vfprintf(file, format, args);
		va_end(args);
		std::fputc('\n', file);
		std::fclose(file);
	}
	LeaveCriticalSection(&g_logLock);
}

void OnRefusal(LONG count) {
	Log("Refused the AC3 decoder to DirectShow (%ld so far)", count);
}

crashfix::RefusingClassFactory g_factory(&OnRefusal);

void OnBlockDone(const crashfix::BlockResult& result) {
	if (result.failedStep == crashfix::BlockStep::None) {
		Log("AC3 decoder blocked for this process");
	} else {
		Log("Could not block the AC3 decoder: %s failed with 0x%08lX - music track changes may crash",
		    crashfix::DescribeStep(result.failedStep), static_cast<unsigned long>(result.hr));
	}
}

}  // namespace

extern "C" {

__declspec(dllexport) bool OBSEPlugin_Query(const obse::Interface* obse, obse::Info* info) {
	info->infoVersion = obse::Info::kInfoVersion;
	info->name = kPluginName;
	info->version = kPluginVersion;

	InitializeCriticalSection(&g_logLock);
	if (FILE* file = std::fopen(kLogFile, "w")) {
		std::fclose(file);
	}
	Log("%s %u - OBSE %u, editor=%u", kPluginName, kPluginVersion, obse->obseVersion, obse->isEditor);
	return true;
}

__declspec(dllexport) bool OBSEPlugin_Load(const obse::Interface* obse) {
	if (obse->isEditor != 0) {
		Log("Construction Set: nothing to do");
		return true;
	}
	if (!crashfix::StartBlockThread(&g_factory, &OnBlockDone)) {
		Log("Could not start the registration thread (error %lu) - music track changes may crash",
		    GetLastError());
	}
	return true;
}

}  // extern "C"
