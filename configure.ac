dnl ##
dnl ## configure.ac -- RPM Autoconf specification
dnl ##

dnl ##
dnl ## ==== REQUIREMENTS & INITIALIZATION ====
dnl ##

AC_PREREQ(2.57)
AC_DEFUN([PACKAGE_BUGREPORT_DEFAULT], [rpm-devel@rpm5.org])
AC_INIT(rpm, 5.0, [PACKAGE_BUGREPORT_DEFAULT])
AC_CONFIG_SRCDIR([rpmqv.c])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign tar-ustar])
AM_MAINTAINER_MODE

dnl # allow packagers to override the bugreport address
AC_ARG_WITH(bugreport,
    AS_HELP_STRING([--with-bugreport=ADDRESS], [set packager bugreport Email address (PACKAGE_BUGREPORT_DEFAULT)]), [dnl
    case "$withval" in
        *@* ) PACKAGE_BUGREPORT="$withval" ;;
        *   ) AC_MSG_ERROR([invalid bugreport Email address]) ;;
    esac
])

dnl # Library code modified:                              REVISION++
dnl # Interfaces changed/added/removed:   CURRENT++       REVISION=0
dnl # Interfaces added:                             AGE++
dnl # Interfaces removed:                           AGE=0
AC_SUBST(LT_CURRENT,  5)
AC_SUBST(LT_REVISION, 0)
AC_SUBST(LT_AGE,      0)

dnl # set of available languages.
ALL_LINGUAS="cs da de fi fr gl is ja ko no pl pt pt_BR ro ru sk sl sr sv tr uk"

dnl ##
dnl ## ==== TOOL-CHAIN CHECKS ====
dnl ##

AC_MSG_HEADER([TOOL-CHAIN CHECKS])

AC_PROG_CC
AC_PROG_CPP

AC_AIX
AC_MINIX

AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_YACC

AC_PATH_PROG(AS, as, as)

dnl # GCC specifics
AC_PROG_GCC_TRADITIONAL
AC_ARG_ENABLE(build-pic,
    AS_HELP_STRING([--enable-build-pic], [build RPM with Position Independent Code (PIC) (GCC only)]), [dnl
    if test ".$enableval" = .yes; then
        if test ".$ac_cv_c_compiler_gnu" = .yes; then
            CFLAGS="$CFLAGS -fPIC -DPIC"
        else
            AC_MSG_WARN([--enable-build-pic requires GCC])
        fi
    fi
])
AC_ARG_ENABLE(build-pie,
    AS_HELP_STRING([--enable-build-pie], [build RPM as Position Independent Executable (PIE) (GCC only)]), [dnl
    if test ".$enableval" = .yes; then
        if test ".$ac_cv_c_compiler_gnu" = .yes; then
            CFLAGS="$CFLAGS -fpie"
            LDFLAGS="$LDFLAGS -pie"
        else
            AC_MSG_WARN([--enable-build-pie requires GCC])
        fi
    fi
])
AC_ARG_ENABLE(build-warnings,
    AS_HELP_STRING([--enable-build-warnings], [build RPM with extra compiler warnings turned on (GCC only)]), [dnl
    if test ".$enableval" = .yes; then
        if test ".$ac_cv_c_compiler_gnu" = .yes; then
            CFLAGS="$CFLAGS -Wall -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wno-char-subscripts"
        else
            AC_MSG_WARN([--enable-build-warnings requires GCC])
        fi
    fi
])

dnl # support static linking of RPM via GNU libtool
LDFLAGS_STATIC=""
AC_ARG_ENABLE(build-static,
    AS_HELP_STRING([--enable-build-static], [build RPM as a static executable]), [dnl
    AC_MSG_CHECKING([whether to build RPM as a static executable (via GNU libtool)])
    if test ".$enableval" = .yes; then
        case "$target" in
            *-*-darwin*)    LDFLAGS_STATIC="";;             # Mac OS X does not do static binaries.
            *-*-sysv5uw*)   LDFLAGS_STATIC="-static";;      # Unixware has no shared libthread.
            *-*-linux*)     LDFLAGS_STATIC="-all-static";;
            *-*-freebsd*)   LDFLAGS_STATIC="-all-static";;
            *-*-osf*)       LDFLAGS_STATIC="";;             # OSF5 has no shared pthreads libs
            *-*-aix*)       LDFLAGS_STATIC="-static";;      # -Wl,-bnso doesn't seem to work...
            *-*-hpux*)      LDFLAGS_STATIC="-static";;
            *-*-solaris*)   LDFLAGS_STATIC="-static";;
            *-*-irix*)      LDFLAGS_STATIC="-static";;      # should be -non_shared, but can't link because of crt1.o then.
            *-*-ultrix*)    LDFLAGS_STATIC="-all-static";;  # ultrix doesn't have shared libs.
            *-*-*)          LDFLAGS_STATIC="-static";;      # let GNU libtool decide how to do it for the particular platform
        esac
        case "$target" in
            *-*-linux*)
                dnl # use libpthread.a that is NPTL aware
                if test -f /usr/lib/nptl/libpthread.a; then
                    LDFLAGS_STATIC="$LDFLAGS_STATIC -L/usr/lib/nptl"
                fi
                if test -f /usr/lib64/nptl/libpthread.a; then
                    LDFLAGS_STATIC="$LDFLAGS_STATIC -L/usr/lib64/nptl"
                fi
                ;;
        esac
        AC_MSG_RESULT([yes (flag "$LDFLAGS_STATIC")])
    else
        AC_MSG_RESULT([no])
    fi
])
AC_SUBST(LDFLAGS_STATIC)

