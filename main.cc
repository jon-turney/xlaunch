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

#include "window/util.h"
#include "window/wizard.h"
#include "resources/resources.h"
#include "config.h"
#include "file.h"

#include <prsht.h>
#include <commctrl.h>
#include <htmlhelp.h>
#include <stdio.h>
#include <sys/cygwin.h>
#include <stdexcept>

#include <X11/Xlib.h>

#ifdef _DEBUG
static bool debug = true;
#else
static bool debug = false;
#endif

/// @brief Send WM_ENDSESSION to all program windows.
/// This will shutdown the started xserver
BOOL CALLBACK KillWindowsProc(HWND hwnd, LPARAM lParam)
{
    SendMessage(hwnd, WM_ENDSESSION, 0, 0);
    return TRUE;
}

/// @brief Actual wizard implementation.
/// This is based on generic CWizard but handles the special dialogs
class CMyWizard : public CWizard
{
    public:
    private:
	CConfig config; /// Storage for config options.
    public:
        /// @brief Constructor.
        /// Set wizard pages.
        CMyWizard() : CWizard()
        {
          AddPage(IDD_DISPLAY, IDS_DISPLAY_CAPTION, IDS_DISPLAY_TITLE, IDS_DISPLAY_SUBTITLE);
          AddPage(IDD_CLIENTS, IDS_CLIENTS_CAPTION, IDS_CLIENTS_TITLE, IDS_CLIENTS_SUBTITLE);
          AddPage(IDD_PROGRAM, IDS_PROGRAM_CAPTION, IDS_PROGRAM_TITLE, IDS_PROGRAM_SUBTITLE);
          AddPage(IDD_XDMCP, IDS_XDMCP_CAPTION, IDS_XDMCP_TITLE, IDS_XDMCP_SUBTITLE);
          AddPage(IDD_EXTRA, IDS_EXTRA_CAPTION, IDS_EXTRA_TITLE, IDS_EXTRA_SUBTITLE);
          AddPage(IDD_FINISH, IDS_FINISH_CAPTION, IDS_FINISH_TITLE, IDS_FINISH_SUBTITLE);
        }

	virtual void LoadConfig(const char *filename)
	{
	    try {
		config.Load(filename);
	    } catch (std::runtime_error &e)
	    {
		printf("Error: %s\n", e.what());
	    }
	}

