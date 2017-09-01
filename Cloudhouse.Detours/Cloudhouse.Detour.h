#pragma once

namespace Cloudhouse
{
  class Detour
  {
  public:
    static bool Hook(LPCSTR szModule, LPCSTR szFunction, PVOID* ppTrueFunction, PVOID pDetourFunction);

  private:
    static bool Initialize();

    static CRITICAL_SECTION _hookCS;
    static bool _criticalSectionInitialized;

  };
}