dnl # use ld_version scripts on platforms known to support it.
have_ld_version_script=no
case "${host}" in
    *-*-linux* ) have_ld_version_script=yes ;;
    *-*-gnu*   ) have_ld_version_script=yes ;;
esac
AC_ARG_ENABLE(
    [build-versionscript],
    AC_HELP_STRING(
        [--enable-build-versionscript],
        [enable/disable use of GNU ld(1) version script (default is system dependent)]),
    [have_ld_version_script=$enableval], [:])
AM_CONDITIONAL(
    [HAVE_LD_VERSION_SCRIPT],
    [test ".$have_ld_version_script" = .yes])

dnl # FIXME: Linux hack: choose /usr/lib or /usr/lib64 for library installs and linking
MARK64=
case "$target" in
    *-*-linux* )
        if ! echo "${libdir}" | grep -q '64$'; then
            case "${target_cpu}" in
                x86_64*|ppc64*|powerpc64*|sparc64*|s390x*) MARK64=64 ;;
            esac
        fi
        ;;
esac
AC_ARG_ENABLE(
    [build-lib64],
    AC_HELP_STRING([--enable-build-lib64], [enable use of /usr/lib64 (default is system dependent)]),
    [ if test ".$enableval" = .yes; then MARK64=64; elif test ".$enableval" = .no; then MARK64=""; fi ], [:])
AC_SUBST(MARK64)

dnl # FIXME: partly Linux-specific only!?
dnl # (but at least it usually doesn't harm on other platforms)
CFLAGS="$CFLAGS -D_GNU_SOURCE -D_REENTRANT"

dnl # FIXME: old RPM 4.x style platform-specific flag extension hack
addlib () {
   l=$1
   shift
   case "$target" in 
       *-*-solaris2.[234567] ) LIBS="$LIBS -L$l -R$l $*" ;;
       * )                     LIBS="$LIBS -L$l $*"      ;;
   esac
}
if  test "$cross_compiling" != "yes" && \
    test ".$prefix" != .NONE && \
    test ".$prefix" != "./usr"; then
    if test -d $prefix/lib; then
        addlib $prefix/lib
    fi
    if test -d $prefix/include; then
        CPPFLAGS="$CPPFLAGS -I$prefix/include"
    fi
fi
if test -d /usr/ucblib; then
    case $build in
        mips-sni-sysv4 )
            addlib /usr/ccs/lib -lc
            addlib /usr/ucblib
        ;;
    esac
fi

dnl ##
dnl ## ==== PROGRAM CHECKS ====
dnl ##

AC_MSG_HEADER([PROGRAM CHECKS])

dnl # look for POSIX chmod attributes
AC_MSG_CHECKING(POSIX chmod)
touch foo.chmodtest
chmod 744 foo.chmodtest
chmod +X foo.chmodtest 2>/dev/null || true
a=`ls -l foo.chmodtest | awk '{print $1}'`
rm -f foo.chmodtest
if test "$a" = "-rwxr-xr-x"; then
    AC_MSG_RESULT(yes)
    FIXPERMS=a+rX,u+w,g-w,o-w 
else
    AC_MSG_RESULT([no (tell your OS vendor about GNU fileutils)])
    FIXPERMS=a+r,u+w,g-w,o-w 
fi
AC_SUBST(FIXPERMS)

dnl # see if we have a mkdir that supports `-p'.
AC_PATH_PROGS(MKDIR, mkdir, mkdir)
AC_MSG_CHECKING(if $MKDIR supports -p)
rm -rf conftest
$MKDIR -p conftest/a 2>/dev/null
if test $? = 0 ; then
	rmdir conftest/a 2>/dev/null
	if test $? != 0; then
		MKDIR_P=0
	fi
	rmdir conftest 2>/dev/null
	if test $? = 0; then
		MKDIR_P="$MKDIR -p"
	else
		MKDIR_P=0
	fi
else
	MKDIR_P=0
fi
if test X"$MKDIR_P" = X0; then
	AC_MSG_RESULT(no)
	MKDIR_P="`echo ${prefix}/lib/rpm/mkinstalldirs`"
