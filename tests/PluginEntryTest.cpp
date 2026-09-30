// Unit tests for PluginEntry: every flow of Query and Load for each script
// extender, with the side effects replaced by fakes.

#include "PluginEntry.h"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	if (!condition) {
		++g_failures;
		std::printf("FAIL: %s\n", what);
	}
}

struct Fake {
	int resets = 0;
	int starts = 0;
	bool startResult = true;
	std::vector<std::string> lines;
};
Fake* g_fake = nullptr;

void FakeReset() { ++g_fake->resets; }
void FakeLog(const char* format, ...) {
	char buffer[512];
	va_list args;
	va_start(args, format);
	std::vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	g_fake->lines.push_back(buffer);
}
bool FakeStart() { ++g_fake->starts; return g_fake->startResult; }
DWORD FakeLastError() { return 8; }

const crashfix::Host kFakeHost{&FakeReset, &FakeLog, &FakeStart, &FakeLastError};

using crashfix::Extender;

void TestNames() {
	Check(std::string(crashfix::ExtenderName(Extender::Obse)) == "OBSE", "name OBSE");
	Check(std::string(crashfix::ExtenderName(Extender::Fose)) == "FOSE", "name FOSE");
	Check(std::string(crashfix::ExtenderName(Extender::Nvse)) == "NVSE", "name NVSE");
	Check(std::string(crashfix::ExtenderName(static_cast<Extender>(9))) == "unknown", "name out of range");
	Check(crashfix::InfoVersionFor(Extender::Obse) == 3, "OBSE info version 3");
	Check(crashfix::InfoVersionFor(Extender::Fose) == 1, "FOSE info version 1");
	Check(crashfix::InfoVersionFor(Extender::Nvse) == 1, "NVSE info version 1");
}

void TestQuery(Extender extender, const char* expectedLog) {
	Fake fake;
	g_fake = &fake;
	crashfix::PluginInfo info{};
	const crashfix::ExtenderInterfaceHead xse{22, 0x040020D0, 0, 0};
	Check(crashfix::Query(extender, xse, &info, kFakeHost), "query accepts");
	Check(info.infoVersion == crashfix::InfoVersionFor(extender), "query sets info version");
	Check(std::string(info.name) == "Sep2026LoadingCrashFix" && info.version == crashfix::kPluginVersion,
	      "query sets name and version");
	Check(fake.resets == 1, "query starts a fresh log");
	Check(fake.lines.size() == 1 && fake.lines[0] == expectedLog, "query logs extender and runtime");
	Check(fake.starts == 0, "query does not start the block");
}

void TestLoadEditor() {
	Fake fake;
	g_fake = &fake;
	Check(crashfix::Load(Extender::Nvse, {6, 0, 1, 1}, kFakeHost), "editor load returns true");
	Check(fake.starts == 0, "editor: no block");
	Check(fake.lines == std::vector<std::string>{"Editor: nothing to do"}, "editor logged");
}

void TestLoadStarts() {
	Fake fake;
	g_fake = &fake;
	Check(crashfix::Load(Extender::Fose, {1, 0x01070030, 0, 0}, kFakeHost), "game load returns true");
	Check(fake.starts == 1 && fake.lines.empty(), "game: block started, nothing logged yet");
}

void TestLoadStartFails() {
	Fake fake;
	fake.startResult = false;
	g_fake = &fake;
	Check(crashfix::Load(Extender::Obse, {22, 0, 0, 0}, kFakeHost), "failed start still returns true");
	Check(fake.lines.size() == 1 &&
	          fake.lines[0] == "Could not start the registration thread (error 8) - music track changes may crash",
	      "failed start logged with error");
}

}  // namespace

int main() {
	TestNames();
	TestQuery(Extender::Obse, "Sep2026LoadingCrashFix 2 - OBSE 22, runtime 040020D0, editor=0");
	TestQuery(Extender::Fose, "Sep2026LoadingCrashFix 2 - FOSE 22, runtime 040020D0, editor=0");
	TestQuery(Extender::Nvse, "Sep2026LoadingCrashFix 2 - NVSE 22, runtime 040020D0, editor=0");
	TestLoadEditor();
	TestLoadStarts();
	TestLoadStartFails();
	std::printf(g_failures == 0 ? "PluginEntryTest: all passed\n" : "PluginEntryTest: %d failed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
