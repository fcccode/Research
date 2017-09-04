#include "stdafx.h"
#include "CppUnitTest.h"

#include "../Cloudhouse.Detours/Cloudhouse.Detour.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// This is used to prove that the hooked function was called.
static bool g_HookedFunctionCalled = false;

// Function prototype taken from Windows header file - just wrap WINAPI as a pointer to a function
typedef BOOL(WINAPI* T_Beep)(
  _In_ DWORD dwFreq,
  _In_ DWORD dwDuration
  );

// Where we store the original function
static T_Beep TrueBeep = nullptr;

// My implementation of the the function.
BOOL WINAPI MyBeep(
  _In_ DWORD dwFreq,
  _In_ DWORD dwDuration
)
{
  g_HookedFunctionCalled = true;

  const auto result = TrueBeep(dwFreq, dwDuration);

  return result;
}

/**
 * \brief Simple tests that proves that hooking is functioning.
 */
namespace CloudhouseDetoursTest
{
  TEST_CLASS(UnitTest1)
  {
  public:
    TEST_METHOD(TestMethod1)
    {
      // This is how we hook a function, specify the dll, function, where to store the original function, and the hooked function
      Assert::IsTrue(Cloudhouse::Detour::Hook("kernel32.dll", "Beep", reinterpret_cast<PVOID*>(&TrueBeep), &MyBeep));

      Assert::IsNotNull(&TrueBeep);

      Beep(500, 100);

      Assert::IsTrue(g_HookedFunctionCalled);
    }
  };
}