else
	AC_MSG_RESULT(yes)
fi
AC_DEFINE_UNQUOTED(MKDIR_P, "${MKDIR_P}", [full path to a 'mkdir -p' style program])
AC_SUBST(MKDIR_P)

dnl # check for chown and symlink handling 
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

dnl # find some common programs
if test "$cross_compiling" = "yes"; then
    MYPATH=":"
else
    MYPATH=$PATH
fi
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

dnl # check for id -u
AC_MSG_CHECKING(checking whether id supports -u)
if ${__ID} -u 2>&1 > /dev/null; then
    __ID_U="%{__id} -u"
    AC_MSG_RESULT(yes)
else
    __ID_U="%{__id} | %{__sed} 's/[[^=]]*=\\\\([[0-9]][[0-9]]*\\\\).*$/\\\\1/'"
    AC_MSG_RESULT(no)
fi
AC_SUBST(__ID_U)

dnl # auto-detect whether doxygen generated API docs should be included.
AC_PATH_PROG(__DOXYGEN, doxygen, no, $PATH)
withval=auto
AC_ARG_WITH(apidocs, AS_HELP_STRING([--with-apidocs], [build RPM API documentation]))
if test $withval = auto -a $__DOXYGEN != no; then
  withval=yes
elif test $withval = yes -a $__DOXYGEN = no; then
  AC_MSG_ERROR([RPM API docs need doxygen in PATH])
fi
if test $withval = yes; then
  WITH_APIDOCS_TARGET=apidocs
  WITH_APIDOCS=1
else
  WITH_APIDOCS=0
fi
AC_SUBST(WITH_APIDOCS_TARGET)
AC_SUBST(WITH_APIDOCS)

dnl ##
dnl ## ==== API CHECKS ====
dnl ##

AC_MSG_HEADER([API CHECKS])

dnl # platform specific checks
AC_ISC_POSIX

dnl # standard checks
AM_C_PROTOTYPES
AC_C_INLINE

dnl # standard headers (specific)
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_DIRENT
AC_HEADER_TIME

dnl # standard headers (generic)
AC_CHECK_HEADERS([dnl
    fcntl.h getopt.h grp.h memory.h netdb.h pwd.h utime.h dnl
    signal.h sys/ipc.h sys/socket.h sys/select.h dnl
    sys/types.h sys/stdtypes.h sys/mman.h sys/resource.h dnl
    sys/utsname.h sys/wait.h netinet/in_systm.h dnl
    machine/types.h mntent.h sys/mnttab.h sys/systemcfg.h dnl
    sys/mount.h sys/mntctl.h sys/param.h sys/vmount.h dnl
    libio.h err.h mcheck.h limits.h libgen.h float.h dnl
    glob.h poll.h dnl
])

dnl # GNU gettext support
AC_CHECK_HEADERS(locale.h)
AM_GNU_GETTEXT_VERSION(0.16)
AM_GNU_GETTEXT([external])
AM_CONDITIONAL(USE_NLS, [ test ".$USE_NLS" = .yes ])

dnl # specific features
AC_SYS_LARGEFILE
AC_CHECK_VA_COPY
AC_CHECK_STATFS

dnl # specific functions (specific)
AC_CHECK_FUNC(setreuid,      [], [ AC_CHECK_LIB(ucb, setreuid, [if echo $LIBS | grep -- -lucb >/dev/null; then :; else LIBS="$LIBS -lc -lucb" USEUCB=y; fi]) ])
AC_CHECK_FUNC(rand,          [], [ AC_CHECK_LIB(rand, rand, []) ])
AC_CHECK_FUNC(getdomainname, [], [ AC_CHECK_LIB(nsl, getdomainname) ])
AC_CHECK_FUNC(socket,        [], [ AC_CHECK_LIB(socket, socket) ])

dnl # specific functions (generic)
AC_CHECK_FUNCS(vsnprintf snprintf)

dnl # Error API
AC_CHECK_HEADERS(error.h)
AC_CHECK_FUNCS(error)

dnl # Pthread API
AC_CHECK_HEADERS(thread.h pthread.h synch.h semaphore.h)
AC_CHECK_LIB(pthread, pthread_mutex_trylock, [], [
    AC_CHECK_LIB(pthread, __pthread_mutex_trylock, [], [
        AC_CHECK_LIB(thread, mutex_lock)
    ])
])

dnl # AIO API
AC_CHECK_HEADERS(aio.h)
AC_SEARCH_LIBS(aio_read, [c rt aio posix4])

dnl # FIXME: platform-hack: MiNT
AC_CHECK_LIB(port, writev)

dnl # FIXME: platform-hack: AmigaOS and IXEmul have a fork() dummy
case "$target" in
    m68k-*-amigaos ) CFLAGS="$CFLAGS -Dfork=vfork" ;;
