#include "stdafx.h"
#include "AavHook.h"
#include "LoadLibraryHook.h"
#include "COMHooks.h"

// Need a dummy function to export (for detours)
AAVHOOK_API int fnAavHook(void)
{
  return 42;
}

void HookFunctions()
{
  HookLoadLibrary();

  if (GetModuleHandle(L"ole32.dll") || GetModuleHandle(L"combase.dll"))
  {
    HookCOMFunctions();
  }
}
