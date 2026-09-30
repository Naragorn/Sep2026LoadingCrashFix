#pragma once

#include <windows.h>

// What the plugin does when a script extender queries and loads it. The same
// DLL serves xOBSE (Oblivion), FOSE (Fallout 3) and xNVSE (New Vegas): each
// loader looks only in its own Data\<XSE>\Plugins folder and only for its own
// export names, and all three pass the same interface head and PluginInfo
// layout (xOBSE/obse/PluginAPI.h, xNVSE nvse/PluginAPI.h, FOSE 1.2b2
// fose/PluginAPI.h).

namespace crashfix {

// First fields of OBSEInterface / FOSEInterface / NVSEInterface; nothing
// after isEditor is used.
struct ExtenderInterfaceHead {
	UINT32 extenderVersion;
	UINT32 runtimeVersion;
	UINT32 editorVersion;
	UINT32 isEditor;
};

// PluginInfo of all three extenders.
struct PluginInfo {
	UINT32 infoVersion;
	const char* name;
	UINT32 version;
};

enum class Extender { Obse, Fose, Nvse };

constexpr const char* kPluginName = "Sep2026LoadingCrashFix";
constexpr UINT32 kPluginVersion = 2;

// xOBSE rejects infoVersion < 2 and its current value is 3; FOSE and xNVSE
// define 1 and do not check it.
UINT32 InfoVersionFor(Extender extender);
const char* ExtenderName(Extender extender);

// The side effects, replaceable in tests.
struct Host {
	void (*resetLog)();
	void (*log)(const char* format, ...);
	bool (*startBlock)();  // false if the registration thread could not start
	DWORD (*lastError)();
};

bool Query(Extender extender, const ExtenderInterfaceHead& xse, PluginInfo* info, const Host& host);
bool Load(Extender extender, const ExtenderInterfaceHead& xse, const Host& host);

}  // namespace crashfix