esac

dnl # FIXME: platform-hack: SCO and SunOS 4.x
if echo "$build_os" | grep sco > /dev/null; then
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(HAVE_STRUCT_MNTTAB, 1, [Define as 1 if you have "struct mnttab" (only sco?)])
elif echo "$build_os" | grep sunos > /dev/null; then
	CFLAGS="$CFLAGS -D__USE_FIXED_PROTOTYPES__"
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(NEED_MYREALLOC, 1, [Define as 1 if we need myrealloc])
	AC_LIBOBJ(myrealloc)
fi

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

dnl check for missing typedefs
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

dnl checks for library functions (specific)
AC_FUNC_ALLOCA
AC_FUNC_VPRINTF
AC_FUNC_MMAP

dnl checks for library functions (generic)
AC_CHECK_FUNCS([dnl
    basename getaddrinfo getcwd getnameinfo getwd inet_aton dnl
    mtrace putenv realpath setenv clearenv stpcpy stpncpy strcspn strdup dnl
    strndup strerror strtol strtoul strspn strstr sighold sigrelse sigpause dnl
    sigprocmask sigemptyset sigaddset sigdelset sigsuspend madvise dnl
    __secure_getenv regcomp ftok mkstemp setlocale getpassphrase dnl
])

dnl # check for getmntent and alternatives
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

dnl ##
dnl ## ==== THIRD-PARTY LIBRARIES (1/2) ====
dnl ##

AC_MSG_HEADER([THIRD-PARTY LIBRARIES (1/2)])

dnl # optional ELF support
WITH_LIBELF_DEBUGEDIT=""
AC_MSG_CHECKING([whether to use libelf/gelf API])
AC_ARG_WITH(libelf, AS_HELP_STRING([--with-libelf], [use libelf/gelf API]), [
    if test ".$withval" = .yes; then
        AC_MSG_RESULT(yes)
        AC_CHECK_HEADERS([libelf.h gelf.h])
        if test ".$ac_cv_header_libelf_h" = .no; then
            dnl # <libelf.h> on Solaris is incompatible with LFS. If we couldn't
            dnl # include <libelf.h>, see if we can when _FILE_OFFSET_BITS is
            dnl # set for non-LFS. Same applies for <gelf.h>, since it probably
            dnl # includes <libelf.h>
            AC_MSG_CHECKING([if libelf.h conflicts with _LARGEFILE_SOURCE])
            AC_COMPILE_IFELSE([
                AC_LANG_PROGRAM([[
#undef _LARGEFILE64_SOURCE
#undef _LARGEFILE_SOURCE
#if !defined(_LP64)
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif
#include <libelf.h>
                ]], [[
                    Elf_Data foo;
                ]])
            ], [
                AC_DEFINE(HAVE_LIBELF_H, 1, [Define to 1 if you have libelf.h])
                ac_cv_header_libelf_h=yes
                AC_DEFINE(LIBELF_H_LFS_CONFLICT, 1, [Define to 1 if libelf.h is incompatible with LFS API])
                AC_MSG_RESULT(yes)
            ], [
                AC_MSG_RESULT(no)
            ])
        fi
        if test ".$ac_cv_header_gelf_h" = .no; then
            AC_MSG_CHECKING([if gelf.h conflicts with _LARGEFILE_SOURCE])
            AC_COMPILE_IFELSE([
                AC_LANG_PROGRAM([[
#undef _LARGEFILE64_SOURCE
#undef _LARGEFILE_SOURCE
#if !defined(_LP64)
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif
#include <gelf.h>
                ]], [[
                    Elf32_Verdef foo;
                ]])
            ], [
                AC_DEFINE(HAVE_GELF_H, 1, [Define to 1 if you have gelf.h])
                ac_cv_header_gelf_h=yes
                AC_MSG_RESULT(yes)
            ], [
                AC_MSG_RESULT(no)
          ])
        fi
        if test ".$ac_cv_header_gelf_h" = .yes; then
            AC_CHECK_LIB([elf], [elf_version], [
                AC_CHECK_LIB([elf], [gelf_getvernaux], [
                    AC_DEFINE(HAVE_GELF_GETVERNAUX, 1, [Define to 1 if you have the gelf_getvernaux() function.])
                ])
                AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have ELF library])
                WITH_LIBELF_DEBUGEDIT="debugedit"
                LIBS="$LIBS -lelf"
            ])
        else
            AC_MSG_ERROR([libelf/gelf API requested but not found or not usable])
        fi
    else
        AC_MSG_RESULT(no)
    fi
], [
    AC_MSG_RESULT(no)
])
AC_SUBST(WITH_LIBELF_DEBUGEDIT)

