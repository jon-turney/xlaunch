/*
 * Copyright (c) 2005 Alexander Gottwald
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */

#include <stdio.h>
#include "wizard.h"
#include "util.h"
#include "../resources/resources.h"

CWizard::CWizard() : pages()
{
};

void CWizard::AddPage(const PROPSHEETPAGE &page)
{
    pages.push_back(page);
}

void CWizard::AddPage(const char *page, HINSTANCE instance)
{
    PROPSHEETPAGE psp;
    if (instance == NULL)
        instance = GetModuleHandle(NULL);

    memset(&psp, 0, sizeof(psp));
    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = PSP_DEFAULT;
    psp.hInstance = instance;
    psp.pszTemplate = page;
    psp.pfnDlgProc = WizardDialogProc;
    psp.lParam = (LPARAM)this;

    AddPage(psp);
}

#define countof(x) (sizeof(x)/sizeof(x[0]))

static const char *
LoadStringFromResource(UINT id)
{
  static char szBuffer[256];
  int nCount = countof(szBuffer);
  int nLen = LoadString(GetModuleHandle(NULL), id, szBuffer, nCount);

  // XXX: if nLen is >= nCount-1, we should try again with a larger buffer,
  // as we can't be sure we got the whole string

  if (nLen > 0)
      return szBuffer;

  return NULL;
}

void CWizard::AddPage(DWORD id, DWORD caption, DWORD title, DWORD subtitle, HINSTANCE instance)
{
    PROPSHEETPAGE psp;
    if (instance == NULL)
        instance = GetModuleHandle(NULL);

    memset(&psp, 0, sizeof(psp));
    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = PSP_DEFAULT;
#if _WIN32_IE >= 0x0500
    if (title != 0)
    {
        psp.dwFlags |= PSP_USEHEADERTITLE;
        psp.pszHeaderTitle = MAKEINTRESOURCE(title);
    }
    if (subtitle != 0)
    {
        psp.dwFlags |= PSP_USEHEADERSUBTITLE;
        psp.pszHeaderSubTitle = MAKEINTRESOURCE(subtitle);
    }
#endif
    if (caption != 0)
    {
        static const char *prefix = "XLaunch - ";
        psp.dwFlags |= PSP_USETITLE;

        const char *szCaption = LoadStringFromResource(caption);
        char *szAugmentedCaption = (char *)malloc(strlen(szCaption) +  strlen(prefix) + 1);

        strcpy(szAugmentedCaption, prefix);
        strcat(szAugmentedCaption, szCaption);

        psp.pszTitle = szAugmentedCaption;
    }

    psp.hInstance = instance;
    psp.pszTemplate = MAKEINTRESOURCE(id);
    psp.pfnDlgProc = WizardDialogProc;
    psp.lParam = (LPARAM)this;

    AddPage(psp);
}

HWND CWizard::CreateWindowHandle()
{
    PROPSHEETHEADER psh;
    HWND ret;

    PrepareSheetHeader(psh, FALSE);
    ret = (HWND)PropertySheet(&psh);
    free(psh.phpage);
    if (ret == NULL)
        throw win32_error("PropertySheet failed");
    return ret;
}

int CWizard::ShowModal()
{
    PROPSHEETHEADER psh;
    int ret;

    PrepareSheetHeader(psh, TRUE);
    ret = PropertySheet(&psh);
    free(psh.phpage);
    return ret;
}

/*
   there seems to be no way to get some extra data down to the PSCB_INITIALIZED
   callback, so use a static (terrible!)
*/
static CWizard *thisWizard;

static int CALLBACK
PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
  switch (uMsg)
    {
    case PSCB_INITIALIZED:
      {
        /* PSH_USEICONID only sets the small icon, so we must set the big icon ourselves */
        SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_XLAUNCH)));

        /* Subclass the standard dialog wndproc */
        thisWizard->SetHandle(hwndDlg);
        thisWizard->Subclass();

        /* Add "About..." item to the system menu */
        HMENU sysMenu = GetSystemMenu(hwndDlg, FALSE);
        AppendMenu(sysMenu, MF_SEPARATOR, 0, 0);
        AppendMenu(sysMenu, MF_ENABLED | MF_STRING, IDM_ABOUTBOX, "About...");
        return TRUE;
      }
    }
  return TRUE;
}

void CWizard::PrepareSheetHeader(PROPSHEETHEADER &psh, BOOL modal)
{
    HPROPSHEETPAGE *phpage = (HPROPSHEETPAGE*)malloc(pages.size() * sizeof(HPROPSHEETPAGE));
    DWORD modeflag;

    if (modal)
        modeflag = 0;
    else
        modeflag = PSH_MODELESS;

    for (unsigned i = 0; i < pages.size(); i++)
    {
        phpage[i] = CreatePropertySheetPage(&pages[i]);
        if (phpage[i] == NULL)
        {
            DWORD err = GetLastError();
            free(phpage);
            throw win32_error("CreatePropertySheetPage failed", err);
        }
    }

    memset(&psh, 0, sizeof(psh));
    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_HASHELP | PSH_USEICONID | PSH_USECALLBACK | PSH_WIZARD97 | modeflag;
    psh.hwndParent = NULL;
    psh.hInstance = GetModuleHandle(NULL);
    psh.pszIcon = MAKEINTRESOURCE(IDI_XLAUNCH);
    psh.pszCaption = (LPSTR) "XLaunch";
    psh.nPages = pages.size();
    psh.nStartPage = 0;
    psh.phpage = phpage;
    psh.pfnCallback = PropSheetProc;

    thisWizard = this;
}

