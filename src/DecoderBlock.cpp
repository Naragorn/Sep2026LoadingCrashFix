#include "DecoderBlock.h"

namespace crashfix {

const CLSID kClsidAc3Decoder = {0x5261169D, 0x9B6C, 0x435F, {0xB1, 0xD5, 0xF7, 0x9B, 0xAF, 0x70, 0x0C, 0x71}};

HRESULT STDMETHODCALLTYPE RefusingClassFactory::QueryInterface(REFIID riid, void** object) {
	if (object == nullptr) {
		return E_POINTER;
	}
	if (riid == IID_IUnknown || riid == IID_IClassFactory) {
		*object = static_cast<IClassFactory*>(this);
		return S_OK;
	}
	*object = nullptr;
	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE RefusingClassFactory::AddRef() {
	return 2;
}

ULONG STDMETHODCALLTYPE RefusingClassFactory::Release() {
	return 1;
}

HRESULT STDMETHODCALLTYPE RefusingClassFactory::CreateInstance(IUnknown*, REFIID, void** object) {
	if (object != nullptr) {
		*object = nullptr;
	}
	const LONG count = InterlockedIncrement(&refusals_);
	if (onRefusal_ != nullptr) {
		onRefusal_(count);
	}
	// The same answer COM gives for a class that cannot be created here;
	// the graph builder treats it like any other candidate that fails to load.
	return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT STDMETHODCALLTYPE RefusingClassFactory::LockServer(BOOL) {
	return S_OK;
}

namespace {

HRESULT InitializeMta() {
	return CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

HRESULT IncrementMtaUsage(CO_MTA_USAGE_COOKIE* cookie) {
	return CoIncrementMTAUsage(cookie);
}

void Uninitialize() {
	CoUninitialize();
}

HRESULT RegisterInproc(REFCLSID clsid, IUnknown* object, DWORD* cookie) {
	return CoRegisterClassObject(clsid, object, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, cookie);
}

struct ThreadArgs {
	IClassFactory* factory;
	void (*onDone)(const BlockResult& result);
};

DWORD WINAPI BlockThread(void* param) {
	ThreadArgs* args = static_cast<ThreadArgs*>(param);
	const BlockResult result = RegisterBlock(RealComOps(), args->factory);
	args->onDone(result);
	delete args;
	return 0;
}

}  // namespace

ComOps RealComOps() {
	return ComOps{&InitializeMta, &IncrementMtaUsage, &RegisterInproc, &Uninitialize};
}

const char* DescribeStep(BlockStep step) {
	switch (step) {
		case BlockStep::None: return "none";
		case BlockStep::Initialize: return "CoInitializeEx";
		case BlockStep::KeepMtaAlive: return "CoIncrementMTAUsage";
		case BlockStep::Register: return "CoRegisterClassObject";
	}
	return "unknown";
}

BlockResult RegisterBlock(const ComOps& ops, IClassFactory* factory) {
	HRESULT hr = ops.initializeMta();
	if (FAILED(hr)) {
		return {BlockStep::Initialize, hr};
	}

	// Never released: the class object has to stay registered for as long as
	// the game builds music graphs, which is until the process ends.
	CO_MTA_USAGE_COOKIE mtaCookie = nullptr;
	hr = ops.incrementMtaUsage(&mtaCookie);
	if (FAILED(hr)) {
		ops.uninitialize();
		return {BlockStep::KeepMtaAlive, hr};
	}

	DWORD registrationCookie = 0;
	hr = ops.registerClassObject(kClsidAc3Decoder, factory, &registrationCookie);
	ops.uninitialize();
	if (FAILED(hr)) {
		return {BlockStep::Register, hr};
	}
	return {BlockStep::None, hr};
}

bool StartBlockThread(IClassFactory* factory, void (*onDone)(const BlockResult& result),
                      CreateThreadFn createThread) {
	ThreadArgs* args = new ThreadArgs{factory, onDone};
	HANDLE thread = createThread(nullptr, 0, &BlockThread, args, 0, nullptr);
	if (thread == nullptr) {
		delete args;
		return false;
	}
	CloseHandle(thread);
	return true;
}

}  // namespace crashfix
