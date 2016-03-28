dnl ##
dnl ## configure.ac -- RPM Autoconf specification
dnl ##

dnl ##
dnl ## ==== REQUIREMENTS & INITIALIZATION ====
dnl ##

AC_PREREQ([2.63])
m4_define([PACKAGE_BUGREPORT_DEFAULT], [rpm-devel@rpm5.org])
AC_INIT([rpm],[5.4.16],[PACKAGE_BUGREPORT_DEFAULT])
PACKAGE_TIMESTAMP="2016-03-15" dnl # YYYY[-MM[-DD[ HH[:MM]]]]
AC_MSG_TITLE([RPM Package Manager (RPM)], [$PACKAGE_VERSION])

AC_MSG_HEADER([INITIALIZATION])
AC_CONFIG_SRCDIR([rpmqv.c])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign tar-ustar subdir-objects color-tests])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_REQUIRE_AUX_FILE([install-sh])
AC_REQUIRE_AUX_FILE([mkinstalldirs])
AM_MAINTAINER_MODE

dnl # allow packagers to override the officially branding name
RPM_NAME="RPM"
AC_ARG_WITH(name,
    AS_HELP_STRING([--with-name=NAME], [set official name of program (default: "RPM")]), [dnl
    RPM_NAME="$withval"
])
AC_DEFINE_UNQUOTED(RPM_NAME, "$RPM_NAME", [official name of program])

dnl # allow packagers to override the bugreport address
AC_ARG_WITH(bugreport,
    AS_HELP_STRING([--with-bugreport=ADDRESS], [set packager bugreport Email/Web address (PACKAGE_BUGREPORT_DEFAULT)]), [dnl
    case "$withval" in
        *@*|http*://* ) PACKAGE_BUGREPORT="$withval" ;;
        *   ) AC_MSG_ERROR([invalid bugreport Email/Web address]) ;;
    esac
])

dnl # Library code modified:                              REVISION++
dnl # Interfaces changed/added/removed:   CURRENT++       REVISION=0
dnl # Interfaces added:                             AGE++
dnl # Interfaces removed:                           AGE=0
AC_SUBST(LT_CURRENT,  5)
AC_SUBST(LT_REVISION, 4)
AC_SUBST(LT_AGE,      0)

dnl # provide RPM exported version
RPM_VERSION="0,0,_,0,0,_"
case "$PACKAGE_VERSION" in
    *.*.DEVEL )
        RPM_VERSION=`echo "$PACKAGE_VERSION" |\
            sed -e 's;^\([[^.]]*\)\.\([[^.]]*\).*;\1,\2,_,0,0,_;'`
        ;;
    *.*.SNAPSHOT.* )
        RPM_VERSION=`echo "$PACKAGE_VERSION" |\
            sed -e 's;^\([[^.]]*\)\.\([[^.]]*\).*;\1,\2,_,0,0,s;'`
        ;;
    *.*a* )
        RPM_VERSION=`echo "$PACKAGE_VERSION" |\
            sed -e 's;^;X;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)a\([[^.]]*\)\.\([[^.]]*\)$;\1,\2,a,\3,\4,_;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)a\([[^.]]*\)$;\1,\2,a,\3,0,_;'`
        ;;
    *.*b* )
        RPM_VERSION=`echo "$PACKAGE_VERSION" |\
            sed -e 's;^;X;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)b\([[^.]]*\)\.\([[^.]]*\)$;\1,\2,b,\3,\4,_;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)b\([[^.]]*\)$;\1,\2,b,\3,0,_;'`
        ;;
    *.*rc* )
        RPM_VERSION=`echo "$PACKAGE_VERSION" |\
            sed -e 's;^;X;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)rc\([[^.]]*\)\.\([[^.]]*\)$;\1,\2,c,\3,\4,_;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)rc\([[^.]]*\)$;\1,\2,c,\3,0,_;'`
        ;;
    *.*.* )
        RPM_VERSION=`echo "$PACKAGE_VERSION" |\
            sed -e 's;^;X;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)\.\([[^.]]*\)\.\([[^.]]*\)$;\1,\2,c,\3,\4,_;' \
                -e 's;^X\([[^.]]*\)\.\([[^.]]*\)\.\([[^.]]*\)$;\1,\2,r,\3,0,_;'`
        ;;
esac
RPM_TIMESTAMP=`echo "$PACKAGE_TIMESTAMP" |\
    sed -e 's;^;X;' \
        -e 's;^X\([[0-9]]*\)-\([[0-9]]*\)-\([[0-9]]*\) \([[0-9]]*\):\([[0-9]]*\)$;\1,\2,\3,\4,\5;' \
        -e 's;^X\([[0-9]]*\)-\([[0-9]]*\)-\([[0-9]]*\) \([[0-9]]*\)$;\1,\2,\3,\4,0;' \
        -e 's;^X\([[0-9]]*\)-\([[0-9]]*\)-\([[0-9]]*\)$;\1,\2,\3,0,0;' \
        -e 's;^X\([[0-9]]*\)-\([[0-9]]*\)$;\1,\2,0,0,0;' \
        -e 's;^X\([[0-9]]*\)$;\1,0,0,0,0;' \
        -e 's;^X.*$;0,0,0,0,0;' \
        -e 's;,0\([[0-9]]\);,\1;g'`
RPM_VENDOR="'R','P','M','5'"
AC_SUBST([RPM_VERSION])
AC_SUBST([RPM_TIMESTAMP])
AC_SUBST([RPM_VENDOR])

dnl ##
dnl ## ==== TOOL-CHAIN CHECKS ====
dnl ##

AC_MSG_HEADER([TOOL-CHAIN CHECKS])

AC_PROG_CC_STDC
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX

AC_USE_SYSTEM_EXTENSIONS

AC_CPP_FUNC

AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_RANLIB
AC_PROG_YACC

AC_PATH_PROG(AS, as, as)

dnl # GCC specifics
AC_PROG_GCC_TRADITIONAL
AC_ARG_ENABLE(build-pic,
    AS_HELP_STRING([--enable-build-pic], [build RPM with Position Independent Code (PIC)]), [dnl
    if test ".$enableval" = .yes; then
        if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \) -o \
                \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
            dnl # GNU GCC (usually "gcc") or LLVM (usually "clang")
            CFLAGS="$CFLAGS -fPIC -DPIC"
        elif test ".`$CC -V 2>&1 | grep 'Sun C'`" != .; then
            dnl # Sun Studio (usually "cc")
            CFLAGS="$CFLAGS -KPIC -DPIC"
        elif test ".`$CC -help 2>&1 | grep 'Intel Corporation'`" != .; then
            dnl # Intel C/C++ Compiler (usually "icc")
            CFLAGS="$CFLAGS -fPIC -DPIC"
        else
            AC_MSG_WARN([--enable-build-pic requires GCC/clang, Sun Studio or Intel C/C++])
        fi
    fi
])
AC_ARG_ENABLE(build-pie,
    AS_HELP_STRING([--enable-build-pie], [build RPM as Position Independent Executable (PIE) (GCC/clang only)]), [dnl
    if test ".$enableval" = .yes; then
        if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \) -o \
                \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
            CFLAGS="$CFLAGS -fPIE"
	    LDFLAGS="$LDFLAGS -pie"
        else
            AC_MSG_WARN([--enable-build-pie requires GCC or clang])
        fi
    fi
])

RPM_CPPFLAGS=""
RPM_CXXFLAGS=""
RPM_CFLAGS=""
RPM_LDFLAGS=""

AC_ARG_ENABLE(build-warnings,
    AS_HELP_STRING([--enable-build-warnings], [build RPM with extra compiler warnings turned on (GCC only)]), [dnl
    if test ".$enableval" = .yes; then
        if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \) -o \
                \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
            dnl # GNU GCC (usually "gcc") or LLVM (usually "clang")
#           CFLAGS="$CFLAGS -Wall -W -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wno-char-subscripts -Wno-unused-parameter -Wformat-security"
# XXX gcc-4.2 on Mac OS X hasn't
#           CFLAGS="$CFLAGS -Wno-unused-but-set-variable"
# XXX rpmio/set.c needs this
#           CFLAGS="$CFLAGS -Wno-override-init"

            rpm_CFLAGS_ADD([-Wall], [RPM_CFLAGS])
            rpm_CFLAGS_ADD([-W], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fdiagnostics-show-option],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fcheck-diagnostics-color=auto], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wextra], [RPM_CFLAGS])

          if test \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
  rpm_CPPFLAGS_ADD([-Wno-unused-command-line-argument], [RPM_CPPFLAGS])
dnl    rpm_CFLAGS_ADD([-Wdocumentation], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wno-documentation-deprecated-sync], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wno-array-bounds], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wno-parentheses-equality], [RPM_CFLAGS])
  # XXX clang-3.7.0 noisy
  rpm_CFLAGS_ADD([-Wno-string-compare], [RPM_CFLAGS])
          fi

dnl #  rpm_CFLAGS_ADD([-pedantic], [RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-pedantic-errors], [RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wsystem-headers],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Weverything], [RPM_CFLAGS])

dnl # === implied by gcc -Wall
dnl #       rpm_CFLAGS_ADD([-Wno-char-subscripts], [RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wcomment], [RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wcomments], [RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wformat],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wnonull],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wformat-extra-args],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wformat-zero-length],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wimplicit],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wmissing-braces],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-parentheses],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wsequence-point],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wreturn-type],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wswitch],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wswitch-default],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wswitch-enum],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wtrigraphs],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wunused],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wuninitialized],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-unknown-pragmas],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wno-strict-aliasing],[RPM_CFLAGS])

  rpm_CFLAGS_ADD([-Wswitch-bool],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wlogical-not-parentheses],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wsizeof-array-argument],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wc90-c99-compat],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wc99-c11-compat],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-incompatible-pointer-types],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-int-conversion],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-discarded-qualifiers],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-discarded-array-qualifiers],[RPM_CFLAGS])

dnl # --- implied by gcc -Wunused
  rpm_CFLAGS_ADD([-Wno-unused-function],[RPM_CFLAGS])
            rpm_CFLAGS_ADD([-Wno-unused-parameter], [RPM_CFLAGS])
dnl # XXX not gcc-4.4.7
dnl #  rpm_CFLAGS_ADD([-Wunused-result],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wno-unused-value],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-unused-variable],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-clobbered],[RPM_CFLAGS])

dnl # --- implied by gcc -Wformat=2
dnl #  rpm_CFLAGS_ADD([-Wformat=2],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wformat-nonliteral],[RPM_CFLAGS])
            rpm_CFLAGS_ADD([-Wformat-security],[RPM_CFLAGS])

dnl # --- implied by gcc -Wimplicit
dnl #  rpm_CFLAGS_ADD([-Wimplicit-int],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wimplicit-function-declaration],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Werror-implicit-function-declaration],[RPM_CFLAGS])

dnl # --- implied by clang -Wthread-safety
dnl    rpm_CFLAGS_ADD([-Wthread-safety],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wthread-safety-attributes],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wthread-safety-analysis],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wthread-safety-precise],[RPM_CFLAGS])

dnl # --- other
dnl #  rpm_CFLAGS_ADD([-Wno-div-by-zero],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wfloat-equal],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wtraditional],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wimport],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wundef],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wunused-macros],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wendif-labels],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Werror],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-w],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wshadow],[RPM_CFLAGS])
dnl #MAX_OBJECT_SIZE=32768
dnl #  rpm_CFLAGS_ADD([-Wlarger-than-$MAX_OBJECT_SIZE],[RPM_CFLAGS])
            rpm_CFLAGS_ADD([-Wpointer-arith],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wbad-function-cast],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wcast-qual],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wcast-align],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wwrite-strings],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wsign-compare],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Waggregate-return],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wmissing-noreturn],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wmissing-format-attribute],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-multichar],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-deprecated-declarations],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wpacked],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wpadded],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wno-redundant-decls],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wnested-externs],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wunreachable-code],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wattributes],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Winline],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wlong-long],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wdisabled-optimization],[RPM_CFLAGS])

dnl # --- C/ObjC but not C++
dnl #  rpm_CFLAGS_ADD([-Wjump-misses-init],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wmissing-declarations],[RPM_CFLAGS])
dnl #       rpm_CFLAGS_ADD([-Wmissing-prototypes],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wold-style-definition],[RPM_CFLAGS])
dnl # XXX rpmio/set.c
dnl #     if test \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
dnl #         rpm_CFLAGS_ADD([-Wno-initializer-overrides], [RPM_CFLAGS])
dnl #     fi
dnl #     if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \); then
dnl #         rpm_CFLAGS_ADD([-Wno-override-init], [RPM_CFLAGS])
dnl #     fi
dnl #  rpm_CFLAGS_ADD([-Wpointer-to-int-cast],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wno-strict-prototypes],[RPM_CFLAGS])

dnl # --- useful
dnl #MAX_STACK_SIZE=32768
dnl #  rpm_CFLAGS_ADD([-Wframe-larger-than=$MAX_STACK_SIZE],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wlogical-op],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wno-missing-field-initializers],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wmissing-include-dirs],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Woverflow],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Woverlength-strings],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wstrict-overflow],[RPM_CFLAGS])
dnl #       rpm_CFLAGS_ADD([-Wstrict-prototypes],[RPM_CFLAGS])
          if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \); then
  rpm_CFLAGS_ADD([-Wmaybe-uninitialized],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wsuggest-attribute=const],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wsuggest-attribute=noreturn],[RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wsuggest-attribute=pure],[RPM_CFLAGS])
            rpm_CFLAGS_ADD([-Wno-unused-but-set-variable], [RPM_CFLAGS])
dnl # -fno-delete-null-pointer as the kernel does http://patchwork.kernel.org/patch/36060/
  rpm_CFLAGS_ADD([-fno-delete-null-pointer-checks], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-Wbool-compare],[RPM_CFLAGS])
          fi
dnl #  rpm_CFLAGS_ADD([-Wdouble-promotion],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wvariadic-macros],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wvla],[RPM_CFLAGS])

dnl # --- noisy and/or mostly useless
dnl #  rpm_CFLAGS_ADD([-Wbuiltin-macro-redefined],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wcoverage-mismatch],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wcpp],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wdeclaration-after-statement],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wformat-contains-nul],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wformat-y2k], [RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Winit-self],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Winvalid-pch],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wmudflap],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wnormalized=nfc],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wpacked-bitfield-compat],[RPM_CFLAGS])
dnl # XXX noisy/broken on gcc-4.4.7
  rpm_CFLAGS_ADD([-Wno-pragmas],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wsync-nand],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wtrampolines],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-Wvolatile-register-var],[RPM_CFLAGS])

dnl # --- implied by gcc -O
dnl #  rpm_CFLAGS_ADD([-fauto-inc-dec],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fcompare-elim],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fcprop-registers],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fdce],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fdefer-pop],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fdelayed-branch],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fdse],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fguess-branch-probability],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fif-conversion2],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fif-conversion],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-finline-functions-called-once],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fipa-pure-const],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fipa-profile],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fipa-reference],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fmerge-constants],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fsplit-wide-types],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-bit-ccp],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-builtin-call-dce],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-ccp],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-ch],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-copy-rename],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-dce],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-dominator-opts],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-dse],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-forwprop],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-fre],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-phiprop],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-slsr],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-sra],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-pta],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-ter],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-funit-at-a-time],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fomit-frame-pointer],[RPM_CFLAGS])

dnl # --- implied by gcc -O2
dnl #  rpm_CFLAGS_ADD([-fthread-jumps],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-falign-functions],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-falign-jumps],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-falign-labels],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-falign-loops],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fcaller-saves],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fcrossjumping],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fcse-follow-jumps],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fcse-skip-blocks],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fdevirtualize],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fdevirtualize-speculatively],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fexpensive-optimizations],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fgcse],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fgcse-lm],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fhoist-adjacent-loads],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-finline-small-functions],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-findirect-inlining],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fipa-sra],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fisolate-erroneous-paths-dereference],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-foptimize-sibling-calls],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fpartial-inlining],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fpeephole2],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-freorder-blocks],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-freorder-functions],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-frerun-cse-after-loop],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fsched-interblock],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fsched-spec],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fschedule-insns],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fschedule-insns2],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fstrict-aliasing],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fstrict-overflow],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-switch-conversion],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-tail-merge],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-pre],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-vrp],[RPM_CFLAGS])