dnl # optional SELinux support
AC_ARG_WITH(selinux, AS_HELP_STRING([--with-selinux], [build RPM with SELinux support]), [
    if test ".$withval" = .yes; then
        AC_DEFINE(WITH_SELINUX, 1, [Build with SELinux support?])
        LIBS="$LIBS -lselinux"
    fi
])

dnl # optional Perl language bindings
WITH_PERL_SUBDIR=""
WITH_PERL_SUBPACKAGE=0
AC_ARG_WITH(perl, AS_HELP_STRING([--with-perl], [build with RPM Perl language bindings]), [
    if test ".$withval" != .no; then
        WITH_PERL_SUBDIR=perl
        WITH_PERL_SUBPACKAGE=1
    fi
])
AC_SUBST(WITH_PERL_SUBDIR)
AC_SUBST(WITH_PERL_SUBPACKAGE)

dnl # optional Python language bindings
WITH_PYTHON_SUBDIR=""
WITH_PYTHON_SUBPACKAGE=0
WITH_PYTHON_VERSION=""
AC_ARG_WITH(python, AS_HELP_STRING([--with-python], [build with RPM Python language bindings]), [
    if test ".$withval" != .no; then
        case "$withval" in
             2.x|2.4|2.5)
                  python_version=$with_val
                  ;;
             yes)
                  python_version="2.5 2.4 2.x"
                  ;;
             *)
                  AC_MSG_WARN([--with-python only supports values 2.x, 2.4, or 2.5, ignoring...])
                  python_version=""
                  ;;
        esac
	for python_ver in $python_version ; do
             if test "$python_ver" != "2.x" ; then
                  AC_MSG_CHECKING([for Python ${python_ver} API])
                  AC_RUN_IFELSE([AC_LANG_SOURCE(
                  [[#include <python${python_ver}/Python.h>
                     main() { exit(strncmp("${python_ver}", PY_VERSION, 3)); } ]])
                  ], [
                     WITH_PYTHON_VERSION="${python_ver}"
                     AC_MSG_RESULT(yes)
                  ], [ 
                     AC_MSG_RESULT(no)
                  ], [
                     dnl if we're cross compiling, assume the user has a clue
                     if test "$withval" = "$python_ver"; then
                         WITH_PYTHON_VERSION="${python_ver}"
                         AC_MSG_RESULT(yes)
                     else
                         AC_MSG_RESULT(no)
                     fi
                  ])
             else
                  AC_MSG_CHECKING([for Python 2.x API])
                  AC_RUN_IFELSE([AC_LANG_SOURCE(
                  [[#include <python/Python.h>
                     main() { exit(strncmp("2.", PY_VERSION, 2)); } ]])
                  ], [
                     WITH_PYTHON_VERSION=""
                     AC_MSG_RESULT(yes)
                  ], [ 
                     AC_MSG_RESULT(no)
                  ], [
                     dnl if we're cross compiling, assume the user has a clue
                     if test "$withval" = "$python_ver"; then
                         WITH_PYTHON_VERSION="${python_ver}"
                         AC_MSG_RESULT(yes)
                     else
                         AC_MSG_RESULT(no)
                     fi
                  ])
             fi
             if test ".$WITH_PYTHON_VERSION" != "."; then
                  WITH_PYTHON_SUBDIR=python
                  WITH_PYTHON_SUBPACKAGE=1
                  break
             fi
	done
    fi
])
AC_SUBST(WITH_PYTHON_SUBDIR)
AC_SUBST(WITH_PYTHON_SUBPACKAGE)
AC_SUBST(WITH_PYTHON_VERSION)

dnl ##
dnl ## ==== THIRD-PARTY LIBRARIES (2/2) ====
dnl ##

AC_MSG_HEADER([THIRD-PARTY LIBRARIES (2/2)])

dnl # ZLib
RPM_CHECK_LIB(
    [ZLib], [zlib],
    [z gz], [gzread], [zlib.h],
    [yes,internal:none], [zlib],
    [ AC_CHECK_FUNC(gzseek)
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_ZLIB_H, 1, [Define to 1 if you have <zlib.h>])
      fi
    ], [])

dnl # GNU BZip2
RPM_CHECK_LIB(
    [GNU BZip2], [bzip2],
    [bz2], [BZ2_bzread], [bzlib.h],
    [no,external:none], [],
    [], [])

dnl # BeeCrypt
RPM_CHECK_LIB(
    [BeeCrypt], [beecrypt],
    [beecrypt], [mpfprintln], [beecrypt/api.h],
    [yes,external], [beecrypt],
    [], [ AC_MSG_ERROR([mandatory BeeCrypt library not found]) ])

