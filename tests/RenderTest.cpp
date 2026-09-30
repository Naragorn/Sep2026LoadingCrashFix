// Integration test against the real DirectShow stack of this machine.
//
//   RenderTest.exe <file.mp3>
//
// Runs each scenario in a child process, because the unprotected scenario is
// expected to die with a fail-fast on affected systems:
//   control    - no block, two graphs in a row. Exit 0xC0000602 means this
//                system has the broken decoder; exit 0 means it does not.
//                Either is reported; only the protected runs decide PASS.
//   block-sta  - block installed, four graphs built from an STA thread.
//   block-mta  - block installed, four graphs built from an MTA thread.
// The protected runs must exit 0 with every RenderFile succeeding, the MP3
// Decoder DMO decoding, and the AC3 decoder refused at least once per graph.

#include "DecoderBlock.h"

#include <dshow.h>

#include <cstdio>
#include <cwchar>
#include <string>

namespace {

constexpr int kGraphs = 4;
constexpr DWORD kFailFast = 0xC0000602;

HANDLE g_blockDone = nullptr;
crashfix::BlockResult g_blockResult{crashfix::BlockStep::Initialize, E_FAIL};
void OnBlockDone(const crashfix::BlockResult& result) {
	g_blockResult = result;
	SetEvent(g_blockDone);
}

// Builds, runs briefly and releases one graph, as Oblivion does per track.
bool PlayOnce(const wchar_t* file, int index) {
	IGraphBuilder* graph = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder,
	                              reinterpret_cast<void**>(&graph));
	if (FAILED(hr)) {
		std::printf("graph %d: CoCreateInstance 0x%08lX\n", index, hr);
		return false;
	}
	hr = graph->RenderFile(file, nullptr);
	bool mp3Dmo = false;
	IEnumFilters* filters = nullptr;
	if (SUCCEEDED(graph->EnumFilters(&filters))) {
		IBaseFilter* filter = nullptr;
		while (filters->Next(1, &filter, nullptr) == S_OK) {
			FILTER_INFO info{};
			filter->QueryFilterInfo(&info);
			mp3Dmo = mp3Dmo || std::wcscmp(info.achName, L"MP3 Decoder DMO") == 0;
			if (info.pGraph != nullptr) {
				info.pGraph->Release();
			}
			filter->Release();
		}
		filters->Release();
	}
	IMediaControl* control = nullptr;
	HRESULT runHr = E_NOINTERFACE;
	if (SUCCEEDED(graph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&control)))) {
		runHr = control->Run();
		Sleep(300);
		control->Stop();
		control->Release();
	}
	graph->Release();
	std::printf("graph %d: RenderFile 0x%08lX, Run 0x%08lX, MP3 Decoder DMO %s\n", index, hr, runHr,
	            mp3Dmo ? "yes" : "no");
	std::fflush(stdout);
	return SUCCEEDED(hr) && SUCCEEDED(runHr) && mp3Dmo;
}

int Child(const std::wstring& scenario, const wchar_t* file) {
	const bool block = scenario != L"control";
	static crashfix::RefusingClassFactory factory;
	if (block) {
		g_blockDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (!crashfix::StartBlockThread(&factory, &OnBlockDone) || WaitForSingleObject(g_blockDone, 10000) != WAIT_OBJECT_0 ||
		    g_blockResult.failedStep != crashfix::BlockStep::None) {
			std::printf("block not installed: %s 0x%08lX\n", crashfix::DescribeStep(g_blockResult.failedStep),
			            g_blockResult.hr);
			return 3;
		}
	}
	CoInitializeEx(nullptr, scenario == L"block-mta" ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED);
	const int graphs = block ? kGraphs : 2;
	bool ok = true;
	for (int i = 1; i <= graphs; ++i) {
		ok = PlayOnce(file, i) && ok;
	}
	std::printf("refusals %ld\n", factory.Refusals());
	if (block && factory.Refusals() < graphs) {
		std::printf("AC3 decoder was not refused once per graph\n");
		ok = false;
	}
	CoUninitialize();
	return ok ? 0 : 4;
}

DWORD RunChild(const wchar_t* self, const wchar_t* scenario, const wchar_t* file) {
	std::wstring command = L"\"" + std::wstring(self) + L"\" --child " + scenario + L" \"" + file + L"\"";
	STARTUPINFOW startup{sizeof(startup)};
	PROCESS_INFORMATION process{};
	if (!CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process)) {
		return 0xFFFFFFFF;
	}
	WaitForSingleObject(process.hProcess, 60000);
	DWORD code = 0xFFFFFFFF;
	GetExitCodeProcess(process.hProcess, &code);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return code;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
	if (argc == 4 && std::wstring(argv[1]) == L"--child") {
		return Child(argv[2], argv[3]);
	}
	if (argc != 2 || GetFileAttributesW(argv[1]) == INVALID_FILE_ATTRIBUTES) {
		std::printf("usage: RenderTest.exe <existing file.mp3>\n");
		return 2;
	}

	std::printf("== control (no block)\n");
	std::fflush(stdout);
	const DWORD control = RunChild(argv[0], L"control", argv[1]);
	if (control == kFailFast) {
		std::printf("control: exit 0x%08lX - this system HAS the broken decoder\n", control);
	} else {
		std::printf("control: exit 0x%08lX - this system does not show the crash\n", control);
	}

	int failures = 0;
	for (const wchar_t* scenario : {L"block-sta", L"block-mta"}) {
		std::printf("== %ls\n", scenario);
		std::fflush(stdout);
		const DWORD code = RunChild(argv[0], scenario, argv[1]);
		std::printf("%ls: exit 0x%08lX %s\n", scenario, code, code == 0 ? "PASS" : "FAIL");
		failures += code == 0 ? 0 : 1;
	}
	std::printf(failures == 0 ? "RenderTest: all passed\n" : "RenderTest: %d failed\n", failures);
	return failures == 0 ? 0 : 1;
}
