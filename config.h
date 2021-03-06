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
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>
struct CConfig
{
    enum {MultiWindow, Fullscreen, Windowed, Nodecoration} window;
    enum {NoClient, StartProgram, XDMCP} client;
    bool local;
    std::string display;
    std::string protocol;
    std::string localprogram;
    std::string remoteprogram;
    std::string host;
    std::string user;
    bool broadcast;
    bool indirect;
    std::string xdmcp_host;
    bool clipboard;
    bool wgl;
    bool disableac;
    bool xdmcpterminate;
    std::string extra_params;
    bool keychain;
    bool terminal;
    std::string extra_ssh;
    CConfig() : window(MultiWindow),
                client(NoClient),
                local(true),
#ifdef _DEBUG
                display("1"),
#else
                display("0"),
#endif
                localprogram("xterm"),
                remoteprogram("xterm"),
                host(""),
                user(""),
                broadcast(false),
                indirect(false),
                xdmcp_host(""),
                clipboard(true),
                wgl(true),
                disableac(false),
                xdmcpterminate(false),
                extra_params(),
                keychain(false),
                terminal(true),
                extra_ssh()
    {
    };
    void Load(const char * filename);
    void Save(const char * filename);
};

#endif
