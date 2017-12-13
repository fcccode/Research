#include "stdafx.h"
#include "AavHook.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  UNREFERENCED_PARAMETER(hModule);
  UNREFERENCED_PARAMETER(lpReserved);

  if (ul_reason_for_call == DLL_PROCESS_ATTACH)
  {
    TCHAR szMutex[20] = {};
    _stprintf_s(szMutex, 20, _T("Global\\CH%06dAAV"), GetCurrentProcessId());
    CreateMutex(nullptr, FALSE, szMutex);

    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
      HookFunctions();
    }
  }

  return TRUE;
}