dnl # Neon
RPM_CHECK_LIB(
    [Neon], [neon],
    [neon], [ne_session_create], [ne_session.h],
    [yes,external:none], [],
    [ AC_CHECK_LIB(neon, ne_get_response_header, [
          AC_DEFINE(HAVE_NEON_NE_GET_RESPONSE_HEADER, 1,
                    [Define to 1 if you have ne_get_response_header() in libneon.]) ])
      AC_CHECK_LIB(neon, ne_send_request_chunk, [
          AC_DEFINE(HAVE_NEON_NE_SEND_REQUEST_CHUNK, 1,
                    [Define to 1 if you have ne_send_request_chunk() in libneon.]) ])
    ], [ AC_MSG_WARN([No Neon library found, using unsupported configuration]) ])

dnl # File (magic)
RPM_CHECK_LIB(
    [File (magic)], [file],
    [magic], [magic_open], [magic.h],
    [yes,external:internal:none], [file:src:src],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_MAGIC_H, 1, [Define to 1 if you have <magic.h>])
      fi
    ], [])

dnl # POPT
RPM_CHECK_LIB(
    [POPT], [popt],
    [popt], [poptGetContext], [popt.h],
    [yes,external], [],
    [], [AC_MSG_ERROR([mandatory POPT library not found])])

dnl # Berkeley-DB & SQLite
DBLIBSRCS=""
RPM_CHECK_LIB(
    [Berkeley-DB], [db],
    [db-4.5 db], [db_create], [db.h],
    [yes,internal:external], [db3],
    [ DBLIBSRCS="$DBLIBSRCS db3.c"
      AM_CONDITIONAL(WITH_DB, [ true ])
      AM_CONDITIONAL(WITH_DB_INTERNAL, [ test ".$RPM_CHECK_LIB_LOCATION" = .internal ])
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_DB_H, 1, [Have <db.h> header])
      elif test ".$PACKAGE_BUGREPORT" != ".]m4_defn([PACKAGE_BUGREPORT_DEFAULT])["; then
          AC_MSG_WARN([using external Berkeley-DB not recommended -- proceeding on packagers responsibility])
      else
          AC_MSG_ERROR([using external Berkeley-DB not supported (without packager taking responsibility first)])
      fi
    ],
    [ AM_CONDITIONAL(WITH_DB, [ false ])
      AM_CONDITIONAL(WITH_DB_INTERNAL, [ false ])
      if test ".$PACKAGE_BUGREPORT" != ".]m4_defn([PACKAGE_BUGREPORT_DEFAULT])["; then
          AC_MSG_WARN([not using Berkeley-DB at all not recommended -- proceeding on packagers responsibility])
      else
          AC_MSG_ERROR([not using Berkeley-DB at all not supported (without packager taking responsibility first)])
      fi
    ])
RPM_CHECK_LIB(
    [SQLite], [sqlite],
    [sqlite3], [sqlite3_open], [sqlite3.h],
    [no,external:none], [],
    [ DBLIBSRCS="$DBLIBSRCS sqlite.c" ],
    [])
DBLIBOBJS=`echo $DBLIBSRCS | sed -e "s/\.c/\.lo/g"`
AC_SUBST(DBLIBSRCS)
AC_SUBST(DBLIBOBJS)
AC_ARG_WITH(db-largefile, AS_HELP_STRING([--with-db-largefile], [build Berkeley-DB with LARGEFILE support]))
AC_ARG_WITH(db-mutex,     AS_HELP_STRING([--with-db-mutex=ARG], [build Berkeley-DB with MUTEX type ARG]))
AC_ARG_WITH(db-rpc,       AS_HELP_STRING([--with-db-rpc],       [build Berkeley-DB with RPC support]))
AM_CONDITIONAL(WITH_DB_RPC, test ".$with_db_rpc" = .yes)

dnl # determine default RPM DB API and configuration
DBAPI=0
case ".$DBLIBSRCS" in
    *db3.c*    ) DBAPI=3 ;;
    *sqlite.c* ) DBAPI=4 ;;
    *          ) AC_MSG_ERROR([either Berkeley-DB (--with-db) or SQLite (--with-sqlite) have to be used]) ;;
esac
AC_ARG_WITH([dbapi],
    AS_HELP_STRING([--with-dbapi=ID], [use particular default RPM DB API ("db", "sqlite" or numeric)]), [
    case "$withval" in
        db|3     ) DBAPI=3 ;;
        sqlite|4 ) DBAPI=4 ;;
        [0-9]    ) DBAPI=$withval ;;
    esac
])
AC_SUBST(DBAPI)

dnl # Lua
WITH_LUA_SUBDIR_DEF=""
WITH_LUA_SUBDIR_LIB=""
RPM_CHECK_LIB(
    [Lua], [lua],
    [lua], [_], [_],
    [no,internal:none], [lua:include,local],
    [ AC_CHECK_LIB(dl, dlopen, [ LIBS="-ldl $LIBS"; WITH_LUA_SUBDIR_LIB="-ldl" ])
      AC_CHECK_FUNC(dlopen, [ WITH_LUA_SUBDIR_DEF="-DUSE_DLOPEN" ]) ],
    [])
