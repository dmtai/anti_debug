#include "Windows.h"

namespace anti_debug {

bool CheckDebuggerPresent();
void HideThreadFromDebugger();
namespace { void NTAPI OnThreadStart(PVOID module, DWORD reason, PVOID reserved); }

constexpr int kThreadHideFromDebugger{ 0x11 };
constexpr int kNtCurrentThread{ -2 };

// If macro ANTI_DEBUG_RELEASE defined, anti-debug will be enabled 
// in project release mode.
// If macro ANTI_DEBUG_DEBUG defined, anti-debug will be enabled 
// in project debug mode.
#define ANTI_DEBUG_RELEASE

#if (!defined(_DEBUG) && defined(ANTI_DEBUG_RELEASE)) || \
    (defined(_DEBUG) && defined(ANTI_DEBUG_DEBUG))
// Add anti-debug tls-callback that will be run before main(). 
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
  // This won't allow a debugger to work correctly
  // throughout all the life of the process.
  HideThreadFromDebugger();

  // Exit the process if the program is running under a debugger
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
  // Hide thread from a debugger.
  NtSetInformationThread(reinterpret_cast<HANDLE>(kNtCurrentThread), 
                         kThreadHideFromDebugger, 0, 0);
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
