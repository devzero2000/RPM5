#!/bin/sh

export CFLAGS
export LDFLAGS

LTV="libtoolize (GNU libtool) 1.4.3"
ACV="autoconf (GNU Autoconf) 2.57"
AMV="automake (GNU automake) 1.6.3"
USAGE="
This script documents the versions of the tools I'm using to build rpm:
	libtool-1.4.3
	autoconf-2.57
	automake-1.6.3
Simply edit this script to change the libtool/autoconf/automake versions
checked if you need to, as rpm should build (and has built) with all
recent versions of libtool/autoconf/automake.
"

[ "`libtoolize --version`" != "$LTV" ] && echo "$USAGE" && exit 1
[ "`autoconf --version | head -1`" != "$ACV" ] && echo "$USAGE" && exit 1
[ "`automake --version | head -1 | sed -e 's/1\.4[a-z]/1.4/'`" != "$AMV" ] && echo "$USAGE" && exit 1

(echo "--- popt"; cd popt; ./autogen.sh --noconfigure "$@")
(echo "--- zlib"; cd zlib; ./autogen.sh --noconfigure "$@")
(echo "--- beecrypt"; cd beecrypt; ./autogen.sh --noconfigure "$@")
#(echo "--- libelf"; cd libelf; ./autogen.sh --noconfigure "$@")

echo "--- rpm"
libtoolize --copy --force
aclocal
autoheader
automake -a -c
autoconf

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    if [ -d /usr/share/man ]; then
	mandir=/usr/share/man
	infodir=/usr/share/info
    else
	mandir=/usr/man
	infodir=/usr/info
    fi
    if [ -d /usr/lib/nptl ]; then
	enable_posixmutexes="--enable-posixmutexes"
    else
	enable_posixmutexes=
    fi
    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --infodir=${infodir} --mandir=${mandir} --without-javaglue ${enable_posixmutexes} "$@"
else
    ./configure "$@"
fi
