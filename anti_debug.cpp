#include "Windows.h"

namespace anti_debug {

bool CheckDebuggerPresent();
void HideThreadFromDebugger();
namespace { void NTAPI OnThreadStart(PVOID module, DWORD reason, PVOID reserved); }

constexpr int kThreadHideFromDebugger{ 0x11 };
const auto kNtCurrentThread = reinterpret_cast<HANDLE>(-2);

// If macro ANTI_DEBUG_RELEASE defined, anti-debug will be enabled in release mode.
// If macro ANTI_DEBUG_DEBUG defined, anti-debug will be enabled in debug mode.
#define ANTI_DEBUG_RELEASE

#if (!defined(_DEBUG) && defined(ANTI_DEBUG_RELEASE)) || \
    (defined(_DEBUG) && defined(ANTI_DEBUG_DEBUG))
// Add tls-callback that will be run before main(). 
// He will run anti-debug code and disable the debugger.
#pragma comment (linker, "/INCLUDE:_tls_used")
#pragma comment (linker, "/INCLUDE:antiDbgTlsCallback")
#pragma const_seg(".CRT$XLF")
EXTERN_C const PIMAGE_TLS_CALLBACK antiDbgTlsCallback = OnThreadStart;
#pragma const_seg()
#endif

namespace {

void NTAPI OnThreadStart(PVOID module, DWORD reason, PVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH && reason != DLL_THREAD_ATTACH) {
    return;
  }
  // Disable sending debug messages by all threads.
  // This won't allow the debugger to work correctly
  // throughout all the life of the process.
  HideThreadFromDebugger();

  // Exit the process if the program was started from debugger.
  if (CheckDebuggerPresent()) {
    TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
  }
}

} // namespace

void HideThreadFromDebugger() {
  const auto ntdll = GetModuleHandle("ntdll.dll");
  if (!ntdll) {
    return;
  }
  using NtSetInformationThreadType = NTSTATUS(*)(HANDLE, ULONG, PVOID, ULONG);
  const auto NtSetInformationThread = reinterpret_cast<NtSetInformationThreadType>(
      GetProcAddress(ntdll, "NtSetInformationThread"));
  if (!NtSetInformationThread) {
    return;
  }
  // Hide thread from the debugger.
  NtSetInformationThread(kNtCurrentThread, kThreadHideFromDebugger, 0, 0);
}

bool CheckDebuggerPresent() {
  if (IsDebuggerPresent()) {
    return true;
  }
  BOOL isDbgPresent{ FALSE };
  if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDbgPresent) != 0 &&
    isDbgPresent == TRUE) {
    return true;
  }
  return false;
}

} // namespace anti_debug
