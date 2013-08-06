/*
  Copied from https://github.com/yumeyao/7zip
  "building 7zip using vc8(x86) and vc11(x64) while still linking to msvcrt.dll"

  To the best of my knowledge this file is licensed under the LGPL.

  PSDK versions of htmlhelp.lib for x86_64 depend on bufferoverflowU.lib for __security_cookie,
  since they are compiled with VC using the /GS flag

  This also improves on the x86 library in previous SDK, since it adds functionality to expand
  REG_EXPAND_SZ to a full path.
*/

#include <windows.h>

static HMODULE g_hmodHHCtrl = NULL;
static BOOL g_fTriedAndFailed = FALSE;
typedef HWND (WINAPI *pHtmlHelpA_t)(HWND hwndCaller, LPCSTR pszFile, UINT uCommand, DWORD_PTR dwData);
typedef HWND (WINAPI *pHtmlHelpW_t)(HWND hwndCaller, LPCWSTR pszFile, UINT uCommand, DWORD_PTR dwData);
pHtmlHelpA_t pHtmlHelpA = NULL;
pHtmlHelpW_t pHtmlHelpW = NULL;
#define HtmlHelpAHint 14
#define HtmlHelpWHint 15

__attribute__((always_inline)) static LPSTR GetRegisteredLocation(LPSTR pszPath, LPSTR pszExpandedPath)
{
	LPSTR ret = NULL;
	HKEY hKey;
	DWORD cbData;
	DWORD dwType;
	if (ERROR_SUCCESS != RegOpenKeyExA(
			HKEY_CLASSES_ROOT,
			"CLSID\\{ADB880A6-D8FF-11CF-9377-00AA003B7A11}\\InprocServer32",
			0, KEY_READ, &hKey))
		return ret;
	cbData = MAX_PATH;
	if (ERROR_SUCCESS == RegQueryValueExA(hKey, NULL, NULL, &dwType, (LPBYTE)pszPath, &cbData)) {
		if (REG_EXPAND_SZ==dwType) {
			if (MAX_PATH == ExpandEnvironmentStringsA(pszPath, pszExpandedPath, MAX_PATH)) {
				ret = pszExpandedPath;
			}
		} else {
			ret = pszPath;
		}
	}
	RegCloseKey(hKey);
	return ret;
}

__declspec(noinline) static HMODULE WINAPI TryLoadingModule()
{
	CHAR szPath[MAX_PATH];
	CHAR szExpandedPath[MAX_PATH];
	HMODULE hMod;
	CHAR *pszPath;

	if (pszPath = GetRegisteredLocation(szPath, szExpandedPath)) {
		hMod = LoadLibraryA(pszPath);
		if (!hMod)
			goto LoadDefaultPath;
	} else {
LoadDefaultPath:
		hMod = LoadLibraryA("hhctrl.ocx");
	}
	return hMod;
}

#define _mkFuncPtr(FuncName) p##FuncName
#define _mkFuncPtrType(FuncName) p##FuncName##_t
#define _mkFuncHint(FuncName) FuncName##Hint
#define mkFuncPtr(FuncName) _mkFuncPtr(FuncName)
#define mkFuncPtrType(FuncName) _mkFuncPtrType(FuncName)
#define mkFuncHint(FuncName) _mkFuncHint(FuncName)

#define FuncName HtmlHelpA
#define StrType LPCSTR
#include "sub.c"

#undef FuncName
#undef StrType
#define FuncName HtmlHelpW
#define StrType LPCWSTR
#include "sub.c"