        /// @brief Handle the PSN_WIZNEXT message.
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise.
	virtual BOOL WizardNext(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("%s %d\n", __FUNCTION__, index);
#endif
	    switch (PageID(index))
	    {
		case IDD_DISPLAY:
                    // Check for select window mode
		    if (IsDlgButtonChecked(hwndDlg, IDC_MULTIWINDOW))
			config.window = CConfig::MultiWindow;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_FULLSCREEN))
			config.window = CConfig::Fullscreen;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_WINDOWED))
			config.window = CConfig::Windowed;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_NODECORATION))
			config.window = CConfig::Nodecoration;
		    else
		    {
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, -1);
			return TRUE;
		    }
                    // Get selected display number
		    {
			char buffer[512];
			GetDlgItemText(hwndDlg, IDC_DISPLAY, buffer, 512);
			buffer[511] = 0;
			config.display = buffer;
                    }
                    // Check for valid input
                    if (config.display.empty())
                      {
			MessageBox(hwndDlg,"Please fill in a display number.","Error",MB_OK);
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, -1);
                      }
                    else
                        SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_CLIENTS);
		    return TRUE;
		case IDD_CLIENTS:
                    // Check for select client startup method
		    if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT))
		    {
			config.client = CConfig::StartProgram;
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_PROGRAM);
		    } else if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP))
		    {
			config.client = CConfig::XDMCP;
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_XDMCP);
		    } else if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT_NONE))
		    {
			config.client = CConfig::NoClient;
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_EXTRA);
		    } else
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, -1);
		    return TRUE;
		case IDD_PROGRAM:
                    // Check whether local or remote client should be started
		    if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT_LOCAL))
                      {
			config.local = true;
			config.protocol = "";
                      }
		    else if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT_REMOTE))
                      {
			char buffer[512];
			config.local = false;
			GetDlgItemText(hwndDlg, IDC_CLIENT_PROTOCOL, buffer, 512);
			buffer[511] = 0;
			config.protocol = buffer;
                      }
		    else
		    {
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, -1);
			return TRUE;
		    }
                    // Read program, user and host name
		    {
			char buffer[512];
			GetDlgItemText(hwndDlg, IDC_CLIENT_USER, buffer, 512);
			buffer[511] = 0;
			config.user = buffer;
			GetDlgItemText(hwndDlg, IDC_CLIENT_HOST, buffer, 512);
			buffer[511] = 0;
			config.host = buffer;
			GetDlgItemText(hwndDlg, IDC_CLIENT_PROGRAM, buffer, 512);
			buffer[511] = 0;
			config.localprogram = buffer;
			GetDlgItemText(hwndDlg, IDC_CLIENT_REMOTEPROGRAM, buffer, 512);
			buffer[511] = 0;
			config.remoteprogram = buffer;

                        if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT_SSH_KEYCHAIN))
                          config.keychain = true;
                        else
                          config.keychain = false;

                        if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT_SSH_TERMINAL))
                          config.terminal = true;
                        else
                          config.terminal = false;

			GetDlgItemText(hwndDlg, IDC_CLIENT_PROTOCOL_EXTRA_PARAMS, buffer, 512);
			buffer[511] = 0;
			config.extra_ssh = buffer;
		    }
                    // Check for valid input
		    if ((!config.local && (config.host.empty() || config.remoteprogram.empty())) || config.localprogram.empty())
                      {
			MessageBox(hwndDlg,"Please fill in details of the program to start", "Error", MB_OK);
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, -1);
                      }
		    else
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_EXTRA);
		    return TRUE;
		case IDD_XDMCP:
                    // Check for broadcast
		    if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP_BROADCAST))
			config.broadcast = true;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP_QUERY))
			config.broadcast = false;
		    else
		    {
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, -1);
			return TRUE;
		    }
                    // Check for indirect mode
		    if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP_INDIRECT))
			config.indirect = true;
		    else
			config.indirect = false;
                    // Read hostname
		    {
			char buffer[512];
			GetDlgItemText(hwndDlg, IDC_XDMCP_HOST, buffer, 512);
			buffer[511] = 0;
			config.xdmcp_host = buffer;
		    }
                    // Check for valid input
		    if (!config.broadcast && config.xdmcp_host.empty())
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, -1);
		    else
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_EXTRA);
                    if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP_TERMINATE))
                        config.xdmcpterminate = true;
                    else
                        config.xdmcpterminate = false;
		    return TRUE;
                case IDD_EXTRA:
                    // check for clipboard
                    if (IsDlgButtonChecked(hwndDlg, IDC_CLIPBOARD))
                        config.clipboard = true;
                    else
                        config.clipboard = false;
                    // check for wgl
                    if (IsDlgButtonChecked(hwndDlg, IDC_WGL))
                        config.wgl = true;
                    else
                        config.wgl = false;
                    // check for access control
                    if (IsDlgButtonChecked(hwndDlg, IDC_DISABLEAC))
                        config.disableac = true;
                    else
                        config.disableac = false;
                    // read parameters
		    {
			char buffer[512];
			GetDlgItemText(hwndDlg, IDC_EXTRA_PARAMS, buffer, 512);
			buffer[511] = 0;
			config.extra_params = buffer;
		    }
                    SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_FINISH);
                    return TRUE;
		default:
		    break;
	    }
	    return FALSE;
	}
        /// @brief Handle PSN_WIZFINISH message.
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise.
	virtual BOOL WizardFinish(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("%s %d\n", __FUNCTION__, index);
#endif
	    return FALSE;
	}
        /// @brief Handle PSN_WIZBACK message.
        /// Basicly handles switching to proper page (skipping XDMCP or program page
        /// if required).
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise.
	virtual BOOL WizardBack(HWND hwndDlg, unsigned index)
	{
	    switch (PageID(index))
	    {
		case IDD_PROGRAM:
		case IDD_XDMCP:
		    SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_CLIENTS);
		    return TRUE;
		case IDD_EXTRA:
		    switch (config.client)
		    {
			case CConfig::NoClient:
			    SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_CLIENTS);
			    return TRUE;
			case CConfig::StartProgram:
			    SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_PROGRAM);
			    return TRUE;
			case CConfig::XDMCP:
			    SetWindowLong(hwndDlg, DWLP_MSGRESULT, IDD_XDMCP);
			    return TRUE;
		    }
		    break;
	    }
	    return FALSE;
	}
        /// @brief Handle PSN_SETACTIVE message.
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise.
	virtual BOOL WizardActivate(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("%s %d\n", __FUNCTION__, index);
#endif
	    switch (PageID(index))
	    {
		case IDD_CLIENTS:
                    // Enable or disable XDMCP radiobutton and text
		    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP), config.window != CConfig::MultiWindow);
		    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP_DESC), config.window != CConfig::MultiWindow);
		    break;
	    }
	    return FALSE;
	}
	/// @brief Handle F1 and PSN_HELP message.
	/// @param hwndDlg Handle to active page dialog.
	/// @param index Index of current page.
	virtual void WizardHelp(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("%s %d\n", __FUNCTION__, index);
#endif
	    int offset = 0;
	    int idd = PageID(index);
	    if (idd==IDD_DISPLAY) offset = 0;
	    else if (idd==IDD_CLIENTS) offset = 1;
	    else if (idd==IDD_PROGRAM) offset = 2;
	    else if (idd==IDD_XDMCP) offset = 3;
	    else if (idd==IDD_EXTRA) offset = 4;
	    else if (idd==IDD_FINISH) offset = 5;

            // first look for local chm file, so we work uninstalled
            const char *chmFile;
            if (FileExists("htmlhelp/xlaunch.chm"))
              chmFile = "htmlhelp/xlaunch.chm";
            else
              chmFile = DOCDIR "/xlaunch.chm";

            // convert the .chm file path to it's Windows form before passing to
            // HtmlHelp()
            char *winPath = (char *)(cygwin_create_path(CCP_POSIX_TO_WIN_A | CCP_ABSOLUTE, chmFile));

#ifdef _DEBUG
            printf("chmfile %s = %s\n", chmFile, winPath);
#endif

            HtmlHelp(hwndDlg, winPath, HH_HELP_CONTEXT, 500 + offset);

            free(winPath);

	}

    protected:
        /// @brief Enable or disable the control for remote clients.
        /// @param hwndDlg Handle to active page dialog.
        /// @param state State of control group.
	void EnableRemoteProgramGroup(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROTOCOL), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_HOST), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_USER), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROTOCOL_DESC), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_HOST_DESC), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_USER_DESC), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_REMOTEPROGRAM), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_REMOTEPROGRAM_DESC), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_SSH_TERMINAL), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_SSH_KEYCHAIN), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROTOCOL_EXTRA_PARAMS_DESC), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROTOCOL_EXTRA_PARAMS), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROGRAM), !state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROGRAM_DESC), !state);
	}
        /// @brief Enable or disable the control for XDMCP connection.
        /// @param hwndDlg Handle to active page dialog.
        /// @param state State of control group.
	void EnableXDMCPQueryGroup(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP_HOST), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP_INDIRECT), state);
	}
        /// @brief Fill program box with default values.
        /// @param hwndDlg Handle to active page dialog.
	void FillProgramBox(HWND hwndDlg)
	{
	    HWND cbwnd = GetDlgItem(hwndDlg, IDC_CLIENT_PROGRAM);
	    if (cbwnd == NULL)
		return;
	    SendMessage(cbwnd, CB_RESETCONTENT, 0, 0);
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "xterm");
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "~/.xinitrc");
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "openbox-session");
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "wmaker");
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "startkde");
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "gnome-session");
	    SendMessage(cbwnd, CB_SETCURSEL, 0, 0);
	}
        /// @brief Fill protocol box with default values.
        /// @param hwndDlg Handle to active page dialog.
	void FillProtocolBox(HWND hwndDlg)
	{
	    HWND cbwnd = GetDlgItem(hwndDlg, IDC_CLIENT_PROTOCOL);
	    if (cbwnd == NULL)
		return;
	    SendMessage(cbwnd, CB_RESETCONTENT, 0, 0);
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "ssh");
	    SendMessage(cbwnd, CB_ADDSTRING, 0, (LPARAM) "rsh");
	    SendMessage(cbwnd, CB_SETCURSEL, 0, 0);
	}
	void ShowSaveDialog(HWND parent)
	{
	    char szTitle[512];
	    char szFilter[512];
	    char szFileTitle[512];
	    char szFile[MAX_PATH];
	    HINSTANCE hInst = GetModuleHandle(NULL);

	    LoadString(hInst, IDS_SAVE_TITLE, szTitle, sizeof(szTitle));
	    LoadString(hInst, IDS_SAVE_FILETITLE, szFileTitle, sizeof(szFileTitle));
	    LoadString(hInst, IDS_SAVE_FILTER, szFilter, sizeof(szFilter));
	    for (unsigned i=0; szFilter[i]; i++)
		if (szFilter[i] == '%')
		    szFilter[i] = '\0';

	    strcpy(szFile, "config.xlaunch");

	    OPENFILENAME ofn;
	    memset(&ofn, 0, sizeof(OPENFILENAME));
	    ofn.lStructSize = sizeof(OPENFILENAME);
	    ofn.hwndOwner = parent;
	    ofn.lpstrFilter = szFilter;
	    ofn.lpstrFile= szFile;
	    ofn.nMaxFile = sizeof(szFile)/ sizeof(*szFile);
	    ofn.lpstrFileTitle = szFileTitle;
	    ofn.nMaxFileTitle = sizeof(szFileTitle);
	    ofn.lpstrInitialDir = (LPSTR)NULL;
	    ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
	    ofn.lpstrTitle = szTitle;

	    if (GetSaveFileName(&ofn))
	    {
		try {
		    config.Save(ofn.lpstrFile);
		} catch (std::runtime_error &e)
		{
		    printf("Error: %s\n", e.what());
		}
	    }
	}
    public:

        /// @brief Handle messages fo the dialog pages.
        /// @param hwndDlg Handle of active dialog.
        /// @param uMsg Message code.
        /// @param wParam Message parameter.
        /// @param lParam Message parameter.
        /// @param psp Handle to sheet paramters.
        virtual INT_PTR PageDispatch(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, PROPSHEETPAGE *psp)
        {
            switch (uMsg)
            {
                case WM_INITDIALOG:
                    switch (PageID(PageIndex(psp)))
                    {
                        case IDD_DISPLAY:
			    psp->dwFlags |= PSP_HASHELP;
                            // Init display dialog. Enable correct check buttons
			    switch (config.window)
			    {
				default:
				case CConfig::MultiWindow:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_MULTIWINDOW);
				    break;
				case  CConfig::Fullscreen:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_FULLSCREEN);
				    break;
				case  CConfig::Windowed:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_WINDOWED);
				    break;
				case  CConfig::Nodecoration:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_NODECORATION);
				    break;
			    }
                            // Set display number
                            SetDlgItemText(hwndDlg, IDC_DISPLAY, config.display.c_str());
                            break;
                        case IDD_CLIENTS:
			    psp->dwFlags |= PSP_HASHELP;
                            // Init client dialog. Enable correct check buttons
			    switch (config.client)
			    {
				default:
				case CConfig::NoClient:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_NONE, IDC_CLIENT, IDC_CLIENT_NONE);
				    break;
				case CConfig::StartProgram:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_NONE, IDC_CLIENT, IDC_CLIENT);
				    break;
				case CConfig::XDMCP:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_NONE, IDC_CLIENT, IDC_XDMCP);
				    break;
			    }
                            break;
			case IDD_PROGRAM:
			    psp->dwFlags |= PSP_HASHELP;
                            // Init program dialog. Check local and remote buttons
                            CheckRadioButton(hwndDlg, IDC_CLIENT_LOCAL, IDC_CLIENT_REMOTE, config.local?IDC_CLIENT_LOCAL:IDC_CLIENT_REMOTE);
			    EnableRemoteProgramGroup(hwndDlg, config.local?FALSE:TRUE);
                            // Fill combo boxes
			    FillProgramBox(hwndDlg);
			    FillProtocolBox(hwndDlg);
                            // Set edit fields
			    if (!config.localprogram.empty())
				SetDlgItemText(hwndDlg, IDC_CLIENT_PROGRAM, config.localprogram.c_str());
			    if (!config.remoteprogram.empty())
				SetDlgItemText(hwndDlg, IDC_CLIENT_REMOTEPROGRAM, config.remoteprogram.c_str());
			    SetDlgItemText(hwndDlg, IDC_CLIENT_PROTOCOL, config.protocol.c_str());
			    SetDlgItemText(hwndDlg, IDC_CLIENT_USER, config.user.c_str());
			    SetDlgItemText(hwndDlg, IDC_CLIENT_HOST, config.host.c_str());
			    CheckDlgButton(hwndDlg, IDC_CLIENT_SSH_KEYCHAIN, config.keychain?BST_CHECKED:BST_UNCHECKED);
			    CheckDlgButton(hwndDlg, IDC_CLIENT_SSH_TERMINAL, config.terminal?BST_CHECKED:BST_UNCHECKED);
                            SetDlgItemText(hwndDlg, IDC_CLIENT_PROTOCOL_EXTRA_PARAMS, config.extra_ssh.c_str());
			    break;
			case IDD_XDMCP:
			    psp->dwFlags |= PSP_HASHELP;
                            // Init XDMCP dialog. Check broadcast and indirect button
                            CheckRadioButton(hwndDlg, IDC_XDMCP_QUERY, IDC_XDMCP_BROADCAST, config.broadcast?IDC_XDMCP_BROADCAST:IDC_XDMCP_QUERY);
                            CheckDlgButton(hwndDlg, IDC_XDMCP_INDIRECT, config.indirect?BST_CHECKED:BST_UNCHECKED);
			    EnableXDMCPQueryGroup(hwndDlg, config.broadcast?FALSE:TRUE);
                            // Set hostname
			    SetDlgItemText(hwndDlg, IDC_XDMCP_HOST, config.xdmcp_host.c_str());
			    CheckDlgButton(hwndDlg, IDC_XDMCP_TERMINATE, config.xdmcpterminate?BST_CHECKED:BST_UNCHECKED);
			    break;
                        case IDD_EXTRA:
			    psp->dwFlags |= PSP_HASHELP;
                            CheckDlgButton(hwndDlg, IDC_CLIPBOARD, config.clipboard?BST_CHECKED:BST_UNCHECKED);
                            CheckDlgButton(hwndDlg, IDC_WGL, config.wgl?BST_CHECKED:BST_UNCHECKED);
                            CheckDlgButton(hwndDlg, IDC_DISABLEAC, config.disableac?BST_CHECKED:BST_UNCHECKED);
                            SetDlgItemText(hwndDlg, IDC_EXTRA_PARAMS, config.extra_params.c_str());
                            break;
			case IDD_FINISH:
			    psp->dwFlags |= PSP_HASHELP;
			    break;

                    }
                case WM_COMMAND:
                    // Handle control messages
                    switch (LOWORD(wParam))
                    {
                        // Handle clicks on images. Check proper radiobutton
                        case IDC_MULTIWINDOW_IMG:
                        case IDC_FULLSCREEN_IMG:
                        case IDC_WINDOWED_IMG:
                        case IDC_NODECORATION_IMG:
                            CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, LOWORD(wParam)-4);
                            SetFocus(GetDlgItem(hwndDlg, LOWORD(wParam)-4));
                            break;
                        // Disable unavailable controls
                        case IDC_CLIENT_REMOTE:
                        case IDC_CLIENT_LOCAL:
			    EnableRemoteProgramGroup(hwndDlg, LOWORD(wParam) == IDC_CLIENT_REMOTE);
                            break;
			case IDC_XDMCP_QUERY:
			case IDC_XDMCP_BROADCAST:
			    EnableXDMCPQueryGroup(hwndDlg, LOWORD(wParam) == IDC_XDMCP_QUERY);
			    break;
			case IDC_FINISH_SAVE:
			    ShowSaveDialog(hwndDlg);
			    break;
                    }
            }
            // pass messages to parent
            return CWizard::PageDispatch(hwndDlg, uMsg, wParam, lParam, psp);
        }

        /// @brief Try to connect to server.
        /// Repeat until successful, server died or maximum number of retries
        /// reached.
        Display *WaitForServer(HANDLE serverProcess, std::string &display)
        {
            int     ncycles  = 120;         /* # of cycles to wait */
            int     cycles;                 /* Wait cycle count */
            Display *xd;

            for (cycles = 0; cycles < ncycles; cycles++) {
              if ((xd = XOpenDisplay(display.c_str()))) {
                    return xd;
                }
                else {
                    if (WaitForSingleObject(serverProcess, 1000) == WAIT_TIMEOUT)
                        continue;
                    else
                      break;
                }
            }
            return NULL;
        }

        /// @brief Do the actual start of X server and clients
	void StartUp()
	{
	    std::string buffer;
	    std::string client;
	    BOOL showconsole = FALSE;

            // Construct display strings
	    std::string display_id = ":" + config.display;
	    std::string display = display_id + ".0";

            // Build X server commandline
#if defined (__CYGWIN__)
	    buffer = "XWin " + display_id + " ";
#elif defined (__MINGW__)
	    buffer = "Xming " + display_id + " ";
#else
#error "Don't know X server name"
#endif
	    switch (config.window)
	    {
		case CConfig::MultiWindow:
		    buffer += "-multiwindow ";
		    break;
		case CConfig::Fullscreen:
		    buffer += "-fullscreen ";
		    break;
		case CConfig::Nodecoration:
		    buffer += "-nodecoration ";
		    break;
		default:
		    break;
	    }
            // Add XDMCP parameter
	    if (config.client == CConfig::XDMCP)
	    {
		if (config.broadcast)
		    buffer += "-broadcast ";
		else
		{
		    if (config.indirect)
			buffer += "-indirect ";
		    else
			buffer += "-query ";
		    buffer += config.xdmcp_host;
            buffer += " ";
		}
                if (config.xdmcpterminate)
                    buffer += "-terminate ";
	    }
            if (config.clipboard)
                buffer += "-clipboard ";
            else
                buffer += "-noclipboard ";
            if (config.wgl)
                buffer += "-wgl ";
            else
                buffer += "-nowgl ";
            if (config.disableac)
                buffer += "-ac ";
            if (!config.extra_params.empty())
            {
                buffer += config.extra_params;
                buffer += " ";
            }

            // Construct client commandline
	    if (config.client == CConfig::StartProgram)
	    {
		if (!config.local)
		{
		    char cmdline[512];
                    std::string host = config.host;
                    std::string rsh_l_option = "";
                    if (!config.user.empty())
                      {
                        host = config.user + "@" + config.host;
                        rsh_l_option = "-l " + config.user;
                      }

                    if (config.protocol == "ssh")
                      {
                        snprintf(cmdline,512,"ssh -Y %s %s %s",
                                 host.c_str(), config.extra_ssh.c_str(), config.remoteprogram.c_str());
                        client = cmdline;

                        if (config.keychain)
                          {
                            client = ". ~/.keychain/$HOSTNAME-sh ; " + client;
                            client = "bash -c \"" + client + "\"";
                          }

                        if (config.terminal)
                          {
                            showconsole = TRUE;
                            client = "mintty -e " + client;
                          }
                      }
                    else if (config.protocol == "rsh")
                      {
                        snprintf(cmdline,512,"rsh %s %s %s",
                                 rsh_l_option.c_str(),
                                 config.host.c_str(),config.remoteprogram.c_str());
                        client = cmdline;
                      }
		} else {
#if defined (__CYGWIN__)
                  client = "bash -l -c \"" + config.localprogram + "\"";
#elif defined (__MINGW__)
                  client = config.localprogram.c_str();
#else
#error "Don't know how to start child process on target"
#endif
                }
	    }

            // Prepare program startup
     	    STARTUPINFO si, sic;
	    PROCESS_INFORMATION pi, pic;
	    HANDLE handles[2];
	    DWORD hcount = 0;
            Display *dpy = NULL;

	    ZeroMemory( &si, sizeof(si) );
	    si.cb = sizeof(si);
	    ZeroMemory( &pi, sizeof(pi) );
	    ZeroMemory( &sic, sizeof(sic) );
	    sic.cb = sizeof(sic);
	    ZeroMemory( &pic, sizeof(pic) );

	    // Start X server process
            if (debug)
              printf("Server: %s\n", buffer.c_str());

	    if( !CreateProcess( NULL, (CHAR*)buffer.c_str(), NULL, NULL,
                        FALSE, 0, NULL, NULL, &si, &pi ))
		throw win32_error("CreateProcess failed");
	    handles[hcount++] = pi.hProcess;

	    if (!client.empty())
	    {
                // Set DISPLAY variable
		SetEnvironmentVariable("DISPLAY",display.c_str());

                // Wait for server to startup
                dpy = WaitForServer(pi.hProcess, display);
                if (dpy == NULL)
                {
                    while (hcount--)
                        TerminateProcess(handles[hcount], (DWORD)-1);
		    throw std::runtime_error("Connection to server failed");
                }

                if (debug)
                  printf("Client: %s\n", client.c_str());

                // Hide console window, unless showconsole is true
		sic.dwFlags = STARTF_USESHOWWINDOW;
		sic.wShowWindow = SW_HIDE;
		if (showconsole) sic.wShowWindow = SW_NORMAL;

		// Start the child process.
		if( !CreateProcess( NULL, (CHAR*)client.c_str(), NULL, NULL,
                            FALSE, 0, NULL, NULL, &sic, &pic ))
		{
                    DWORD err = GetLastError();
                    while (hcount--)
                        TerminateProcess(handles[hcount], (DWORD)-1);
		    throw win32_error("CreateProcess failed", err);
		}
		handles[hcount++] = pic.hProcess;
	    }

	    // Wait until any child process exits.
	    WaitForMultipleObjects(hcount, handles, FALSE, INFINITE);

            // Check if X server is still running, but only when we started a local program
            if (config.local)
            {
#ifdef _DEBUG
	    printf("killing process\n");
#endif

	    DWORD exitcode;
	    GetExitCodeProcess(pi.hProcess, &exitcode);
	    unsigned counter = 0;
	    while (exitcode == STILL_ACTIVE)
	    {
		if (++counter > 10)
                  {
                    if (debug)
                      printf("X server didn't stop after WM_ENDSESSION, force terminating process\n");

		    TerminateProcess(pi.hProcess, (DWORD)-1);
                  }
		else
		    // Shutdown X server (the soft way!)
		    EnumThreadWindows(pi.dwThreadId, KillWindowsProc, 0);

		Sleep(500);
		GetExitCodeProcess(pi.hProcess, &exitcode);
	    }
	    // Kill the client
	    TerminateProcess(pic.hProcess, (DWORD)-1);
            }

	    // Close process and thread handles.
	    CloseHandle( pi.hProcess );
	    CloseHandle( pi.hThread );
	    CloseHandle( pic.hProcess );
	    CloseHandle( pic.hThread );
	}
};