dnl # --- implied by gcc -O3
dnl #  rpm_CFLAGS_ADD([-finline-functions],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-funswitch-loops],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fpredictive-commoning],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fgcse-after-reload],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-loop-vectorize],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-slp-vectorize],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-f-vect-cost-model],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-ftree-partial-pre],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fips-cp-clone],[RPM_CFLAGS])

dnl # --- implied by gcc -Os
dnl #  rpm_CFLAGS_ADD([-fno-reorder-blocks-and-partition],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fno-prefetch-loop-arrays],[RPM_CFLAGS])

dnl # --- implied by gcc -Ofast
  rpm_CFLAGS_ADD([-ffast-math],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fno-protect-parens],[RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fstack-arrays],[RPM_CFLAGS])

dnl # build RPM instrumented for extra optimization/security (GCC only)
dnl # --- other optimizations
  rpm_CFLAGS_ADD([-fexceptions], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-D_FORTIFY_SOURCE=2 -fstack-protector], [RPM_CFLAGS])
dnl #  rpm_CFLAGS_ADD([-fstack-protector-all],[RPM_CFLAGS])

     if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \); then
         rpm_CFLAGS_ADD([-fgnu-tm], [RPM_CFLAGS])
     fi

dnl # --- options below are added to RPM_CFLAGS but _NOT_ added to CFLAGS
            CPPFLAGS="$CPPFLAGS $RPM_CPPFLAGS"
            CXXFLAGS="$CXXFLAGS $RPM_CXXFLAGS"
            CFLAGS="$CFLAGS $RPM_CFLAGS"
            LDFLAGS="$LDFLAGS $RPM_LDFLAGS"
dnl # --- configure --enable-build-pic
  rpm_CFLAGS_ADD([-fPIC -DPIC], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-KPIC -DPIC], [RPM_CFLAGS])
dnl # --- configure --enable-build-pie
  rpm_CFLAGS_ADD([-fPIE], [RPM_CFLAGS])
  rpm_LDFLAGS_ADD([-pie], [RPM_LDFLAGS])
dnl # --- configure --enable-build-gcov
  rpm_CFLAGS_ADD([-fprofile-arcs -ftest-coverage], [RPM_CFLAGS])
dnl # --- configure --enable-build-sanitize-address
  rpm_CFLAGS_ADD([-fsanitize=address], [RPM_CFLAGS])
dnl # --- configure --enable-build-sanitize-memory
  rpm_CFLAGS_ADD([-fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer], [RPM_CFLAGS])
dnl # --- configure --enable-build-sanitize-thread
  rpm_CFLAGS_ADD([-fsanitize=thread -fPIE -pie], [RPM_CFLAGS])

  rpm_CFLAGS_ADD([-fsanitize=integer], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fsanitize=undefined], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fsanitize=undefined-trap], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fsanitize=float-divide-by-zero], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fsanitize=float-cast-overflow], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fsanitize=bounds], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fsanitize=alignment], [RPM_CFLAGS])
  rpm_CFLAGS_ADD([-fsanitize=object-size], [RPM_CFLAGS])

  rpm_CFLAGS_ADD([-fcheck-pointer-bounds], [RPM_CFLAGS])

dnl # --- -fsilkplus -lcilkrts -I.../cilkplus-4.8-install/include
  rpm_CFLAGS_ADD([-fcilkplus], [RPM_CFLAGS])

        elif test ".`$CC -V 2>&1 | grep 'Sun C'`" != .; then
            dnl # Sun Studio (usually "cc")
            CFLAGS="$CFLAGS -v"
        elif test ".`$CC -help 2>&1 | grep 'Intel Corporation'`" != .; then
            dnl # Intel C/C++ Compiler (usually "icc")
            CFLAGS="$CFLAGS -Wall -Wcheck -w1 -wr188 -wr981 -wr593 -wr869 -wr810"
        else
            AC_MSG_WARN([--enable-build-warnings requires GCC, Sun Studio or Intel C/C++])
        fi
    fi
])
AC_ARG_ENABLE(build-debug,
    AS_HELP_STRING([--enable-build-debug], [build RPM instrumented for debugging]), [dnl
    if test ".$enableval" = .yes; then
        if test ".`$CC --version 2>&1 | grep 'GCC'`" != .; then
            dnl # GNU GCC (usually "gcc")
            CFLAGS="$CFLAGS -g3"
        elif test ".`$CC -V 2>&1 | grep 'Sun C'`" != .; then
            dnl # Sun Studio (usually "cc")
            CFLAGS="$CFLAGS -xO0 -g"
        elif test ".`$CC -help 2>&1 | grep 'Intel Corporation'`" != .; then
            dnl # Intel C/C++ Compiler (usually "icc")
            CFLAGS="$CFLAGS -O0 -g"
        else
            CFLAGS="$CFLAGS -g"
        fi
    fi
])

dnl # Add -fprofile-arcs -ftest-coverage (gcc/clang)
AC_ARG_ENABLE(build-gcov,
    AS_HELP_STRING([--enable-build-gcov], [build RPM instrumented for gcov]), [dnl
    if test ".$enableval" = .yes; then
        if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \) -o \
                \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
            dnl # GNU GCC (usually "gcc")
#            CFLAGS="$CFLAGS -fprofile-arcs -ftest-coverage"
#            LDFLAGS="$LDFLAGS -lgcov"
            CFLAGS="$CFLAGS --coverage"
            CXXFLAGS="$CXXFLAGS --coverage"
            LDFLAGS="$LDFLAGS --coverage"
        fi
    fi
])

dnl # FIXME: all of -fsanitize={address,memory,thread} are mutually exclusive
dnl # Add -fsanitize=address (clang only, RHEL6 gcc-4.4.7 has not)
AC_ARG_ENABLE(build-sanitize-address,
    AS_HELP_STRING([--enable-build-sanitize-address], [build RPM instrumented for with AddressSanitizer]), [dnl
    if test ".$enableval" = .yes; then
        if test \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
            rpm_CFLAGS_ADD([-fsanitize=address], [CFLAGS])
        fi
    fi
])

dnl # Add -fsanitize=memory (clang)
AC_ARG_ENABLE(build-sanitize-memory,
    AS_HELP_STRING([--enable-build-sanitize-memory], [build RPM instrumented for with MemorySanitizer]), [dnl
    if test ".$enableval" = .yes; then
        if test \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
            rpm_CFLAGS_ADD([-fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer], [CFLAGS])
        fi
    fi
])

dnl # Add -fsanitize=thread (clang)
AC_ARG_ENABLE(build-sanitize-thread,
    AS_HELP_STRING([--enable-build-sanitize-thread], [build RPM instrumented for with ThreadSanitizer]), [dnl
    if test ".$enableval" = .yes; then
        if test \( ".`$CC --version 2>&1 | grep 'GCC'`" != . \); then
            rpm_CFLAGS_ADD([-fsanitize=thread -fPIE -pie], [CFLAGS])
        fi
        if test \( ".`$CC --version 2>&1 | grep 'clang'`" != . \); then
            rpm_CFLAGS_ADD([-fsanitize=thread], [CFLAGS])
        fi
    fi
])

dnl # (NOTYET) -fsanitize=foo (clang)
#           rpm_CFLAGS_ADD([-fsanitize=integer], [CFLAGS])
#           rpm_CFLAGS_ADD([-fsanitize=undefined], [CFLAGS])
#           rpm_CFLAGS_ADD([-fsanitize=undefined-trap], [CFLAGS])

dnl # optional valgrind/cachegrind support
AC_ARG_WITH(valgrind, AS_HELP_STRING([--with-valgrind], [build RPM insturmented for valgrind tools]), [
    if test ".$withval" = .yes; then
	AC_CHECK_HEADERS(valgrind/valgrind.h)
        AC_DEFINE(WITH_VALGRIND, 1, [Build with valgrind support?])
    fi
])

dnl # Checks for OpenMP support
AC_LANG_PUSH(C)
AC_OPENMP
AC_LANG_POP(C)
AC_LANG_PUSH(C++)
AC_OPENMP
AC_LANG_POP(C++)

dnl # RPM tries hard to use adaptive "feature tests" instead of
dnl # problematic "platform tests". Nevertheless for some extremely
dnl # platform specific parts of the code, we have to know for what
dnl # particular target Unix platform we are building.
RPM_OS_NAME=`dnl
    [echo "$host_os" | \
    sed -e 's;^;X;' \
        -e 's;^X[^a-zA-Z]*\([a-zA-Z][a-zA-Z/-]*\).*$;\1;' \
        -e 's;^X.*;UNKNOWN;' | \
    sed -e 's;[/-];;' | \
    tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ']`
case "$RPM_OS_NAME" in
    LINUX* ) RPM_OS_NAME="LINUX" ;;
esac
v1=0; v2=0; v3=0
my_os="$host_os"
case "$my_os" in
    *[[0-9]]* ) ;;
    * ) my_os="${my_os}`uname -r`" ;;
esac
eval `dnl
    [echo "$my_os" | \
    sed -e 's/^[^0-9]*\([0-9][0-9.]*\).*$/\1/' | \
    sed -e 's/^/X/' \
        -e 's/^X\([0-9][0-9]*\)\.\([0-9][0-9]*\)\.\([0-9][0-9]*\).*$/v1="\1"; v2="\2"; v3="\3"/' \
        -e 's/^X\([0-9][0-9]*\)\.\([0-9][0-9]*\).*$/v1="\1"; v2="\2"/' \
        -e 's/^X\([0-9][0-9]*\).*$/v1="\1"/' \
        -e 's/^X.*$/:/']`
RPM_OS_VERS=`echo . |\
    awk '{ printf("%02d%02d%02d", v1, v2, v3); }' \
    v1="$v1" v2="$v2" v3="$v3"`
CPPFLAGS="$CPPFLAGS -DRPM_OS_${RPM_OS_NAME}=${RPM_OS_VERS}"

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

AC_SUBST(RPM_CFLAGS)
AC_SUBST(RPM_LDFLAGS)

dnl check for version script support in the linker (GNU ld, or Solaris ld style)
AC_ARG_ENABLE([build-versionscript],
              AS_HELP_STRING([--enable-build-versionscript],[enable/disable use of linker version script.
                              (default is system dependent)]),
              [have_ld_version_script=$enableval],
              [have_ld_version_script=yes] )

AS_IF([test "x$have_ld_version_script" = xyes],
  [ 
    AC_MSG_CHECKING([if ld version-script works - GNU ld, Solaris ld])
    cat > conftest.map <<EOF
RELEASE
{
	global:
		rpm_*;
	local:
		*;
};
PRIVATE
{
	global:
		rpmi_*;
	local:
		*;
};
EOF
    save_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS -Wl,--version-script=conftest.map"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                   void rpm_symbol1(void) {}
                   void rpmi_symbol2(void) {}
                   ]], [])],
                   [have_ld_version_script=yes], [have_ld_version_script=no])
    LDFLAGS="$save_LDFLAGS"
    if test x$have_ld_version_script != xyes
    then
    	save_LDFLAGS="$LDFLAGS"
    	LDFLAGS="$LDFLAGS -Wl,-M,conftest.map"
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                   void rpm_symbol1(void) {}
                   void rpmi_symbol2(void) {}
                   ]], [])],
                   [have_ld_version_script=yes], [have_ld_version_script=no])
    	LDFLAGS="$save_LDFLAGS"
    	if test x$have_ld_version_script = xyes
    	then
		LD_VERSION_SCRIPT_FLAG=-M
    	fi
    else
      LD_VERSION_SCRIPT_FLAG=--version-script
    fi
    AC_MSG_RESULT($have_ld_version_script)
    rm -f conftest.map conftest.c
])
AC_SUBST([LD_VERSION_SCRIPT_FLAG])
AM_CONDITIONAL([HAVE_LD_VERSION_SCRIPT],[test "$have_ld_version_script" = "yes"])

dnl # support implicit internal library dependencies
AC_ARG_ENABLE(
    [build-intlibdep],
    AS_HELP_STRING([--disable-build-intlibdep],[disable use of implicit internal library dependencies]),
    [ do_build_intlibdep=$enableval ],
    [ do_build_intlibdep=yes ])
AM_CONDITIONAL(
    [ENABLE_BUILD_INTLIBDEP],
    [test ".$do_build_intlibdep" = .yes])

dnl # support implicit external library dependencies
AC_ARG_ENABLE(
    [build-extlibdep],
    AS_HELP_STRING([--disable-build-extlibdep],[disable use of implicit external library dependencies]),
    [ do_build_extlibdep=$enableval ],
    [ do_build_extlibdep=yes ])
AM_CONDITIONAL(
    [ENABLE_BUILD_EXTLIBDEP],
    [test ".$do_build_extlibdep" = .yes])

dnl # support implicit maximum external library dependencies
AC_ARG_ENABLE(
    [build-maxextlibdep],
    AS_HELP_STRING([--disable-build-maxextlibdep],[disable use of implicit maximum external library dependencies]),
    [ do_build_maxextlibdep=$enableval ],
    [ do_build_maxextlibdep=yes ])
AM_CONDITIONAL(
    [ENABLE_BUILD_MAXEXTLIBDEP],
    [test ".$do_build_maxextlibdep" = .yes])

dnl # support removal of GNU libtool *.la files
AC_ARG_ENABLE(
    [build-lafiles],
    AS_HELP_STRING([--disable-build-lafiles],[disable the removal of GNU libtool *.la files on installation]),
    [ do_build_lafiles=$enableval ],
    [ do_build_lafiles=yes ])
AM_CONDITIONAL(
    [ENABLE_BUILD_LAFILES],
    [test ".$do_build_lafiles" = .yes])

dnl # optional symlink and parentdir dependency
AC_ARG_ENABLE(
    [dirname-and-symlink-deps],
    AS_HELP_STRING([--disable-dirname-and-symlink-deps],[permit to disable via macro the automatic dirname and symlink dependecy]),
    [ do_optional_dirname_and_symlink_deps=$enableval ],
    [ do_optional_dirname_and_symlink_deps=yes ])

AS_IF([test "x$do_optional_dirname_and_symlink_deps" != xyes],
  [ 
  AC_DEFINE([RPM_OPTIONAL_DIRNAME_AND_SYMLINK_DEPS],[1],[permit to disable via macro the automatic dirname and symlink dependecy])
  ])


dnl #  Adds RPM Lua extensions based on rpmlib functionality
AC_ARG_ENABLE(
    [rpm-lua-extensions-based-on-rpmlib],
    AS_HELP_STRING([--enable-rpm-lua-extensions-based-on-rpmlib],[Adds RPM Lua extensions based on rpmlib functionality (default:no)]),
    [ do_rpm_lua_extensions_based_on_rpmlib=$enableval ],
    [ do_rpm_lua_extensions_based_on_rpmlib=no ])

AS_IF([test "x$do_rpm_lua_extensions_based_on_rpmlib" != xno],
  [ 
  AC_DEFINE([RPM_LUA_EXTENSIONS_BASED_ON_RPMLIB],[1],[Adds RPM Lua extensions based on rpmlib functionality])
  ])


dnl # Permit to revert to the old rpmvercmp behavior where digits beat alpha
dnl # called in VENDOR old-comparision-behaviour
AC_ARG_ENABLE(
    [rpmvercmp-digits-beat-alpha],
    AS_HELP_STRING([--enable-rpmvercmp-digits-beat-alpha],[Permit in RPM version comparison that digits beat alpha (default:no)]),
    [ do_rpmvercmp_digits_beat_alpha=$enableval ],
    [ do_rpmvercmp_digits_beat_alpha=no ])

AS_IF([test "x$do_rpmvercmp_digits_beat_alpha" != xno],
  [ 
  AC_DEFINE([RPMVERCMP_DIGITS_BEAT_ALPHA],[1],[Permit in RPM version comparison that digits beat alpha])
  ])

