dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the "Software"),
dnl to deal in the Software without restriction, including without limitation
dnl the rights to use, copy, modify, merge, publish, distribute, sublicense,
dnl and/or sell copies of the Software, and to permit persons to whom the
dnl Software is furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice (including the next
dnl paragraph) shall be included in all copies or substantial portions of the
dnl Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
dnl THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
dnl DEALINGS IN THE SOFTWARE.

AC_INIT([xlaunch], [0.1])
AM_INIT_AUTOMAKE([foreign no-define silent-rules dist-bzip2 no-dist-gzip])
AM_SILENT_RULES([yes])

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debugging (default: disabled)]),
                     [DEBUG=$enableval], [DEBUG=no])
AM_CONDITIONAL(DEBUG, [test "x$DEBUG" = xyes])

AC_PROG_CXX
AC_CHECK_TOOL(WINDRES, windres)

PKG_CHECK_MODULES([LIBX11], [x11])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