AC_SUBST(WITH_LUA_SUBDIR_DEF)
AC_SUBST(WITH_LUA_SUBDIR_LIB)

dnl # DMalloc
RPM_CHECK_LIB(
    [DMalloc], [dmalloc],
    [dmalloc], [dmalloc_malloc], [dmalloc.h],
    [no,external:none], [],
    [], [])

dnl # Electric Fence
RPM_CHECK_LIB(
    [Electric Fence], [efence],
    [efence], [malloc], [stdlib.h],
    [no,external:none], [],
    [], [])

dnl # Linux keyutils
RPM_CHECK_LIB(
    [Linux keyutils], [keyutils],
    [keyutils], [keyctl_read], [keyutils.h],
    [no,external:none], [],
    [], [])

dnl ##
dnl ## ==== INSTALLATION PARAMETERS ====
dnl ##

AC_MSG_HEADER([INSTALLATION PARAMETERS])

dnl # figure out what root's primary group is
AC_MSG_CHECKING([root's primary group])
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
AC_MSG_RESULT([$ROOT_GROUP])
AC_SUBST(ROOT_GROUP)

dnl # provide platform-specific "find-provides" and "find-requires" script symlinks
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
    ln -s ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_exact}.prov ; then
    ln -s ${srcdir}/autodeps/${build_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_major}.prov ; then
    ln -s ${srcdir}/autodeps/${build_os_major}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_noversion}.prov ; then
    ln -s ${srcdir}/autodeps/${build_os_noversion}.prov ./find-provides
else
    ln -s ${srcdir}/autodeps/none ./find-provides
fi
rm -f ./find-requires
if test -f ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req ; then
    ln -s ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_exact}.req ; then
    ln -s ${srcdir}/autodeps/${build_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_major}.req ; then
    ln -s ${srcdir}/autodeps/${build_os_major}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_noversion}.req ; then
    ln -s ${srcdir}/autodeps/${build_os_noversion}.req ./find-requires
else
    ln -s ${srcdir}/autodeps/none ./find-requires
fi

dnl # determine the canonical arch-vendor-os for the build machine
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

dnl # RPM installation user/group and uid/gid
: ${RPMUSER=rpm}
: ${RPMUID=37}
: ${RPMGROUP=rpm}
: ${RPMGID=37}
export RPMUSER RPMUID RPMGROUP RPMGID
AC_SUBST(RPMUSER)
AC_SUBST(RPMUID)
AC_SUBST(RPMGROUP)
AC_SUBST(RPMGID)

dnl # FIXME: hard-coded paths?!
if test "x$varprefix" = "x"; then
    # For /usr and /usr/local, we want the 'var' directory to go
    # in /var and /var/local respectively. For everything else, 
    # just put the 'var' directory in prefix/var.
    case $prefix in
	/usr | /usr/local ) varprefix=`echo $prefix | sed 's/usr/var/'` ;;
	NONE              ) varprefix=`echo $ac_default_prefix | sed 's/usr/var/'` ;;
	*                 ) varprefix=$prefix/var ;;
    esac	
fi
AC_SUBST(varprefix)
if test X"$prefix" = XNONE ; then
    usrprefix="$ac_default_prefix"
else
    usrprefix=$prefix
fi

dnl # determine RPM locale directory path
AC_MSG_CHECKING([for RPM locale directory])
LOCALEDIR=`echo "${usrprefix}/share/locale"`
AC_ARG_WITH(
    [path-locale],
    AS_HELP_STRING([--with-path-locale=ARG], [build with RPM locale directory path]),
    [LOCALEDIR="$withval"]
)
AC_DEFINE_UNQUOTED(
    [LOCALEDIR], ["$LOCALEDIR"],
    [Full path to RPM locale directory]
)
AC_SUBST(LOCALEDIR)
AC_MSG_RESULT([$LOCALEDIR])

dnl # determine RPM global library directory path
AC_MSG_CHECKING([for RPM global library directory])
LIBDIR=`echo "$libdir" | sed 's-/lib$-/%{_lib}-'`
AC_ARG_WITH(
    [path-lib],
    AS_HELP_STRING([--with-path-lib=ARG], [build with RPM global library directory path]),
    [LIBDIR="$withval"]
)
AC_SUBST(LIBDIR)
AC_MSG_RESULT([$LIBDIR])

dnl # determine RPM system library directory path
AC_MSG_CHECKING([for RPM system library directory])
USRLIBRPM=`echo "${usrprefix}/lib/rpm"`
AC_ARG_WITH(
    [path-usrlib],
    AS_HELP_STRING([--with-path-usrlib=ARG], [build with RPM system library directory path]),
    [USRLIBRPM="$withval"]
)
AC_DEFINE_UNQUOTED(
    [USRLIBRPM], ["$USRLIBRPM"],
    [Full path to RPM system library directory]
)
AC_SUBST(USRLIBRPM)
AC_MSG_RESULT([$USRLIBRPM])