dnl # FIXME: partly Linux-specific only!?
dnl # (but at least it usually doesn't harm on other platforms)
CFLAGS="$CFLAGS -D_GNU_SOURCE -D_REENTRANT"

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

dnl # see if we have a mkdir(1) that supports option "-p".
AC_PATH_PROGS(MKDIR, mkdir, mkdir)
AC_MSG_CHECKING([if $MKDIR supports -p])
rm -rf conftest
__MKDIR_P=""
$MKDIR -p conftest/a 2>/dev/null
if test $? = 0; then
    rmdir conftest/a 2>/dev/null
    if test $? = 0; then
        rmdir conftest 2>/dev/null
        if test $? = 0; then
            __MKDIR_P="$MKDIR -p"
        fi
    fi
fi
if test ".$__MKDIR_P" = .; then
    AC_MSG_RESULT(no)
    __MKDIR_P="%{_rpmhome}/mkinstalldirs"
else
    AC_MSG_RESULT(yes)
fi
AC_DEFINE_UNQUOTED(__MKDIR_P, "${__MKDIR_P}", [full path to a 'mkdir -p' style program])
AC_SUBST(__MKDIR_P)

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
    if echo "$build" | egrep "(aix|hpux|linux)" >/dev/null; then
	result=yes
    elif echo "$build" | grep "nextstep" >/dev/null; then
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

dnl # find the Perl interpreter
AC_PATH_PROG(PERL, perl perl5 miniperl)
if test ".$PERL" = .; then
    AC_MSG_ERROR([Perl required in PATH for building rpmconstant/])
fi
AC_SUBST(PERL)

dnl # find some common programs
if test "$cross_compiling" = "yes"; then
    MYPATH=":"
else
    MYPATH=$PATH
fi

DBXY=db61

AC_PATH_PROG(__BASH, bash, %{_bindir}/bash, $MYPATH)
AC_PATH_PROG(__BZIP2, bzip2, %{_bindir}/bzip2, $MYPATH)
AC_PATH_PROG(__CAT, cat, /bin/cat, $MYPATH)
AC_PATH_PROG(__CHATTR, chattr, %{_bindir}/chattr, $MYPATH)
AC_PATH_PROG(__CHGRP, chgrp, /bin/chgrp, $MYPATH)
AC_PATH_PROG(__CHMOD, chmod, /bin/chmod, $MYPATH)
AC_PATH_PROG(__CHOWN, chown, /bin/chown, $MYPATH)
AC_PATH_PROG(__CMAKE, cmake, %{_bindir}/cmake, $MYPATH)
AC_PATH_PROG(__CPIO, cpio, /bin/cpio, $MYPATH)
AC_PATH_PROG(__CURL, curl, %{_bindir}/curl, $MYPATH)
AC_PATH_PROG(__CVS, cvs, %{_bindir}/cvs, $MYPATH)
AC_PATH_PROG(__DB_ARCHIVE, ${DBXY}_archive, %{_bindir}/${DBXY}_archive, $MYPATH)
AC_PATH_PROG(__DB_CHECKPOINT, ${DBXY}_checkpoint, %{_bindir}/${DBXY}_checkpoint, $MYPATH)
AC_PATH_PROG(__DB_DEADLOCK, ${DBXY}_deadlock, %{_bindir}/${DBXY}_deadlock, $MYPATH)
AC_PATH_PROG(__DB_DUMP, ${DBXY}_dump, %{_bindir}/${DBXY}_dump, $MYPATH)
AC_PATH_PROG(__DB_HOTBACKUP, ${DBXY}_hotbackup, %{_bindir}/${DBXY}_hotbackup, $MYPATH)
AC_PATH_PROG(__DB_LOAD, ${DBXY}_load, %{_bindir}/${DBXY}_load, $MYPATH)
AC_PATH_PROG(__DB_LOG_VERIFY, ${DBXY}_log_verify, %{_bindir}/${DBXY}_log_verify, $MYPATH)
AC_PATH_PROG(__DB_PRINTLOG, ${DBXY}_printlog, %{_bindir}/${DBXY}_printlog, $MYPATH)
AC_PATH_PROG(__DB_RECOVER, ${DBXY}_recover, %{_bindir}/${DBXY}_recover, $MYPATH)
AC_PATH_PROG(__DB_REPLICATE, ${DBXY}_replicate, %{_bindir}/${DBXY}_replicate, $MYPATH)
AC_PATH_PROG(__DBSQL, ${DBXY}sql, %{_bindir}/${DBXY}sql, $MYPATH)
AC_PATH_PROG(__DB_SQL_CODEGEN, ${DBXY}_sql_codegen, %{_bindir}/${DBXY}_sql_codegen, $MYPATH)
AC_PATH_PROG(__DB_STAT, ${DBXY}_stat, %{_bindir}/${DBXY}_stat, $MYPATH)
AC_PATH_PROG(__DB_TUNER, ${DBXY}_tuner, %{_bindir}/${DBXY}_tuner, $MYPATH)
AC_PATH_PROG(__DB_UPGRADE, ${DBXY}_upgrade, %{_bindir}/${DBXY}_upgrade, $MYPATH)
AC_PATH_PROG(__DB_VERIFY, ${DBXY}_verify, %{_bindir}/${DBXY}_verify, $MYPATH)
AC_PATH_PROG(__DIFF, diff, /bin/diff, $MYPATH)
AC_PATH_PROG(__DITTO, ditto, %{_bindir}/ditto, $MYPATH)
AC_PATH_PROG(__FILE, file, %{_bindir}/file, $MYPATH)
AC_PATH_PROG(__GIT, git, %{_bindir}/git, $MYPATH)
AC_PATH_PROG(__GPG, gpg2, %{_bindir}/gpg2, $MYPATH)
AC_PATH_PROG(__GSR, gsr, %{_bindir}/gsr, $MYPATH)
AC_PATH_PROG(__GST_INSPECT, gst-inspect-0.10, %{_bindir}/gst-inspect-0.10, $MYPATH)
AC_PATH_PROG(__GZIP, gzip, /bin/gzip, $MYPATH)
AC_PATH_PROG(__HG, hg, %{_bindir}/hg, $MYPATH)
AC_PATH_PROG(__ID, id, /usr/bin/id, $MYPATH)
AC_PATH_PROG(__INSTALL, install, /usr/bin/install, $MYPATH)
AC_PATH_PROG(__LZIP, lzip, /usr/bin/lzip, $MYPATH)
AC_PATH_PROG(__LRZIP, lrzip, /usr/bin/lrzip, $MYPATH)
AC_PATH_PROG(__INSTALL_INFO, install-info, /sbin/install-info, $MYPATH)
AC_PATH_PROG(__LDCONFIG, ldconfig, /sbin/ldconfig, $MYPATH)
AC_PATH_PROG(__LN, ln, /bin/ln, $MYPATH)
AC_PATH_PROG(__LSATTR, lsattr, %{_bindir}/lsattr, $MYPATH)
AC_PATH_PROG(__LZMA, lzma, %{_bindir}/lzma, $MYPATH)
AC_PATH_PROG(__LZOP, lzop, %{_bindir}/lzop, $MYPATH)
AC_PATH_PROGS(__7ZIP, [7zip 7za 7z], /usr/bin/7za, $MYPATH)
AC_PATH_PROG(__MAKE, make, %{_bindir}/make, $MYPATH)
AC_PATH_PROG(__MKDIR, mkdir, /bin/mkdir, $MYPATH)
AC_PATH_PROG(__MRUBY, mruby, %{_bindir}/mruby, $MYPATH)
AC_PATH_PROG(__MONGO, mongo, %{_bindir}/mongo, $MYPATH)
AC_PATH_PROG(__MODINFO, modinfo, /sbin/modinfo, $MYPATH)
AC_PATH_PROG(__MV, mv, /bin/mv, $MYPATH)
AC_PATH_PROG(__PATCH, patch, %{_bindir}/patch, $MYPATH)
AC_PATH_PROG(__PAX, pax, %{_bindir}/pax, $MYPATH)
AC_PATH_PROG(__PERL, perl, %{_bindir}/perl, $MYPATH)
AC_PATH_PROG(__PGP, pgp, %{_bindir}/pgp, $MYPATH)
AC_PATH_PROG(__PHP, php, %{_bindir}/php, $MYPATH)
AC_PATH_PROG(__PYTHON, python, %{_bindir}/python, $MYPATH)
AC_PATH_PROG(__RM, rm, /bin/rm, $MYPATH)
AC_PATH_PROG(__RMDIR, rmdir, /bin/rmdir, $MYPATH)
AC_PATH_PROG(__RSH, rsh, %{_bindir}/rsh, $MYPATH)
AC_PATH_PROG(__RUBY, ruby, %{_bindir}/ruby, $MYPATH)
AC_PATH_PROG(__SED, sed, /bin/sed, $MYPATH)
AC_PATH_PROG(__SH, sh, /bin/sh, $MYPATH)
AC_PATH_PROG(__SQLITE3, sqlite3, %{_bindir}/sqlite3, $MYPATH)
AC_PATH_PROG(__SSH, ssh, %{_bindir}/ssh, $MYPATH)
AC_PATH_PROG(__SVN, svn, %{_bindir}/svn, $MYPATH)
AC_PATH_PROG(__TAR, tar, /bin/tar, $MYPATH)
AC_PATH_PROG(__TCLSH, tclsh, %{_bindir}/tclsh, $MYPATH)
AC_PATH_PROG(__UNZIP, unzip, %{_bindir}/unzip, $MYPATH)
AC_PATH_PROG(__XZ, xz, %{_bindir}/xz, $MYPATH)
AC_PATH_PROG(__LD, ld, %{_bindir}/ld, $MYPATH)
AC_PATH_PROG(__NM, nm, %{_bindir}/nm, $MYPATH)
AC_PATH_PROG(__OBJCOPY, objcopy, %{_bindir}/objcopy, $MYPATH)
AC_PATH_PROG(__OBJDUMP, objdump, %{_bindir}/objdump, $MYPATH)
AC_PATH_PROG(__STRIP, strip, %{_bindir}/strip, $MYPATH)


AC_CHECK_PROGS(LIBTOOLIZE, libtoolize glibtoolize, libtoolize)
AC_SUBST(LIBTOOLIZE)

dnl # check for autotest
dnl AC_CONFIG_TESTDIR([tests/auto])
dnl AC_PATH_PROG(AUTOM4TE,autom4te,:)
dnl AC_PATH_PROG(__FAKECHROOT, fakechroot, no, $MYPATH)
dnl AM_CONDITIONAL(HAVE_FAKECHROOT, [test "$__FAKECHROOT" != "no"])

dnl # check for id -u
AC_MSG_CHECKING(whether id supports -u)
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

dnl # auto-detect abi-compliance-checker/api-sanity-checker testing
AC_PATH_PROG(__ABI_COMPLIANCE_CHECKER, abi-compliance-checker, no, $PATH)
withval=auto
AC_ARG_WITH(abi-compliance-checker, AS_HELP_STRING([--with-abi-compliance-checker], [run abi-compliance-checker with "make check"]))
if test $withval = auto -a $__ABI_COMPLIANCE_CHECKER != no; then
  withval=yes
elif test $withval = yes -a $__ABI_COMPLIANCE_CHECKER = no; then
  AC_MSG_ERROR([abi-compliance-checker is not in PATH])
fi
if test $withval = yes; then
  ABI_COMPLIANCE_CHECKER="$__ABI_COMPLIANCE_CHECKER"
else
  ABI_COMPLIANCE_CHECKER=true
fi
AC_SUBST(ABI_COMPLIANCE_CHECKER)

AC_PATH_PROG(__API_SANITY_CHECKER, api-sanity-checker, no, $PATH)
AC_ARG_WITH(api-sanity-checker, AS_HELP_STRING([--with-api-sanity-checker], [run api-sanity-checker with "make check"]))
if test $withval = auto -a $__API_SANITY_CHECKER != no; then
  withval=yes
elif test $withval = yes -a $__API_SANITY_CHECKER = no; then
  AC_MSG_ERROR([api-sanity-checker is not in PATH])
fi
if test $withval = yes; then
  API_SANITY_CHECKER="$__API_SANITY_CHECKER"
else
  API_SANITY_CHECKER=true
fi
AC_SUBST(API_SANITY_CHECKER)

AC_PATH_PROG(__CPPCHECK, cppcheck, /bin/true, $PATH)
AC_PATH_PROG(__LCOV, lcov, /bin/true, $PATH)
AC_PATH_PROG(__PMCCABE, pmccabe, /bin/true, $PATH)
AC_PATH_PROG(__RSYNC, rsync, /bin/true, $PATH)
AC_PATH_PROG(__SPLINT, splint, /bin/true, $PATH)

dnl ##
dnl ## ==== API CHECKS ====
dnl ##

AC_MSG_HEADER([API CHECKS])

dnl # platform specific checks
AC_SEARCH_LIBS([strerror],[cposix])

dnl # standard checks
AC_C_INLINE
AC_C_RESTRICT
AC_C_BIGENDIAN

