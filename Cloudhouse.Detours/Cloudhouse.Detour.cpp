#include "stdafx.h"
#include "Cloudhouse.Detour.h"
#include "detours.h"

using namespace Cloudhouse;

CRITICAL_SECTION Detour::_hookCS;
bool Detour::_criticalSectionInitialized = Detour::Initialize();

bool Detour::Initialize()
{
  InitializeCriticalSection(&_hookCS);

  return true;
}

bool Detour::Hook(LPCSTR szModule, LPCSTR szFunction, PVOID* ppTrueFunction, PVOID pDetourFunction)
{
  bool success = false;

  EnterCriticalSection(&_hookCS);

  if ((ppTrueFunction) && (*ppTrueFunction == nullptr)) // not hooked
  {
    PVOID pOriginalFunction = DetourFindFunction(szModule, szFunction); // find original

    if (pOriginalFunction)  // found orig
    {
      if (DetourTransactionBegin() == NO_ERROR)
      {
        DetourUpdateThread(GetCurrentThread());

        DetourAttach(&pOriginalFunction, pDetourFunction);

        DetourTransactionCommit();

        // sanity check incase another thread has just set this value!
        if ((ppTrueFunction) && (*ppTrueFunction == NULL))
        {
          *ppTrueFunction = pOriginalFunction;

          success = true;
        }
      }
    }
  }
  
  LeaveCriticalSection(&_hookCS);

  return success;
}
