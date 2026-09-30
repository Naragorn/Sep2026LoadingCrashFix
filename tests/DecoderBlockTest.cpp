// Unit tests for DecoderBlock: every flow of RefusingClassFactory and
// RegisterBlock, with the COM calls replaced by fakes.

#include "DecoderBlock.h"

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

// ---- RefusingClassFactory ----

LONG g_lastNotified = 0;
void RecordRefusal(LONG count) { g_lastNotified = count; }

void TestQueryInterface() {
	crashfix::RefusingClassFactory factory;
	void* object = reinterpret_cast<void*>(1);
	Check(factory.QueryInterface(IID_IUnknown, &object) == S_OK && object == &factory, "QI IUnknown");
	object = nullptr;
	Check(factory.QueryInterface(IID_IClassFactory, &object) == S_OK && object == &factory, "QI IClassFactory");
	object = reinterpret_cast<void*>(1);
	Check(factory.QueryInterface(IID_IDispatch, &object) == E_NOINTERFACE && object == nullptr, "QI other interface");
	Check(factory.QueryInterface(IID_IUnknown, nullptr) == E_POINTER, "QI null out pointer");
}

void TestLifetimeAndLock() {
	crashfix::RefusingClassFactory factory;
	Check(factory.AddRef() == 2, "AddRef does not count");
	Check(factory.Release() == 1, "Release does not count");
	Check(factory.LockServer(TRUE) == S_OK && factory.LockServer(FALSE) == S_OK, "LockServer");
}

void TestCreateInstanceRefuses() {
	crashfix::RefusingClassFactory silent;
	void* object = reinterpret_cast<void*>(1);
	Check(silent.CreateInstance(nullptr, IID_IUnknown, &object) == CLASS_E_CLASSNOTAVAILABLE, "refuses without notify");
	Check(object == nullptr, "clears out pointer");
	Check(silent.CreateInstance(nullptr, IID_IUnknown, nullptr) == CLASS_E_CLASSNOTAVAILABLE, "refuses with null out pointer");
	Check(silent.Refusals() == 2, "counts refusals");

	g_lastNotified = 0;
	crashfix::RefusingClassFactory notifying(&RecordRefusal);
	IUnknown* outer = &silent;  // aggregation request: refused all the same
	notifying.CreateInstance(outer, IID_IUnknown, &object);
	Check(g_lastNotified == 1, "notifies first refusal");
	notifying.CreateInstance(nullptr, IID_IUnknown, &object);
	Check(g_lastNotified == 2 && notifying.Refusals() == 2, "notifies running count");
}

// ---- RegisterBlock ----

struct FakeCom {
	HRESULT initialize = S_OK;
	HRESULT increment = S_OK;
	HRESULT registerHr = S_OK;
	std::vector<std::string> calls;
	CLSID registeredClsid{};
	IUnknown* registeredObject = nullptr;
};
FakeCom* g_com = nullptr;

HRESULT FakeInitialize() { g_com->calls.push_back("init"); return g_com->initialize; }
HRESULT FakeIncrement(CO_MTA_USAGE_COOKIE*) { g_com->calls.push_back("pin"); return g_com->increment; }
HRESULT FakeRegister(REFCLSID clsid, IUnknown* object, DWORD*) {
	g_com->calls.push_back("register");
	g_com->registeredClsid = clsid;
	g_com->registeredObject = object;
	return g_com->registerHr;
}
void FakeUninitialize() { g_com->calls.push_back("uninit"); }

const crashfix::ComOps kFakeOps{&FakeInitialize, &FakeIncrement, &FakeRegister, &FakeUninitialize};

crashfix::BlockResult Run(FakeCom& com) {
	g_com = &com;
	crashfix::RefusingClassFactory factory;
	crashfix::BlockResult result = crashfix::RegisterBlock(kFakeOps, &factory);
	if (result.failedStep == crashfix::BlockStep::None) {
		Check(com.registeredObject == static_cast<IUnknown*>(&factory), "registers the given factory");
	}
	return result;
}

using Calls = std::vector<std::string>;

void TestRegisterSuccess() {
	FakeCom com;
	crashfix::BlockResult result = Run(com);
	Check(result.failedStep == crashfix::BlockStep::None && result.hr == S_OK, "success result");
	Check(com.calls == Calls{"init", "pin", "register", "uninit"}, "success call order: pin before register, thread leaves MTA");
	Check(IsEqualCLSID(com.registeredClsid, crashfix::kClsidAc3Decoder), "registers the AC3 decoder CLSID");
}

void TestRegisterSuccessWhenAlreadyInMta() {
	FakeCom com;
	com.initialize = S_FALSE;  // thread was already in the MTA
	crashfix::BlockResult result = Run(com);
	Check(result.failedStep == crashfix::BlockStep::None, "S_FALSE from CoInitializeEx is success");
	Check(com.calls == Calls{"init", "pin", "register", "uninit"}, "S_FALSE is balanced by uninit");
}

