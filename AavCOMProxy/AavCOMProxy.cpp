#include "stdafx.h"
#include "../Cloudhouse.Detours/Cloudhouse.Detour.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hInstance);
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);

  DWORD dwCreationFlags = 0;
  STARTUPINFO startupInfo = {};
  PROCESS_INFORMATION processInformation = {};

  if (Cloudhouse::Detour::CreateHookedProcess(L"C:\\Program Files (x86)\\Microsoft Office\\root\\Office16\\WINWORD.EXE", L" -Embedding",
    nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startupInfo, &processInformation, "C:\\Dev\\Research\\Debug\\AavHook.dll"))
  {
    CloseHandle(processInformation.hThread);
    CloseHandle(processInformation.hProcess);
  }

  return 0;
}
