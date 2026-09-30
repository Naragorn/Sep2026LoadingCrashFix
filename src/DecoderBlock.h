#pragma once

#include <windows.h>
#include <objbase.h>

// Keeps DirectShow from instantiating the "Microsoft DSHOW AC3 Decoder"
// (msmpeg2ac3dec.dll) inside the game process.
//
// Why: with msmpeg2ac3dec.dll 10.0.26100.9549 (Windows 11 update KB5124010)
// the second IGraphBuilder::RenderFile of an MP3 in one process ends the
// process with a fail-fast (0xC0000602) inside that DLL. Intelligent connect
// tries the AC3 decoder as a candidate for every MP3 graph even though it
// never ends up in the graph - the MP3 Decoder DMO decodes the music. Oblivion
// builds one graph per music track, so the game dies at the first track change
// (main menu -> loading screen).
//
// How: a class object registered with CoRegisterClassObject takes precedence
// over the registry for CoCreateInstance in this process. Ours refuses every
// instance, so intelligent connect skips the AC3 decoder and moves on.
//
// The class object is registered from the multithreaded apartment. COM only
// consults it for callers in the MTA - an STA caller still gets the real
// decoder - but the filter graph (CLSID_FilterGraph, which Oblivion uses)
// creates its candidate filters from the MTA even when the graph is driven
// from an STA thread. A registration made in an STA is never consulted by it.
// Both measured on Windows 11 build 26200.9550; see tests/RenderTest.cpp.

namespace crashfix {

// {5261169D-9B6C-435F-B1D5-F79BAF700C71}, "Microsoft DSHOW AC3 Decoder".
extern const CLSID kClsidAc3Decoder;

// Class factory that refuses every instance. It lives as long as the process
// (a static object), so AddRef/Release do not count.
class RefusingClassFactory final : public IClassFactory {
public:
	using Notify = void (*)(LONG refusalCount);

	explicit RefusingClassFactory(Notify onRefusal = nullptr) : onRefusal_(onRefusal) {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;
	HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** object) override;
	HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override;

	LONG Refusals() const { return refusals_; }

private:
	Notify onRefusal_;
	volatile LONG refusals_ = 0;
};

// The COM calls RegisterBlock makes, replaceable in tests.
struct ComOps {
	HRESULT (*initializeMta)();
	HRESULT (*incrementMtaUsage)(CO_MTA_USAGE_COOKIE* cookie);
	HRESULT (*registerClassObject)(REFCLSID clsid, IUnknown* object, DWORD* cookie);
	void (*uninitialize)();
};

ComOps RealComOps();

enum class BlockStep {
	None,          // success
	Initialize,    // CoInitializeEx(COINIT_MULTITHREADED) failed
	KeepMtaAlive,  // CoIncrementMTAUsage failed
	Register,      // CoRegisterClassObject failed
};

struct BlockResult {
	BlockStep failedStep;
	HRESULT hr;
};

const char* DescribeStep(BlockStep step);

// Runs on the calling thread, which must not have entered an apartment yet.
// Joins the MTA, pins it for the rest of the process (the registration lives
// only as long as the MTA does), and registers `factory` for the AC3 decoder.
BlockResult RegisterBlock(const ComOps& ops, IClassFactory* factory);

// Starts a thread that runs RegisterBlock with the real COM calls and reports
// the result through `onDone` (required). Does not wait for it: OBSE may load plugins
// under the loader lock, where waiting for a new thread would deadlock.
// Returns false if the thread could not be created. `createThread` is
// replaceable in tests.
using CreateThreadFn = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
bool StartBlockThread(IClassFactory* factory, void (*onDone)(const BlockResult& result),
                      CreateThreadFn createThread = &CreateThread);

}  // namespace crashfix
