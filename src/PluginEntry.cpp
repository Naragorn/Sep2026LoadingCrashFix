#include "PluginEntry.h"

namespace crashfix {

UINT32 InfoVersionFor(Extender extender) {
	return extender == Extender::Obse ? 3 : 1;
}

const char* ExtenderName(Extender extender) {
	switch (extender) {
		case Extender::Obse: return "OBSE";
		case Extender::Fose: return "FOSE";
		case Extender::Nvse: return "NVSE";
	}
	return "unknown";
}

bool Query(Extender extender, const ExtenderInterfaceHead& xse, PluginInfo* info, const Host& host) {
	info->infoVersion = InfoVersionFor(extender);
	info->name = kPluginName;
	info->version = kPluginVersion;

	host.resetLog();
	host.log("%s %u - %s %u, runtime %08X, editor=%u", kPluginName, kPluginVersion, ExtenderName(extender),
	         xse.extenderVersion, xse.runtimeVersion, xse.isEditor);
	// No version gate: the fix touches no game code, only COM in this process.
	return true;
}

bool Load(Extender, const ExtenderInterfaceHead& xse, const Host& host) {
	if (xse.isEditor != 0) {
		host.log("Editor: nothing to do");
		return true;
	}
	if (!host.startBlock()) {
		host.log("Could not start the registration thread (error %lu) - music track changes may crash",
		         host.lastError());
	}
	// Always true: refusing to load would not make the game any safer.
	return true;
}

}  // namespace crashfix
