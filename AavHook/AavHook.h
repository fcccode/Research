#ifdef AAVHOOK_EXPORTS
#define AAVHOOK_API __declspec(dllexport)
#else
#define AAVHOOK_API __declspec(dllimport)
#endif

AAVHOOK_API int fnAavHook(void);

void HookFunctions();

