#include "stdafx.h"

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
  LoadLibrary(L"C:\\Dev\\Research\\Debug\\AavHook.dll");

  CoInitialize(nullptr);

  CLSID rclsid;
  CLSIDFromProgID(OLESTR("Word.Document"), &rclsid);

  IUnknown* punk = nullptr;
  HRESULT hr = CoCreateInstance(rclsid, NULL, CLSCTX_LOCAL_SERVER, IID_IUnknown, (void**)&punk);

  UNREFERENCED_PARAMETER(hr);

  CoUninitialize();

  return 0;
}