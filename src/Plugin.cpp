// The DLL's exports and real side effects; the decisions live in PluginEntry.

#include "DecoderBlock.h"
#include "PluginEntry.h"

#include <cstdarg>
#include <cstdio>

namespace {

constexpr const char* kLogFile = "Sep2026LoadingCrashFix.log";

SRWLOCK g_logLock = SRWLOCK_INIT;

void AppendLine(const char* format, va_list args) {
	AcquireSRWLockExclusive(&g_logLock);
	if (FILE* file = std::fopen(kLogFile, "a")) {
		std::vfprintf(file, format, args);
		std::fputc('\n', file);
		std::fclose(file);
	}
	ReleaseSRWLockExclusive(&g_logLock);
}

void Log(const char* format, ...) {
	va_list args;
	va_start(args, format);
	AppendLine(format, args);
	va_end(args);
}

void ResetLog() {
	AcquireSRWLockExclusive(&g_logLock);
	if (FILE* file = std::fopen(kLogFile, "w")) {
		std::fclose(file);
	}
	ReleaseSRWLockExclusive(&g_logLock);
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

bool StartBlock() {
	return crashfix::StartBlockThread(&g_factory, &OnBlockDone);
}

DWORD LastError() {
	return GetLastError();
}

const crashfix::Host kHost{&ResetLog, &Log, &StartBlock, &LastError};

using crashfix::Extender;
using crashfix::ExtenderInterfaceHead;
using crashfix::PluginInfo;

}  // namespace

extern "C" {

__declspec(dllexport) bool OBSEPlugin_Query(const ExtenderInterfaceHead* xse, PluginInfo* info) {
	return crashfix::Query(Extender::Obse, *xse, info, kHost);
}
__declspec(dllexport) bool OBSEPlugin_Load(const ExtenderInterfaceHead* xse) {
	return crashfix::Load(Extender::Obse, *xse, kHost);
}

__declspec(dllexport) bool FOSEPlugin_Query(const ExtenderInterfaceHead* xse, PluginInfo* info) {
	return crashfix::Query(Extender::Fose, *xse, info, kHost);
}
__declspec(dllexport) bool FOSEPlugin_Load(const ExtenderInterfaceHead* xse) {
	return crashfix::Load(Extender::Fose, *xse, kHost);
}

__declspec(dllexport) bool NVSEPlugin_Query(const ExtenderInterfaceHead* xse, PluginInfo* info) {
	return crashfix::Query(Extender::Nvse, *xse, info, kHost);
}
__declspec(dllexport) bool NVSEPlugin_Load(const ExtenderInterfaceHead* xse) {
	return crashfix::Load(Extender::Nvse, *xse, kHost);
}

}  // extern "C"