dnl # platform-hack: don't trust darwin's configure-time check for endianness
dnl # configure result is used on platforms not using GCC compile-time macros
AH_VERBATIM([WORDS_BIGENDIAN],
[/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
#if defined(__BIG_ENDIAN__)
#define WORDS_BIGENDIAN 1
#elif defined(__LITTLE_ENDIAN__)
/* #undef WORDS_BIGENDIAN */
#else
/* use configure result: */
#undef WORDS_BIGENDIAN
#endif])

dnl # standard headers (specific)
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_DIRENT
AC_HEADER_TIME

dnl # standard headers (generic)
AC_CHECK_HEADERS([dnl
    arpa/inet.h err.h fcntl.h float.h getopt.h glob.h grp.h dnl
    inttypes.h langinfo.h libgen.h libintl.h limits.h dnl
    mach/mach.h malloc.h machine/types.h mcheck.h memory.h dnl
    mntent.h mnttab.h netdb.h netinet/in.h netinet/in_systm.h dnl
    paths.h poll.h pwd.h signal.h stdbool.h shadow.h dnl
    stddef.h stdint.h stdlib.h string.h strings.h dnl
    sys/acl.h sys/byteorder.h sys/endian.h sys/file.h dnl
    sys/ioctl.h sys/ipc.h sys/mman.h sys/mntctl.h sys/mnttab.h dnl
    sys/mount.h sys/param.h sys/prctl.h sys/resource.h dnl
    sys/select.h sys/socket.h sys/statfs.h sys/statvfs.h dnl
    sys/stdtypes.h sys/systemcfg.h sys/time.h sys/timeb.h dnl
    sys/types.h sys/utsname.h sys/vfs.h sys/vmount.h sys/wait.h dnl
    syslog.h termio.h termios.h unistd.h utime.h utmp.h utmpx.h dnl
    wchar.h dnl
])
AC_CHECK_HEADER_STDBOOL

dnl # GNU gettext support
AC_CHECK_HEADERS(locale.h)
AM_GNU_GETTEXT_VERSION(0.19.2)
AM_GNU_GETTEXT([external])
AM_ICONV
AM_CONDITIONAL(USE_NLS, [ test ".$USE_NLS" = .yes ])

dnl # specific features
AC_SYS_LARGEFILE

dnl Does this platform require array notation to assign to a va_list?
dnl If cross-compiling, we assume va_list is "normal".  If this breaks
dnl you, set ac_cv_valistisarray=true and maybe define HAVE_VA_LIST_AS_ARRAY
dnl also just to be sure.
AC_MSG_CHECKING(whether va_list assignments need array notation)
AC_CACHE_VAL(ac_cv_valistisarray,
	[AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>
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

AC_CHECK_VA_COPY
AC_CHECK_STATFS

dnl # specific functions (specific)
AC_CHECK_FUNC(setreuid,      [], [ AC_CHECK_LIB(ucb, setreuid, [if echo $LIBS | grep -- -lucb >/dev/null; then :; else LIBS="$LIBS -lc -lucb" USEUCB=y; fi]) ])
AC_CHECK_FUNC(rand,          [], [ AC_CHECK_LIB(rand, rand) ])
AC_CHECK_FUNC(getdomainname, [], [ AC_CHECK_LIB(nsl, getdomainname) ])
AC_CHECK_FUNC(socket,        [], [ AC_CHECK_LIB(socket, socket) ])

dnl # specific functions (for linking against *external* Berkeley-DB under Solaris)
AC_CHECK_FUNC(fdatasync,     [], [ AC_CHECK_LIB(rt, fdatasync) ])
AC_CHECK_FUNC(sched_yield,   [], [ AC_CHECK_LIB(rt, sched_yield) ])
AC_CHECK_FUNC(clock_gettime, [], [ AC_CHECK_LIB(rt, clock_gettime) ])

dnl # specific functions (generic)
AC_CHECK_FUNCS(vsnprintf snprintf)
AC_CHECK_FUNCS(strcoll strverscmp)

dnl # Error API
AC_CHECK_HEADERS(error.h)
AC_CHECK_FUNCS(error)

dnl # POSIX search(3) API
AC_CHECK_HEADERS(search.h)
AC_CHECK_FUNC(insque, [], [ AC_CHECK_LIB(compat, insque) ])

dnl # POSIX fmemopen(3) et al API.
AC_CHECK_FUNCS(fmemopen open_memstream open_wmemstream)
AC_CHECK_HEADERS(libio.h)
AC_CHECK_FUNCS(fopencookie)
AC_CHECK_FUNCS(funopen)

dnl # POSIX pthreads API
WITH_PTHREADS=no
AC_ARG_WITH([pthreads],
    AS_HELP_STRING([--without-pthreads], [build RPM without POSIX pthreads]),
    [ WITH_PTHREADS="$withval" ], [ WITH_PTHREADS=yes ])
if test ".$WITH_PTHREADS" = .yes; then
    AC_DEFINE(WITH_PTHREADS, 1, [Define to 1 for using Pthreads])
    AC_CHECK_HEADERS(thread.h pthread.h synch.h semaphore.h)
    AC_CHECK_LIB(pthread, pthread_mutex_trylock, [], [
        AC_CHECK_LIB(pthread, __pthread_mutex_trylock, [], [
            AC_CHECK_LIB(thread, mutex_lock)
        ])
    ])
fi

dnl # <cpuid.h> API
AC_CHECK_HEADERS(cpuid.h)

dnl # <sys/getauxv.h> API
AC_CHECK_HEADERS(sys/getauxv.h)
AC_CHECK_FUNC(getauxv)

dnl # <beecrypt/api.h> tries to retrofit an assert(x) macro
AC_CHECK_HEADERS(assert.h)

dnl # FIXME: platform-hack: MiNT
AC_CHECK_LIB(port, writev)

dnl # FIXME: platform-hack: AmigaOS and IXEmul have a fork() dummy
case "$target" in
    m68k-*-amigaos ) CFLAGS="$CFLAGS -Dfork=vfork" ;;
esac

dnl # platform-hack: The Mac OS X (aka Darwin) ld(1) uses a strange
dnl # non-standard Unix library search path order. This causes great
dnl # problems when linking against the third-party libraries.
dnl # Force ld(1) to stick standard Unix search path order.
case "$host" in
    *-*-darwin* ) LDFLAGS="$LDFLAGS -Wl,-search_paths_first" ;;
esac

dnl # FIXME: platform-hack: SCO and SunOS 4.x
if echo "$target_os" | grep sco > /dev/null; then
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(HAVE_STRUCT_MNTTAB, 1, [Define as 1 if you have "struct mnttab" (only sco?)])
elif echo "$target_os" | grep sunos > /dev/null; then
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

dnl Handle uglix dialectical differences with S_ISTXT and S_ISVTX
AC_MSG_CHECKING(if <sys/stat.h> defines S_ISTXT)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_ISTXT)]])],[HAS_S_ISTXT=yes],[HAS_S_ISTXT=no])
AC_MSG_RESULT($HAS_S_ISTXT)
if test $HAS_S_ISTXT = yes; then
	AC_DEFINE(HAVE_S_ISTXT, 1, [Define as 1 if <sys/stat.h> defines S_ISTXT])
fi
AC_MSG_CHECKING(if <sys/stat.h> defines S_ISVTX)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_ISVTX)]])],[HAS_S_ISVTX=yes],[HAS_S_ISVTX=no])
AC_MSG_RESULT($HAS_S_ISVTX)
if test $HAS_S_ISVTX = yes; then
	AC_DEFINE(HAVE_S_ISVTX, 1, [Define as 1 if <sys/stat.h> defines S_ISVTX])
fi

# Find the best function to set timestamps.
AC_CHECK_FUNCS([futimens futimes futimesat utimes utime], [break])

dnl # The command line tool can copy high resolution timestamps if such
dnl # information is available in struct stat. Otherwise one second accuracy
dnl # is used.
AC_CHECK_MEMBERS([
	struct stat.st_atim.tv_nsec,
	struct stat.st_atimespec.tv_nsec,
	struct stat.st_atimensec,
	struct stat.st_uatime,
	struct stat.st_atim.st__tim.tv_nsec,
	struct stat.st_birthtime,
	struct stat.st_flags])
AC_CHECK_MEMBERS([
	struct stat.st_blksize,
	struct stat.st_rdev])
AC_STRUCT_ST_BLOCKS

AC_MSG_CHECKING(if timezone is defined)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[printf("%ld", timezone)]])],[HAS_TIMEZONE=yes],[HAS_TIMEZONE=no])
AC_MSG_RESULT($HAS_TIMEZONE)
AC_STRUCT_TIMEZONE

dnl check for missing typedefs
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

AC_TYPE_INT32_T		dnl system.h:37
AC_TYPE_INT64_T		dnl tests/bson/test-atomic.c:39
AC_TYPE_INT8_T		dnl rpmio/bson.h:619
AC_TYPE_SSIZE_T		dnl lua/local/lposix.c:791
AC_TYPE_UID_T		dnl lua/shadow/copydir.c:153
AC_TYPE_UINT16_T	dnl tests/bson/test-endian.c:31
AC_TYPE_UINT32_T	dnl python/rpmts-py.c:361
AC_TYPE_UINT64_T	dnl tests/bson/TestSuite.c:136
AC_TYPE_UINT8_T		dnl python/rpmkeyring-py.c:36

AC_CHECK_TYPES([ptrdiff_t])

dnl check size of built-in types
AC_CHECK_TYPES([long long, long double])
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(unsigned char, 1)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(unsigned short, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(unsigned int, 4)
AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(unsigned long, 4)
AC_CHECK_SIZEOF(long long, 8)
AC_CHECK_SIZEOF(unsigned long long, 8)

dnl linux/posix and *BSD signals differ still, sigh
AC_CHECK_TYPES([sighandler_t, sig_t])

dnl checks for library functions (specific)
AC_FUNC_ALLOCA
AC_FUNC_MMAP
AC_FUNC_VPRINTF

AC_FUNC_CHOWN		dnl lua/shadow/copydir.c:238
AC_FUNC_ERROR_AT_LINE	dnl system.h:175
AC_FUNC_FORK		dnl rpmqv.cc:810
AC_FUNC_FSEEKO		dnl rpmio/bspatch.c:46
AC_FUNC_GETGROUPS	dnl lua/local/lposix.c:2070
AC_FUNC_GETMNTENT	dnl system.h:717
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK	dnl lua/shadow/commonio.c:61
AC_FUNC_MALLOC		dnl debug.h:48
AC_FUNC_MKTIME		dnl lua/loslib.c:256
AC_FUNC_STRCOLL		dnl lua/lvm.c:216
AC_FUNC_STRERROR_R	dnl rpmio/bson.c:7553
AC_FUNC_STRNLEN		dnl rpmio/fnmatch.c:297
AC_FUNC_STRTOD		dnl rpmio/bson.c:2750

dnl XXX causes rpl_realloc() undefined failures
dnl AC_FUNC_REALLOC		dnl debug.h:65

dnl checks for library functions (generic)
dnl AC_CHECK_FUNCS([dnl
dnl     asprintf basename chflags clearenv confstr fallocate fchflags fchmod dnl
dnl     ftok getaddrinfo getattrlist getcwd getdelim getline getmode getnameinfo dnl
dnl     getpassphrase getxattr getwd iconv inet_aton lchflags lchmod lchown dnl
dnl     lgetxattr lsetxattr lutimes madvise mempcpy mkdtemp mkstemp mtrace dnl
dnl     posix_fadvise posix_fallocate pow putenv realpath regcomp dnl
dnl     secure_getenv __secure_getenv dnl
dnl     setattrlist setenv setlocale setmode setxattr dnl
dnl     sigaddset sigdelset sigemptyset sighold sigrelse sigpause dnl
dnl     sigprocmask sigsuspend sigaction sqrt dnl
dnl     stpcpy stpncpy strcspn strdup strerror strmode strndup strspn strstr dnl
dnl     strtol strtoul dnl
dnl ])
AC_CHECK_FUNCS([dnl
    asprintf atexit basename chflags clearenv clock_gettime dnl
    confstr dup2 endgrent endpwent fallocate fchdir fchflags dnl
    fchmod fdatasync floor ftok ftruncate getaddrinfo dnl
    getattrlist getcwd getdelim gethostbyaddr gethostbyname dnl
    gethostname getline getmode getmntent getmntinfo dnl
    getnameinfo getpass getpassphrase gettimeofday getwd dnl
    getxattr hasmntopt iconv inet_aton inet_ntoa isascii dnl
    lchflags lchmod lchown lgetxattr localtime_r lsetxattr dnl
    lutimes madvise mbrlen memchr mempcpy memset mkdir dnl
    mkdtemp mkfifo mkstemp mtrace munmap nl_langinfo dnl
    pathconf posix_fadvise posix_fallocate pow putenv dnl
    realpath regcomp rmdir rpmatch __secure_getenv secure_getenv dnl
    select setattrlist setenv setmode setxattr dnl
    sigaction sigaddset sigdelset sigemptyset sighold sigpause dnl
    sigprocmask sigrelse sigsuspend setlocale socket sqrt dnl
    stpcpy stpncpy strcspn strdup strerror strmode dnl
    strncasecmp strndup strpbrk strspn strstr strtol strtoul strtoull dnl
    tzset uname dnl
])

dnl # specific additional tests needed to replace Berkeley-DB db_config.h with RPM config.h
AC_CHECK_FUNCS([dnl
    isalpha isdigit isprint isspace localtime memcmp memcpy memmove printf dnl
    qsort raise rand strcasecmp strcat strchr strncat strncmp strrchr strsep dnl
    time atoi atol getopt fclose fgetc fgets fopen fwrite dnl
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

AC_CHECK_FUNCS(syslog)
if test $ac_cv_func_syslog = no; then
  # syslog is not in the default libraries.  See if it's in some other.
  for lib in bsd socket inet; do
    AC_CHECK_LIB($lib, syslog, [AC_DEFINE(HAVE_SYSLOG)
      LIBS="$LIBS -l$lib"; break])
  done
fi

AC_CHECK_FUNCS(setproctitle)
if test $ac_cv_func_setproctitle = no; then
  # setproctitle is not in the default libraries.  See if it's in some other.
  AC_CHECK_LIB(setproctitle, setproctitle, [AC_DEFINE(HAVE_SETPROCTITLE)
  LIBS="$LIBS -lsetproctitle"])
fi

dnl # Check how to find out the amount of physical memory in the system. The
dnl # xz command line tool uses this to automatically limit its memory usage.
dnl # - sysconf() gives all the needed info on GNU+Linux and Solaris.
dnl # - BSDs use sysctl().
AC_MSG_CHECKING([how to detect the amount of physical memory])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
int
main()
{
	long i;
	i = sysconf(_SC_PAGESIZE);
	i = sysconf(_SC_PHYS_PAGES);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_PHYSMEM_SYSCONF], [1],
		[Define to 1 if the amount of physical memory can be detected
		with sysconf(_SC_PAGESIZE) and sysconf(_SC_PHYS_PAGES).])
	AC_MSG_RESULT([sysconf])
], [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main()
{
	int name[2] = { CTL_HW, HW_PHYSMEM };
	unsigned long mem;
	size_t mem_ptr_size = sizeof(mem);
	sysctl(name, 2, &mem, &mem_ptr_size, NULL, NULL);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_PHYSMEM_SYSCTL], [1],
		[Define to 1 if the amount of physical memory can be detected
		with sysctl().])
	AC_MSG_RESULT([sysctl])
], [
	AC_MSG_RESULT([unknown])
])])

