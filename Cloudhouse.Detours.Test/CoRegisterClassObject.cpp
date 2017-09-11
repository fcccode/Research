#include "stdafx.h"
#include "CppUnitTest.h"

#include "../Cloudhouse.Detours/Cloudhouse.Detour.h"
#include "ComRules.h"
#include "IMike_h.h"
#include "CMike.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

typedef HRESULT(STDAPICALLTYPE* T_CoRegisterClassObject)(
  _In_ REFCLSID rclsid,
  _In_ LPUNKNOWN pUnk,
  _In_ DWORD dwClsContext,
  _In_ DWORD flags,
  _Out_ LPDWORD lpdwRegister
);

static T_CoRegisterClassObject TrueCoRegisterClassObject = nullptr;

HRESULT STDAPICALLTYPE MyCoRegisterClassObject(
  _In_ REFCLSID rclsid,
  _In_ LPUNKNOWN pUnk,
  _In_ DWORD dwClsContext,
  _In_ DWORD flags,
  _Out_ LPDWORD lpdwRegister
)
{
  COMRules rulesEngine;

  rulesEngine.InitializeRules();

  if (rulesEngine.MatchIID(rclsid))
  {
    flags &= ~(REGCLS_MULTIPLEUSE | REGCLS_MULTI_SEPARATE);
  }

  const auto result = TrueCoRegisterClassObject(rclsid, pUnk, dwClsContext, flags, lpdwRegister);

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
    TEST_METHOD(TestCoRegisterClassObject)
    {
      CoInitialize(nullptr);

      //Assert::IsTrue(Cloudhouse::Detour::Hook("ole32.dll", "CoRegisterClassObject", reinterpret_cast<PVOID*>(&TrueCoRegisterClassObject), &MyCoRegisterClassObject));

      //Assert::IsNotNull(&TrueCoRegisterClassObject);

      DWORD dwRegister = 0;

      CMike* pMike = new CMike();

      HRESULT hr = 0;

      hr = CoRegisterClassObject(IID_IMike, pMike, CLSCTX_INPROC_SERVER, REGCLS_SINGLEUSE , &dwRegister);
      hr = CoRegisterClassObject(IID_IMike, pMike, CLSCTX_LOCAL_SERVER, REGCLS_SINGLEUSE, &dwRegister);
      hr = CoRegisterClassObject(IID_IMike, pMike, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &dwRegister);
      hr = CoRegisterClassObject(IID_IMike, pMike, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &dwRegister);
      hr = CoRegisterClassObject(IID_IMike, pMike, CLSCTX_INPROC_SERVER, REGCLS_MULTI_SEPARATE, &dwRegister);
      hr = CoRegisterClassObject(IID_IMike, pMike, CLSCTX_LOCAL_SERVER, REGCLS_MULTI_SEPARATE, &dwRegister);
      //      HRESULT hr = CoRegisterClassObject(IID_IMike, nullptr, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &dwRegister);
      Assert::AreNotEqual(S_OK, hr);

      dwRegister = 0;
      hr = CoRegisterClassObject(IID_IMike, pMike, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED, &dwRegister);

      Assert::AreEqual(S_OK, hr);

      if (SUCCEEDED(hr))
        hr = CoRevokeClassObject(dwRegister);

      Assert::AreEqual(S_OK, hr);

      pMike->Release();

      CoUninitialize();
    }
  };
}