dnl # determine RPM global configuration directory path
AC_MSG_CHECKING([for RPM global configuration directory])
SYSCONFIGDIR="/etc/rpm"
AC_ARG_WITH(
    [path-cfg],
    AS_HELP_STRING([--with-path-cfg=ARG], [build with RPM global configuration directory path]),
    [SYSCONFIGDIR="$withval"]
)
AC_DEFINE_UNQUOTED(
    [SYSCONFIGDIR], ["$SYSCONFIGDIR"],
    [Full path to RPM global configuration directory]
)
AC_SUBST(SYSCONFIGDIR)
AC_MSG_RESULT([$SYSCONFIGDIR])

dnl # determine RPM macro files path
AC_MSG_CHECKING([for RPM macro files path])
MACROFILES="${USRLIBRPM}/${VERSION}/macros"
MACROFILES="${MACROFILES}:${USRLIBRPM}/%{_target}/macros"
MACROFILES="${MACROFILES}:${SYSCONFIGDIR}/macros.*"
MACROFILES="${MACROFILES}:${SYSCONFIGDIR}/macros"
MACROFILES="${MACROFILES}:${SYSCONFIGDIR}/%{_target}/macros"
MACROFILES="${MACROFILES}:~/.rpmmacros"
AC_ARG_WITH(
    [path-macros],
    AS_HELP_STRING([--with-path-macros=ARG], [build with colon-separated RPM macro files path]),
    [MACROFILES="$withval"]
)
AC_DEFINE_UNQUOTED(
    [MACROFILES], ["$MACROFILES"],
    [Colon separated RPM macro files path]
)
AC_SUBST(MACROFILES)
AC_MSG_RESULT([$MACROFILES])

dnl # determine RPM run-command files path
AC_MSG_CHECKING([for RPM run-command files path])
RPMRCFILES="${USRLIBRPM}/${RPMCANONVENDOR}/rpmrc"
RPMRCFILES="${RPMRCFILES}:${USRLIBRPM}/${RPMCANONVENDOR}/rpmrc"
RPMRCFILES="${RPMRCFILES}:/etc/rpmrc"
RPMRCFILES="${RPMRCFILES}:~/.rpmrc"
AC_ARG_WITH(
    [path-rpmrc],
    AS_HELP_STRING([--with-path-rpmrc=ARG], [build with colon-separated RPM run-command files path]),
    [RPMRCFILES="$withval"]
)
AC_DEFINE_UNQUOTED(
    [RPMRCFILES], ["$RPMRCFILES"],
    [Colon separated RPM run-command files path]
)
AC_SUBST(RPMRCFILES)
AC_MSG_RESULT([$RPMRCFILES])

dnl # determine POPT configuration file path
AC_MSG_CHECKING([for POPT configuration file path])
RPMPOPTFILE="${USRLIBRPM}/${VERSION}/rpmpopt"
AC_ARG_WITH(
    [path-rpmpopt],
    AS_HELP_STRING([--with-path-rpmpopt=ARG], [build with POPT configuration file path]),
    [RPMPOPTFILE="$withval"]
)
AC_DEFINE_UNQUOTED(
    [RPMPOPTFILE], ["$RPMPOPTFILE"],
    [Full path to "rpmpopt" configuration file]
)
AC_SUBST(RPMPOPTFILE)
AC_MSG_RESULT([$RPMPOPTFILE])

testdir="`pwd`/tests"
AC_SUBST(testdir)

dnl ##
dnl ## ==== OUTPUT GENERATION ====
dnl ##

AC_MSG_HEADER([OUTPUT GENERATION])

dnl # explicit list of sub-directories to step into for additional "configure" scripts
dnl # (RPM_CHECK_LIB extends this implicitly, too)
AC_CONFIG_SUBDIRS()

dnl # explicit list of files to generate
AC_CONFIG_FILES([ po/Makefile.in
    Doxyfile Makefile rpmrc macros rpmpopt rpm.spec
    rpmio/Makefile rpmdb/Makefile lib/Makefile build/Makefile
    scripts/Makefile scripts/macros.perl scripts/macros.php
    scripts/macros.python tools/Makefile misc/Makefile doc/Makefile
    doc/manual/Makefile doc/fr/Makefile doc/ja/Makefile doc/ko/Makefile
    doc/pl/Makefile doc/ru/Makefile doc/sk/Makefile python/Makefile
    python/rpm/Makefile lua/Makefile scripts/rpm.pc
])

dnl # explicit list of commands to execute
AC_CONFIG_COMMANDS([default], [
    if test "$.with_perl" = .yes && test -d perl; then
        ( cd perl && perl Makefile.PL )
    fi
],[])

dnl # final step: generate output
AC_OUTPUT