dnl # Check how to find out the number of available CPU cores in the system.
dnl # sysconf(_SC_NPROCESSORS_ONLN) works on most systems, except that BSDs
dnl # use sysctl().
AC_MSG_CHECKING([how to detect the number of available CPU cores])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
int
main()
{
	long i;
	i = sysconf(_SC_NPROCESSORS_ONLN);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_NCPU_SYSCONF], [1],
		[Define to 1 if the number of available CPU cores can be
		detected with sysconf(_SC_NPROCESSORS_ONLN).])
	AC_MSG_RESULT([sysconf])
], [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main()
{
	int name[2] = { CTL_HW, HW_NCPU };
	int cpus;
	size_t cpus_size = sizeof(cpus);
	sysctl(name, 2, &cpus, &cpus_size, NULL, NULL);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_NCPU_SYSCTL], [1],
		[Define to 1 if the number of available CPU cores can be
		detected with sysctl().])
	AC_MSG_RESULT([sysctl])
], [
	AC_MSG_RESULT([unknown])
])])

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
                AC_CHECK_LIB([elf], [gelf_getnote], [
                    AC_DEFINE(HAVE_GELF_GETNOTE, 1, [Define to 1 if you have the gelf_getnote() function.])
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

dnl # optional Perl language bindings
WITH_PERL_SUBDIR=""
WITH_PERL_SUBPACKAGE=0
WITH_PERL_LIBS=""
if test ".$WITH_PTHREADS" = .yes; then
    WITH_PERL_LIBS="$LIBS"
fi
AC_ARG_WITH(perl, AS_HELP_STRING([--with-perl], [build with RPM Perl language bindings]), [
    if test ".$withval" != .no; then
	PERL_INSTALLDIRS=""
	   if test "$withval" == "vendor"; then
		    PERL_INSTALLDIRS="'INSTALLDIRS'	=> 'vendor',"
	    fi
	    if test "$withval" == "site"; then
		    PERL_INSTALLDIRS="'INSTALLDIRS'     => 'site',"
	    fi
	    AC_DEFINE_UNQUOTED([PERL_INSTALLDIRS], [$PERL_INSTALLDIRS], [Perl install directory (vendor/site)])
	    AC_SUBST(PERL_INSTALLDIRS)
        WITH_PERL_SUBDIR=perl
        WITH_PERL_SUBPACKAGE=1
        AC_CONFIG_FILES([perl/Makefile.PL])
        AC_CONFIG_FILES([perl/Makefile])
    fi
])
AC_SUBST(WITH_PERL_SUBDIR)
AC_SUBST(WITH_PERL_SUBPACKAGE)
AC_SUBST(WITH_PERL_LIBS)

dnl # optional Perl-URPM language bindings
WITH_PERL_URPM_SUBDIR=""
WITH_PERL_URPM_SUBPACKAGE=0
WITH_PERL_URPM_LIBS=""
if test ".$WITH_PTHREADS" = .yes; then
    WITH_PERL_URPM_LIBS="$LIBS"
fi
AC_ARG_WITH(perl-urpm, AS_HELP_STRING([--with-perl-urpm], [build with Perl URPM language bindings]), [
    if test ".$withval" != .no; then
	PERL_URPM_INSTALLDIRS=""
	   if test "$withval" == "vendor"; then
		    PERL_URPM_INSTALLDIRS="'INSTALLDIRS'	=> 'vendor',"
	    fi
	    if test "$withval" == "site"; then
		    PERL_URPM_INSTALLDIRS="'INSTALLDIRS'     => 'site',"
	    fi
	    AC_DEFINE_UNQUOTED([PERL_URPM_INSTALLDIRS], [$PERL_URPM_INSTALLDIRS], [Perl install directory (vendor/site)])
	    AC_SUBST(PERL_URPM_INSTALLDIRS)
        WITH_PERL_URPM_SUBDIR=perl-URPM
        WITH_PERL_URPM_SUBPACKAGE=1
        AC_CONFIG_FILES([perl-URPM/Makefile.PL])
        AC_CONFIG_FILES([perl-URPM/Makefile])
    fi
])
AC_SUBST(WITH_PERL_URPM_SUBDIR)
AC_SUBST(WITH_PERL_URPM_SUBPACKAGE)
AC_SUBST(WITH_PERL_URPM_LIBS)

dnl # optional Python language bindings
WITH_PYTHON_SUBDIR=""
WITH_PYTHON_SUBPACKAGE=0
WITH_PYTHON_INCDIR=""

PYTHON=""
PYTHON_VERSION=""
PYTHON_PREFIX=""
PYTHON_EXEC_PREFIX=""
#PYTHON_PLATFORM=""
pythondir=""
pkgpythondir=""
pyexecdir=""
pkgpyexecdir=""

AC_ARG_WITH(python, AS_HELP_STRING([--with-python], [build with RPM Python language bindings]), [
    if test ".$withval" != .no; then
        case "$withval" in
             2.x|2.3|2.4|2.5|2.6|2.7)
                  python_version=$withval
                  ;;
             yes)
                  python_version="2.7 2.6 2.5 2.4 2.3 2.x"
                  ;;
             *)
                  AC_MSG_WARN([--with-python only supports values "2.x", "2.3", "2.4", "2.5", "2.6" or "2.7" ignoring...])
                  python_version=""
                  ;;
        esac
	for python_ver in $python_version ; do
             if test "$python_ver" != "2.x" ; then
                  AC_MSG_CHECKING([for Python ${python_ver} API])
                  save_CFLAGS="${CFLAGS}"
                  for i in "$prefix" "/usr/local" "/usr"; do
                  CFLAGS="${save_CFLAGS} -I$i/include/python${python_ver}"
                  AC_RUN_IFELSE([AC_LANG_SOURCE(
                  [[#include "Python.h"
                     main() { exit(strncmp("${python_ver}", PY_VERSION, 3)); } ]])
                  ], [
                     PYTHON_VERSION="${python_ver}"
                     AC_MSG_RESULT(yes)
                     PYTHON_PREFIX="${i}"
                     break
                  ], [ 
                  ], [
                     dnl if we're cross compiling, assume the user has a clue
                     if test "$withval" = "$python_ver"; then
                         PYTHON_VERSION="${python_ver}"
                         AC_MSG_RESULT(yes)
                         break
                     fi
                  ])
                  done
                  CFLAGS="${save_CFLAGS}"
                  if test ".$PYTHON_VERSION" == "."; then
                     dnl check Mac OS X framework
                     save_CFLAGS="${CFLAGS}"
                     for f in "~" "" "/System" "/Network"; do
                     CFLAGS="${save_CFLAGS} -I$f/Library/Frameworks/Python.framework/Versions/${python_ver}/include/python${python_ver}"
                     AC_RUN_IFELSE([AC_LANG_SOURCE(
                     [[#include "Python.h"
                        main() { exit(strncmp("${python_ver}", PY_VERSION, 3)); } ]])
                     ], [
                        PYTHON_VERSION="${python_ver}"
                        AC_MSG_RESULT([yes (using Python.framework)])
                        PYTHON_PREFIX="$f/Library/Frameworks/Python.framework/Versions/${python_ver}"
                        break
                     ], [
                     ])
                     done
                     CFLAGS="${save_CFLAGS}"
                  fi
                  if test ".$PYTHON_VERSION" == "."; then
                     AC_MSG_RESULT(no)
                  fi
             else
                  AC_MSG_CHECKING([for Python 2.x API])
                  AC_RUN_IFELSE([AC_LANG_SOURCE(
                  [[#include <python/Python.h>
                     main() { exit(strncmp("2.", PY_VERSION, 2)); } ]])
                  ], [
                     PYTHON_VERSION=""
                     AC_MSG_RESULT(yes)
                  ], [ 
                     AC_MSG_RESULT(no)
                  ], [
                     dnl if we're cross compiling, assume the user has a clue
                     if test "$withval" = "$python_ver"; then
                         PYTHON_VERSION="${python_ver}"
                         AC_MSG_RESULT(yes)
                     else
                         AC_MSG_RESULT(no)
                     fi
                  ])
             fi
             if test ".$PYTHON_VERSION" != "."; then
                  WITH_PYTHON_SUBDIR=python
                  WITH_PYTHON_SUBPACKAGE=1
                  break
             fi
	done
    fi
    AC_ARG_WITH(python-inc-dir, AS_HELP_STRING([--with-python-inc-dir=DIR], [directory with Python include files]),
	    [WITH_PYTHON_INCDIR="$withval"], [WITH_PYTHON_INCDIR="`(${__PYTHON} -c 'from distutils.sysconfig import get_python_inc; print get_python_inc()') 2>/dev/null`"])
    AC_ARG_WITH(python-lib-dir, AS_HELP_STRING([--with-python-lib-dir=DIR], [directory with Python library]),
	    [pythondir="$withval"], [pythondir="`(${__PYTHON} -c 'from distutils.sysconfig import get_python_lib; print get_python_lib(1)') 2>/dev/null`"])
])
AC_SUBST(WITH_PYTHON_SUBDIR)
AC_SUBST(WITH_PYTHON_SUBPACKAGE)
AC_SUBST(WITH_PYTHON_INCDIR)

if test ".$pythondir" != "."; then
    PYTHON="${__PYTHON}"
    PYTHON_PREFIX="\${prefix}"
    PYTHON_EXEC_PREFIX="\${exec_prefix}"
#   PYTHON_PLATFORM="???"
    pkgpythondir="\${pythondir}/\${PACKAGE}"
    pyexecdir="\${pythondir}"
    pkgpyexecdir="\${pyexecdir}/\${PACKAGE}"
fi

AC_SUBST(PYTHON)
AC_SUBST(PYTHON_VERSION)
AC_SUBST(PYTHON_PREFIX)
AC_SUBST(PYTHON_EXEC_PREFIX)
#AC_SUBST(PYTHON_PLATFORM)
AC_SUBST(pythondir)
AC_SUBST(pkgpythondir)
AC_SUBST(pyexecdir)
AC_SUBST(pkgpyexecdir)

dnl ##
dnl ## ==== THIRD-PARTY LIBRARIES (2/2) ====
dnl ##

AC_MSG_HEADER([THIRD-PARTY LIBRARIES (2/2)])

dnl # POPT
RPM_CHECK_LIB(
    [POPT], [popt],
    [popt], [poptGetContext], [popt.h],
    [yes,internal:external], [popt],
    [], [AC_MSG_ERROR([mandatory POPT library not found])])

HAVE_RPM_COMPRESSION=no
dnl # ZLib
RPM_CHECK_LIB(
    [ZLib], [zlib],
    [z gz], [gzread], [zlib.h],
    [yes,external:none], [zlib],
    [ AC_CHECK_FUNC(gzseek)
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_ZLIB_H, 1, [Define to 1 if you have <zlib.h>])
      fi
      HAVE_RPM_COMPRESSION=yes 
    ], [])

dnl # GNU BZip2
RPM_CHECK_LIB(
    [GNU BZip2], [bzip2],
    [bz2], [BZ2_bzread], [bzlib.h],
    [yes,external:none], [],
    [
      HAVE_RPM_COMPRESSION=yes 
    ], [])

dnl # libLZMA
WITH_XZ_INTERNAL=false
RPM_CHECK_LIB(
    [XZ libLZMA], [xz],
    [lzma], [lzma_auto_decoder], [lzma.h],
    [yes,external:internal:none], [xz:src/liblzma/api:src/liblzma],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          WITH_XZ_INTERNAL=true
          AC_DEFINE(HAVE_LZMA_H, 1, [Define to 1 if you have <lzma.h>])
      fi
      HAVE_RPM_COMPRESSION=yes 
    ], [])
if test ".$HAVE_RPM_COMPRESSION" = .no; then
    AC_MSG_ERROR([you have passed --without-{xz,bzip2,zlib} together but it isn't possible to build rpm without any form of compression library. At a minimum, i suggest adding --with-zlib if you want to actually build and install a *.rpm package])
fi

dnl # BeeCrypt
RPM_CHECK_LIB(
    [BeeCrypt], [beecrypt],
    [beecrypt], [mpfprintln], [beecrypt/api.h],
    [yes,internal:external], [beecrypt:include],
    [], [ AC_MSG_ERROR([mandatory BeeCrypt library not found]) ])
if test ".$WITH_BEECRYPT" = .no; then
    AC_MSG_ERROR([BEECRYPT is mandatory])
fi

dnl # GCrypt
RPM_CHECK_LIB(
    [GCrypt], [gcrypt],
    [gcrypt], [gcry_pk_verify], [gcrypt.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_GCRYPT, 1, [Define if building with GCrypt])
    ], [])

dnl # Mozilla NSS
RPM_CHECK_LIB(
    [Mozilla NSS], [nss],
    [nss3 nss], [NSS_NoDB_Init], [nss.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_NSS, 1, [Define if building with Mozilla NSS])
      AC_CHECK_LIB([nss3], [NSS_Initialize], [
          AC_DEFINE(HAVE_NSS_INITIALIZE, 1, [Define to 1 if you have the NSS_Initialize() function.])
      ])
      AC_CHECK_LIB([nss3], [NSS_InitContext], [
          AC_DEFINE(HAVE_NSS_INITCONTEXT, 1, [Define to 1 if you have the NSS_InitContext() function.])
      ])
    ], [])

dnl # OpenSSL
RPM_CHECK_LIB(
    [OpenSSL], [openssl],
    [crypto], [ERR_load_CRYPTO_strings], [openssl/crypto.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_SSL, 1, [Define if building with OpenSSL])
      AC_CHECK_LIB([crypto], [RSA_generate_key_ex], [
          AC_DEFINE(HAVE_RSA_GENERATE_KEY_EX, 1, [Define to 1 if you have the RSA_generate_key_ex() function.])
      ])
      AC_CHECK_LIB([crypto], [DSA_generate_parameters_ex], [
          AC_DEFINE(HAVE_DSA_GENERATE_PARAMETERS_EX, 1, [Define to 1 if you have the DSA_generate_parameters_ex() function.])
      ])
    ], [])

dnl # TomFastMath
#RPM_CHECK_LIB(
#    [TomFastMath], [tomfastmath],
#    [tfm], [_init], [tfm.h],
#    [yes,external:none], [],
#    [ AC_DEFINE(WITH_TOMFASTMATH, 1, [Define if building with LibTomFastMath])
#      CPPFLAGS="$CPPFLAGS -DTFM_DESC"
#    ], [])

dnl # TomMath
#RPM_CHECK_LIB(
#    [TomMath], [tommath],
#    [tfm], [_init], [tommath.h],
#    [no,external:none], [],
#    [ AC_DEFINE(WITH_TOMMATH, 1, [Define if building with LibTomMath])
#      CPPFLAGS="$CPPFLAGS -DLTM_DESC"
#      if test -d ${includedir}/tommath; then
#        CPPFLAGS="$CPPFLAGS -I${includedir}/tommath"
#      fi
#    ], [])

dnl # GnuMP
#RPM_CHECK_LIB(
#    [GnuMP], [gmp],
#    [gmp], [_init], [gmp.h],
#    [no,external:none], [],
#    [ AC_DEFINE(WITH_GMP, 1, [Define if building with GnuMP])
#      CPPFLAGS="$CPPFLAGS -DLTM_DESC -DUSE_LTM"
#    ], [])

dnl # TomCrypt
dnl XXX FIXME: add libtomcrypt.pc
RPM_CHECK_LIB(
    [TomCrypt], [tomcrypt],
    [tomcrypt], [ltc_init_multi], [tomcrypt_cfg.h],
    [yes,external:none], [],
    [ AC_DEFINE(WITH_TOMCRYPT, 1, [Define if building with LibTomCrypt])
      CPPFLAGS="$CPPFLAGS -DLTM_DESC -DUSE_LTM"
      if test -d ${includedir}/tommath; then
        CPPFLAGS="$CPPFLAGS -I${includedir}/tommath"
      fi
      AC_CHECK_DECL([LTC_PKCS_1_V1_5],
        [ AC_DEFINE(USE_LTC_LTC_PKCS_1_V1_5, 1, [Define for older LibTomCrypt])
	], [])
      LIBS="$LIBS -ltommath"
    ], [])

dnl # IBM TPM 1.2 software emulation
RPM_CHECK_LIB(
    [IBM TPM], [libtpm],
    [tpm], [TPM_Init], [tpm.h],
    [no,external:internal:none], [libtpm:libtpm/lib:libtpm/lib],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          WITH_LIBTPM_INTERNAL=true
          AC_DEFINE(HAVE_TPM_H, 1, [Define to 1 if you have the <tpm.h> header file.])
          AC_DEFINE(HAVE_LIBTPM, 1, [Define to 1 if you have the libtpm library (-ltpm).])
      fi
      AM_CONDITIONAL([WITH_LIBTPM], [true])
    ],[
      AM_CONDITIONAL([WITH_LIBTPM], [false])
    ])


dnl # select default Cryptography implementation to use (default for CLI option --usecrypto)
AC_ARG_WITH([usecrypto],
    AS_HELP_STRING(
        [--with-usecrypto=ID],
        [use particular default for --usecrypto option ("beecrypt", "cdsa", "gcrypt", "nss", "openssl", "tomcrypt" )]
    ), [
    case "`echo $withval | tr 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 'abcdefghijklmnopqrstuvwxyz'`" in
        beecrypt | bc ) AC_DEFINE(USE_CRYPTO_BEECRYPT, 1, [Define to 1 for using BeeCrypt    as default cryptography library]) ;;
        cdsa          ) AC_DEFINE(USE_CRYPTO_CDSA,     1, [Define to 1 for using Apple CDSA as default cryptography library]) ;;
        gcrypt   | gc ) AC_DEFINE(USE_CRYPTO_GCRYPT,   1, [Define to 1 for using GCrypt      as default cryptography library]) ;;
        nss           ) AC_DEFINE(USE_CRYPTO_NSS,      1, [Define to 1 for using Mozilla NSS as default cryptography library]) ;;
        openssl | ssl ) AC_DEFINE(USE_CRYPTO_OPENSSL,  1, [Define to 1 for using OpenSSL     as default cryptography library]) ;;
        tomcrypt | tc ) AC_DEFINE(USE_CRYPTO_TOMCRYPT, 1, [Define to 1 for using TomCrypt     as default cryptography library]) ;;
        * ) AC_MSG_ERROR([invalid cryptography library identifier specified for --with-usecrypto option]) ;;
    esac
])

dnl # Libsasl2 (for mongoc)
RPM_CHECK_LIB(
    [Cyrus Libsasl2], [sasl2],
    [sasl2], [sasl_client_init], [sasl/sasl.h],
    [yes,external:none], [],
    [ AC_DEFINE(WITH_SASL2, 1, [Define if building with Cyrus Libsasl2])
    ], [])

dnl # Libselinux
RPM_CHECK_LIB(
    [Libselinux], [selinux],
    [selinux], [is_selinux_enabled], [selinux/selinux.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_SELINUX, 1, [Define if building with Libselinux])
    ], [])

dnl # Libsepol
RPM_CHECK_LIB(
    [Libsepol], [sepol],
    [sepol], [sepol_policydb_create], [sepol/sepol.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_SEPOL, 1, [Define if building with Libsepol])
    ], [])

dnl # Libsemanage
WITH_SEMANAGE_SEMODULE=""
RPM_CHECK_LIB(
    [Libsemanage], [semanage],
    [semanage], [semanage_handle_create], [semanage/semanage.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_SEMANAGE, 1, [Define if building with Libsemanage])
      WITH_SEMANAGE_SEMODULE="semodule spooktool"
    ], [])