void TestInitializeFails() {
	FakeCom com;
	com.initialize = RPC_E_CHANGED_MODE;
	crashfix::BlockResult result = Run(com);
	Check(result.failedStep == crashfix::BlockStep::Initialize && result.hr == RPC_E_CHANGED_MODE, "init failure reported");
	Check(com.calls == Calls{"init"}, "init failure: nothing else, no uninit");
}

void TestKeepAliveFails() {
	FakeCom com;
	com.increment = E_OUTOFMEMORY;
	crashfix::BlockResult result = Run(com);
	Check(result.failedStep == crashfix::BlockStep::KeepMtaAlive && result.hr == E_OUTOFMEMORY, "pin failure reported");
	Check(com.calls == Calls{"init", "pin", "uninit"}, "pin failure: no register, uninit");
}

void TestRegisterFails() {
	FakeCom com;
	com.registerHr = CO_E_OBJISREG;
	crashfix::BlockResult result = Run(com);
	Check(result.failedStep == crashfix::BlockStep::Register && result.hr == CO_E_OBJISREG, "register failure reported");
	Check(com.calls == Calls{"init", "pin", "register", "uninit"}, "register failure: uninit");
}

void TestDescribeStep() {
	Check(std::string(crashfix::DescribeStep(crashfix::BlockStep::None)) == "none", "describe none");
	Check(std::string(crashfix::DescribeStep(crashfix::BlockStep::Initialize)) == "CoInitializeEx", "describe init");
	Check(std::string(crashfix::DescribeStep(crashfix::BlockStep::KeepMtaAlive)) == "CoIncrementMTAUsage", "describe pin");
	Check(std::string(crashfix::DescribeStep(crashfix::BlockStep::Register)) == "CoRegisterClassObject", "describe register");
	Check(std::string(crashfix::DescribeStep(static_cast<crashfix::BlockStep>(99))) == "unknown", "describe out of range");
}

// ---- StartBlockThread ----

HANDLE WINAPI FailingCreateThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD) {
	return nullptr;
}

bool g_doneCalled = false;
void NeverDone(const crashfix::BlockResult&) { g_doneCalled = true; }

void TestStartBlockThreadCreateFails() {
	crashfix::RefusingClassFactory factory;
	g_doneCalled = false;
	Check(!crashfix::StartBlockThread(&factory, &NeverDone, &FailingCreateThread), "thread creation failure returns false");
	Check(!g_doneCalled, "no result reported without a thread");
}

HANDLE g_doneEvent = nullptr;
crashfix::BlockResult g_threadResult{crashfix::BlockStep::Initialize, E_FAIL};
void SignalDone(const crashfix::BlockResult& result) {
	g_threadResult = result;
	SetEvent(g_doneEvent);
}

// Real COM: the thread registers the factory, and CoCreateInstance for the AC3
// decoder from an MTA thread of this process then reaches it.
//
// Only MTA callers are probed: an STA caller does not see a class object
// registered in the MTA and would get the real decoder (measured). DirectShow's
// graph creates its candidate filters from the MTA, which RenderTest covers
// with graphs driven from both apartments.
void TestStartBlockThreadRegistersForProcess() {
	static crashfix::RefusingClassFactory factory;
	g_doneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	Check(crashfix::StartBlockThread(&factory, &SignalDone), "thread starts");
	Check(WaitForSingleObject(g_doneEvent, 10000) == WAIT_OBJECT_0, "thread reports within 10 s");
	Check(g_threadResult.failedStep == crashfix::BlockStep::None, "real registration succeeds");

	HRESULT hr = S_OK;
	HANDLE thread = CreateThread(nullptr, 0, [](void* p) -> DWORD {
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		IUnknown* object = nullptr;
		*static_cast<HRESULT*>(p) = CoCreateInstance(crashfix::kClsidAc3Decoder, nullptr, CLSCTX_INPROC_SERVER,
		                                             IID_IUnknown, reinterpret_cast<void**>(&object));
		if (object != nullptr) {
			object->Release();
		}
		CoUninitialize();
		return 0;
	}, &hr, 0, nullptr);
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
	Check(hr == CLASS_E_CLASSNOTAVAILABLE, "MTA caller gets the refusal");
	Check(factory.Refusals() == 1, "the caller reached the registered factory");
	CloseHandle(g_doneEvent);
}
}  // namespace

int main() {
	TestStartBlockThreadCreateFails();
	TestStartBlockThreadRegistersForProcess();
	TestQueryInterface();
	TestLifetimeAndLock();
	TestCreateInstanceRefuses();
	TestRegisterSuccess();
	TestRegisterSuccessWhenAlreadyInMta();
	TestInitializeFails();
	TestKeepAliveFails();
	TestRegisterFails();
	TestDescribeStep();
	std::printf(g_failures == 0 ? "DecoderBlockTest: all passed\n" : "DecoderBlockTest: %d failed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