DWORD CWizard::PageID(unsigned index)
{
    if (index < pages.size() && IS_INTRESOURCE(pages[index].pszTemplate))
      return (DWORD)(INT_PTR)pages[index].pszTemplate;
    return (DWORD)-1;
}

unsigned CWizard::PageIndex(PROPSHEETPAGE *psp)
{
    for (unsigned i = 0; i < pages.size(); i++)
    {
	if (IS_INTRESOURCE(psp->pszTemplate) || IS_INTRESOURCE(pages[i].pszTemplate ))
	{
	    if (psp->pszTemplate == pages[i].pszTemplate)
		return i;
	}
	else if (psp->pszTemplate && pages[i].pszTemplate)
	{
	    if (strcmp(psp->pszTemplate, pages[i].pszTemplate) == 0)
		return i;
	}
    }
    return (unsigned)-1;
}

INT_PTR CWizard::DlgDispatch(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return CBaseDialog::DlgDispatch(hwndDlg, uMsg, wParam, lParam);
}

INT_PTR CWizard::PageDispatch(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, PROPSHEETPAGE *psp)
{
    LPNMHDR pnmh = (LPNMHDR)lParam;
    DWORD flags;
    unsigned pageindex;
    switch (uMsg)
    {
	case WM_HELP:
	    WizardHelp(hwndDlg, PageIndex(psp));
	    break;
	case WM_NOTIFY:
	    switch (pnmh->code)
	    {
		case PSN_SETACTIVE:
#ifdef _DEBUG
                    printf("PSN_SETACTIVE %d\n", PageIndex(psp));
#endif
		    pageindex = PageIndex(psp);
		    if (pageindex != (unsigned)-1)
		    {
			flags = 0;
			if (pageindex > 0)
			    flags |= PSWIZB_BACK;
			if ((unsigned)pageindex + 1 == pages.size())
			    flags |= PSWIZB_FINISH;
			if ((unsigned)pageindex + 1 < pages.size())
                            flags |= PSWIZB_NEXT;
                        PropSheet_SetWizButtons(GetParent(hwndDlg), flags);
		    }
		    WizardActivate(hwndDlg, pageindex);
		    break;
		case PSN_WIZNEXT:
                    if (WizardNext(hwndDlg, PageIndex(psp)))
                        return TRUE;
		    break;
		case PSN_WIZBACK:
                    if (WizardBack(hwndDlg, PageIndex(psp)))
                        return TRUE;
		    break;
                case PSN_WIZFINISH:
                    if (WizardFinish(hwndDlg, PageIndex(psp)))
                        return TRUE;
                    DestroyWindow(GetParent(hwndDlg));
		    break;
		case PSN_HELP:
		    WizardHelp(hwndDlg, PageIndex(psp));
		    break;
		case PSN_RESET:
                    if (WizardReset(hwndDlg, PageIndex(psp)))
                        return TRUE;
                    DestroyWindow(GetParent(hwndDlg));
		    break;
	    }
    }
    return DlgDispatch(hwndDlg, uMsg, wParam, lParam);
}

INT_PTR CALLBACK CWizard::WizardDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MessageDebug::debug(hwndDlg, uMsg, wParam, lParam, __FUNCTION__);
    PROPSHEETPAGE *psp = (PROPSHEETPAGE*)lParam;
    switch (uMsg)
    {
	case WM_INITDIALOG:
	    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)psp);
	    break;
    }
    psp = (PROPSHEETPAGE*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    CWizard* wizard = psp?(CWizard*)psp->lParam:NULL;
    if (wizard != NULL)
        return wizard->PageDispatch(hwndDlg, uMsg, wParam, lParam, psp);
    return FALSE;
}

static INT_PTR CALLBACK
AboutDlgProc(HWND hwndDialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
      {
      case WM_INITDIALOG:
        {
          HICON hIcon;

          /* Set Window not to show in the task bar */
          SetWindowLongPtr(hwndDialog, GWL_EXSTYLE,
                           GetWindowLongPtr(hwndDialog, GWL_EXSTYLE) & ~WS_EX_APPWINDOW);

          /* Set icon */
          hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_XLAUNCH));
          PostMessage(hwndDialog, WM_SETICON, ICON_BIG, (LPARAM) hIcon);

          return TRUE;
        }

      case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
          {
            EndDialog(hwndDialog, 0);
            return TRUE;
          }
      case WM_CLOSE:
        EndDialog(hwndDialog, 0);
        return TRUE;
      }

    return FALSE;
}

LRESULT CWizard::Dispatch(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MessageDebug::debug(hwndDlg, uMsg, wParam, lParam, __FUNCTION__);

    switch (uMsg)
      {
      case WM_SYSCOMMAND:
        if (LOWORD(wParam & 0xfff0) == IDM_ABOUTBOX)
          {
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwndDlg, AboutDlgProc);
            return 0;
          }
      }

    return CBaseDialog::Dispatch(hwndDlg, uMsg, wParam, lParam);
}