AC_SUBST(WITH_SEMANAGE_SEMODULE)

# Expat
RPM_CHECK_LIB(
    [Expat], [expat],
    [expat], [XML_SetXmlDeclHandler], [expat.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_EXPAT, 1, [Define if building with Expat])
    ], [])

# Libtasn1
RPM_CHECK_LIB(
    [Libtasn1], [libtasn1],
    [tasn1], [asn1_array2tree], [libtasn1.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_LIBTASN1, 1, [Define if building with Libtasn1])
    ], [])

# Pakchois
RPM_CHECK_LIB(
    [Pakchois], [pakchois],
    [pakchois], [pakchois_module_load], [pakchois/pakchois.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_PAKCHOIS, 1, [Define if building with Pakchois])
    ], [])

# GnuTLS
RPM_CHECK_LIB(
    [GnuTLS], [gnutls],
    [gnutls], [gnutls_global_init], [gnutls/gnutls.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_GNUTLS, 1, [Define if building with GnuTLS])
    ], [])

dnl # Neon
RPM_CHECK_LIB(
    [Neon], [neon],
    [neon], [ne_session_create], [ne_session.h],
    [no,internal:external:none], [neon:src:src],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
              AC_DEFINE(HAVE_NEON_NE_GET_RESPONSE_HEADER, 1,
                    [Define to 1 if you have ne_get_response_header() in libneon.])
      else
          AC_CHECK_LIB(neon, ne_get_response_header, [
              AC_DEFINE(HAVE_NEON_NE_GET_RESPONSE_HEADER, 1,
                    [Define to 1 if you have ne_get_response_header() in libneon.]) ])
      fi
    ], [ AC_MSG_WARN([No Neon library found, using unsupported configuration]) ])

# Libgit2
# XXX internal needs *.a/*.la files to merge into -lrpmmisc
RPM_CHECK_LIB(
    [Libgit2], [libgit2],
    [git2], [git_repository_open], [git2.h],
    [no,external:internal:none], [libgit2:include],
    [
      AM_CONDITIONAL([WITH_LIBGIT2], [true])
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_GIT2_H, 1, [Define to 1 if you have <git2.h>])
      fi
    ],[
      AM_CONDITIONAL([WITH_LIBGIT2], [false])
    ])

# APR
RPM_CHECK_LIB(
    [Apr], [apr],
    [apr-1], [apr_initialize], [apr-1/apr.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_APR, 1, [Define if building with Apr])
      CPPFLAGS="$CPPFLAGS -I$prefix/include/apr-1"
    ], [])

# Subversion
RPM_CHECK_LIB(
    [Subversion], [subversion],
    [svn_client-1], [svn_client_create_context], [subversion-1/svn_client.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_SUBVERSION, 1, [Define if building with Subversion])
      CPPFLAGS="$CPPFLAGS -I$prefix/include/subversion-1"
    ], [])

dnl # File (magic)
WITH_FILE_INTERNAL=false
RPM_CHECK_LIB(
    [File (magic)], [file],
    [magic], [magic_open], [magic.h],
    [yes,internal:external:none], [file:src:src],
    [ AM_CONDITIONAL(WITH_FILE_INTERNAL, [ test ".$RPM_CHECK_LIB_LOCATION" = .internal ])
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          WITH_FILE_INTERNAL=true
          AC_DEFINE(HAVE_MAGIC_H, 1, [Define to 1 if you have <magic.h>])
      fi
    ],
    [ AM_CONDITIONAL(WITH_FILE_INTERNAL, [ false ])
    ])

# UnixODBC
RPM_CHECK_LIB(
    [UnixODBC], [unixodbc],
    [odbc], [SQLAllocHandle], [sqlext.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_UNIXODBC, 1, [Define if building with UnixODBC])
      AM_CONDITIONAL([WITH_UNIXODBC], [true])
    ],
    [ AM_CONDITIONAL([WITH_UNIXODBC], [false])
    ])

dnl # Berkeley-DB & SQLite
DBLIBSRCS=""
# XXX won't handle --includedir override
CPPFLAGS="${CPPFLAGS} -I${prefix}/include/${DBXY}"
RPM_CHECK_LIB(
    [Berkeley-DB], [db],
    [db-6.1], [db_create], [db.h],
    [yes,external], [db3],
    [ DBLIBSRCS="$DBLIBSRCS db3.c"
      AM_CONDITIONAL(WITH_DB, [ true ])
      AM_CONDITIONAL(WITH_DB_INTERNAL, [ test ".$RPM_CHECK_LIB_LOCATION" = .internal ])
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_DB_H, 1, [Have <db.h> header])
      else
          WITH_DB_SUBDIR=""
      fi
    ],
    [ AM_CONDITIONAL(WITH_DB, [ false ])
      AM_CONDITIONAL(WITH_DB_INTERNAL, [ false ])
    ])

dnl # Sqlite external
RPM_CHECK_LIB(
    [SQLite], [sqlite],
    [sqlite3], [sqlite3_open], [sqlite3.h],
    [no,external:none], [sqlite],
    [ DBLIBSRCS="$DBLIBSRCS sqlite.c" ],
    [])

dnl # Sqlite 3.8.3.1 from db-6.1.19
RPM_CHECK_LIB(
    [Berkeley-DB (+SQLite3)], [dbsql],
    [db_sql-6.1], [sqlite3_open], [dbsql.h],
    [yes,external], [db3/sql],
    [
      AM_CONDITIONAL(WITH_DBSQL, [ true ])
      AC_DEFINE(WITH_SQLITE, 1, [Define as 1 if building with SQLite library])
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
        WITH_DB_CPPFLAGS="${WITH_DB_CPPFLAGS} -I\$(top_srcdir)/db/sql/generated"
        WITH_DB_LIBS="${WITH_DBSQL_LIBS}"
        DBLIBSRCS="$DBLIBSRCS sqlite.c"
      else
        WITH_DBSQL_SUBDIR=""
      fi
    ], [
      AM_CONDITIONAL(WITH_DBSQL, [ false ])
    ])

DBLIBOBJS=`echo $DBLIBSRCS | sed -e "s/\.c/\.lo/g"`

AC_SUBST(DBLIBSRCS)
AC_SUBST(DBLIBOBJS)

AC_ARG_WITH(db-largefile, AS_HELP_STRING([--with-db-largefile], [build Berkeley-DB with LARGEFILE support]))
AC_ARG_WITH(db-mutex,     AS_HELP_STRING([--with-db-mutex=ARG], [build Berkeley-DB with MUTEX type ARG]))

AC_ARG_WITH(db-rpc,       AS_HELP_STRING([--with-db-rpc],       [build Berkeley-DB with RPC support]))
AM_CONDITIONAL(WITH_DB_RPC, test ".$with_db_rpc" = .yes)

AC_ARG_WITH(db-tools-integrated, AS_HELP_STRING([--with-db-tools-integrated], [install integrated Berkeley-DB tools]))
AM_CONDITIONAL(WITH_DB_TOOLS_INTEGRATED, test ".$with_db_tools_integrated" = .yes)

dnl # determine whether the db sqlite3 tool/library should be built
dnl XXX hot-wire --with-dbsql; --with-dbsql=external will need RPM_CHECK_LIB
AM_CONDITIONAL(WITH_DBSQL_CODEGEN, true)

dnl # determine default RPM DB API and configuration
DBAPI=3
dnl case ".$DBLIBSRCS" in
dnl     *db3.c*    ) DBAPI=3 ;;
dnl     *sqlite.c* ) DBAPI=4 ;;
dnl     *          ) AC_MSG_ERROR([either Berkeley-DB (--with-db) or SQLite (--with-sqlite) have to be used]) ;;
dnl esac
dnl AC_ARG_WITH([dbapi],
dnl     AS_HELP_STRING([--with-dbapi=ID], [use particular default RPM DB API ("db", "sqlite" or numeric)]), [
dnl     case "$withval" in
dnl         db|3     ) DBAPI=3 ;;
dnl         sqlite|4 ) DBAPI=4 ;;
dnl         [0-9]    ) DBAPI=$withval ;;
dnl     esac
dnl ])
AC_SUBST(DBAPI)

dnl # Readline (eventually needed by augtool)
RPM_CHECK_LIB(
    [Readline], [readline],
    [readline], [readline], [readline/readline.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_READLINE, 1, [Define to 1 if you want to use Readline])
      AC_CHECK_HEADERS([readline/history.h])
      AC_CHECK_FUNCS([rl_completion_matches])
    ], [])

dnl # Augeas
WITH_AUGEAS_AUGTOOL=""
RPM_CHECK_LIB(
    [Augeas], [augeas],
    [augeas], [aug_init], [augeas.h],
    [no,external:none], [],
    [ WITH_AUGEAS_AUGTOOL="augtool"
    ], [])
AC_SUBST(WITH_AUGEAS_AUGTOOL)

dnl # CUDF
WITH_CUDF_CUDFTOOL=""
RPM_CHECK_LIB(
    [Libcudf], [cudf],
    [cudf], [cudf_parse_from_file], [cudf.h],
    [no,external:none], [],
    [ WITH_CUDF_CUDFTOOL="cudftool"
    ], [])
AC_SUBST(WITH_CUDF_CUDFTOOL)

dnl # Perl ExtUtils::Embed
RPM_CHECK_LIB(
    [Perl ExtUtils::Embed], [perlembed],
    [perl], [perl_alloc], [EXTERN.h],
    [no,external:none], [],
    [ if test ".$RPM_CHECK_LIB_LOCATION" != .internal; then
         LDFLAGS="$LDFLAGS -Wl,-rpath,$RPM_CHECK_LIB_LOCATION"
      fi
    ], [])

dnl # Python embedding
RPM_CHECK_LIB(
    [Python embedding], [pythonembed],
    [python2.7 python2.6 python2.5 python2.4], [Py_Initialize], [Python.h],
    [no,external:none], [],
    [
    ], [])

dnl # Ruby
WITH_RUBY_SUBDIR=""
WITH_RUBY_CPPFLAGS=""
WITH_RUBY_VENDORARCHDIR=""
RPM_CHECK_LIB(
    [Ruby], [ruby],
    [ruby], [ruby_init], [ruby.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_RUBYEMBED, 1, [Define to 1 if you want embedded Ruby])
      WITH_RUBY_SUBDIR=ruby
    ], [])
AC_SUBST(WITH_RUBY_CPPFLAGS)
AC_SUBST(WITH_RUBY_SUBDIR)
AC_SUBST(WITH_RUBY_VENDORARCHDIR)

dnl # Java prerequisites (swiped from db-6.1.19/dist/aclocal_java et al)
WITH_JAVA=no
AC_ARG_WITH([java],
    AS_HELP_STRING([--with-java], [build RPM with java support]),
    [ WITH_JAVA="$withval" ], [ WITH_JAVA=no ])
if test ".$WITH_JAVA" = .yes; then
    if test ".$JAVA_HOME" != .; then
        PATH="$JAVA_HOME/bin:$PATH"
        export PATH
    fi
    AC_CHECK_CLASSPATH
dnl AC_JAVA_OPTIONS
    AC_PROG_JAVAC
dnl AC_PROG_JAVAH
dnl AC_PROG_JAR
    AC_PROG_JAVA

dnl AC_MSG_CHECKING(java version)
dnl case "$JAVA" in
dnl *kaffe* )
dnl         JAVA_VERSION=`$JAVA -version 2>&1 |
dnl                 sed -e '/Java Version:/!d' -e 's/.*Java Version: \([[^  ]]*\)[[         ]]*/\1/'` ;;
dnl * )     JAVA_VERSION=`$JAVA -version 2>&1 |
dnl                 sed -e '/ version /!d' -e 's/.*"\(.*\)".*/\1/'` ;;
dnl esac
dnl AC_MSG_RESULT($JAVA_VERSION)
dnl case "$JAVA_VERSION" in
dnl 1.[[3456789]]* | 1.[[1-9]][[0-9]]* | [[23456789]]* ) ;;
dnl * )
dnl         AC_MSG_ERROR([Java version 1.3 or higher required, got $JAVA_VERSION]) ;;
dnl esac

    AC_JNI_INCLUDE_DIR
    for JNI_INCLUDE_DIR in $JNI_INCLUDE_DIRS; do
            CPPFLAGS="$CPPFLAGS -I$JNI_INCLUDE_DIR"
    done
    # XXX Set --with-jvm=$JAVA_HOME/ implicitly (final / to follow symlink)
    if test ".$with_jvm" = .; then with_jvm="$_JTOPDIR/"; fi

dnl # JVM
RPM_CHECK_LIB(
    [JVM embedding], [jvm],
    [jvm], [JNI_CreateJavaVM], [jni.h],
    [no,external:none], [],
    [
      if test ".$RPM_CHECK_LIB_LOCATION" != .internal; then
        LDFLAGS="$LDFLAGS -Wl,-rpath,$__rcl_dir"
        LIBS="$LIBS -ljvm"
      fi
      AC_DEFINE(WITH_JNIEMBED, 1, [Define to 1 if you want an embedded JVM])
      AC_CHECK_CLASS(bsh.Interpreter)
    ], [])
fi

dnl # MRuby
RPM_CHECK_LIB(
    [MRuby], [mruby],
    [mruby], [mrb_open], [mruby.h],
    [no,external:none], [],
    [ AC_DEFINE(WITH_MRBEMBED, 1, [Define to 1 if you want embedded MRuby])
    ], [])

dnl # Squirrel (needs -lstdc++)
LIBSsave="$LIBS"
LIBS="$LIBS -lstdc++"
RPM_CHECK_LIB(
    [Squirrel], [squirrel],
    [squirrel], [sq_open], [squirrel.h],
    [no,external:none], [],
    [
      LIBS="$LIBS -lsqstdlib"
    ], [
      LIBS="$LIBSsave"
    ])

dnl # Tcl (should tcl8.4 be supported too?)
WITH_TCL_SUBDIR=""
RPM_CHECK_LIB(
    [Tcl], [tcl],
    [tcl8.6 tcl8.5 tcl], [Tcl_CreateInterp], [tcl.h],
    [no,external:none], [],
    [
    ], [])
AC_SUBST(WITH_TCL_SUBDIR)

dnl # FICL
RPM_CHECK_LIB(
    [FICL], [ficl],
    [ficl], [ficlSystemCreate], [ficl.h],
    [no,internal:external:none], [ficl],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_FICL_H, 1, [Define to 1 if you have <ficl.h>])
	  AC_DEFINE(HAVE_LIBFICL, 1, [Define to 1 if you have the 'ficl' library (-lficl).])
      fi
    ], [])

dnl # ATerm (needed by Nix)
RPM_CHECK_LIB(
    [ATerm], [aterm],
    [ATerm], [ATinit], [aterm1.h],
    [no,external:none], [],
    [
    ], [])
dnl # Nix
dnl # XXX the tests are mostly pointless, all bow to the AutoFu!
AC_LANG_PUSH(C++)
RPM_CHECK_LIB(
    [Nix expressions], [nix],
    [format], [_init], [globals.hh],
    [no,external:none], [],
    [
    ], [])
AC_LANG_POP(C++)

dnl # JavaScript
WITH_JS_SUBDIR=""

WITH_SPIDERMONKEY_SUBDIR=""
RPM_CHECK_LIB(
    [MozJS (js-1.8.5)], [mozjs185],
    [mozjs185], [JS_NewContext], [jsapi.h],
    [no,external:none], [mozjs185:src:src],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          WITH_SPIDERMONKEY_SUBDIR="$WITH_JS_SUBDIR/src"
      else
          WITH_JS_SUBDIR=js
      fi
    ], [])
AC_SUBST(WITH_SPIDERMONKEY_SUBDIR)

#WITH_SPIDERMONKEY_SUBDIR=""
#AC_LANG_PUSH(C++)
#RPM_CHECK_LIB(
#    [MozJS (mozjs-24)], [mozjs24],
#    [mozjs-24], [_fini], [jsapi.h],
#    [no,external:none], [mozjs-24:src:src],
#    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
#          WITH_SPIDERMONKEY_SUBDIR="$WITH_JS_SUBDIR/src"
#      else
#          WITH_JS_SUBDIR=js
#      fi
#    ], [])
#AC_LANG_POP(C++)
#AC_SUBST(WITH_SPIDERMONKEY_SUBDIR)

