#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

git describe >version

autoreconf --force -v --install || exit 1
cd $ORIGDIR || exit $?

if test -z "$NOCONFIGURE"; then
    $srcdir/configure "$@"
fi
