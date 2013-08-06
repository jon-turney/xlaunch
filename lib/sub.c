
HWND WINAPI FuncName(HWND hwndCaller, StrType pszFile, UINT uCommand, DWORD_PTR dwData)
{
	HMODULE hMod = g_hmodHHCtrl;
	mkFuncPtrType(FuncName) pFunc;
	if (!hMod && g_fTriedAndFailed == FALSE) {
		if (hMod = TryLoadingModule())
			g_hmodHHCtrl = hMod;
		else
			goto Fail;
	}

	pFunc = mkFuncPtr(FuncName);
	if (pFunc) goto CallFunc;
	if (pFunc = (mkFuncPtrType(FuncName))GetProcAddress(g_hmodHHCtrl, (LPCSTR)mkFuncHint(FuncName))) {
		mkFuncPtr(FuncName) = pFunc;
CallFunc:
		return pFunc((HWND)hwndCaller, pszFile, uCommand, dwData);
	}

Fail:
	g_fTriedAndFailed = ~FALSE;
	return NULL;
}