#WITH_SPIDERMONKEY_SUBDIR=""
#AC_LANG_PUSH(C++)
#RPM_CHECK_LIB(
#    [MozJS (mozjs-31)], [mozjs31],
#    [mozjs-31], [_fini], [jsapi.h],
#    [no,external:none], [mozjs-31:src:src],
#    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
#          WITH_SPIDERMONKEY_SUBDIR="$WITH_JS_SUBDIR/src"
#      else
#          WITH_JS_SUBDIR=js
#      fi
#    ], [])
#AC_LANG_POP(C++)
#AC_SUBST(WITH_SPIDERMONKEY_SUBDIR)

AC_SUBST(WITH_JS_SUBDIR)

dnl # GPSEE
dnl # XXX hotwire the GPSEE -> JS dependency implictly
RPM_CHECK_LIB(
    [GPSEE], [gpsee],
    [gpsee], [gpsee_createInterpreter], [gpsee.h],
    [no,external:internal:none], [gpsee],
    [ AC_DEFINE(WITH_GPSEE, 1, [Define to 1 if to use GPSEE as the embedded JS interpreter])
      CPPFLAGS="$CPPFLAGS -I${includedir}/js"
      LIBS="$LIBS -lmozjs185"
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_GPSEE_H, 1, [Define to 1 if you have <gpsee.h>])
	  AC_DEFINE(HAVE_LIBGPSEE, 1, [Define to 1 if you have the 'gpsee' library (-ljs).])
      fi
    ], [])
GPSEE_MODULES_PATH="$prefix/libexec/gpsee"
AC_SUBST(GPSEE_MODULES_PATH)

dnl # Mongo
dnl # XXX mostly bogus tests, but sufficient to get WITH_MONGO defined
dnl RPM_CHECK_LIB(
dnl     [Mongo DB], [mongo],
dnl     [mongoclient], [md5_init], [targetver.h],
dnl     [no,external:none], [mongo],
dnl     [], [])

dnl # Rc
RPM_CHECK_LIB(
    [Rc], [rc],
    [rc], [rc_main], [rc.h],
    [no,internal:none], [rc],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_RC_H, 1, [Define to 1 if you have <rc.h>])
	  AC_DEFINE(HAVE_LIBRC, 1, [Define to 1 if you have the 'rc' library (-lrc).])
      fi
    ], [])

dnl # Bash
RPM_CHECK_LIB(
    [Bash], [bash],
    [bash], [bash_main], [bashtypes.h],
    [no,internal:none], [bash],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
	  AC_DEFINE(HAVE_LIBBASH, 1, [Define to 1 if you have the 'bash' library (-lbash).])
      fi
    ], [])

dnl # Lua
WITH_LUA_SUBDIR_DEF=""
WITH_LUA_SUBDIR_LIB=""
USE_LUA_SHADOW=no
USE_LUA_CHKCONFIG=no
USE_LUA_CRYPTO=no
USE_LUA_SOCKET=no
RPM_CHECK_LIB(
    [Lua], [lua],
    [lua], [lua_newstate], [lua.h],
    [no,internal:external:none], [lua:local],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_CHECK_LIB(m, fabs)
          AC_CHECK_LIB(dl, dlopen)
          AC_CHECK_FUNC(dlopen, [ WITH_LUA_SUBDIR_DEF="$WITH_LUA_SUBDIR_DEF -DLUA_USE_DLOPEN" ])
          AC_CHECK_FUNC(regcomp, [ WITH_LUA_SUBDIR_DEF="$WITH_LUA_SUBDIR_DEF -DLUA_USE_POSIX" ])
dnl       case "$host" in
dnl           *-*-linux* ) USE_LUA_SHADOW=yes; USE_LUA_CHKCONFIG=yes; ;;
dnl       esac
          AC_DEFINE(WITH_LUA_INTERNAL, 1, [Define if building with internal Lua])
          AC_CONFIG_FILES([lua/Makefile])
      else
          AC_CHECK_LIB(m, fabs)
          AC_CHECK_LIB(dl, dlopen)
          AC_CHECK_FUNC(dlopen)
      fi
    ], [])
AC_SUBST(WITH_LUA_SUBDIR_DEF)
AC_SUBST(WITH_LUA_SUBDIR_LIB)
AM_CONDITIONAL(USE_LUA_SHADOW, [ test ".$USE_LUA_SHADOW" = .yes ])
AM_CONDITIONAL(USE_LUA_CHKCONFIG, [ test ".$USE_LUA_CHKCONFIG" = .yes ])
AM_CONDITIONAL(USE_LUA_CRYPTO, [ test ".$USE_LUA_CRYPTO" = .yes ])
AM_CONDITIONAL(USE_LUA_SOCKET, [ test ".$USE_LUA_SOCKET" = .yes ])

dnl # PCRE
dnl # use cases:
dnl # - RPM under WITH_PCRE uses <pcre.h>+libpcre in addition to <regex.h>
dnl # - RPM under WITH_PCRE+WITH_PCRE_POSIX uses <pcreposix.h>+libpcreposix instead of <regex.h>
dnl # - RPM/Lua under LUA_USE_PCRE uses <pcre.h>+libpcre in addition to <regex.h>
dnl # - RPM/Lua under REX_POSIX_INCLUDE uses <pcreposix.h>+libposixpcre instead of <regex.h>
dnl # attention:
dnl # - In the locally bundled PCRE all libpcre objects are embedded into libpcreposix
dnl #   because of s/lib/noinst/ changes in pcre/Makefile.am. Here we have to link
dnl #   against libpcreposix only!
WITH_PCRE_INTERNAL=false
RPM_CHECK_LIB(
    [PCRE], [pcre],
    [pcre], [pcre_compile], [pcre.h],
    [yes,external:internal:none], [pcre],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          WITH_PCRE_INTERNAL=true
          AC_DEFINE(HAVE_PCRE_H, 1, [Define to 1 if you have <pcre.h>])
	  AC_DEFINE(HAVE_LIBPCRE, 1, [Define to 1 if you have the 'pcre' library (-lpcre).])
          AC_DEFINE(WITH_PCRE_POSIX, 1, [Define as 1 if PCRE's POSIX API is available])
	  WITH_PCRE_LIBS="-lpcreposix" # override, not prepend! (see attention above)
          if test ".$WITH_LUA" = .yes; then
              WITH_LUA_SUBDIR_DEF="$WITH_LUA_SUBDIR_DEF -DREX_POSIX_INCLUDE=\"<pcreposix.h>\""
          fi
	  AC_SUBST(WITH_PCRE_LIBS)
      else
	dnl # make sure PCRE POSIX API can be really _USED_ by RPM
	LIBS_SAVED="$LIBS"
	LIBS="-lpcreposix $LIBS"
	AC_LINK_IFELSE([
          AC_LANG_PROGRAM([#include <pcreposix.h>], [(void)regcomp(0, 0, 0)])
	], [
          AC_DEFINE(WITH_PCRE_POSIX, 1, [Define as 1 if PCRE's POSIX API is available])
          if test ".$WITH_LUA" = .yes; then
              WITH_LUA_SUBDIR_DEF="$WITH_LUA_SUBDIR_DEF -DREX_POSIX_INCLUDE=\"<pcreposix.h>\""
          fi
	], [
          LIBS="$LIBS_SAVED"
	])
      fi
      dnl # enable PCRE native API support for embedded Lua
      if test ".$WITH_LUA" = .yes; then
          WITH_LUA_SUBDIR_DEF="$WITH_LUA_SUBDIR_DEF -DLUA_USE_PCRE"
      fi
    ], [])
    dnl # check PCRE 
    if test ".$WITH_PCRE" = .no; then
         AC_MSG_ERROR([PCRE is mandatory. If you don't have  libpcre then --with-pcre=internal could be used])
    fi

dnl # OSSP uuid
dnl XXX hack-o-round typedef __darwin_uuid_t uuid_t; in Mac OS X <unistd.h>
case "$host" in
    *-*-darwin* )
mv confdefs.h confdefs.h-SAVE
grep -v 'define HAVE_UNISTD_H' confdefs.h-SAVE > confdefs.h
;;
esac
RPM_CHECK_LIB(
    [OSSP uuid], [uuid],
    [ossp-uuid uuid], [uuid_import], [uuid.h],
    [no,external:none], [],
    [ dnl # enable OSSP uuid native API support for embedded Lua
      if test ".$WITH_LUA" = .yes; then
          WITH_LUA_SUBDIR_DEF="$WITH_LUA_SUBDIR_DEF -DLUA_USE_UUID"
      fi
    ], [])
case "$host" in
    *-*-darwin* )
grep 'define HAVE_UNISTD_H' confdefs.h-SAVE >> confdefs.h
rm -f confdefs.h-SAVE
;;
esac

dnl # Syck
RPM_CHECK_LIB(
    [Syck], [syck],
    [syck], [syckparse], [syck.h],
    [no,internal:external:none], [syck:lib:lib],
    [ if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
          AC_DEFINE(HAVE_SYCK_H, 1, [Have <syck.h> header])
      fi
    ], [])

dnl # FIXME: AutoFu tests for <sys/acl.h> and -lacl are needed w/o --with-xar
RPM_CHECK_LIB(
    [ATTR], [attr],
    [attr], [attr_get], [sys/xattr.h],
    [no,external:none], [],
    [], [])
RPM_CHECK_LIB(
    [ACL], [acl],
    [acl], [acl_free], [sys/acl.h],
    [no,external:none], [],
    [], [])

dnl # XAR
RPM_CHECK_LIB(
    [XAR], [xar],
    [xar], [xar_open], [xar.h],
    [no,external:internal:none], [xar:include:lib],
    [ AM_CONDITIONAL(WITH_XAR, [ true ])
      if test ".$RPM_CHECK_LIB_LOCATION" = .internal; then
        AC_DEFINE(HAVE_XAR_H, 1, [Have <xar.h> header])
        AC_DEFINE(HAVE_XAR_GET_SIZE, 1, [Have xar_get_size()])
          AC_DEFINE(HAVE_SYS_ACL_H, 1, [Have <sys/acl.h> header])
      else
        AC_CHECK_LIB(xar, xar_get_size, [
          AC_DEFINE(HAVE_XAR_GET_SIZE, 1,
                    [Define to 1 if you have xar_get_size() in libxar.]) ])
      fi
    ],
    [ AM_CONDITIONAL(WITH_XAR, [ false ])
    ])

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
WITH_KEYUTILS_RPMKEY=""
RPM_CHECK_LIB(
    [Linux keyutils], [keyutils],
    [keyutils], [keyctl_read], [keyutils.h],
    [no,external:none], [],
    [ WITH_KEYUTILS_RPMKEY="rpmkey"
    ], [])
AC_SUBST(WITH_KEYUTILS_RPMKEY)

# later..
dnl # CPUInfo
RPM_CHECK_LIB(
    [CPUInfo], [cpuinfo],
    [cpuinfo], [cpuinfo_new], [cpuinfo.h],
    [no,external:none], [],
    [AC_MSG_WARN([CPUInfo support is under development])], [])

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
target_os_gnu="%{nil}"
if echo "$target_os" | grep '.*-gnulibc1' > /dev/null ; then
	target_os=`echo "${target_os}" | sed 's/-gnulibc1$//'`
	target_os_gnu="-gnulibc1"
fi
if echo "$target_os" | grep '.*-gnu' > /dev/null ; then
	target_os=`echo "${target_os}" | sed 's/-gnu$//'`
	target_os_gnu="-gnu"
fi
changequote(<, >)
target_os_exact="${target_os}"
target_os_major=`echo "${target_os}" | sed 's/\..*$//'`
target_os_noversion=`echo "${target_os}" | sed 's/[0-9]*\..*$//'`
changequote([, ])
rm -f ./find-provides
if test -f ${srcdir}/autodeps/${target_cpu}-${target_os_exact}.prov ; then
    ln -s ${srcdir}/autodeps/${target_cpu}-${target_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${target_os_exact}.prov ; then
    ln -s ${srcdir}/autodeps/${target_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${target_os_major}.prov ; then
    ln -s ${srcdir}/autodeps/${target_os_major}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${target_os_noversion}.prov ; then
    ln -s ${srcdir}/autodeps/${target_os_noversion}.prov ./find-provides
else
    ln -s ${srcdir}/autodeps/none ./find-provides
fi
rm -f ./find-requires
if test -f ${srcdir}/autodeps/${target_cpu}-${target_os_exact}.req ; then
    ln -s ${srcdir}/autodeps/${target_cpu}-${target_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${target_os_exact}.req ; then
    ln -s ${srcdir}/autodeps/${target_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${target_os_major}.req ; then
    ln -s ${srcdir}/autodeps/${target_os_major}.req ./find-requires
elif test -f ${srcdir}/autodeps/${target_os_noversion}.req ; then
    ln -s ${srcdir}/autodeps/${target_os_noversion}.req ./find-requires
else
    ln -s ${srcdir}/autodeps/none ./find-requires
fi

dnl # determine the canonical arch-vendor-os for the build machine
autorelocate_path='%{nil}'
autorelocate_dcolor='0'
RPMCANONCOLOR=3
case "${target_cpu}" in
*86)		RPMCANONARCH=i386 ;;
ia32e*)		RPMCANONARCH=ia32e ;;
amd64*)		RPMCANONARCH=amd64 ;;
x86_64*)	RPMCANONARCH=x86_64 ;;
alpha*)		RPMCANONARCH=alpha ;;
sparc64*)	RPMCANONARCH=sparc64 ;;
sparc*)		RPMCANONARCH=sparc ;;
ia64*)		RPMCANONCOLOR=2; RPMCANONARCH=ia64;
		autorelocate_path='/emul/%%{ARCH}-%%{OS}'
		autorelocate_dcolor='1' ;;
s390x*)		RPMCANONARCH=s390x ;;
s390*)		RPMCANONARCH=s390 ;;
powerpc64*|ppc64*)	RPMCANONARCH=ppc64 ;;
powerpc*|ppc*)	RPMCANONARCH=ppc ;;
armv3l*)	RPMCANONARCH=armv3l ;;
armv4l*)	RPMCANONARCH=armv4l ;;
armv4b*)	RPMCANONARCH=armv4b ;;
arm*)		RPMCANONARCH="${target_cpu}" ;;
mipsel*)	RPMCANONCOLOR=7; RPMCANONARCH=mipsel ;;
mips*)		RPMCANONCOLOR=7; RPMCANONARCH=mips ;;
m68k*)		RPMCANONARCH=m68k ;;
*)		RPMCANONARCH=unknown ;;
esac
case "${target_os_noversion}" in
mint)		RPMCANONARCH=m68kmint ;;
esac
RPMCANONVENDOR="$target_vendor"
case "${target_vendor}" in
unknown|pc|ibm|redhat|pld|mandriva|conectiva|lvr|yellowdog|caos|crux|ark|momonga|openmamba)
	test -f /etc/SuSE-release &&		RPMCANONVENDOR=suse
	test -f /etc/momonga-release &&		RPMCANONVENDOR=momonga
        test -f /etc/owl-release -o -f /etc/owl-setup.conf -o -d /etc/mtree && RPMCANONVENDOR=openwall
	test -f /etc/fedora-release &&		RPMCANONVENDOR=fedora
	test -f /etc/redhat-release &&		RPMCANONVENDOR=redhat
	test -f /etc/pld-release &&		RPMCANONVENDOR=pld
	test -f /etc/mandriva-release &&	RPMCANONVENDOR=mandriva
	test -f /etc/conectiva-release &&	RPMCANONVENDOR=conectiva
	test -f /etc/lvr-release &&		RPMCANONVENDOR=lvr
	test -f /etc/yellowdog-release &&	RPMCANONVENDOR=yellowdog
	test -f /etc/caos-release &&		RPMCANONVENDOR=caos
	test -f /usr/bin/crux &&		RPMCANONVENDOR=crux
	test -f /etc/ark-release &&		RPMCANONVENDOR=ark
	test -f /etc/openmamba-release &&	RPMCANONVENDOR=openmamba
	test -r /etc/os-release && ID=$(. /etc/os-release && test -n "$ID" && echo "$ID") && RPMCANONVENDOR="$ID"
	;;
