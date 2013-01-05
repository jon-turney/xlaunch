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

#include <stdio.h>
#include <stdexcept>

#include "config.h"
#include "window/util.h"

#include <errno.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

extern bool debug;

static void
Execute(const char *s)
{
  char *argv[4];

  argv[0] = (char *)"sh";
  argv[1] = (char *)"-c";
  argv[2] = (char *) s;
  argv[3] = NULL;

  execv("/bin/sh", argv);
}

/*
 * return TRUE if we timeout waiting for pid to exit, FALSE otherwise.
 */
static Bool
processTimeout(int serverpid, int timeout, const char *string)
{
    int i = 0, pidfound = -1;
    static const char *laststring;
    int status;

    for (;;) {
        if ((pidfound = waitpid(serverpid, &status, WNOHANG)) == serverpid)
            break;
        if (timeout) {
            if (i == 0 && string != laststring)
                fprintf(stderr, "\r\nwaiting for %s ", string);
            else
                fprintf(stderr, ".");
            fflush(stderr);
            sleep(1);
        }
        if (++i > timeout)
            break;
    }
    if (i > 0) fputc('\n', stderr);     /* tidy up after message */
    laststring = string;
    return (serverpid != pidfound);
}

/*
 *    waitforserver - wait for X server to start up
 */
static Bool
waitforserver(int serverpid, const char *displayNum)
{
    int ncycles = 120;        /* # of cycles to wait */
    int cycles;               /* Wait cycle count */
    Display *xd = NULL;       /* server connection */

    for (cycles = 0; cycles < ncycles; cycles++) {
        if ((xd = XOpenDisplay(displayNum))) {
            return(TRUE);
        }
        else {
          if (!processTimeout(serverpid, 1, "X server to begin accepting connections"))
              break;
        }
    }

    fprintf(stderr, "giving up\n");

    return(FALSE);
}

static int
startServer(const char *server)
{
    sigset_t mask, old;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &old);

    int serverpid = fork();

    switch(serverpid) {
    case 0:
        /* Unblock */
        sigprocmask(SIG_SETMASK, &old, NULL);

        /*
         * don't hang on read/write to control tty
         */
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        /*
         * ignore SIGUSR1 in child.  The server
         * will notice this and send SIGUSR1 back
         * at xinit when ready to accept connections
         */
        signal(SIGUSR1, SIG_IGN);
        /*
         * prevent server from getting sighup from vhangup()
         * if client is xterm -L
         */
        setpgid(0,getpid());
        Execute(server);

        fprintf(stderr, "unable to run server \"%s\"\n", server);
        exit(EXIT_FAILURE);

        break;
    case -1:
        break;
    default:
        /*
         * don't nice server
         */
        setpriority(PRIO_PROCESS, serverpid, -1);

        /*
         * check if serverpid is valid
         */
        errno = 0;
        if(! processTimeout(serverpid, 0, "")) {
            serverpid = -1;
            break;
        }

        /*
         * Wait for SIGUSR1 or 15 seconds for server to become ready
         * to accept connections
         *
         * If your machine is substantially slower than 15 seconds,
         * you can easily adjust this value.
         */
        alarm(15);
        sigsuspend(&old);
        alarm(0);
        sigprocmask(SIG_SETMASK, &old, NULL);

        break;
    }

    return(serverpid);
}

void
shutdownServer()
{
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

  // Start X server process
  if (debug)
    printf("Server: %s\n", buffer.c_str());

  int serverpid = startServer(buffer.c_str());

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
      if (!waitforserver(serverpid, display.c_str()))
        {
          shutdownServer();
          throw std::runtime_error("Connection to server failed");
        }

      if (debug)
        printf("Client: %s\n", client.c_str());

      // Prepare for client program startup
      STARTUPINFO sic;
      PROCESS_INFORMATION pic;
      HANDLE handle;
      ZeroMemory( &sic, sizeof(sic) );
      sic.cb = sizeof(sic);
      ZeroMemory( &pic, sizeof(pic) );

      // Hide console window, unless showconsole is true
      sic.dwFlags = STARTF_USESHOWWINDOW;
      sic.wShowWindow = SW_HIDE;
      if (showconsole) sic.wShowWindow = SW_NORMAL;

      // Start the X client process
      if( !CreateProcess( NULL, (CHAR*)client.c_str(), NULL, NULL,
                          FALSE, 0, NULL, NULL, &sic, &pic ))
        {
          DWORD err = GetLastError();
          shutdownServer();
          throw win32_error("CreateProcess failed", err);
        }
      handle = pic.hProcess;

      // Wait until client process exits.
      WaitForMultipleObjects(1, &handle, FALSE, INFINITE);
    }

  // Check if X server is still running, but only when we started a local program
  if (config.local)
    {
#ifdef _DEBUG
      printf("killing X server process\n");
#endif

      shutdownServer();
    }
}
