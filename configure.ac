AC_PREREQ(2.57)
AC_INIT(rpm, 5.0, rpm-devel@rpm5.org)
AC_CANONICAL_TARGET
AC_CONFIG_SRCDIR([rpmqv.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign])

# Library code modified:                              REVISION++
# Interfaces changed/added/removed:   CURRENT++       REVISION=0
# Interfaces added:                             AGE++
# Interfaces removed:                           AGE=0
#
AC_SUBST(LT_CURRENT, 5)
AC_SUBST(LT_REVISION, 0)
AC_SUBST(LT_AGE, 0)

dnl Set of available languages.
ALL_LINGUAS="cs da de fi fr gl is ja ko no pl pt pt_BR ro ru sk sl sr sv tr uk"

AC_AIX
AC_MINIX

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_YACC

AS=${AS-as}
AC_SUBST(AS)

if test "$ac_cv_c_compiler_gnu" = yes; then
    CFLAGS="$CFLAGS -fPIC -DPIC"
    CFLAGS="$CFLAGS -D_GNU_SOURCE -D_REENTRANT"
    CFLAGS="$CFLAGS -Wall -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wno-char-subscripts"
    case "$target" in
        *-*-linux* ) CFLAGS="$CFLAGS -fpie"; LDFLAGS="$LDFLAGS -pie" ;;
    esac
fi
export CFLAGS LDFLAGS

AC_PROG_GCC_TRADITIONAL
AC_SYS_LARGEFILE

dnl Does this platform require array notation to assign to a va_list?
dnl If cross-compiling, we assume va_list is "normal".  If this breaks
dnl you, set ac_cv_valistisarray=true and maybe define HAVE_VA_LIST_AS_ARRAY
dnl also just to be sure.
AC_MSG_CHECKING(whether va_list assignments need array notation)
AC_CACHE_VAL(ac_cv_valistisarray,
	[AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <stdlib.h>
		     #include <stdarg.h>
		     void foo(int i, ...) {
			va_list ap1, ap2;
			va_start(ap1, i);
			ap2 = ap1;
			if (va_arg(ap2, int) != 123 || va_arg(ap1, int) != 123)
			    { exit(1); }
			va_end(ap1); va_end(ap2);
		     }
		     int main() { foo(0, 123); return(0); }]])],[ac_cv_valistisarray=false],[ac_cv_valistisarray=true],[ac_cv_valistisarray=false])])

if test "$ac_cv_valistisarray" = true ; then
	AC_DEFINE(HAVE_VA_LIST_AS_ARRAY, 1,
		[Define as 1 if your va_list type is an array])
	AC_MSG_RESULT(yes)
else
       AC_MSG_RESULT(no)
fi

RPMUSER=rpm
RPMUID=37
RPMGROUP=rpm
RPMGID=37
export RPMUSER RPMUID RPMGROUP RPMGID
AC_SUBST(RPMUSER)
AC_SUBST(RPMUID)
AC_SUBST(RPMGROUP)
AC_SUBST(RPMGID)

AC_PATH_PROG(CTAGS, ctags, /bin/true)
AC_PATH_PROG(CSCOPE, cscope, /bin/true)

dnl
dnl This now uses libtool. Put
dnl	LDFLAGS_STATIC="-all"
dnl to attempt static executables using libtool. Otherwise
dnl	LDFLAGS_STATIC=""
dnl
AC_MSG_CHECKING(flag used by libtool to link rpm)
if test X"$GCC" = Xyes ; then
	case "$target" in
		*-*-linux*)	LDFLAGS_STATIC="-all-static" ;;
		*-*-solaris*)	LDFLAGS_STATIC="-static";;
		*-*-hpux*)	LDFLAGS_STATIC="-static";;
		*-*-darwin*)	LDFLAGS_STATIC="";; # Mac OS X does not do static binaries.
		*-*-sysv5uw*)   LDFLAGS_STATUS="-static";; # Unixware has no shared libthread.
		*-*-*)		LDFLAGS_STATIC="";;
	esac
elif test X"$CC" = Xcc ; then
	case "$target" in
		*-*-linux*)	LDFLAGS_STATIC="-all-static";;
		*-*-freebsd*)	LDFLAGS_STATIC="-all-static";;
		*-*-osf*)	LDFLAGS_STATIC="";; # OSF5 has no shared pthreads libs
		*-*-aix*)	LDFLAGS_STATIC="-static";;  # -Wl,-bnso doesn't seem to work...
		*-*-hpux*)	LDFLAGS_STATIC="-static";;
		*-*-solaris*)	LDFLAGS_STATIC="-static";;
		*-*-irix*)	LDFLAGS_STATIC="-static";;  #should be -non_shared, but can't
											# link because of crt1.o then.
		*-*-ultrix*)	LDFLAGS_STATIC="-all-static";;  #ultrix doesn't have shared libs.
		*-*-*)		LDFLAGS_STATIC=""
AC_MSG_WARN([

Unable to guess what option to pass to $CC to generate a static
executable.  You will need to set the LDFLAGS_STATIC macro in Makefile.inc to
the appropriate argument(s) if you want to build a static rpm executable.

])
		;;
	esac
else
	# just link it dynamically
	LDFLAGS_STATIC=""
fi
LDFLAGS_STATIC="${LDFLAGS} ${LDFLAGS_STATIC}"	# libtool format
AC_MSG_RESULT($LDFLAGS_STATIC)
AC_SUBST(LDFLAGS_STATIC)

dnl
dnl XXX Test for libpthread.a that is NPTL aware (static link only).
dnl
LDFLAGS_NPTL=
if test -f /usr/lib/nptl/libpthread.a ; then
    LDFLAGS_NPTL="-L/usr/lib/nptl"
#    INCPATH="$INCPATH -I/usr/include/nptl"
fi
if test -f /usr/lib64/nptl/libpthread.a ; then
    LDFLAGS_NPTL="-L/usr/lib64/nptl"
#    INCPATH="$INCPATH -I/usr/include/nptl"
fi
AC_SUBST(LDFLAGS_NPTL)

dnl
dnl look for POSIX chmod attributes
dnl
AC_MSG_CHECKING(POSIX chmod)
touch foo.chmodtest
chmod 744 foo.chmodtest
chmod +X foo.chmodtest 2>/dev/null
a=`ls -l foo.chmodtest | awk '{print $1}'`
rm -f foo.chmodtest
if test "$a" = "-rwxr-xr-x"; then
    AC_MSG_RESULT(yes)
    FIXPERMS=a+rX,u+w,g-w,o-w 
else
    AC_MSG_RESULT(no (tell your OS vendor about GNU fileutils))
    FIXPERMS=a+r,u+w,g-w,o-w 
fi
AC_SUBST(FIXPERMS)