esac
RPMCANONOS="$target_os_noversion"
RPMCANONGNU="$target_os_gnu"
AC_SUBST(RPMCANONCOLOR)
AC_SUBST(autorelocate_path)
AC_SUBST(autorelocate_dcolor)
AC_SUBST(RPMCANONARCH)
AC_SUBST(RPMCANONVENDOR)
AC_SUBST(RPMCANONOS)
AC_SUBST(RPMCANONGNU)

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

dnl # determine number of CPUs command
AC_MSG_CHECKING([how to determine NCPUS])
NCPUS_COMMAND="echo 1"
case "$host" in
    *-*-linux* ) NCPUS_COMMAND="/usr/bin/getconf _NPROCESSORS_ONLN" ;;
    *-*-freebsd* ) NCPUS_COMMAND="/usr/bin/getconf NPROCESSORS_ONLN" ;;
    *-*-solaris* ) NCPUS_COMMAND="psrinfo | egrep 'on-line' | wc -l  | sed -e 's+^ *++'" ;;
    *-*-darwin* ) NCPUS_COMMAND="/usr/sbin/sysctl -n hw.ncpu" ;;
esac
NCPUS_RESULT=`eval $NCPUS_COMMAND`
AC_MSG_RESULT([`$NCPUS_COMMAND` ($NCPUS_RESULT)])
AC_SUBST(NCPUS_COMMAND)

dnl # support for versioned installation paths
AC_ARG_WITH(
    [path-versioned],
    AS_HELP_STRING([--with-path-versioned],[install with versioned paths]),
    [WITH_PATH_VERSIONED=$withval],
    [WITH_PATH_VERSIONED=no])
AM_CONDITIONAL(
    [WITH_PATH_VERSIONED],
    [ test ".$WITH_PATH_VERSIONED" = .yes ])
if test ".$WITH_PATH_VERSIONED" = .yes; then
    WITH_PATH_VERSIONED_SUFFIX="-${VERSION}"
else
    WITH_PATH_VERSIONED_SUFFIX=""
fi
AC_SUBST([WITH_PATH_VERSIONED_SUFFIX])
AC_SUBST([WITH_PATH_VERSIONED])

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
AC_DEFINE_UNQUOTED(
    [VARPREFIX], ["$varprefix"],
    [Path to /var]
)
AC_SUBST(varprefix)
if test X"$prefix" = XNONE ; then
    usrprefix="$ac_default_prefix"
else
    usrprefix=$prefix
fi
AC_DEFINE_UNQUOTED(
    [USRPREFIX], ["$usrprefix"],
    [Path to /usr]
)
AC_SUBST(usrprefix)

dnl # Object file suffix (normally "o").
objext="$ac_cv_objext"
AC_SUBST(objext)

dnl # Static library suffix (normally "a").
libext="$acl_cv_libext"
AC_SUBST(libext)

dnl # Shared library suffix (normally "so").
shlibext="$acl_cv_shlibext"
AC_SUBST(shlibext)

dnl # Executable file suffix (normally "").
exeext="$ac_exeext"
AC_SUBST(exeext)

dnl # determine RPM database directory path
AC_MSG_CHECKING([for RPM database directory])
DBPATH=`echo "${varprefix}/lib/rpm"`
if test ".$WITH_PATH_VERSIONED" = .yes; then
    DBPATH="${DBPATH}-${VERSION}"
fi
AC_ARG_WITH(
    [path-database],
    AS_HELP_STRING([--with-path-database=ARG], [build with RPM database directory path]),
    [DBPATH="$withval"]
)
AC_DEFINE_UNQUOTED(
    [DBPATH], ["$DBPATH"],
    [Full path to RPM database directory]
)
AC_SUBST(DBPATH)
AC_MSG_RESULT([$DBPATH])
DBPATH_MACRO=`echo "$DBPATH" | sed -e "s;^${varprefix}/;%{_var}/;" -e "s;^${usrprefix}/;%{_usr}/;"`
AC_SUBST(DBPATH_MACRO)

dnl # determine RPM package sources directory path
AC_MSG_CHECKING([for RPM package sources directory])
PKGSRCDIR=`echo "${usrprefix}/src/rpm"`
if test ".$WITH_PATH_VERSIONED" = .yes; then
    PKGSRCDIR="${PKGSRCDIR}-${VERSION}"
fi
AC_ARG_WITH(
    [path-sources],
    AS_HELP_STRING([--with-path-sources=ARG], [build with RPM package sources directory path]),
    [PKGSRCDIR="$withval"]
)
AC_DEFINE_UNQUOTED(
    [PKGSRCDIR], ["$PKGSRCDIR"],
    [Full path to RPM package sources directory]
)
AC_SUBST(PKGSRCDIR)
AC_MSG_RESULT([$PKGSRCDIR])
PKGSRCDIR_MACRO=`echo "$PKGSRCDIR" | sed -e "s;^${varprefix}/;%{_var}/;" -e "s;^${usrprefix}/;%{_usr}/;"`
AC_SUBST(PKGSRCDIR_MACRO)

dnl # determine RPM cache directory path
AC_MSG_CHECKING([for RPM cache directory])
CACHEDIR=`echo "${varprefix}/cache/rpm"`
if test ".$WITH_PATH_VERSIONED" = .yes; then
    CACHEDIR="${CACHEDIR}-${VERSION}"
fi
AC_ARG_WITH(
    [path-cache],
    AS_HELP_STRING([--with-path-cache=ARG], [build with RPM cache directory path]),
    [CACHEDIR="$withval"]
)
AC_DEFINE_UNQUOTED(
    [CACHEDIR], ["$CACHEDIR"],
    [Full path to RPM cache directory]
)
AC_SUBST(CACHEDIR)
AC_MSG_RESULT([$CACHEDIR])
CACHEDIR_MACRO=`echo "$CACHEDIR" | sed -e "s;^${varprefix}/;%{_var}/;" -e "s;^${usrprefix}/;%{_usr}/;"`
AC_SUBST(CACHEDIR_MACRO)

dnl # determine RPM repackage directory path
AC_MSG_CHECKING([for RPM repackage directory])
REPACKAGEDIR=`echo "${varprefix}/spool/repackage"`
if test ".$WITH_PATH_VERSIONED" = .yes; then
    REPACKAGEDIR="${REPACKAGEDIR}-${VERSION}"
fi
AC_ARG_WITH(
    [path-repackage],
    AS_HELP_STRING([--with-path-repackage=ARG], [build with RPM repackage directory path]),
    [REPACKAGEDIR="$withval"]
)
AC_DEFINE_UNQUOTED(
    [REPACKAGEDIR], ["$REPACKAGEDIR"],
    [Full path to RPM repackage directory]
)
AC_SUBST(REPACKAGEDIR)
AC_MSG_RESULT([$REPACKAGEDIR])
REPACKAGEDIR_MACRO=`echo "$REPACKAGEDIR" | sed -e "s;^${varprefix}/;%{_var}/;" -e "s;^${usrprefix}/;%{_usr}/;"`
AC_SUBST(REPACKAGEDIR_MACRO)

dnl # determine RPM magic file path
AC_MSG_CHECKING([for RPM magic file])
MAGIC_MACRO=`echo "${prefix}/share/file/magic"`
if $WITH_FILE_INTERNAL; then
    MAGIC_MACRO="%{_rpmhome}/magic"
fi
AC_ARG_WITH(
    [path-magic],
    AS_HELP_STRING([--with-path-magic=ARG], [build with RPM magic file path (or macro)]),
    [MAGIC_MACRO="$withval"]
)
AC_MSG_RESULT([$MAGIC_MACRO])
AC_SUBST(MAGIC_MACRO)

dnl # determine RPM buildroot directory path
AC_MSG_CHECKING([for RPM buildroot directory])
BUILDROOTDIR_MACRO="%{_tmppath}"
AC_ARG_WITH(
    [path-buildroot],
    AS_HELP_STRING([--with-path-buildroot=ARG], [build with RPM buildroot path (or macro)]),
    [BUILDROOTDIR_MACRO="$withval"]
)
AC_MSG_RESULT([$BUILDROOTDIR_MACRO])
AC_SUBST(BUILDROOTDIR_MACRO)

dnl # determine RPM locale directory path
AC_MSG_CHECKING([for RPM locale directory])
LOCALEDIR=`echo "${usrprefix}/share/locale"`
if test ".$WITH_PATH_VERSIONED" = .yes; then
    LOCALEDIR="$LOCALEDIR/rpm-$VERSION"
fi
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
localedir="$LOCALEDIR" # (pass-through to foreign generated po/Makefile, too)

dnl # determine RPM system library directory path
AC_MSG_CHECKING([for RPM system library directory])
USRLIBRPM=`echo "${usrprefix}/lib/rpm"`
if test ".$WITH_PATH_VERSIONED" = .yes; then
    USRLIBRPM="${USRLIBRPM}-${VERSION}"
fi
AC_ARG_WITH(
    [path-lib],
    AS_HELP_STRING([--with-path-lib=ARG], [build with RPM system library directory path]),
    [USRLIBRPM="$withval"], [ ac_configure_args="$ac_configure_args --with-path-lib=\"$USRLIBRPM\"" ]
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
if test ".$WITH_PATH_VERSIONED" = .yes; then
    SYSCONFIGDIR="$SYSCONFIGDIR/$VERSION"
fi
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
MACROFILES="${USRLIBRPM}/macros"
MACROFILES="${MACROFILES}:${USRLIBRPM}/%{_target}/macros"
MACROFILES="${MACROFILES}:${SYSCONFIGDIR}/macros.*"
MACROFILES="${MACROFILES}:${SYSCONFIGDIR}/macros"
MACROFILES="${MACROFILES}:${SYSCONFIGDIR}/%{_target}/macros"
AC_ARG_WITH(
    [extra-path-macros],
    AS_HELP_STRING([--with-extra-path-macros=ARG], [build with additional colon-separated RPM macro files path not overriding defaults]),
    [MACROFILES="$MACROFILES:$withval"]
)
if test ".$WITH_PATH_VERSIONED" = .yes; then
    MACROFILES="${MACROFILES}:~/.rpmmacros-${VERSION}"
else
    MACROFILES="${MACROFILES}:~/.rpmmacros"
fi
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

dnl # determine POPT configuration files path
AC_MSG_CHECKING([for RPM POPT configuration files path])
RPMPOPTFILES="${USRLIBRPM}/rpmpopt"
RPMPOPTFILES="${RPMPOPTFILES}:${USRLIBRPM}/%{_target}/rpmpopt"
RPMPOPTFILES="${RPMPOPTFILES}:${SYSCONFIGDIR}/rpmpopt.*"
RPMPOPTFILES="${RPMPOPTFILES}:${SYSCONFIGDIR}/rpmpopt"
RPMPOPTFILES="${RPMPOPTFILES}:${SYSCONFIGDIR}/%{_target}/rpmpopt"
if test ".$WITH_PATH_VERSIONED" = .yes; then
    RPMPOPTFILES="${RPMPOPTFILES}:~/.rpmpopt-${VERSION}"
else
    RPMPOPTFILES="${RPMPOPTFILES}:~/.rpmpopt"
fi
AC_ARG_WITH(
    [path-rpmpopt],
    AS_HELP_STRING([--with-path-rpmpopt=ARG], [build with colon-separated RPM POPT configuration file path]),
    [RPMPOPTFILES="$withval"]
)
AC_DEFINE_UNQUOTED(
    [RPMPOPTFILES], ["$RPMPOPTFILES"],
    [Colon separated RPM POPT configuration files path]
)
AC_SUBST(RPMPOPTFILES)
AC_MSG_RESULT([$RPMPOPTFILES])

dnl # determine RPM Lua script files path
AC_MSG_CHECKING([for RPM Lua script files path])
RPMLUAFILES="${USRLIBRPM}/init.lua:${USRLIBRPM}/rpmlua"
RPMLUAFILES="${RPMLUAFILES}:${USRLIBRPM}/%{_target}/rpmlua"
RPMLUAFILES="${RPMLUAFILES}:${SYSCONFIGDIR}/rpmlua.*"
RPMLUAFILES="${RPMLUAFILES}:${SYSCONFIGDIR}/rpmlua"
RPMLUAFILES="${RPMLUAFILES}:${SYSCONFIGDIR}/%{_target}/rpmlua"
if test ".$WITH_PATH_VERSIONED" = .yes; then
    RPMLUAFILES="${RPMLUAFILES}:~/.rpmlua-${VERSION}"
else
    RPMLUAFILES="${RPMLUAFILES}:~/.rpmlua"
fi
AC_ARG_WITH(
    [path-rpmlua],
    AS_HELP_STRING([--with-path-rpmlua=ARG], [build with colon-separated RPM Lua script file path]),
    [RPMLUAFILES="$withval"]
)
AC_DEFINE_UNQUOTED(
    [RPMLUAFILES], ["$RPMLUAFILES"],
    [Colon separated RPM Lua script files path]
)
AC_SUBST(RPMLUAFILES)
AC_MSG_RESULT([$RPMLUAFILES])

DISTRO_MACROS="#"
dnl # determine whether to build using a specific vendor's changes or not
AC_MSG_CHECKING([if building for a specific vendor])
AC_ARG_WITH(
    [vendor],
    AS_HELP_STRING([--with-vendor=VENDOR], [build with a supported vendor's specific set of changes: mandriva, ark, fedora, openmamba]),
    [if test ".$withval" != .no; then
	    case "$withval" in
	    "mandriva")
		    AC_DEFINE([RPM_VENDOR_MANDRIVA], 1, Vendor is Mandriva)
		    DISTRO_MACROS=""
		    ;;
	    "ark")
		    AC_DEFINE([RPM_VENDOR_ARK], 1, Vendor is Ark)
		    ;;
	    "fedora")
		    AC_DEFINE([RPM_VENDOR_FEDORA], 1, Vendor is Fedora)
		    ;;
	    "openmamba")
		    AC_DEFINE([RPM_VENDOR_OPENMAMBA], 1, Vendor is openmamba)
		    ;;
	    "suse")
		    AC_DEFINE([RPM_VENDOR_SUSE], 1, Vendor is suse)
		    ;;
	    esac
	    AC_MSG_RESULT([$withval])
    else
	    AC_MSG_RESULT(no)
    fi]
)
AC_SUBST(DISTRO_MACROS)

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
    Doxyfile Makefile rpmpopt rpm.spec
    misc/Makefile
    rpmio/Makefile rpmio/auto/Makefile rpmio/auto/desc rpmio/auto/types
    rpmdb/Makefile rpmdb/auto/Makefile rpmdb/auto/desc rpmdb/auto/types
    lib/Makefile lib/auto/Makefile lib/auto/desc lib/auto/types
    build/Makefile build/auto/Makefile build/auto/desc build/auto/types
    tools/Makefile
    scripts/Makefile
    rpmdb/DB_CONFIG
    macros/macros macros/macros.rpmbuild
    macros/cmake macros/gstreamer macros/java macros/kernel macros/libtool
    macros/mandriva macros/suse macros/fedora macros/mono macros/perl macros/pkgconfig macros/php
    macros/python macros/ruby macros/selinux macros/tcl
    lua/tests/Makefile lua/tests/libs/Makefile
    doc/Makefile
    doc/manual/Makefile doc/fr/Makefile doc/ja/Makefile doc/ko/Makefile
    doc/pl/Makefile doc/ru/Makefile doc/sk/Makefile
    js/Makefile python/Makefile
    ruby/Makefile scripts/rpm.pc lib/rpmversion.h rpmconstant/Makefile
    tests/Makefile tests/macros
    tests/bson/Makefile tests/mongoc/Makefile tests/yajl/Makefile
])
dnl tests/auto/Makefile

dnl # final step: generate output
AC_OUTPUT