void usage(void)
{
  printf("Usage: xlaunch [OPTION]...\n");
  printf("GUI tool for configuring and starting XWin\n");
  printf("\n");
  printf("  -debug         enable debug output\n");
  printf("  -load filename load configuration from file\n");
  printf("  -run filename  load and run configuration from file\n");
  printf("  -help          display this help and exit\n");
  printf("  -version       output version information and exit\n");
  printf("\n");
  printf("Report xlaunch bugs to " PACKAGE_BUGREPORT "\n");
}

int main(int argc, char **argv)
{
    // It seems that HtmlHelp() may need to create a temporary file using
    // GetTempPath().  Create a full Win32 environment, so that it can find a
    // suitable location, rather than falling back to %WINDIR%, which will fail
    // if we don't have Adminstrator privileges.
    cygwin_internal(CW_SYNC_WINENV);

    try {
        InitCommonControls();
        CMyWizard dialog;

	bool skip_wizard = false;

	for (int i = 1; i < argc; i++)
	{
	    if (argv[i] == NULL)
		continue;

	    std::string arg(argv[i]);
            if (arg == "-help")
              {
                usage();
                return 0;
              }
            else if (arg == "-version")
              {
                printf(PACKAGE_STRING);
                return 0;
              }
            else if (arg == "-debug")
              {
                debug = true;
              }
            else if (arg == "-load" && i + 1 < argc)
              {
		i++;
		dialog.LoadConfig(argv[i]);
              }
            else if (arg == "-run" && i + 1 < argc)
              {
		i++;
		dialog.LoadConfig(argv[i]);
		skip_wizard = true;
              }
	}

	int ret = 0;
        if (skip_wizard || (ret =dialog.ShowModal()) != 0)
	    dialog.StartUp();
#ifdef _DEBUG
	printf("return %d\n", ret);
#endif
	return 0;
    } catch (std::runtime_error &e)
    {
        printf("Error: %s\n", e.what());
        return -1;
    }
}