dnl
dnl see if we have a mkdir that supports `-p'.
dnl
AC_PATH_PROGS(MKDIR, mkdir, mkdir)
AC_MSG_CHECKING(if $MKDIR supports -p)
rm -rf conftest
$MKDIR -p conftest/a 2>/dev/null
if test $? = 0 ; then
	rmdir conftest/a 2>/dev/null
	if test $? = 0  ; then
		:
	else
		MKDIR_P=0
	fi

	rmdir conftest 2>/dev/null
	if test $? = 0 ; then
		MKDIR_P="$MKDIR -p"
	else
		MKDIR_P=0
	fi
else
	MKDIR_P=0
fi

if test X"$MKDIR_P" = X0 ; then
	AC_MSG_RESULT(no)
	MKDIR_P="`echo ${prefix}/lib/rpm/mkinstalldirs`"
else
	AC_MSG_RESULT(yes)
fi
dnl
dnl substitute this into config.h, so the C source picks it up.
dnl
AC_DEFINE_UNQUOTED(MKDIR_P, "${MKDIR_P}",
[A full path to a program, possibly with arguments, that will create a
   directory and all necessary parent directories, ala 'mkdir -p'])
AC_SUBST(MKDIR_P)

AC_ISC_POSIX

dnl This test must precede tests of compiler characteristics like
dnl that for the inline keyword, since it may change the degree to
dnl which the compiler supports such features.
AM_C_PROTOTYPES

dnl AM_DISABLE_SHARED
AM_PROG_LIBTOOL

AC_CHECK_TOOL(AR, ar, :)

dnl
dnl use defaults if cross-compiling, otherwise use the default path.
dnl
if test "$cross_compiling" = "yes"; then
    MYPATH=":"
else
    MYPATH=$PATH
fi

dnl
dnl Find some common programs
dnl
AC_PATH_PROG(__BASH, bash, %{_bindir}/bash, $MYPATH)
AC_PATH_PROG(__BZIP2, bzip2, %{_bindir}/bzip2, $MYPATH)
AC_PATH_PROG(__CAT, cat, /bin/cat, $MYPATH)
AC_PATH_PROG(__CHGRP, chgrp, /bin/chgrp, $MYPATH)
AC_PATH_PROG(__CHMOD, chmod, /bin/chmod, $MYPATH)
AC_PATH_PROG(__CHOWN, chown, /bin/chown, $MYPATH)
AC_PATH_PROG(__CP, cp, /bin/cp, $MYPATH)
AC_PATH_PROG(__CPIO, cpio, /bin/cpio, $MYPATH)
AC_PATH_PROG(__CURL, curl, %{_bindir}/curl, $MYPATH)
AC_PATH_PROG(__CVS, cvs, %{_bindir}/cvs, $MYPATH)
AC_PATH_PROG(__DIFF, diff, /bin/diff, $MYPATH)
AC_PATH_PROG(__DITTO, ditto, %{_bindir}/ditto, $MYPATH)
AC_PATH_PROG(__FILE, file, %{_bindir}/file, $MYPATH)
AC_PATH_PROG(__FIND, find, %{_bindir}/find, $MYPATH)
AC_PATH_PROG(__GPG, gpg, %{_bindir}/gpg, $MYPATH)
AC_PATH_PROG(__GREP, grep, /bin/grep, $MYPATH)
AC_PATH_PROG(__GZIP, gzip, /bin/gzip, $MYPATH)
AC_PATH_PROG(__HG, hg, %{_bindir}/hg, $MYPATH)

AC_PATH_PROG(__ID, id, /usr/bin/id, $MYPATH)
AC_MSG_CHECKING(checking whether id supports -u)
    if ${__ID} -u 2>&1 > /dev/null ; then
	__ID_U="%{__id} -u"
	AC_MSG_RESULT(yes)
    else
	__ID_U="%{__id} | %{__sed} 's/[[^=]]*=\\\\([[0-9]][[0-9]]*\\\\).*$/\\\\1/'"
	AC_MSG_RESULT(no)
    fi
AC_SUBST(__ID_U)

AC_PATH_PROG(__INSTALL, install, /usr/bin/install, $MYPATH)
AC_PATH_PROG(__INSTALL_INFO, install-info, /sbin/install-info, $MYPATH)
AC_PATH_PROG(__LDCONFIG, ldconfig, /sbin/ldconfig, $MYPATH)
AC_PATH_PROG(__LUA, lua, %{_bindir}/lua, $MYPATH)
AC_PATH_PROG(__LZMA, lzma, %{_bindir}/lzma, $MYPATH)
AC_PATH_PROG(__LZMASH, lzmash, %{_bindir}/lzmash, $MYPATH)
AC_PATH_PROG(__LZOP, lzop, %{_bindir}/lzop, $MYPATH)
AC_PATH_PROG(__MAKE, make, %{_bindir}/make, $MYPATH)
AC_PATH_PROG(__MKDIR, mkdir, /bin/mkdir, $MYPATH)
AC_PATH_PROG(__MV, mv, /bin/mv, $MYPATH)
AC_PATH_PROG(__PATCH, patch, %{_bindir}/patch, $MYPATH)
AC_MSG_CHECKING(old version of patch)
    PATCHVERSION=`patch --version 2>&1`

    if test "$PATCHVERSION" = "Patch version 2.1"; then
	AC_DEFINE(HAVE_OLDPATCH_21, 1,
		[Define if the patch call you'll be using is 2.1 or older])
	AC_MSG_RESULT(patch older then 2.2 found)
    else
	AC_MSG_RESULT(patch later then 2.2 found)
    fi

AC_PATH_PROG(__PAX, pax, %{_bindir}/pax, $MYPATH)
AC_PATH_PROG(__PERL, perl, %{_bindir}/perl, $MYPATH)
AC_PATH_PROG(__PGP, pgp, %{_bindir}/pgp, $MYPATH)
AC_PATH_PROG(__PHP, php, %{_bindir}/php, $MYPATH)
AC_PATH_PROG(__PYTHON, python, %{_bindir}/python, $MYPATH)
AC_PATH_PROG(__RM, rm, /bin/rm, $MYPATH)
AC_PATH_PROG(__RSH, rsh, %{_bindir}/rsh, $MYPATH)
AC_PATH_PROG(__SED, sed, /bin/sed, $MYPATH)
AC_PATH_PROG(__SH, sh, /bin/sh, $MYPATH)
AC_PATH_PROG(__SSH, ssh, %{_bindir}/ssh, $MYPATH)
AC_PATH_PROG(__SVN, svn, %{_bindir}/svn, $MYPATH)
AC_PATH_PROG(__TAR, tar, /bin/tar, $MYPATH)
AC_PATH_PROG(__TCLSH, tclsh, %{_bindir}/tclsh, $MYPATH)
AC_PATH_PROG(__UNZIP, unzip, %{_bindir}/unzip, $MYPATH)
AC_PATH_PROG(__WGET, wget, %{_bindir}/wget, $MYPATH)
AC_PATH_PROG(__XAR, xar, %{_bindir}/xar, $MYPATH)

AC_PATH_PROG(__LD, ld, %{_bindir}/ld, $MYPATH)
AC_PATH_PROG(__NM, nm, %{_bindir}/nm, $MYPATH)
AC_PATH_PROG(__OBJCOPY, objcopy, %{_bindir}/objcopy, $MYPATH)
AC_PATH_PROG(__OBJDUMP, objdump, %{_bindir}/objdump, $MYPATH)
AC_PATH_PROG(__STRIP, strip, %{_bindir}/strip, $MYPATH)

addlib() {
  l=$1
  shift
  case "$target" in 
    *-*-solaris2.[234567])	LIBS="$LIBS -L$l -R$l $*";;
    *)		LIBS="$LIBS -L$l $*";;
  esac
}

#=================

RPM_CHECK_LIB([ZLib], [zlib],
              [z gz], [gzread], [zlib.h],
              [internal:external:none], [zlib],
              [AC_CHECK_FUNC(gzseek)], [])

RPM_CHECK_LIB([GNU BZip2], [bzip2],
              [bz2], [BZ2_bzread], [bzlib.h],
              [external:none], [],
              [], [])

#=================

localdone=

dirs=$prefix
if test "$cross_compiling" != "yes"; then
  dirs="$dirs"
for dir in $dirs
do
	case $dir in
	NONE|/usr)	continue;;
	/usr/local)
		if test X$localdone != X ; then continue; fi
		localdone="$dir"
		;;
	esac

	AC_MSG_CHECKING(for $dir/lib in LIBS)
	if test -d $dir/lib 2> /dev/null ; then
		addlib $dir/lib
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi

	AC_MSG_CHECKING(for $dir/include in INCPATH)
	if test -d $dir/include 2>/dev/null ; then
		if [[ "$dir" != "/usr/local" ]] ; then
		    INCPATH="$INCPATH -I$dir/include"
		fi
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
done
fi

AC_MSG_CHECKING(for /usr/ucblib in LIBS)
if test -d /usr/ucblib ; then
    case $build in
    mips-sni-sysv4)
	addlib /usr/ccs/lib -lc
	addlib /usr/ucblib
	AC_MSG_RESULT(yes)
	;;
    *-*-solaris2.*)
	AC_MSG_RESULT(yes)
	;;
    *)
	AC_MSG_RESULT(no)
	;;
    esac
fi

dnl
dnl Check for features
dnl

dnl Checks for libraries.

AC_CHECK_FUNC(setreuid, [], [
    AC_CHECK_LIB(ucb, setreuid, [if echo $LIBS | grep -- -lucb >/dev/null ;then :; else LIBS="$LIBS -lc -lucb" USEUCB=y;fi])
])

AC_CHECK_FUNC(rand, [], [
    AC_CHECK_LIB(rand, rand, [])
])

AC_CHECK_FUNC(getdomainname, [], [
    AC_CHECK_LIB(nsl, getdomainname)
])
AC_CHECK_FUNC(socket, [], [
    AC_CHECK_LIB(socket, socket)
])

AC_CHECK_HEADERS(error.h)
AC_CHECK_FUNCS(error)

AC_CHECK_HEADERS(poll.h)

AC_CHECK_HEADERS(thread.h pthread.h synch.h semaphore.h)

AC_CHECK_LIB(pthread, pthread_mutex_trylock, [], [
  dnl OSF 5.0 has the the symbols prefixed with __ in libpthread.
  AC_CHECK_LIB(pthread, __pthread_mutex_trylock, [], [
    AC_CHECK_LIB(thread, mutex_lock)
  ])
])

AC_CHECK_HEADERS(aio.h)
AC_SEARCH_LIBS(aio_read, [c rt aio posix4])

dnl Better not use fchmod at all.
AC_CHECK_FUNC(fchmod)

AC_CHECK_FUNCS(vsnprintf snprintf)

dnl Temporary hack for MiNT.  Some functions (writev, snprintf) are
dnl not in the libc but in libport (for political reasons).  This check
dnl can hopefully be removed soon.  Please use the default action
dnl for this macro (not LIBS=...), otherwise the check for dbopen
dnl will fail.
AC_CHECK_LIB(port, writev)

#=================
# Check for libelf library. Prefer external, then internal, otherwise none.
WITH_ELFUTILS_SUBDIR=
WITH_LIBELF_INCLUDE=
WITH_LIBELF_LIB=
AC_CHECK_HEADERS([libelf.h])

# libelf.h on Solaris is incompatible with Largefile support.  If
# we couldn't include libelf.h, see if we can when _FILE_ACCESS_BITS is
# set for non-LFS.
if test X"$ac_cv_header_libelf_h" = X"no" ; then
  AC_MSG_CHECKING([if libelf.h conflicts with _LARGEFILE_SOURCE])
  AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([[
#undef _LARGEFILE64_SOURCE
#undef _LARGEFILE_SOURCE
#if !defined(_LP64)
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif
#include <libelf.h>
]],[[Elf_Data foo;]])],
  [AC_DEFINE(HAVE_LIBELF_H, 1, [Define to 1 if you have libelf.h])
   AC_DEFINE(LIBELF_H_LFS_CONFLICT, 1,
	[Define to 1 if libelf.h is incompatible with the Large Files API])
   AC_MSG_RESULT(yes)
  ],
  [AC_MSG_RESULT(no)]
 )
fi

AC_CHECK_HEADERS([gelf.h])
# same for gelf.h, since it probably includes libelf.h
if test X"$ac_cv_header_gelf_h" = X"no" ; then
  AC_MSG_CHECKING([if gelf.h conflicts with _LARGEFILE_SOURCE])
  AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([[
#undef _LARGEFILE64_SOURCE
#undef _LARGEFILE_SOURCE
#if !defined(_LP64)
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif
#include <gelf.h>
]],[[Elf32_Verdef foo;]])],
  [AC_DEFINE(HAVE_GELF_H, 1, [Define to 1 if you have gelf.h])
   ac_cv_header_gelf_h=yes
   AC_MSG_RESULT(yes)
  ],
  [AC_MSG_RESULT(no)]
 )
fi

# if we found gelf.h, look for libelf and see if it has gelf_getvernaux
if test X"$ac_cv_header_gelf_h" = X"yes" ; then
	AC_CHECK_LIB(elf, elf_version, [
    		AC_DEFINE(HAVE_LIBELF, 1,
		  [Define to 1 if you have the 'elf' library (-lelf).])
    		WITH_LIBELF_LIB="-lelf"
		AC_CHECK_LIB(elf, gelf_getvernaux, [
		  AC_DEFINE(HAVE_GELF_GETVERNAUX, 1,
		  [Define to 1 if you have the gelf_getvernaux() function.])
		])
	])
else
	# allow for an internalized elfutils
	if test -d elfutils/libelf ; then
	    AC_DEFINE(HAVE_GELF_H, 1, [Define to 1 if you have the <gelf.h> header file.])
	    AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have the 'elf' library (-lelf).])
	    WITH_ELFUTILS_SUBDIR=elfutils
	    WITH_LIBELF_INCLUDE="-I\${top_srcdir}/${WITH_ELFUTILS_SUBDIR}/libelf"
	    WITH_LIBELF_LIB="\${top_builddir}/${WITH_ELFUTILS_SUBDIR}/libelf/libelf.a"
	fi
fi

AC_SUBST(WITH_ELFUTILS_SUBDIR)
AC_SUBST(WITH_LIBELF_INCLUDE)
AC_SUBST(WITH_LIBELF_LIB)

WITH_LIBDWARF_INCLUDE=
WITH_LIBDWARF_DEBUGEDIT=
AC_CHECK_HEADER([dwarf.h], [
  AC_DEFINE(HAVE_DWARF_H, 1, [Define to 1 if you have the <dwarf.h> header file.])
  WITH_LIBDWARF_DEBUGEDIT="debugedit"
],[
  if test -d elfutils/libdwarf ; then
    AC_DEFINE(HAVE_DWARF_H, 1, [Define to 1 if you have the <dwarf.h> header file.])
    WITH_LIBDWARF_INCLUDE="-I\${top_srcdir}/${WITH_ELFUTILS_SUBDIR}/libdwarf"
    WITH_LIBDWARF_DEBUGEDIT="debugedit"
  fi
])
AC_SUBST(WITH_LIBDWARF_INCLUDE)
AC_SUBST(WITH_LIBDWARF_DEBUGEDIT)

#=================

RPM_CHECK_LIB([BeeCrypt], [beecrypt],
              [beecrypt], [mpfprintln], [beecrypt/api.h],
              [external:internal:none], [beecrypt],
              [], [ AC_ERROR([mandatory BeeCrypt library not found]) ])

#=================

RPM_CHECK_LIB([Neon], [neon],
              [neon], [ne_session_create], [ne_session.h],
              [external:internal:abort], [neon:src:src],
              [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
                    AC_DEFINE(HAVE_NEON_NE_GET_RESPONSE_HEADER, 1,
                              [Define to 1 if you have ne_get_response_header() in libneon.])
                else
                    AC_CHECK_LIB(neon, ne_get_response_header, [
                        AC_DEFINE(HAVE_NEON_NE_GET_RESPONSE_HEADER, 1,
                                  [Define to 1 if you have ne_get_response_header() in libneon.]) ])
                    AC_CHECK_LIB(neon, ne_send_request_chunk, [
                        AC_DEFINE(HAVE_NEON_NE_SEND_REQUEST_CHUNK, 1,
                                  [Define to 1 if you have ne_send_request_chunk() in libneon.]) ])
                fi
              ], [ AC_ERROR([mandatory Neon library not found]) ])

#=================

RPM_CHECK_LIB([File (magic)], [file],
              [magic], [magic_open], [magic.h],
              [external:internal:none], [file:src:src],
              ], [ AC_ERROR([mandatory File (magic) library not found]) ])

#=================

RPM_CHECK_LIB([Linux keyutils], [keyutils],
              [keyutils], [keyctl_read], [keyutils.h],
              [external:none], [],
              [], [])

#=================

RPM_CHECK_LIB([POPT], [popt],
              [popt], [poptGetContext], [popt.h],
              [external:abort], [],
              [], [AC_ERROR([mandatory POPT library not found])])

#=================

DBLIBSRCS=""

RPM_CHECK_LIB([Berkeley-DB], [db],
              [db], [db_create], [db.h],
              [internal:external:abort], [db3],
              [ DBLIBSRCS="$DBLIBSRCS db3.c"
                if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
                    AC_DEFINE(HAVE_DB_H, 1, [Have <db.h> header])
                fi
              ],
              [ AC_ERROR([mandatory Berkeley-DB library not found]) ])

# accept and pass-through options for Berkeley-DB "configure"
AC_ARG_WITH(db-largefile, AS_HELP_STRING([--with-db-largefile], [build Berkeley-DB with LARGEFILE support]))
AC_ARG_WITH(db-mutex,     AS_HELP_STRING([--with-db-mutex=ARG], [build Berkeley-DB with MUTEX type ARG]))
AC_ARG_WITH(db-rpc,       AS_HELP_STRING([--with-db-rpc],       [build Berkeley-DB with RPC support]))
AM_CONDITIONAL(WITH_DB_RPC, test ".$with_db_rpc" = .yes)

RPM_CHECK_LIB([SQLite], [sqlite],
              [sqlite3], [sqlite3_open], [sqlite3.h],
              [external:abort], [],
              [ DBLIBSRCS="$DBLIBSRCS sqlite.c" ],
              [ AC_ERROR([mandatory SQLite library not found]) ])

DBLIBOBJS=`echo $DBLIBSRCS | sed -e "s/\.c/\.lo/g"`

AC_SUBST(DBLIBSRCS)
AC_SUBST(DBLIBOBJS)

#=================

dnl AmigaOS and IXEmul have a fork() dummy
  case "$target" in
    m68k-*-amigaos ) 
	echo "Building for AmigaOS: using vfork() instead of fork()"; 
	CFLAGS="$CFLAGS -Dfork=vfork" 
	;;
  esac

AC_CHECK_HEADERS(locale.h)
AM_GNU_GETTEXT([external])

dnl Checks for header files we can live without.
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_DIRENT
AC_HEADER_TIME

AC_CHECK_HEADERS(fcntl.h getopt.h grp.h memory.h netdb.h pwd.h utime.h)

AC_CHECK_HEADERS(signal.h)
AC_CHECK_HEADERS(sys/ipc.h sys/socket.h sys/select.h)
AC_CHECK_HEADERS(sys/types.h sys/stdtypes.h)
AC_CHECK_HEADERS(sys/mman.h sys/resource.h sys/utsname.h sys/wait.h)

AC_CHECK_HEADERS(netinet/in_systm.h)
AC_CHECK_HEADERS(machine/types.h)
AC_CHECK_HEADERS(mntent.h sys/mnttab.h sys/systemcfg.h)
AC_CHECK_HEADERS(sys/mount.h sys/mntctl.h sys/param.h sys/vmount.h)
AC_CHECK_HEADERS(libio.h)
AC_CHECK_HEADERS(err.h mcheck.h)

dnl Some solaris build env needs.
AC_CHECK_HEADERS(limits.h)

dnl basename(), confstr() on some platforms
AC_CHECK_HEADERS(libgen.h)

dnl popt w float/double needs.
AC_CHECK_HEADERS(float.h)

AC_CHECK_HEADERS(glob.h)

dnl statfs portability fiddles.
dnl
dnl We should really emulate/steal sections of the statfs and struct statfs
dnl checks from GNU fileutils.
dnl
AC_MSG_CHECKING(for struct statfs)

dnl
dnl this is easier than nesting AC_TRY_COMPILEs...
dnl
found_struct_statfs=no

if test X$found_struct_statfs = Xno ; then
dnl Solaris 2.6+ wants to use statvfs
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/statvfs.h> ]], [[struct statvfs sfs;]])],[AC_MSG_RESULT(in sys/statvfs.h)
	AC_DEFINE(STATFS_IN_SYS_STATVFS, 1,
		[statfs in <sys/statvfs.h> (for solaris 2.6+ systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl first try including sys/vfs.h
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/vfs.h> ]], [[struct statfs sfs;]])],[AC_MSG_RESULT(in sys/vfs.h)
	AC_DEFINE(STATFS_IN_SYS_VFS, 1, [statfs in <sys/vfs.h> (for linux systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl ...next try including sys/mount.h
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/mount.h> ]], [[struct statfs sfs;]])],[AC_MSG_RESULT(in sys/mount.h)
	AC_DEFINE(STATFS_IN_SYS_MOUNT, 1, [statfs in <sys/mount.h> (for Digital Unix 4.0D systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl ...still no joy.  Try sys/statfs.h
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/statfs.h> ]], [[struct statfs sfs;]])],[AC_MSG_RESULT(in sys/statfs.h)
	AC_DEFINE(STATFS_IN_SYS_STATFS, 1, [statfs in <sys/statfs.h> (for Irix 6.4 systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl ...no luck.  Warn the user of impending doom.
AC_MSG_WARN(not found)
fi

dnl
dnl if we found the struct, see if it has the f_bavail member.  Some OSes
dnl don't, including IRIX 6.5+
dnl
if test X$found_struct_statfs = Xyes ; then
AC_MSG_CHECKING(for f_bavail member in struct statfs)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if STATFS_IN_SYS_STATVFS
# include <sys/statvfs.h>
  typedef struct statvfs STATFS_t;
#else
  typedef struct statfs STATFS_t;
# if STATFS_IN_SYS_VFS
#  include <sys/vfs.h>
# elif STATFS_IN_SYS_MOUNT
#  include <sys/mouht.h>
# elif STATFS_IN_SYS_STATFS
#  include <sys/statfs.h>
# endif
#endif ]], [[STATFS_t sfs;
	sfs.f_bavail = 0;]])],[AC_MSG_RESULT(yes)
	AC_DEFINE(STATFS_HAS_F_BAVAIL, 1, [Define if struct statfs has the f_bavail member])],[AC_MSG_RESULT(no)
])
fi

if test X$found_struct_statfs = Xyes ; then
dnl
dnl now check to see if we have the 4-argument variant of statfs()
dnl this pretty much requires AC_RUN_IFELSE([AC_LANG_SOURCE([[]])],[],[],[])
dnl
AC_MSG_CHECKING([if statfs() requires 4 arguments])
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef STATFS_IN_SYS_VFS
#include <sys/vfs.h>
#elif STATFS_IN_SYS_MOUNT
#include <sys/mouht.h>
#elif STATFS_IN_SYS_STATFS
#include <sys/statfs.h>
#endif
main() {
	struct statfs sfs;
	exit (statfs(".", &sfs, sizeof(sfs), 0));
}
]])],[AC_MSG_RESULT(yes)
	AC_DEFINE(STAT_STATFS4, 1, [Define if the statfs() call takes 4 arguments])],[AC_MSG_RESULT(no)],[AC_MSG_RESULT(no)
])
fi

AC_C_INLINE

dnl look for libc features
PROVIDES_ERRNO=no
AC_MSG_CHECKING(if <netdb.h> defines h_errno)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]], [[printf("%d",h_errno)]])],[PROVIDES_ERRNO=yes],[])
AC_MSG_RESULT($PROVIDES_ERRNO)
if test $PROVIDES_ERRNO = yes; then
	AC_DEFINE(HAVE_HERRNO, 1, [ Define as 1 if <netdb.h> defines h_errno])
fi

dnl If a system doesn't have S_IFSOCK, define it as 0 which will
dnl make S_ISSOCK always return false (nice, eh?)
AC_MSG_CHECKING(if <sys/stat.h> defines S_IFSOCK)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_IFSOCK)]])],[HAS_S_IFSOCK=yes],[HAS_S_IFSOCK=no])
AC_MSG_RESULT($HAS_S_IFSOCK)
if test $HAS_S_IFSOCK = yes; then
	AC_DEFINE(HAVE_S_IFSOCK, 1, [Define as 1 if <sys/stat.h> defines S_IFSOCK])
fi

dnl Some Unix's are missing S_ISLNK, S_ISSOCK
AC_MSG_CHECKING(if <sys/stat.h> defines S_ISLNK)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_ISLNK(0755))]])],[HAS_S_ISLNK=yes],[HAS_S_ISLNK=no])
AC_MSG_RESULT($HAS_S_ISLNK)
if test $HAS_S_ISLNK = yes; then
	AC_DEFINE(HAVE_S_ISLNK, 1, [Define as 1 if <sys/stat.h> defines S_ISLNK])
fi

AC_MSG_CHECKING(if <sys/stat.h> defines S_ISSOCK)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_ISSOCK(0755))]])],[HAS_S_ISSOCK=yes],[HAS_S_ISSOCK=no])
AC_MSG_RESULT($HAS_S_ISSOCK)
if test $HAS_S_ISSOCK = yes; then
	AC_DEFINE(HAVE_S_ISSOCK, 1, [Define as 1 if <sys/stat.h> defines S_ISSOCK])
fi

AC_MSG_CHECKING(if timezone is defined)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[printf("%ld", timezone)]])],[HAS_TIMEZONE=yes],[HAS_TIMEZONE=no])
AC_MSG_RESULT($HAS_TIMEZONE)

dnl Check for missing typedefs
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_VPRINTF
AC_FUNC_MMAP

AC_CHECK_FUNCS(basename getaddrinfo getcwd getnameinfo getwd inet_aton)
AC_CHECK_FUNCS(mtrace putenv realpath setenv clearenv)
AC_CHECK_FUNCS(stpcpy stpncpy strcspn)
AC_CHECK_FUNCS(strdup strndup strerror strtol strtoul strspn strstr)
AC_CHECK_FUNCS(sighold sigrelse sigpause sigprocmask sigemptyset sigaddset sigdelset sigsuspend)

AC_CHECK_FUNCS(__secure_getenv)

AC_CHECK_FUNCS(regcomp)

AC_CHECK_FUNCS(ftok)

AC_CHECK_FUNCS(mkstemp)

dnl XXX Glob *is* broken on linux with libc5, solaris and possibly aix when
dnl %files gets something like
dnl	/usr/*/locale/*/LC_MESSAGES/*.mo
dnl (Note: more than one asterisk in glob pattern.)
dnl
dnl XXX Glob is "fixed" in glibc-2.3.3-61, but the cost is that
dnl dangling symlinks are no longer globbed. Always use the internal glob.
AC_DEFINE(USE_GNU_GLOB, 1, [Use the included glob.c?])
AC_LIBOBJ(glob)
AC_LIBOBJ(fnmatch)

dnl
dnl Auto-detect whether perl bindings should be built.
dnl
withval=auto
AC_ARG_WITH(perl,   [  --with-perl             build rpm perl bindings ])
WITH_PERL_VERSION=$withval
if test "$WITH_PERL_VERSION" != no ; then
  WITH_PERL_SUBDIR=perl
  WITH_PERL_SUBPACKAGE=1
else
  WITH_PERL_SUBDIR=
  WITH_PERL_SUBPACKAGE=0
fi
AC_SUBST(WITH_PERL_SUBDIR)
AC_SUBST(WITH_PERL_SUBPACKAGE)
AC_SUBST(WITH_PERL_VERSION)

dnl
dnl Auto-detect which python bindings should be built.
dnl
withval=auto
AC_ARG_WITH(python, [  --with-python           build rpm python bindings ])

WITH_PYTHON_VERSION=$withval
if test $withval = auto ; then

AC_MSG_CHECKING(for python 2.5)
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <python2.5/Python.h>
main() {
  exit(strncmp("2.5", PY_VERSION, 3));
} ]])],[withval=yes],[withval=no],[withval=yes])
  AC_MSG_RESULT($withval)
  if test $withval = yes ; then
    WITH_PYTHON_VERSION="2.5"
  else
      
  AC_MSG_CHECKING(for python 2.4)
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
  #include <python2.4/Python.h>
  main() {
    exit(strncmp("2.4", PY_VERSION, 3));
  } ]])],[withval=yes],[withval=no],[withval=yes])
    AC_MSG_RESULT($withval)
    if test $withval = yes ; then
      WITH_PYTHON_VERSION="2.4"
    else
      
    AC_MSG_CHECKING(for python 2.3)
    AC_RUN_IFELSE([AC_LANG_SOURCE([[
    #include <python2.3/Python.h>
    main() {
      exit(strncmp("2.3", PY_VERSION, 3));
    } ]])],[withval=yes],[withval=no],[withval=yes])
      AC_MSG_RESULT($withval)
      if test $withval = yes ; then
        WITH_PYTHON_VERSION="2.3"
      else
      
        AC_MSG_CHECKING(for python 2.2)
        AC_RUN_IFELSE([AC_LANG_SOURCE([[
      #include <python2.2/Python.h>
      main() {
        exit(strncmp("2.2", PY_VERSION, 3));
      } ]])],[withval=yes],[withval=no],[withval=yes])
        AC_MSG_RESULT($withval)
        if test $withval = yes ; then
          WITH_PYTHON_VERSION="2.2"
        else
      
          AC_MSG_CHECKING(for python 1.5.2)
          AC_RUN_IFELSE([AC_LANG_SOURCE([[
      #include <python1.5/Python.h>
      main() {
        exit(strcmp("1.5.2", PY_VERSION));
      } ]])],[withval=yes],[withval=no],[withval=yes])
          AC_MSG_RESULT($withval)
          if test $withval = yes ; then
            WITH_PYTHON_VERSION="1.5"
          fi
        fi
      fi
    fi
  fi
fi

if test "$WITH_PYTHON_VERSION" != no ; then
  WITH_PYTHON_SUBDIR=python
  WITH_PYTHON_SUBPACKAGE=1
else
  WITH_PYTHON_SUBDIR=
  WITH_PYTHON_SUBPACKAGE=0
fi

AC_SUBST(WITH_PYTHON_SUBDIR)
AC_SUBST(WITH_PYTHON_SUBPACKAGE)
AC_SUBST(WITH_PYTHON_VERSION)

AC_PATH_PROG(__DOXYGEN, doxygen, no, $PATH)
dnl
dnl Auto-detect whether doxygen generated API docs should be included.
dnl
withval=auto
AC_ARG_WITH(apidocs, [  --with-apidocs          build rpm API docs ])

if test $withval = auto -a $__DOXYGEN != no ; then
  withval=yes
elif test $withval = yes -a $__DOXYGEN = no ; then
  AC_MSG_ERROR(--> rpm API docs needs doxygen in PATH)
fi
if test $withval = yes; then
  WITH_APIDOCS_TARGET=apidocs
  WITH_APIDOCS=1
else
  WITH_APIDOCS=0
fi
AC_SUBST(WITH_APIDOCS_TARGET)
AC_SUBST(WITH_APIDOCS)

WITH_SELINUX_LIB=
with_selinuxval=no
case "$target" in
    *-*-linux*) with_selinuxval=yes ;;
esac
withval=${with_selinuxval}
AC_ARG_WITH(selinux, [  --with-selinux          build with selinux support ])
if test $withval != no ; then
  AC_DEFINE(WITH_SELINUX, 1, [Build with selinux support?])
  WITH_SELINUX_LIB="-lselinux"
fi
AC_SUBST(WITH_SELINUX_LIB)

WITH_LUA_SUBDIR_DEF=""
WITH_LUA_SUBDIR_LIB=""
RPM_CHECK_LIB([Lua], [lua],
              [lua], [_], [_],
              [internal:none], [lua:include,local],
              [ AC_CHECK_LIB(dl, dlopen, [ LIBS="-ldl $LIBS"; WITH_LUA_SUBDIR_LIB="-ldl" ])
                AC_CHECK_FUNC(dlopen, [ WITH_LUA_SUBDIR_DEF="-DUSE_DLOPEN" ]) ], [])
AC_SUBST(WITH_LUA_SUBDIR_DEF)
AC_SUBST(WITH_LUA_SUBDIR_LIB)

RPM_CHECK_LIB([DMalloc], [dmalloc],
              [dmalloc], [dmalloc_malloc], [dmalloc.h],
              [no], [],
              [], [])

RPM_CHECK_LIB([ElectricFence], [efence],
              [efence], [malloc], [stdlib.h],
              [no], [],
              [], [])

AC_CHECK_FUNCS(setlocale)

AC_CHECK_FUNCS(getpassphrase)

AC_CHECK_FUNC(getmntent, AC_DEFINE(HAVE_GETMNTENT, 1, [Define if you have the getmntent() function]), [
  AC_CHECK_FUNC(mntctl, AC_DEFINE(HAVE_MNTCTL, 1, [Define as 1 if you have mntctl() (only aix?)]),[
    AC_CHECK_FUNC(getmntinfo, AC_DEFINE(HAVE_GETMNTINFO, 1, [Define as 1 if you have getmntinfo() (Mac OS X)]), [
      AC_CHECK_FUNC(getmntinfo_r, AC_DEFINE(HAVE_GETMNTINFO_R, 1, [Define as 1 if you have getmntinfo_r() (only osf?)]), [
        AC_CHECK_LIB(c_r, getmntinfo_r, [LIBS="$LIBS -lc_r"; 
					AC_DEFINE(HAVE_GETMNTINFO_R, 1, [Define as 1 if you have getmntinfo_r() (only osf?)])], [
		 AC_DEFINE([USE_GETMNTENT], 1, [Defined if getmntent replacement is used])
                 AC_LIBOBJ(getmntent)])
      ])
    ])
  ])
])

AC_CHECK_FUNC(lchown,
   [__CHOWN_RHF="%{__chown} -Rhf"
    __CHGRP_RHF="%{__chgrp} -Rhf"
    AC_DEFINE(HAVE_LCHOWN, 1, [Define as 1 if you have lchown()])],
   [__CHOWN_RHF="%{__chown} -Rf"
    __CHGRP_RHF="%{__chgrp} -Rf"
    dnl Does chown() follow symlinks? This should be a good enough test.
    AC_MSG_CHECKING(whether chown() follows symlinks)
    AC_ARG_ENABLE([broken-chown],
    [  --enable-broken-chown   this system's chown follows symbolic links], 
	    result=$enableval, result=unknown)
    if echo "$build" | egrep "(aix)|(hpux)|(linux)" > /dev/null ; then
	result=yes
    elif echo "$build" | egrep "(nextstep)" > /dev/null ; then
	result=no
    fi
    if test $result = unknown; then
	if test `${__ID} | cut -f2 -d\= | cut -f1 -d\(` = 0; then
	    rm -f foo bar
	    touch foo
	    ln -s foo bar
	    ${__CHOWN} 10 bar
	    if test `ls -l foo | awk '{print $3}'` != "root"; then
		result=yes
	    else
		result=no
	    fi
	    ${__RM} -f foo bar
	else
	    AC_MSG_CHECKING((cannot check by non-root user))
	    result=no
	fi
    fi

    AC_MSG_RESULT($result)
    if test $result = yes; then
	    AC_DEFINE(CHOWN_FOLLOWS_SYMLINK, 1, [Define as 1 if chown() follows symlinks and you don't have lchown()])
    fi])
AC_SUBST(__CHOWN_RHF)
AC_SUBST(__CHGRP_RHF)

dnl
dnl figure out what root's primary group is
dnl
AC_MSG_CHECKING(root's primary group)
AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

int main()
{
	struct passwd *root = NULL;
	struct group *roots_group = NULL;
	FILE * tempfile = NULL;

	root = getpwuid( (uid_t) 0 );
	if (root != NULL) {
		roots_group = getgrgid(root->pw_gid);
		if (roots_group != NULL) {
			tempfile = fopen("conftest_rootg", "w");
			if (tempfile != NULL) {
				fprintf(tempfile, "%s\n", roots_group->gr_name);
				fclose(tempfile);
				exit(0);
			}
		}
	}

	exit(1);
}]])],[ROOT_GROUP=`cat conftest_rootg`; rm -f conftest_rootg],[ROOT_GROUP="root"],[ROOT_GROUP="root"
])
AC_MSG_RESULT($ROOT_GROUP)
AC_SUBST(ROOT_GROUP)

if test "x$varprefix" = "x"; then
    # For /usr and /usr/local, we want the 'var' directory to go
    # in /var and /var/local respectively. For everything else, 
    # just put the 'var' directory in prefix/var.
    case $prefix in
	/usr | /usr/local )
	    varprefix=`echo $prefix | sed 's/usr/var/'` ;;
	NONE)
 	    varprefix=`echo $ac_default_prefix | sed 's/usr/var/'` ;;
	*) 
	    varprefix=$prefix/var ;;
    esac	
fi
AC_SUBST(varprefix)

if test "x$tmpdir" = "x"; then
    if test -d $varprefix/tmp; then
	tmpdir=$varprefix/tmp
    else
	if test -d /var/tmp; then
	    tmpdir=/var/tmp
	else
	    tmpdir=/tmp
	fi
    fi
fi
AC_SUBST(tmpdir)

if echo "$build_os" | grep sco > /dev/null; then
	echo "hacking things up for sco"
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(HAVE_STRUCT_MNTTAB, 1,
	[Define as 1 if you have "struct mnttab" (only sco?)])
elif echo "$build_os" | grep sunos > /dev/null; then
	echo "hacking things up for sunos"
	CFLAGS="$CFLAGS -D__USE_FIXED_PROTOTYPES__"
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(NEED_MYREALLOC, 1, [Define as 1 if we need myrealloc])
	AC_LIBOBJ(myrealloc)
fi

#
# get rid of the 4-th tuple, if config.guess returned "linux-gnu" for build_os
#
if echo "$build_os" | grep '.*-gnulibc1' > /dev/null ; then
	build_os=`echo "${build_os}" | sed 's/-gnulibc1$//'`
fi
if echo "$build_os" | grep '.*-gnu' > /dev/null ; then
	build_os=`echo "${build_os}" | sed 's/-gnu$//'`
fi

changequote(<, >)
build_os_exact="${build_os}"
build_os_major=`echo "${build_os}" | sed 's/\..*$//'`
build_os_noversion=`echo "${build_os}" | sed 's/[0-9]*\..*$//'`
changequote([, ])

rm -f ./find-provides
if test -f ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.prov ; then
	echo "using ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_exact}.prov ; then
	echo "using ${srcdir}/autodeps/${build_os_exact}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_major}.prov ; then
	echo "using ${srcdir}/autodeps/${build_os_major}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_os_major}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_noversion}.prov ; then
	echo "using ${srcdir}/autodeps/${build_os_noversion}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_os_noversion}.prov ./find-provides
else
    echo "*** no default provides information is available for ${build_os_noversion}"
    ln -s ${srcdir}/autodeps/none ./find-provides
fi

rm -f ./find-requires
if test -f ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req ; then
	echo "using ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_exact}.req ; then
	echo "using ${srcdir}/autodeps/${build_os_exact}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_major}.req ; then
	echo "using ${srcdir}/autodeps/${build_os_major}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_os_major}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_noversion}.req ; then
	echo "using ${srcdir}/autodeps/${build_os_noversion}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_os_noversion}.req ./find-requires
else
    echo "*** no default requires information is available for ${build_os_noversion}"
    ln -s ${srcdir}/autodeps/none ./find-requires
fi

dnl XXX Choose /usr/lib or /usr/lib64 for library installs.
MARK64=
if ! echo "${libdir}" | grep -q '64$' ; then
    case "${target_cpu}" in
    x86_64*|ppc64*|powerpc64*|sparc64*|s390x*)	MARK64=64 ;;
    esac
fi
AC_SUBST(MARK64)

# For some systems we know that we have ld_version scripts.
# Use it then as default.
have_ld_version_script=no
case "${host}" in
    *-*-linux*)
        have_ld_version_script=yes
        ;;
    *-*-gnu*)
        have_ld_version_script=yes
        ;;
esac
AC_ARG_ENABLE([ld-version-script],
              AC_HELP_STRING([--enable-ld-version-script],
                             [enable/disable use of linker version script.
                              (default is system dependent)]),
              [have_ld_version_script=$enableval],
              [ : ] )
AM_CONDITIONAL(HAVE_LD_VERSION_SCRIPT, test "$have_ld_version_script" = "yes")

dnl Determine the canonical arch-vendor-os for the build machine
autorelocate_path='%{nil}'
autorelocate_dcolor='0'
case "${build_cpu}" in
*86)		RPMCANONCOLOR=3; RPMCANONARCH=i386 ;;
ia32e*)		RPMCANONCOLOR=3; RPMCANONARCH=ia32e ;;
amd64*)		RPMCANONCOLOR=3; RPMCANONARCH=amd64 ;;
x86_64*)	RPMCANONCOLOR=3; RPMCANONARCH=x86_64 ;;
alpha*)		RPMCANONCOLOR=0; RPMCANONARCH=alpha ;;
sparc64*)	RPMCANONCOLOR=3; RPMCANONARCH=sparc64 ;;
sparc*)		RPMCANONCOLOR=3; RPMCANONARCH=sparc ;;
ia64*)		RPMCANONCOLOR=2; RPMCANONARCH=ia64;
		autorelocate_path='/emul/%%{ARCH}-%%{OS}'
		autorelocate_dcolor='1' ;;
s390x*)		RPMCANONCOLOR=3; RPMCANONARCH=s390x ;;
s390*)		RPMCANONCOLOR=0; RPMCANONARCH=s390 ;;
powerpc64*|ppc64*)	RPMCANONCOLOR=3; RPMCANONARCH=ppc64 ;;
powerpc*|ppc*)	RPMCANONCOLOR=0; RPMCANONARCH=ppc ;;
armv3l*)	RPMCANONCOLOR=0; RPMCANONARCH=armv3l ;;
armv4l*)	RPMCANONCOLOR=0; RPMCANONARCH=armv4l ;;
armv4b*)	RPMCANONCOLOR=0; RPMCANONARCH=armv4b ;;
arm*)		RPMCANONCOLOR=0; RPMCANONARCH="${build_cpu}" ;;
mipsel*)	RPMCANONCOLOR=0; RPMCANONARCH=mipsel ;;
mips*)		RPMCANONCOLOR=0; RPMCANONARCH=mips ;;
m68k*)		RPMCANONCOLOR=0; RPMCANONARCH=m68k ;;
*)		RPMCANONCOLOR=0; RPMCANONARCH=unknown ;;
esac
case "${build_os_noversion}" in
mint)		RPMCANONARCH=m68kmint ;;
esac
RPMCANONVENDOR="$build_vendor"
case "${build_vendor}" in
unknown|pc|ibm|redhat|pld|mandrake|conectiva|lvr|yellowdog|caos|crux)
	test -f /etc/redhat-release &&		RPMCANONVENDOR=redhat
	test -f /etc/pld-release &&		RPMCANONVENDOR=pld
	test -f /etc/mandrake-release &&	RPMCANONVENDOR=mandrake
	test -f /etc/conectiva-release &&	RPMCANONVENDOR=conectiva
	test -f /etc/lvr-release &&		RPMCANONVENDOR=lvr
	test -f /etc/yellowdog-release &&	RPMCANONVENDOR=yellowdog
	test -f /etc/caos-release &&		RPMCANONVENDOR=caos
	test -f /usr/bin/crux &&		RPMCANONVENDOR=crux
	;;
esac
RPMCANONOS="$build_os_noversion"
AC_SUBST(RPMCANONCOLOR)
AC_SUBST(autorelocate_path)
AC_SUBST(autorelocate_dcolor)
AC_SUBST(RPMCANONARCH)
AC_SUBST(RPMCANONVENDOR)
AC_SUBST(RPMCANONOS)

if test X"$prefix" = XNONE ; then
    usrprefix="$ac_default_prefix"
else
    usrprefix=$prefix
fi
LOCALEDIR="`echo ${usrprefix}/share/locale`"
AC_DEFINE_UNQUOTED(LOCALEDIR, "$LOCALEDIR",
	[Full path to rpm locale directory (usually /usr/share/locale)])
AC_SUBST(LOCALEDIR)
LIBDIR="`echo $libdir | sed 's-/lib$-/%{_lib}-'`"
AC_SUBST(LIBDIR)
USRLIBRPM="`echo ${usrprefix}/lib/rpm`"
AC_DEFINE_UNQUOTED(USRLIBRPM, "$USRLIBRPM",
	[Full path to rpm global configuration directory (usually /usr/lib/rpm)])
AC_SUBST(USRLIBRPM)

SYSCONFIGDIR="`echo /etc/rpm`"
AC_DEFINE_UNQUOTED(SYSCONFIGDIR, "$SYSCONFIGDIR",
	[Full path to rpm system configuration directory (usually /etc/rpm)])
AC_SUBST(SYSCONFIGDIR)

MACROFILES="${USRLIBRPM}/${VERSION}/macros:${USRLIBRPM}/%{_target}/macros:${SYSCONFIGDIR}/macros.*:${SYSCONFIGDIR}/macros:${SYSCONFIGDIR}/%{_target}/macros:~/.rpmmacros"
AC_DEFINE_UNQUOTED(MACROFILES, "$MACROFILES",
	[Colon separated paths of macro files to read.])
AC_SUBST(MACROFILES)

RPMRCFILES="${USRLIBRPM}/${RPMCANONVENDOR}/rpmrc:${USRLIBRPM}/${RPMCANONVENDOR}/rpmrc:/etc/rpmrc:~/.rpmrc"
AC_DEFINE_UNQUOTED(RPMRCFILES, "$RPMRCFILES",
	[Colon separated paths of rpmrc files to read.])
AC_SUBST(RPMRCFILES)

RPMPOPTFILE="${USRLIBRPM}/${VERSION}/rpmpopt"
AC_DEFINE_UNQUOTED(RPMPOPTFILE, "$RPMPOPTFILE",
	[Full path to rpmpopt configuration file (usually /usr/lib/rpm/rpmpopt)])
AC_SUBST(RPMPOPTFILE)

testdir="`pwd`/tests"
AC_SUBST(testdir)

AC_SUBST(INCPATH)
AC_SUBST(RPM)

AC_CONFIG_SUBDIRS()

AC_CONFIG_FILES([ Doxyfile Makefile rpmrc macros platform rpmpopt rpm.spec po/Makefile.in
	rpmio/Makefile rpmdb/Makefile lib/Makefile build/Makefile
	scripts/Makefile
	scripts/macros.perl scripts/macros.php scripts/macros.python
	tools/Makefile
	misc/Makefile
	doc/Makefile
	doc/manual/Makefile
	doc/fr/Makefile
	doc/ja/Makefile
	doc/ko/Makefile
	doc/pl/Makefile
	doc/ru/Makefile
	doc/sk/Makefile
	python/Makefile
	python/rpm/Makefile
 	lua/Makefile
  ])
AC_CONFIG_COMMANDS([default],[
    echo timestamp >stamp-h.in
    if test -d popt; then
        echo timestamp >popt/stamp-h.in
    fi
    if test "$with_perl" = yes && test -d perl; then
        ( cd perl && perl Makefile.PL )
    fi
],[])
AC_OUTPUT
