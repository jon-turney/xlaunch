/*
 * Copyright (c) 2012 Jon Turney
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

#include <windows.h>
#include <stdio.h>
#include <stdexcept>

#include "config.h"
#include "window/util.h"

#include <X11/Xlib.h>

extern bool debug;

/// @brief Send WM_ENDSESSION to all program windows.
/// This will shutdown the started xserver
BOOL CALLBACK KillWindowsProc(HWND hwnd, LPARAM lParam)
{
    SendMessage(hwnd, WM_ENDSESSION, 0, 0);
    return TRUE;
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
void StartUp(CConfig config)
{
  std::string buffer;
  std::string client;
  BOOL showconsole = FALSE;
  int filedes[2];

  // Construct display string
  std::string display_id;
  if (!config.display_auto)
    display_id = ":" + config.display;
  else
    {
      if (pipe(filedes) == 0)
        {
          char displayfd_write_buf[256];
          sprintf(displayfd_write_buf, "%d", filedes[1]);
          display_id = "-displayfd ";
          display_id += displayfd_write_buf;
        }
      else
        {
          throw std::runtime_error("pipe() for -displayfd failed");
        }
    }

  // Build X server commandline
  buffer = "XWin " + display_id + " ";

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
        client = "bash -l -c \"" + config.localprogram + "\"";
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
      if (config.display_auto)
        {
          char displayfd_read_buf[256];

          // wait for the server to write the DISPLAY number to the displayfd pipe...
          int length = read(filedes[0], displayfd_read_buf, 255);

          if (length < 0)
            throw std::runtime_error("reading displayfd pipe failed");
          displayfd_read_buf[length] = '\0';

          if (debug)
            printf("Allocated display number: %s\n", displayfd_read_buf);

          display_id = ":";
          display_id += displayfd_read_buf;
        }

      std::string display = "localhost" + display_id + ".0";

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
