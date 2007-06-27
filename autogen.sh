#!/bin/sh

#   configure the requirements
AMV="automake (GNU automake) 1.10"
ACV="autoconf (GNU Autoconf) 2.61"
LTV="libtoolize (GNU libtool) 1.5.24"
GTT="gettextize (GNU gettext-tools) 0.16.1"
USAGE="
To build RPM from plain CVS sources the following
installed developer tools are mandatory:
    GNU automake  1.10
    GNU autoconf  2.61
    GNU libtool   1.5.24
    GNU gettext   0.16.1
"

#   wrapper for running GNU libtool's libtoolize(1)
libtoolize () {
    _libtoolize=`which glibtoolize 2>/dev/null`
    case "$_libtoolize" in
        /* ) ;;
        *  ) _libtoolize=`which libtoolize 2>/dev/null`
             case "$_libtoolize" in
                 /* ) ;;
                 *  ) _libtoolize="libtoolize" ;;
             esac
             ;;
    esac
    $_libtoolize ${1+"$@"}
}

#   requirements sanity check
[ "`automake   --version | head -1`" != "$AMV" ] && echo "$USAGE" # && exit 1
[ "`autoconf   --version | head -1`" != "$ACV" ] && echo "$USAGE" # && exit 1
[ "`libtoolize --version | head -1`" != "$LTV" ] && echo "$USAGE" # && exit 1
[ "`gettextize --version | head -1 | sed -e 's;^.*/\\(gettextize\\);\\1;'`" != "$GTT" ] && echo "$USAGE" # && exit 1

echo "===> zlib"
( cd zlib && sh ./autogen.sh --noconfigure "$@" )
echo "<=== zlib"

echo "===> rpm"
echo "---> generate files via GNU libtool (libtoolize)"
libtoolize --copy --force
echo "---> generate files via GNU gettext (autopoint)"
autopoint --force
echo "---> generate files via GNU autoconf (aclocal, autoheader)"
aclocal -I m4
autoheader
echo "---> generate files via GNU automake (automake)"
automake -Wall -Wno-override -a -c
echo "---> generate files via GNU autoconf (autoconf)"
autoconf
echo "<=== rpm"

