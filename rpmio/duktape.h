/*
 *  Duktape public API for Duktape 1.4.0.
 *  See the API reference for documentation on call semantics.
 *  The exposed API is inside the DUK_API_PUBLIC_H_INCLUDED
 *  include guard.  Other parts of the header are Duktape
 *  internal and related to platform/compiler/feature detection.
 *
 *  Git commit cad6f595382a0cc1a7e4207794ade5be11b3e397 (v1.4.0).
 *  Git branch master.
 *
 *  See Duktape AUTHORS.rst and LICENSE.txt for copyright and
 *  licensing information.
 */

/* LICENSE.txt */
/*
 *  ===============
 *  Duktape license
 *  ===============
 *  
 *  (http://opensource.org/licenses/MIT)
 *  
 *  Copyright (c) 2013-2016 by Duktape authors (see AUTHORS.rst)
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

/* AUTHORS.rst */
/*
 *  ===============
 *  Duktape authors
 *  ===============
 *  
 *  Copyright
 *  =========
 *  
 *  Duktape copyrights are held by its authors.  Each author has a copyright
 *  to their contribution, and agrees to irrevocably license the contribution
 *  under the Duktape ``LICENSE.txt``.
 *  
 *  Authors
 *  =======
 *  
 *  Please include an e-mail address, a link to your GitHub profile, or something
 *  similar to allow your contribution to be identified accurately.
 *  
 *  The following people have contributed code, website contents, or Wiki contents,
 *  and agreed to irrevocably license their contributions under the Duktape
 *  ``LICENSE.txt`` (in order of appearance):
 *  
 *  * Sami Vaarala <sami.vaarala@iki.fi>
 *  * Niki Dobrev
 *  * Andreas \u00d6man <andreas@lonelycoder.com>
 *  * L\u00e1szl\u00f3 Lang\u00f3 <llango.u-szeged@partner.samsung.com>
 *  * Legimet <legimet.calc@gmail.com>
 *  * Karl Skomski <karl@skomski.com>
 *  * Bruce Pascoe <fatcerberus1@gmail.com>
 *  * Ren\u00e9 Hollander <rene@rene8888.at>
 *  
 *  Other contributions
 *  ===================
 *  
 *  The following people have contributed something other than code (e.g. reported
 *  bugs, provided ideas, etc; roughly in order of appearance):
 *  
 *  * Greg Burns
 *  * Anthony Rabine
 *  * Carlos Costa
 *  * Aur\u00e9lien Bouilland
 *  * Preet Desai (Pris Matic)
 *  * judofyr (http://www.reddit.com/user/judofyr)
 *  * Jason Woofenden
 *  * Micha\u0142 Przyby\u015b
 *  * Anthony Howe
 *  * Conrad Pankoff
 *  * Jim Schimpf
 *  * Rajaran Gaunker (https://github.com/zimbabao)
 *  * Andreas \u00d6man
 *  * Doug Sanden
 *  * Josh Engebretson (https://github.com/JoshEngebretson)
 *  * Remo Eichenberger (https://github.com/remoe)
 *  * Mamod Mehyar (https://github.com/mamod)
 *  * David Demelier (https://github.com/markand)
 *  * Tim Caswell (https://github.com/creationix)
 *  * Mitchell Blank Jr (https://github.com/mitchblank)
 *  * https://github.com/yushli
 *  * Seo Sanghyeon (https://github.com/sanxiyn)
 *  * Han ChoongWoo (https://github.com/tunz)
 *  * Joshua Peek (https://github.com/josh)
 *  * Bruce E. Pascoe (https://github.com/fatcerberus)
 *  * https://github.com/Kelledin
 *  * https://github.com/sstruchtrup
 *  * Michael Drake (https://github.com/tlsa)
 *  * https://github.com/chris-y
 *  * Laurent Zubiaur (https://github.com/lzubiaur)
 *  
 *  If you are accidentally missing from this list, send me an e-mail
 *  (``sami.vaarala@iki.fi``) and I'll fix the omission.
 */

#ifndef DUKTAPE_H_INCLUDED
#define DUKTAPE_H_INCLUDED

#if     __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define DUK_GNUC_PRINTF( format_idx, arg_idx )  \
  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#define DUK_GNUC_SCANF( format_idx, arg_idx )   \
  __attribute__((__format__ (__scanf__, format_idx, arg_idx)))
#define DUK_GNUC_FORMAT( arg_idx )              \
  __attribute__((__format_arg__ (arg_idx)))
#define DUK_GNUC_NORETURN                       \
  __attribute__((__noreturn__))
#define DUK_GNUC_CONST                          \
  __attribute__((__const__))
#define DUK_GNUC_UNUSED                         \
  __attribute__((__unused__))
#define DUK_GNUC_NO_INSTRUMENT                  \
  __attribute__((__no_instrument_function__))
#else   /* !__GNUC__ */
#define DUK_GNUC_PRINTF( format_idx, arg_idx )
#define DUK_GNUC_SCANF( format_idx, arg_idx )
#define DUK_GNUC_FORMAT( arg_idx )
#define DUK_GNUC_NORETURN
#define DUK_GNUC_CONST
#define DUK_GNUC_UNUSED
#define DUK_GNUC_NO_INSTRUMENT
#endif  /* !__GNUC__ */

#define DUK_SINGLE_FILE

/* External duk_config.h provides platform/compiler/OS dependent
 * typedefs and macros, and DUK_USE_xxx config options so that
 * the rest of Duktape doesn't need to do any feature detection.
 */
/*==============================================================*/
/* --- duk_config.h */
/*
 *  duk_config.h configuration header generated by genconfig.py.
 *
 *  Git commit: cad6f595382a0cc1a7e4207794ade5be11b3e397
 *  Git describe: v1.4.0
 *  Git branch: master
 *
 *  Supported platforms:
 *      - Mac OSX, iPhone, Darwin
 *      - OpenBSD
 *      - Generic BSD
 *      - Atari ST TOS
 *      - AmigaOS
 *      - Windows
 *      - Flashplayer (Crossbridge)
 *      - QNX
 *      - TI-Nspire
 *      - Emscripten
 *      - Linux
 *      - Solaris
 *      - Generic POSIX
 *      - Cygwin
 *      - Generic UNIX
 *      - Generic fallback
 *
 *  Supported architectures:
 *      - x86
 *      - x64
 *      - x32
 *      - ARM 32-bit
 *      - ARM 64-bit
 *      - MIPS 32-bit
 *      - MIPS 64-bit
 *      - PowerPC 32-bit
 *      - PowerPC 64-bit
 *      - SPARC 32-bit
 *      - SPARC 64-bit
 *      - SuperH
 *      - Motorola 68k
 *      - Emscripten
 *      - Generic
 *
 *  Supported compilers:
 *      - Clang
 *      - GCC
 *      - MSVC
 *      - Emscripten
 *      - TinyC
 *      - VBCC
 *      - Bruce's C compiler
 *      - Generic
 *
 */

#if !defined(DUK_CONFIG_H_INCLUDED)
#define DUK_CONFIG_H_INCLUDED

/*
 *  Intermediate helper defines
 */

/* DLL build detection */
#if defined(DUK_OPT_DLL_BUILD)
#define DUK_F_DLL_BUILD
#elif defined(DUK_OPT_NO_DLL_BUILD)
#undef DUK_F_DLL_BUILD
#else
/* not configured for DLL build */
#undef DUK_F_DLL_BUILD
#endif

/* Apple OSX, iOS */
#if defined(__APPLE__)
#define DUK_F_APPLE
#endif

/* OpenBSD */
#if defined(__OpenBSD__) || defined(__OpenBSD)
#define DUK_F_OPENBSD
#endif

/* NetBSD */
#if defined(__NetBSD__) || defined(__NetBSD)
#define DUK_F_NETBSD
#endif

/* FreeBSD */
#if defined(__FreeBSD__) || defined(__FreeBSD)
#define DUK_F_FREEBSD
#endif

/* BSD variant */
#if defined(DUK_F_FREEBSD) || defined(DUK_F_NETBSD) || defined(DUK_F_OPENBSD) || \
    defined(__bsdi__) || defined(__DragonFly__)
#define DUK_F_BSD
#endif

/* Atari ST TOS.  __TOS__ defined by PureC.  No platform define in VBCC
 * apparently, so to use with VBCC user must define __TOS__ manually.
  */
#if defined(__TOS__)
#define DUK_F_TOS
#endif

/* Motorola 68K.  Not defined by VBCC, so user must define one of these
 * manually when using VBCC.
 */
#if defined(__m68k__) || defined(M68000) || defined(__MC68K__)
#define DUK_F_M68K
#endif

/* AmigaOS.  Neither AMIGA nor __amigaos__ is defined on VBCC, so user must
 * define 'AMIGA' manually when using VBCC.
 */
#if defined(AMIGA) || defined(__amigaos__)
#define DUK_F_AMIGAOS
#endif

/* PowerPC */
#if defined(__powerpc) || defined(__powerpc__) || defined(__PPC__)
#define DUK_F_PPC
#if defined(__PPC64__) || defined(__LP64__) || defined(_LP64)
#define DUK_F_PPC64
#else
#define DUK_F_PPC32
#endif
#endif

/* Windows, both 32-bit and 64-bit */
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || \
    defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
#define DUK_F_WINDOWS
#if defined(_WIN64) || defined(WIN64)
#define DUK_F_WIN64
#else
#define DUK_F_WIN32
#endif
#endif

/* Flash player (e.g. Crossbridge) */
#if defined(__FLASHPLAYER__)
#define DUK_F_FLASHPLAYER
#endif

/* QNX */
#if defined(__QNX__)
#define DUK_F_QNX
#endif

/* TI-Nspire (using Ndless) */
#if defined(_TINSPIRE)
#define DUK_F_TINSPIRE
#endif

/* Emscripten (provided explicitly by user), improve if possible */
#if defined(EMSCRIPTEN)
#define DUK_F_EMSCRIPTEN
#endif

/* BCC (Bruce's C compiler): this is a "torture target" for compilation */
#if defined(__BCC__) || defined(__BCC_VERSION__)
#define DUK_F_BCC
#endif

/* Linux */
#if defined(__linux) || defined(__linux__) || defined(linux)
#define DUK_F_LINUX
#endif

/* illumos / Solaris */
#if defined(__sun) && defined(__SVR4)
#define DUK_F_SUN
#endif

/* POSIX */
#if defined(__posix)
#define DUK_F_POSIX
#endif

/* Cygwin */
#if defined(__CYGWIN__)
#define DUK_F_CYGWIN
#endif

/* Generic Unix (includes Cygwin) */
#if defined(__unix) || defined(__unix__) || defined(unix) || \
    defined(DUK_F_LINUX) || defined(DUK_F_BSD)
#define DUK_F_UNIX
#endif

/* stdint.h not available */
#if defined(DUK_F_WINDOWS) && defined(_MSC_VER)
#if (_MSC_VER < 1700)
/* VS2012+ has stdint.h, < VS2012 does not (but it's available for download). */
#define DUK_F_NO_STDINT_H
#endif
#endif
#if !defined(DUK_F_NO_STDINT_H) && (defined(DUK_F_TOS) || defined(DUK_F_BCC))
#define DUK_F_NO_STDINT_H
#endif

/* C++ */
#undef DUK_F_CPP
#if defined(__cplusplus)
#define DUK_F_CPP
#endif

/* Intel x86 (32-bit), x64 (64-bit) or x32 (64-bit but 32-bit pointers),
 * define only one of DUK_F_X86, DUK_F_X64, DUK_F_X32.
 * https://sites.google.com/site/x32abi/
 */
#if defined(__amd64__) || defined(__amd64) || \
    defined(__x86_64__) || defined(__x86_64) || \
    defined(_M_X64) || defined(_M_AMD64)
#if defined(__ILP32__) || defined(_ILP32)
#define DUK_F_X32
#else
#define DUK_F_X64
#endif
#elif defined(i386) || defined(__i386) || defined(__i386__) || \
      defined(__i486__) || defined(__i586__) || defined(__i686__) || \
      defined(__IA32__) || defined(_M_IX86) || defined(__X86__) || \
      defined(_X86_) || defined(__THW_INTEL__) || defined(__I86__)
#if defined(__LP64__) || defined(_LP64)
/* This should not really happen, but would indicate x64. */
#define DUK_F_X64
#else
#define DUK_F_X86
#endif
#endif

/* ARM */
#if defined(__arm__) || defined(__thumb__) || defined(_ARM) || defined(_M_ARM)
#define DUK_F_ARM
#if defined(__LP64__) || defined(_LP64) || defined(__arm64) || defined(__arm64__)
#define DUK_F_ARM64
#else
#define DUK_F_ARM32
#endif
#endif

/* MIPS.  Related defines: __MIPSEB__, __MIPSEL__, __mips_isa_rev, __LP64__ */
#if defined(__mips__) || defined(mips) || defined(_MIPS_ISA) || \
    defined(_R3000) || defined(_R4000) || defined(_R5900) || \
    defined(_MIPS_ISA_MIPS1) || defined(_MIPS_ISA_MIPS2) || \
    defined(_MIPS_ISA_MIPS3) || defined(_MIPS_ISA_MIPS4) || \
    defined(__mips) || defined(__MIPS__)
#define DUK_F_MIPS
#if defined(__LP64__) || defined(_LP64) || defined(__mips64) || \
    defined(__mips64__) || defined(__mips_n64)
#define DUK_F_MIPS64
#else
#define DUK_F_MIPS32
#endif
#endif

/* SPARC */
#if defined(sparc) || defined(__sparc) || defined(__sparc__)
#define DUK_F_SPARC
#if defined(__LP64__) || defined(_LP64)
#define DUK_F_SPARC64
#else
#define DUK_F_SPARC32
#endif
#endif

/* SuperH */
#if defined(__sh__) || \
    defined(__sh1__) || defined(__SH1__) || \
    defined(__sh2__) || defined(__SH2__) || \
    defined(__sh3__) || defined(__SH3__) || \
    defined(__sh4__) || defined(__SH4__) || \
    defined(__sh5__) || defined(__SH5__)
#define DUK_F_SUPERH
#endif

/* Clang */
#if defined(__clang__)
#define DUK_F_CLANG
#endif

/* C99 or above */
#undef DUK_F_C99
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define DUK_F_C99
#endif

/* C++11 or above */
#undef DUK_F_CPP11
#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define DUK_F_CPP11
#endif

/* GCC.  Clang also defines __GNUC__ so don't detect GCC if using Clang. */
#if defined(__GNUC__) && !defined(__clang__) && !defined(DUK_F_CLANG)
#define DUK_F_GCC
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
/* Convenience, e.g. gcc 4.5.1 == 40501; http://stackoverflow.com/questions/6031819/emulating-gccs-builtin-unreachable */
#define DUK_F_GCC_VERSION  (__GNUC__ * 10000L + __GNUC_MINOR__ * 100L + __GNUC_PATCHLEVEL__)
#else
#error cannot figure out gcc version
#endif
#endif

/* MinGW.  Also GCC flags (DUK_F_GCC) are enabled now. */
#if defined(__MINGW32__) || defined(__MINGW64__)
#define DUK_F_MINGW
#endif

/* MSVC */
#if defined(_MSC_VER)
/* MSVC preprocessor defines: http://msdn.microsoft.com/en-us/library/b0084kay.aspx
 * _MSC_FULL_VER includes the build number, but it has at least two formats, see e.g.
 * BOOST_MSVC_FULL_VER in http://www.boost.org/doc/libs/1_52_0/boost/config/compiler/visualc.hpp
 */
#define DUK_F_MSVC
#if defined(_MSC_FULL_VER)
#if (_MSC_FULL_VER > 100000000)
#define DUK_F_MSVC_FULL_VER _MSC_FULL_VER
#else
#define DUK_F_MSCV_FULL_VER (_MSC_FULL_VER * 10)
#endif
#endif
#endif  /* _MSC_VER */

/* TinyC */
#if defined(__TINYC__)
/* http://bellard.org/tcc/tcc-doc.html#SEC9 */
#define DUK_F_TINYC
#endif

/* VBCC */
#if defined(__VBCC__)
#define DUK_F_VBCC
#endif

/*
 *  Platform autodetection
 */

/* Workaround for older C++ compilers before including <inttypes.h>,
 * see e.g.: https://sourceware.org/bugzilla/show_bug.cgi?id=15366
 */
#if defined(__cplusplus) && !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS
#endif
#if defined(__cplusplus) && !defined(__STDC_CONSTANT_MACROS)
#define __STDC_CONSTANT_MACROS
#endif

#if defined(DUK_F_APPLE)
/* --- Mac OSX, iPhone, Darwin --- */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <TargetConditionals.h>
#include <architecture/byte_order.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

/* http://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor */
#if TARGET_IPHONE_SIMULATOR
#define DUK_USE_OS_STRING "iphone-sim"
#elif TARGET_OS_IPHONE
#define DUK_USE_OS_STRING "iphone"
#elif TARGET_OS_MAC
#define DUK_USE_OS_STRING "ios"
#else
#define DUK_USE_OS_STRING "ios-unknown"
#endif

/* Use _setjmp() on Apple by default, see GH-55. */
#define DUK_USE_UNDERSCORE_SETJMP
#define DUK_SETJMP(jb)        _setjmp((jb))
#define DUK_LONGJMP(jb)       _longjmp((jb), 1)
#elif defined(DUK_F_OPENBSD)
/* --- OpenBSD --- */
/* http://www.monkey.org/openbsd/archive/ports/0401/msg00089.html */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <sys/types.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_OS_STRING  "openbsd"
#elif defined(DUK_F_BSD)
/* --- Generic BSD --- */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <sys/types.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_OS_STRING  "bsd"
#elif defined(DUK_F_TOS)
/* --- Atari ST TOS --- */
#define DUK_USE_DATE_NOW_TIME
#define DUK_USE_DATE_TZO_GMTIME
/* no parsing (not an error) */
#define DUK_USE_DATE_FMT_STRFTIME
#include <time.h>

#define DUK_USE_OS_STRING  "tos"

/* TOS on M68K is always big endian. */
#if !defined(DUK_USE_BYTEORDER) && defined(DUK_F_M68K)
#define DUK_USE_BYTEORDER 3
#endif
#elif defined(DUK_F_AMIGAOS)
/* --- AmigaOS --- */
#if defined(DUK_F_M68K)
/* AmigaOS on M68k */
#define DUK_USE_DATE_NOW_TIME
#define DUK_USE_DATE_TZO_GMTIME
/* no parsing (not an error) */
#define DUK_USE_DATE_FMT_STRFTIME
#include <time.h>
#elif defined(DUK_F_PPC)
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <time.h>
#ifndef UINTPTR_MAX
#define UINTPTR_MAX UINT_MAX
#endif
#else
#error AmigaOS but not M68K/PPC, not supported now
#endif

#define DUK_USE_OS_STRING "amigaos"

/* AmigaOS on M68K or PPC is always big endian. */
#if !defined(DUK_USE_BYTEORDER) && (defined(DUK_F_M68K) || defined(DUK_F_PPC))
#define DUK_USE_BYTEORDER 3
#endif
#elif defined(DUK_F_WINDOWS)
/* --- Windows --- */
/* Initial fix: disable secure CRT related warnings when compiling Duktape
 * itself (must be defined before including Windows headers).  Don't define
 * for user code including duktape.h.
 */
#if defined(DUK_COMPILING_DUKTAPE) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Windows 32-bit and 64-bit are currently the same. */
/* MSVC does not have sys/param.h */
#define DUK_USE_DATE_NOW_WINDOWS
#define DUK_USE_DATE_TZO_WINDOWS
/* Note: PRS and FMT are intentionally left undefined for now.  This means
 * there is no platform specific date parsing/formatting but there is still
 * the ISO 8601 standard format.
 */
#if defined(DUK_COMPILING_DUKTAPE)
/* Only include when compiling Duktape to avoid polluting application build
 * with a lot of unnecessary defines.
 */
#include <windows.h>
#endif

#define DUK_USE_OS_STRING "windows"

/* On Windows, assume we're little endian.  Even Itanium which has a
 * configurable endianness runs little endian in Windows.
 */
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 1
#endif
#elif defined(DUK_F_FLASHPLAYER)
/* --- Flashplayer (Crossbridge) --- */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <endian.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_OS_STRING "flashplayer"

#if !defined(DUK_USE_BYTEORDER) && defined(DUK_F_FLASHPLAYER)
#define DUK_USE_BYTEORDER 1
#endif
#elif defined(DUK_F_QNX)
/* --- QNX --- */
#if defined(DUK_F_QNX) && defined(DUK_COMPILING_DUKTAPE)
/* See: /opt/qnx650/target/qnx6/usr/include/sys/platform.h */
#define _XOPEN_SOURCE    600
#define _POSIX_C_SOURCE  200112L
#endif

#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_OS_STRING "qnx"
#elif defined(DUK_F_TINSPIRE)
/* --- TI-Nspire --- */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_OS_STRING "tinspire"
#elif defined(DUK_F_EMSCRIPTEN)
/* --- Emscripten --- */
#if defined(DUK_COMPILING_DUKTAPE)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE  200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE      /* e.g. getdate_r */
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE    /* e.g. strptime */
#endif
#endif  /* DUK_COMPILING_DUKTAPE */

#include <sys/types.h>
#if defined(DUK_F_BCC)
/* no endian.h */
#else
#include <endian.h>
#endif  /* DUK_F_BCC */
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME

#define DUK_USE_OS_STRING "emscripten"
#elif defined(DUK_F_LINUX)
/* --- Linux --- */
#if defined(DUK_COMPILING_DUKTAPE)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE  200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE      /* e.g. getdate_r */
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE    /* e.g. strptime */
#endif
#endif  /* DUK_COMPILING_DUKTAPE */

#include <sys/types.h>
#if defined(DUK_F_BCC)
/* no endian.h or stdint.h */
#else
#include <endian.h>
#include <stdint.h>
#endif  /* DUK_F_BCC */
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME

#define DUK_USE_OS_STRING "linux"
#elif defined(DUK_F_SUN)
/* --- Solaris --- */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME

#include <sys/types.h>
#include <ast/endian.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_OS_STRING "solaris"
#elif defined(DUK_F_POSIX)
/* --- Generic POSIX --- */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <sys/types.h>
#include <endian.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_OS_STRING "posix"
#elif defined(DUK_F_CYGWIN)
/* --- Cygwin --- */
/* don't use strptime() for now */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_FMT_STRFTIME
#include <sys/types.h>
#include <endian.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define DUK_USE_UNDERSCORE_SETJMP
#define DUK_SETJMP(jb)        _setjmp((jb))
#define DUK_LONGJMP(jb)       _longjmp((jb), 1)

#define DUK_USE_OS_STRING "windows"
#elif defined(DUK_F_UNIX)
/* --- Generic UNIX --- */
#define DUK_USE_DATE_NOW_GETTIMEOFDAY
#define DUK_USE_DATE_TZO_GMTIME_R
#define DUK_USE_DATE_PRS_STRPTIME
#define DUK_USE_DATE_FMT_STRFTIME
#include <time.h>
#include <sys/time.h>
#define DUK_USE_OS_STRING "unknown"
#else
/* --- Generic fallback --- */
/* The most portable current time provider is time(), but it only has a
 * one second resolution.
 */
#define DUK_USE_DATE_NOW_TIME

/* The most portable way to figure out local time offset is gmtime(),
 * but it's not thread safe so use with caution.
 */
#define DUK_USE_DATE_TZO_GMTIME

/* Avoid custom date parsing and formatting for portability. */
#undef DUK_USE_DATE_PRS_STRPTIME
#undef DUK_USE_DATE_FMT_STRFTIME

/* Rely on C89 headers only; time.h must be here. */
#include <time.h>

#define DUK_USE_OS_STRING "unknown"
#endif  /* autodetect platform */

/* Shared includes: C89 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  /* varargs */
#include <setjmp.h>
#include <stddef.h>  /* e.g. ptrdiff_t */
#include <math.h>
#include <limits.h>

/* date.h is omitted, and included per platform */

/* Shared includes: stdint.h is C99 */
#if defined(DUK_F_NO_STDINT_H)
/* stdint.h not available */
#else
/* Technically C99 (C++11) but found in many systems.  On some systems
 * __STDC_LIMIT_MACROS and __STDC_CONSTANT_MACROS must be defined before
 * including stdint.h (see above).
 */
#include <stdint.h>
#endif

#if defined(DUK_F_CPP)
#include <exception>  /* std::exception */
#endif

/*
 *  Architecture autodetection
 */

#if defined(DUK_F_X86)
/* --- x86 --- */
#define DUK_USE_ARCH_STRING "x86"
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 1
#endif
/* XXX: This is technically not guaranteed because it's possible to configure
 * an x86 to require aligned accesses with Alignment Check (AC) flag.
 */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 1
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_X64)
/* --- x64 --- */
#define DUK_USE_ARCH_STRING "x64"
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 1
#endif
/* XXX: This is technically not guaranteed because it's possible to configure
 * an x86 to require aligned accesses with Alignment Check (AC) flag.
 */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 1
#endif
#undef DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_X32)
/* --- x32 --- */
#define DUK_USE_ARCH_STRING "x32"
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 1
#endif
/* XXX: This is technically not guaranteed because it's possible to configure
 * an x86 to require aligned accesses with Alignment Check (AC) flag.
 */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 1
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_ARM32)
/* --- ARM 32-bit --- */
#define DUK_USE_ARCH_STRING "arm32"
/* Byte order varies, so rely on autodetect. */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 4
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_ARM64)
/* --- ARM 64-bit --- */
#define DUK_USE_ARCH_STRING "arm64"
/* Byte order varies, so rely on autodetect. */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#undef DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_MIPS32)
/* --- MIPS 32-bit --- */
#define DUK_USE_ARCH_STRING "mips32"
/* MIPS byte order varies so rely on autodetection. */
/* Based on 'make checkalign' there are no alignment requirements on
 * Linux MIPS except for doubles, which need align by 4.  Alignment
 * requirements vary based on target though.
 */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 4
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_MIPS64)
/* --- MIPS 64-bit --- */
#define DUK_USE_ARCH_STRING "mips64"
/* MIPS byte order varies so rely on autodetection. */
/* Good default is a bit arbitrary because alignment requirements
 * depend on target.  See https://github.com/svaarala/duktape/issues/102.
 */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#undef DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_PPC32)
/* --- PowerPC 32-bit --- */
#define DUK_USE_ARCH_STRING "ppc32"
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 3
#endif
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_PPC64)
/* --- PowerPC 64-bit --- */
#define DUK_USE_ARCH_STRING "ppc64"
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 3
#endif
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#undef DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_SPARC32)
/* --- SPARC 32-bit --- */
#define DUK_USE_ARCH_STRING "sparc32"
/* SPARC byte order varies so rely on autodetection. */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_SPARC64)
/* --- SPARC 64-bit --- */
#define DUK_USE_ARCH_STRING "sparc64"
/* SPARC byte order varies so rely on autodetection. */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#undef DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_SUPERH)
/* --- SuperH --- */
#define DUK_USE_ARCH_STRING "sh"
/* Byte order varies, rely on autodetection. */
/* Based on 'make checkalign' there are no alignment requirements on
 * Linux SH4, but align by 4 is probably a good basic default.
 */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 4
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_M68K)
/* --- Motorola 68k --- */
#define DUK_USE_ARCH_STRING "m68k"
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 3
#endif
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#define DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#elif defined(DUK_F_EMSCRIPTEN)
/* --- Emscripten --- */
#define DUK_USE_ARCH_STRING "emscripten"
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 1
#endif
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif
#undef DUK_USE_PACKED_TVAL
#define DUK_F_PACKED_TVAL_PROVIDED
#else
/* --- Generic --- */
/* These are necessary wild guesses. */
#define DUK_USE_ARCH_STRING "generic"
/* Rely on autodetection for byte order, alignment, and packed tval. */
#endif  /* autodetect architecture */

/*
 *  Compiler autodetection
 */

#if defined(DUK_F_CLANG)
/* --- Clang --- */
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
/* C99 / C++11 and above: rely on va_copy() which is required. */
#define DUK_VA_COPY(dest,src) va_copy(dest,src)
#else
/* Clang: assume we have __va_copy() in non-C99 mode. */
#define DUK_VA_COPY(dest,src) __va_copy(dest,src)
#endif

#define DUK_NORETURN(decl)  decl __attribute__((noreturn))

#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_unreachable)
#define DUK_UNREACHABLE()  do { __builtin_unreachable(); } while (0)
#endif
#endif

#define DUK_USE_BRANCH_HINTS
#define DUK_LIKELY(x)    __builtin_expect((x), 1)
#define DUK_UNLIKELY(x)  __builtin_expect((x), 0)

#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_NOINLINE        __attribute__((noinline))
#define DUK_INLINE          inline
#define DUK_ALWAYS_INLINE   inline __attribute__((always_inline))
#endif

#if defined(DUK_F_DLL_BUILD) && defined(DUK_F_WINDOWS)
/* MSVC dllexport/dllimport: appropriate __declspec depends on whether we're
 * compiling Duktape or the application.
 */
#if defined(DUK_COMPILING_DUKTAPE)
#define DUK_EXTERNAL_DECL  extern __declspec(dllexport)
#define DUK_EXTERNAL       __declspec(dllexport)
#else
#define DUK_EXTERNAL_DECL  extern __declspec(dllimport)
#define DUK_EXTERNAL       should_not_happen
#endif
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL_DECL  static
#define DUK_INTERNAL       static
#else
#define DUK_INTERNAL_DECL  extern
#define DUK_INTERNAL       /*empty*/
#endif
#define DUK_LOCAL_DECL     static
#define DUK_LOCAL          static

#else
#define DUK_EXTERNAL_DECL  __attribute__ ((visibility("default"))) extern
#define DUK_EXTERNAL       __attribute__ ((visibility("default")))
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL_DECL  static
#define DUK_INTERNAL       static
#else
#define DUK_INTERNAL_DECL  __attribute__ ((visibility("hidden"))) extern
#define DUK_INTERNAL       __attribute__ ((visibility("hidden")))
#endif
#define DUK_LOCAL_DECL     static
#define DUK_LOCAL          static
#endif

#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "clang"
#else
#define DUK_USE_COMPILER_STRING "clang"
#endif

#undef DUK_USE_VARIADIC_MACROS
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_USE_VARIADIC_MACROS
#endif

#undef DUK_USE_FLEX_C99
#undef DUK_USE_FLEX_ZEROSIZE
#undef DUK_USE_FLEX_ONESIZE
#if defined(DUK_F_C99)
#define DUK_USE_FLEX_C99
#else
#define DUK_USE_FLEX_ZEROSIZE
#endif

#undef DUK_USE_GCC_PRAGMAS
#define DUK_USE_PACK_CLANG_ATTR
#define DUK_F_VARIADIC_MACROS_PROVIDED
#elif defined(DUK_F_GCC)
/* --- GCC --- */
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
/* C99 / C++11 and above: rely on va_copy() which is required. */
#define DUK_VA_COPY(dest,src) va_copy(dest,src)
#else
/* GCC: assume we have __va_copy() in non-C99 mode. */
#define DUK_VA_COPY(dest,src) __va_copy(dest,src)
#endif

#if defined(DUK_F_GCC_VERSION) && (DUK_F_GCC_VERSION >= 20500L)
/* since gcc-2.5 */
#define DUK_NORETURN(decl)  decl __attribute__((noreturn))
#endif

#if defined(DUK_F_GCC_VERSION) && (DUK_F_GCC_VERSION >= 40500L)
/* since gcc-4.5 */
#define DUK_UNREACHABLE()  do { __builtin_unreachable(); } while (0)
#endif

#define DUK_USE_BRANCH_HINTS
#if defined(DUK_F_GCC_VERSION) && (DUK_F_GCC_VERSION >= 40500L)
/* GCC: test not very accurate; enable only in relatively recent builds
 * because of bugs in gcc-4.4 (http://lists.debian.org/debian-gcc/2010/04/msg00000.html)
 */
#define DUK_LIKELY(x)    __builtin_expect((x), 1)
#define DUK_UNLIKELY(x)  __builtin_expect((x), 0)
#endif

#if (defined(DUK_F_C99) || defined(DUK_F_CPP11)) && \
    defined(DUK_F_GCC_VERSION) && (DUK_F_GCC_VERSION >= 30101)
#define DUK_NOINLINE        __attribute__((noinline))
#define DUK_INLINE          inline
#define DUK_ALWAYS_INLINE   inline __attribute__((always_inline))
#endif

#if defined(DUK_F_DLL_BUILD) && defined(DUK_F_WINDOWS)
/* MSVC dllexport/dllimport: appropriate __declspec depends on whether we're
 * compiling Duktape or the application.
 */
#if defined(DUK_COMPILING_DUKTAPE)
#define DUK_EXTERNAL_DECL  extern __declspec(dllexport)
#define DUK_EXTERNAL       __declspec(dllexport)
#else
#define DUK_EXTERNAL_DECL  extern __declspec(dllimport)
#define DUK_EXTERNAL       should_not_happen
#endif
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL_DECL  static
#define DUK_INTERNAL       static
#else
#define DUK_INTERNAL_DECL  extern
#define DUK_INTERNAL       /*empty*/
#endif
#define DUK_LOCAL_DECL     static
#define DUK_LOCAL          static

#elif defined(DUK_F_GCC_VERSION) && (DUK_F_GCC_VERSION >= 40000)
#define DUK_EXTERNAL_DECL  __attribute__ ((visibility("default"))) extern
#define DUK_EXTERNAL       __attribute__ ((visibility("default")))
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL_DECL  static
#define DUK_INTERNAL       static
#else
#define DUK_INTERNAL_DECL  __attribute__ ((visibility("hidden"))) extern
#define DUK_INTERNAL       __attribute__ ((visibility("hidden")))
#endif
#define DUK_LOCAL_DECL     static
#define DUK_LOCAL          static
#endif

#if defined(DUK_F_MINGW)
#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "mingw++"
#else
#define DUK_USE_COMPILER_STRING "mingw"
#endif
#else
#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "g++"
#else
#define DUK_USE_COMPILER_STRING "gcc"
#endif
#endif

#undef DUK_USE_VARIADIC_MACROS
#if defined(DUK_F_C99) || (defined(DUK_F_CPP11) && defined(__GNUC__))
#define DUK_USE_VARIADIC_MACROS
#endif

#undef DUK_USE_FLEX_C99
#undef DUK_USE_FLEX_ZEROSIZE
#undef DUK_USE_FLEX_ONESIZE
#if defined(DUK_F_C99)
#define DUK_USE_FLEX_C99
#else
#define DUK_USE_FLEX_ZEROSIZE
#endif

#if defined(DUK_F_GCC_VERSION) && (DUK_F_GCC_VERSION >= 40600)
#define DUK_USE_GCC_PRAGMAS
#else
#undef DUK_USE_GCC_PRAGMAS
#endif

#define DUK_USE_PACK_GCC_ATTR
#define DUK_F_VARIADIC_MACROS_PROVIDED
#elif defined(DUK_F_MSVC)
/* --- MSVC --- */
/* http://msdn.microsoft.com/en-us/library/aa235362(VS.60).aspx */
#define DUK_NORETURN(decl)  __declspec(noreturn) decl

/* XXX: DUK_UNREACHABLE for msvc? */

#undef DUK_USE_BRANCH_HINTS

/* XXX: DUK_LIKELY, DUK_UNLIKELY for msvc? */
/* XXX: DUK_NOINLINE, DUK_INLINE, DUK_ALWAYS_INLINE for msvc? */

#if defined(DUK_F_DLL_BUILD) && defined(DUK_F_WINDOWS)
/* MSVC dllexport/dllimport: appropriate __declspec depends on whether we're
 * compiling Duktape or the application.
 */
#if defined(DUK_COMPILING_DUKTAPE)
#define DUK_EXTERNAL_DECL  extern __declspec(dllexport)
#define DUK_EXTERNAL       __declspec(dllexport)
#else
#define DUK_EXTERNAL_DECL  extern __declspec(dllimport)
#define DUK_EXTERNAL       should_not_happen
#endif
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL_DECL  static
#define DUK_INTERNAL       static
#else
#define DUK_INTERNAL_DECL  extern
#define DUK_INTERNAL       /*empty*/
#endif
#define DUK_LOCAL_DECL     static
#define DUK_LOCAL          static

#endif

#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "msvc++"
#else
#define DUK_USE_COMPILER_STRING "msvc"
#endif

#undef DUK_USE_VARIADIC_MACROS
#if defined(DUK_F_C99)
#define DUK_USE_VARIADIC_MACROS
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
/* VS2005+ should have variadic macros even when they're not C99. */
#define DUK_USE_VARIADIC_MACROS
#endif

#undef DUK_USE_FLEX_C99
#undef DUK_USE_FLEX_ZEROSIZE
#undef DUK_USE_FLEX_ONESIZE
#if defined(DUK_F_C99)
#define DUK_USE_FLEX_C99
#else
#define DUK_USE_FLEX_ZEROSIZE
#endif

#undef DUK_USE_GCC_PRAGMAS

#define DUK_USE_PACK_MSVC_PRAGMA

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#define DUK_SNPRINTF     snprintf
#define DUK_VSNPRINTF    vsnprintf
#else
/* (v)snprintf() is missing before MSVC 2015.  Note that _(v)snprintf() does
 * NOT NUL terminate on truncation, but Duktape code never assumes that.
 * http://stackoverflow.com/questions/2915672/snprintf-and-visual-studio-2010
 */
#define DUK_SNPRINTF     _snprintf
#define DUK_VSNPRINTF    _vsnprintf
#endif
#define DUK_F_VARIADIC_MACROS_PROVIDED
#elif defined(DUK_F_EMSCRIPTEN)
/* --- Emscripten --- */
#define DUK_NORETURN(decl)  decl __attribute__((noreturn))

#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_unreachable)
#define DUK_UNREACHABLE()  do { __builtin_unreachable(); } while (0)
#endif
#endif

#define DUK_USE_BRANCH_HINTS
#define DUK_LIKELY(x)    __builtin_expect((x), 1)
#define DUK_UNLIKELY(x)  __builtin_expect((x), 0)

#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_NOINLINE        __attribute__((noinline))
#define DUK_INLINE          inline
#define DUK_ALWAYS_INLINE   inline __attribute__((always_inline))
#endif

#define DUK_EXTERNAL_DECL  __attribute__ ((visibility("default"))) extern
#define DUK_EXTERNAL       __attribute__ ((visibility("default")))
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL_DECL  static
#define DUK_INTERNAL       static
#else
#define DUK_INTERNAL_DECL  __attribute__ ((visibility("hidden"))) extern
#define DUK_INTERNAL       __attribute__ ((visibility("hidden")))
#endif
#define DUK_LOCAL_DECL     static
#define DUK_LOCAL          static

#define DUK_USE_COMPILER_STRING "emscripten"

#undef DUK_USE_VARIADIC_MACROS
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_USE_VARIADIC_MACROS
#endif

#undef DUK_USE_FLEX_C99
#undef DUK_USE_FLEX_ZEROSIZE
#undef DUK_USE_FLEX_ONESIZE
#if defined(DUK_F_C99)
#define DUK_USE_FLEX_C99
#else
#define DUK_USE_FLEX_ZEROSIZE
#endif

#undef DUK_USE_GCC_PRAGMAS
#define DUK_USE_PACK_CLANG_ATTR
#define DUK_F_VARIADIC_MACROS_PROVIDED
#elif defined(DUK_F_TINYC)
/* --- TinyC --- */
#undef DUK_USE_BRANCH_HINTS

#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "tinyc++"
#else
#define DUK_USE_COMPILER_STRING "tinyc"
#endif

/* http://bellard.org/tcc/tcc-doc.html#SEC7 */
#define DUK_USE_VARIADIC_MACROS

/* Most portable, wastes space */
#define DUK_USE_FLEX_ONESIZE

/* Most portable, potentially wastes space */
#define DUK_USE_PACK_DUMMY_MEMBER
#define DUK_F_VARIADIC_MACROS_PROVIDED
#elif defined(DUK_F_VBCC)
/* --- VBCC --- */
#undef DUK_USE_BRANCH_HINTS

#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "vbcc-c++"
#else
#define DUK_USE_COMPILER_STRING "vbcc"
#endif

#undef DUK_USE_VARIADIC_MACROS
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_USE_VARIADIC_MACROS
#endif

#define DUK_USE_FLEX_ZEROSIZE
#define DUK_USE_PACK_DUMMY_MEMBER
#define DUK_F_VARIADIC_MACROS_PROVIDED
#elif defined(DUK_F_BCC)
/* --- Bruce's C compiler --- */
#undef DUK_USE_BRANCH_HINTS

#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "bcc++"
#else
#define DUK_USE_COMPILER_STRING "bcc"
#endif

/* XXX */
#undef DUK_USE_VARIADIC_MACROS

/* Most portable, wastes space */
#define DUK_USE_FLEX_ONESIZE

/* Most portable, potentially wastes space */
#define DUK_USE_PACK_DUMMY_MEMBER

/* BCC, assume we're on x86. */
#if !defined(DUK_USE_BYTEORDER)
#define DUK_USE_BYTEORDER 1
#endif
#define DUK_F_VARIADIC_MACROS_PROVIDED
#else
/* --- Generic --- */
#undef DUK_USE_BRANCH_HINTS

#if defined(DUK_F_CPP)
#define DUK_USE_COMPILER_STRING "generic-c++"
#else
#define DUK_USE_COMPILER_STRING "generic"
#endif

#undef DUK_USE_VARIADIC_MACROS
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_USE_VARIADIC_MACROS
#endif

/* Most portable, wastes space */
#define DUK_USE_FLEX_ONESIZE

/* Most portable, potentially wastes space */
#define DUK_USE_PACK_DUMMY_MEMBER
#define DUK_F_VARIADIC_MACROS_PROVIDED
#endif  /* autodetect compiler */

/* uclibc */
#if defined(__UCLIBC__)
#define DUK_F_UCLIBC
#endif

/*
 *  Wrapper typedefs and constants for integer types, also sanity check types.
 *
 *  C99 typedefs are quite good but not always available, and we want to avoid
 *  forcibly redefining the C99 typedefs.  So, there are Duktape wrappers for
 *  all C99 typedefs and Duktape code should only use these typedefs.  Type
 *  detection when C99 is not supported is best effort and may end up detecting
 *  some types incorrectly.
 *
 *  Pointer sizes are a portability problem: pointers to different types may
 *  have a different size and function pointers are very difficult to manage
 *  portably.
 *
 *  http://en.wikipedia.org/wiki/C_data_types#Fixed-width_integer_types
 *
 *  Note: there's an interesting corner case when trying to define minimum
 *  signed integer value constants which leads to the current workaround of
 *  defining e.g. -0x80000000 as (-0x7fffffffL - 1L).  See doc/code-issues.txt
 *  for a longer discussion.
 *
 *  Note: avoid typecasts and computations in macro integer constants as they
 *  can then no longer be used in macro relational expressions (such as
 *  #if DUK_SIZE_MAX < 0xffffffffUL).  There is internal code which relies on
 *  being able to compare DUK_SIZE_MAX against a limit.
 */

/* XXX: add feature options to force basic types from outside? */

#if !defined(INT_MAX)
#error INT_MAX not defined
#endif

/* Check that architecture is two's complement, standard C allows e.g.
 * INT_MIN to be -2**31+1 (instead of -2**31).
 */
#if defined(INT_MAX) && defined(INT_MIN)
#if INT_MAX != -(INT_MIN + 1)
#error platform does not seem complement of two
#endif
#else
#error cannot check complement of two
#endif

/* Pointer size determination based on __WORDSIZE or architecture when
 * that's not available.
 */
#if defined(DUK_F_X86) || defined(DUK_F_X32) || \
    defined(DUK_F_M68K) || defined(DUK_F_PPC32) || \
    defined(DUK_F_BCC) || \
    (defined(__WORDSIZE) && (__WORDSIZE == 32))
#define DUK_F_32BIT_PTRS
#elif defined(DUK_F_X64) || \
      (defined(__WORDSIZE) && (__WORDSIZE == 64))
#define DUK_F_64BIT_PTRS
#else
/* not sure, not needed with C99 anyway */
#endif

/* Intermediate define for 'have inttypes.h' */
#undef DUK_F_HAVE_INTTYPES
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && \
    !(defined(DUK_F_AMIGAOS) && defined(DUK_F_VBCC))
/* vbcc + AmigaOS has C99 but no inttypes.h */
#define DUK_F_HAVE_INTTYPES
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
/* C++11 apparently ratified stdint.h */
#define DUK_F_HAVE_INTTYPES
#endif

/* Basic integer typedefs and limits, preferably from inttypes.h, otherwise
 * through automatic detection.
 */
#if defined(DUK_F_HAVE_INTTYPES)
/* C99 or compatible */

#define DUK_F_HAVE_64BIT
#include <inttypes.h>

typedef uint8_t duk_uint8_t;
typedef int8_t duk_int8_t;
typedef uint16_t duk_uint16_t;
typedef int16_t duk_int16_t;
typedef uint32_t duk_uint32_t;
typedef int32_t duk_int32_t;
typedef uint64_t duk_uint64_t;
typedef int64_t duk_int64_t;
typedef uint_least8_t duk_uint_least8_t;
typedef int_least8_t duk_int_least8_t;
typedef uint_least16_t duk_uint_least16_t;
typedef int_least16_t duk_int_least16_t;
typedef uint_least32_t duk_uint_least32_t;
typedef int_least32_t duk_int_least32_t;
typedef uint_least64_t duk_uint_least64_t;
typedef int_least64_t duk_int_least64_t;
typedef uint_fast8_t duk_uint_fast8_t;
typedef int_fast8_t duk_int_fast8_t;
typedef uint_fast16_t duk_uint_fast16_t;
typedef int_fast16_t duk_int_fast16_t;
typedef uint_fast32_t duk_uint_fast32_t;
typedef int_fast32_t duk_int_fast32_t;
typedef uint_fast64_t duk_uint_fast64_t;
typedef int_fast64_t duk_int_fast64_t;
typedef uintptr_t duk_uintptr_t;
typedef intptr_t duk_intptr_t;
typedef uintmax_t duk_uintmax_t;
typedef intmax_t duk_intmax_t;

#define DUK_UINT8_MIN         0
#define DUK_UINT8_MAX         UINT8_MAX
#define DUK_INT8_MIN          INT8_MIN
#define DUK_INT8_MAX          INT8_MAX
#define DUK_UINT_LEAST8_MIN   0
#define DUK_UINT_LEAST8_MAX   UINT_LEAST8_MAX
#define DUK_INT_LEAST8_MIN    INT_LEAST8_MIN
#define DUK_INT_LEAST8_MAX    INT_LEAST8_MAX
#define DUK_UINT_FAST8_MIN    0
#define DUK_UINT_FAST8_MAX    UINT_FAST8_MAX
#define DUK_INT_FAST8_MIN     INT_FAST8_MIN
#define DUK_INT_FAST8_MAX     INT_FAST8_MAX
#define DUK_UINT16_MIN        0
#define DUK_UINT16_MAX        UINT16_MAX
#define DUK_INT16_MIN         INT16_MIN
#define DUK_INT16_MAX         INT16_MAX
#define DUK_UINT_LEAST16_MIN  0
#define DUK_UINT_LEAST16_MAX  UINT_LEAST16_MAX
#define DUK_INT_LEAST16_MIN   INT_LEAST16_MIN
#define DUK_INT_LEAST16_MAX   INT_LEAST16_MAX
#define DUK_UINT_FAST16_MIN   0
#define DUK_UINT_FAST16_MAX   UINT_FAST16_MAX
#define DUK_INT_FAST16_MIN    INT_FAST16_MIN
#define DUK_INT_FAST16_MAX    INT_FAST16_MAX
#define DUK_UINT32_MIN        0
#define DUK_UINT32_MAX        UINT32_MAX
#define DUK_INT32_MIN         INT32_MIN
#define DUK_INT32_MAX         INT32_MAX
#define DUK_UINT_LEAST32_MIN  0
#define DUK_UINT_LEAST32_MAX  UINT_LEAST32_MAX
#define DUK_INT_LEAST32_MIN   INT_LEAST32_MIN
#define DUK_INT_LEAST32_MAX   INT_LEAST32_MAX
#define DUK_UINT_FAST32_MIN   0
#define DUK_UINT_FAST32_MAX   UINT_FAST32_MAX
#define DUK_INT_FAST32_MIN    INT_FAST32_MIN
#define DUK_INT_FAST32_MAX    INT_FAST32_MAX
#define DUK_UINT64_MIN        0
#define DUK_UINT64_MAX        UINT64_MAX
#define DUK_INT64_MIN         INT64_MIN
#define DUK_INT64_MAX         INT64_MAX
#define DUK_UINT_LEAST64_MIN  0
#define DUK_UINT_LEAST64_MAX  UINT_LEAST64_MAX
#define DUK_INT_LEAST64_MIN   INT_LEAST64_MIN
#define DUK_INT_LEAST64_MAX   INT_LEAST64_MAX
#define DUK_UINT_FAST64_MIN   0
#define DUK_UINT_FAST64_MAX   UINT_FAST64_MAX
#define DUK_INT_FAST64_MIN    INT_FAST64_MIN
#define DUK_INT_FAST64_MAX    INT_FAST64_MAX

#define DUK_UINTPTR_MIN       0
#define DUK_UINTPTR_MAX       UINTPTR_MAX
#define DUK_INTPTR_MIN        INTPTR_MIN
#define DUK_INTPTR_MAX        INTPTR_MAX

#define DUK_UINTMAX_MIN       0
#define DUK_UINTMAX_MAX       UINTMAX_MAX
#define DUK_INTMAX_MIN        INTMAX_MIN
#define DUK_INTMAX_MAX        INTMAX_MAX

#define DUK_SIZE_MIN          0
#define DUK_SIZE_MAX          SIZE_MAX
#undef DUK_SIZE_MAX_COMPUTED

#else  /* C99 types */

/* When C99 types are not available, we use heuristic detection to get
 * the basic 8, 16, 32, and (possibly) 64 bit types.  The fast/least
 * types are then assumed to be exactly the same for now: these could
 * be improved per platform but C99 types are very often now available.
 * 64-bit types are not available on all platforms; this is OK at least
 * on 32-bit platforms.
 *
 * This detection code is necessarily a bit hacky and can provide typedefs
 * and defines that won't work correctly on some exotic platform.
 */

#if (defined(CHAR_BIT) && (CHAR_BIT == 8)) || \
    (defined(UCHAR_MAX) && (UCHAR_MAX == 255))
typedef unsigned char duk_uint8_t;
typedef signed char duk_int8_t;
#else
#error cannot detect 8-bit type
#endif

#if defined(USHRT_MAX) && (USHRT_MAX == 65535UL)
typedef unsigned short duk_uint16_t;
typedef signed short duk_int16_t;
#elif defined(UINT_MAX) && (UINT_MAX == 65535UL)
/* On some platforms int is 16-bit but long is 32-bit (e.g. PureC) */
typedef unsigned int duk_uint16_t;
typedef signed int duk_int16_t;
#else
#error cannot detect 16-bit type
#endif

#if defined(UINT_MAX) && (UINT_MAX == 4294967295UL)
typedef unsigned int duk_uint32_t;
typedef signed int duk_int32_t;
#elif defined(ULONG_MAX) && (ULONG_MAX == 4294967295UL)
/* On some platforms int is 16-bit but long is 32-bit (e.g. PureC) */
typedef unsigned long duk_uint32_t;
typedef signed long duk_int32_t;
#else
#error cannot detect 32-bit type
#endif

/* 64-bit type detection is a bit tricky.
 *
 * ULLONG_MAX is a standard define.  __LONG_LONG_MAX__ and __ULONG_LONG_MAX__
 * are used by at least GCC (even if system headers don't provide ULLONG_MAX).
 * Some GCC variants may provide __LONG_LONG_MAX__ but not __ULONG_LONG_MAX__.
 *
 * ULL / LL constants are rejected / warned about by some compilers, even if
 * the compiler has a 64-bit type and the compiler/system headers provide an
 * unsupported constant (ULL/LL)!  Try to avoid using ULL / LL constants.
 * As a side effect we can only check that e.g. ULONG_MAX is larger than 32
 * bits but can't be sure it is exactly 64 bits.  Self tests will catch such
 * cases.
 */
#undef DUK_F_HAVE_64BIT
#if !defined(DUK_F_HAVE_64BIT) && defined(ULONG_MAX)
#if (ULONG_MAX > 4294967295UL)
#define DUK_F_HAVE_64BIT
typedef unsigned long duk_uint64_t;
typedef signed long duk_int64_t;
#endif
#endif
#if !defined(DUK_F_HAVE_64BIT) && defined(ULLONG_MAX)
#if (ULLONG_MAX > 4294967295UL)
#define DUK_F_HAVE_64BIT
typedef unsigned long long duk_uint64_t;
typedef signed long long duk_int64_t;
#endif
#endif
#if !defined(DUK_F_HAVE_64BIT) && defined(__ULONG_LONG_MAX__)
#if (__ULONG_LONG_MAX__ > 4294967295UL)
#define DUK_F_HAVE_64BIT
typedef unsigned long long duk_uint64_t;
typedef signed long long duk_int64_t;
#endif
#endif
#if !defined(DUK_F_HAVE_64BIT) && defined(__LONG_LONG_MAX__)
#if (__LONG_LONG_MAX__ > 2147483647L)
#define DUK_F_HAVE_64BIT
typedef unsigned long long duk_uint64_t;
typedef signed long long duk_int64_t;
#endif
#endif
#if !defined(DUK_F_HAVE_64BIT) && \
    (defined(DUK_F_MINGW) || defined(DUK_F_MSVC))
/* Both MinGW and MSVC have a 64-bit type. */
#define DUK_F_HAVE_64BIT
typedef unsigned long duk_uint64_t;
typedef signed long duk_int64_t;
#endif
#if !defined(DUK_F_HAVE_64BIT)
/* cannot detect 64-bit type, not always needed so don't error */
#endif

typedef duk_uint8_t duk_uint_least8_t;
typedef duk_int8_t duk_int_least8_t;
typedef duk_uint16_t duk_uint_least16_t;
typedef duk_int16_t duk_int_least16_t;
typedef duk_uint32_t duk_uint_least32_t;
typedef duk_int32_t duk_int_least32_t;
typedef duk_uint8_t duk_uint_fast8_t;
typedef duk_int8_t duk_int_fast8_t;
typedef duk_uint16_t duk_uint_fast16_t;
typedef duk_int16_t duk_int_fast16_t;
typedef duk_uint32_t duk_uint_fast32_t;
typedef duk_int32_t duk_int_fast32_t;
#if defined(DUK_F_HAVE_64BIT)
typedef duk_uint64_t duk_uint_least64_t;
typedef duk_int64_t duk_int_least64_t;
typedef duk_uint64_t duk_uint_fast64_t;
typedef duk_int64_t duk_int_fast64_t;
#endif
#if defined(DUK_F_HAVE_64BIT)
typedef duk_uint64_t duk_uintmax_t;
typedef duk_int64_t duk_intmax_t;
#else
typedef duk_uint32_t duk_uintmax_t;
typedef duk_int32_t duk_intmax_t;
#endif

/* Note: the funny looking computations for signed minimum 16-bit, 32-bit, and
 * 64-bit values are intentional as the obvious forms (e.g. -0x80000000L) are
 * -not- portable.  See code-issues.txt for a detailed discussion.
 */
#define DUK_UINT8_MIN         0UL
#define DUK_UINT8_MAX         0xffUL
#define DUK_INT8_MIN          (-0x80L)
#define DUK_INT8_MAX          0x7fL
#define DUK_UINT_LEAST8_MIN   0UL
#define DUK_UINT_LEAST8_MAX   0xffUL
#define DUK_INT_LEAST8_MIN    (-0x80L)
#define DUK_INT_LEAST8_MAX    0x7fL
#define DUK_UINT_FAST8_MIN    0UL
#define DUK_UINT_FAST8_MAX    0xffUL
#define DUK_INT_FAST8_MIN     (-0x80L)
#define DUK_INT_FAST8_MAX     0x7fL
#define DUK_UINT16_MIN        0UL
#define DUK_UINT16_MAX        0xffffUL
#define DUK_INT16_MIN         (-0x7fffL - 1L)
#define DUK_INT16_MAX         0x7fffL
#define DUK_UINT_LEAST16_MIN  0UL
#define DUK_UINT_LEAST16_MAX  0xffffUL
#define DUK_INT_LEAST16_MIN   (-0x7fffL - 1L)
#define DUK_INT_LEAST16_MAX   0x7fffL
#define DUK_UINT_FAST16_MIN   0UL
#define DUK_UINT_FAST16_MAX   0xffffUL
#define DUK_INT_FAST16_MIN    (-0x7fffL - 1L)
#define DUK_INT_FAST16_MAX    0x7fffL
#define DUK_UINT32_MIN        0UL
#define DUK_UINT32_MAX        0xffffffffUL
#define DUK_INT32_MIN         (-0x7fffffffL - 1L)
#define DUK_INT32_MAX         0x7fffffffL
#define DUK_UINT_LEAST32_MIN  0UL
#define DUK_UINT_LEAST32_MAX  0xffffffffUL
#define DUK_INT_LEAST32_MIN   (-0x7fffffffL - 1L)
#define DUK_INT_LEAST32_MAX   0x7fffffffL
#define DUK_UINT_FAST32_MIN   0UL
#define DUK_UINT_FAST32_MAX   0xffffffffUL
#define DUK_INT_FAST32_MIN    (-0x7fffffffL - 1L)
#define DUK_INT_FAST32_MAX    0x7fffffffL

/* 64-bit constants.  Since LL / ULL constants are not always available,
 * use computed values.  These values can't be used in preprocessor
 * comparisons; flag them as such.
 */
#if defined(DUK_F_HAVE_64BIT)
#define DUK_UINT64_MIN        ((duk_uint64_t) 0)
#define DUK_UINT64_MAX        ((duk_uint64_t) -1)
#define DUK_INT64_MIN         ((duk_int64_t) (~(DUK_UINT64_MAX >> 1)))
#define DUK_INT64_MAX         ((duk_int64_t) (DUK_UINT64_MAX >> 1))
#define DUK_UINT_LEAST64_MIN  DUK_UINT64_MIN
#define DUK_UINT_LEAST64_MAX  DUK_UINT64_MAX
#define DUK_INT_LEAST64_MIN   DUK_INT64_MIN
#define DUK_INT_LEAST64_MAX   DUK_INT64_MAX
#define DUK_UINT_FAST64_MIN   DUK_UINT64_MIN
#define DUK_UINT_FAST64_MAX   DUK_UINT64_MAX
#define DUK_INT_FAST64_MIN    DUK_INT64_MIN
#define DUK_INT_FAST64_MAX    DUK_INT64_MAX
#define DUK_UINT64_MIN_COMPUTED
#define DUK_UINT64_MAX_COMPUTED
#define DUK_INT64_MIN_COMPUTED
#define DUK_INT64_MAX_COMPUTED
#define DUK_UINT_LEAST64_MIN_COMPUTED
#define DUK_UINT_LEAST64_MAX_COMPUTED
#define DUK_INT_LEAST64_MIN_COMPUTED
#define DUK_INT_LEAST64_MAX_COMPUTED
#define DUK_UINT_FAST64_MIN_COMPUTED
#define DUK_UINT_FAST64_MAX_COMPUTED
#define DUK_INT_FAST64_MIN_COMPUTED
#define DUK_INT_FAST64_MAX_COMPUTED
#endif

#if defined(DUK_F_HAVE_64BIT)
#define DUK_UINTMAX_MIN       DUK_UINT64_MIN
#define DUK_UINTMAX_MAX       DUK_UINT64_MAX
#define DUK_INTMAX_MIN        DUK_INT64_MIN
#define DUK_INTMAX_MAX        DUK_INT64_MAX
#define DUK_UINTMAX_MIN_COMPUTED
#define DUK_UINTMAX_MAX_COMPUTED
#define DUK_INTMAX_MIN_COMPUTED
#define DUK_INTMAX_MAX_COMPUTED
#else
#define DUK_UINTMAX_MIN       0UL
#define DUK_UINTMAX_MAX       0xffffffffUL
#define DUK_INTMAX_MIN        (-0x7fffffffL - 1L)
#define DUK_INTMAX_MAX        0x7fffffffL
#endif

/* This detection is not very reliable. */
#if defined(DUK_F_32BIT_PTRS)
typedef duk_int32_t duk_intptr_t;
typedef duk_uint32_t duk_uintptr_t;
#define DUK_UINTPTR_MIN       DUK_UINT32_MIN
#define DUK_UINTPTR_MAX       DUK_UINT32_MAX
#define DUK_INTPTR_MIN        DUK_INT32_MIN
#define DUK_INTPTR_MAX        DUK_INT32_MAX
#elif defined(DUK_F_64BIT_PTRS) && defined(DUK_F_HAVE_64BIT)
typedef duk_int64_t duk_intptr_t;
typedef duk_uint64_t duk_uintptr_t;
#define DUK_UINTPTR_MIN       DUK_UINT64_MIN
#define DUK_UINTPTR_MAX       DUK_UINT64_MAX
#define DUK_INTPTR_MIN        DUK_INT64_MIN
#define DUK_INTPTR_MAX        DUK_INT64_MAX
#define DUK_UINTPTR_MIN_COMPUTED
#define DUK_UINTPTR_MAX_COMPUTED
#define DUK_INTPTR_MIN_COMPUTED
#define DUK_INTPTR_MAX_COMPUTED
#else
#error cannot determine intptr type
#endif

/* SIZE_MAX may be missing so use an approximate value for it. */
#undef DUK_SIZE_MAX_COMPUTED
#if !defined(SIZE_MAX)
#define DUK_SIZE_MAX_COMPUTED
#define SIZE_MAX              ((size_t) (-1))
#endif
#define DUK_SIZE_MIN          0
#define DUK_SIZE_MAX          SIZE_MAX

#endif  /* C99 types */

/* A few types are assumed to always exist. */
typedef size_t duk_size_t;
typedef ptrdiff_t duk_ptrdiff_t;

/* The best type for an "all around int" in Duktape internals is "at least
 * 32 bit signed integer" which is most convenient.  Same for unsigned type.
 * Prefer 'int' when large enough, as it is almost always a convenient type.
 */
#if defined(UINT_MAX) && (UINT_MAX >= 0xffffffffUL)
typedef int duk_int_t;
typedef unsigned int duk_uint_t;
#define DUK_INT_MIN           INT_MIN
#define DUK_INT_MAX           INT_MAX
#define DUK_UINT_MIN          0
#define DUK_UINT_MAX          UINT_MAX
#else
typedef duk_int_fast32_t duk_int_t;
typedef duk_uint_fast32_t duk_uint_t;
#define DUK_INT_MIN           DUK_INT_FAST32_MIN
#define DUK_INT_MAX           DUK_INT_FAST32_MAX
#define DUK_UINT_MIN          DUK_UINT_FAST32_MIN
#define DUK_UINT_MAX          DUK_UINT_FAST32_MAX
#endif

/* Same as 'duk_int_t' but guaranteed to be a 'fast' variant if this
 * distinction matters for the CPU.  These types are used mainly in the
 * executor where it might really matter.
 */
typedef duk_int_fast32_t duk_int_fast_t;
typedef duk_uint_fast32_t duk_uint_fast_t;
#define DUK_INT_FAST_MIN      DUK_INT_FAST32_MIN
#define DUK_INT_FAST_MAX      DUK_INT_FAST32_MAX
#define DUK_UINT_FAST_MIN     DUK_UINT_FAST32_MIN
#define DUK_UINT_FAST_MAX     DUK_UINT_FAST32_MAX

/* Small integers (16 bits or more) can fall back to the 'int' type, but
 * have a typedef so they are marked "small" explicitly.
 */
typedef int duk_small_int_t;
typedef unsigned int duk_small_uint_t;
#define DUK_SMALL_INT_MIN     INT_MIN
#define DUK_SMALL_INT_MAX     INT_MAX
#define DUK_SMALL_UINT_MIN    0
#define DUK_SMALL_UINT_MAX    UINT_MAX

/* Fast variants of small integers, again for really fast paths like the
 * executor.
 */
typedef duk_int_fast16_t duk_small_int_fast_t;
typedef duk_uint_fast16_t duk_small_uint_fast_t;
#define DUK_SMALL_INT_FAST_MIN    DUK_INT_FAST16_MIN
#define DUK_SMALL_INT_FAST_MAX    DUK_INT_FAST16_MAX
#define DUK_SMALL_UINT_FAST_MIN   DUK_UINT_FAST16_MIN
#define DUK_SMALL_UINT_FAST_MAX   DUK_UINT_FAST16_MAX

/* Boolean values are represented with the platform 'int'. */
typedef duk_small_int_t duk_bool_t;
#define DUK_BOOL_MIN              DUK_SMALL_INT_MIN
#define DUK_BOOL_MAX              DUK_SMALL_INT_MAX

/* Index values must have at least 32-bit signed range. */
typedef duk_int_t duk_idx_t;
#define DUK_IDX_MIN               DUK_INT_MIN
#define DUK_IDX_MAX               DUK_INT_MAX

/* Unsigned index variant. */
typedef duk_uint_t duk_uidx_t;
#define DUK_UIDX_MIN              DUK_UINT_MIN
#define DUK_UIDX_MAX              DUK_UINT_MAX

/* Array index values, could be exact 32 bits.
 * Currently no need for signed duk_arridx_t.
 */
typedef duk_uint_t duk_uarridx_t;
#define DUK_UARRIDX_MIN           DUK_UINT_MIN
#define DUK_UARRIDX_MAX           DUK_UINT_MAX

/* Duktape/C function return value, platform int is enough for now to
 * represent 0, 1, or negative error code.  Must be compatible with
 * assigning truth values (e.g. duk_ret_t rc = (foo == bar);).
 */
typedef duk_small_int_t duk_ret_t;
#define DUK_RET_MIN               DUK_SMALL_INT_MIN
#define DUK_RET_MAX               DUK_SMALL_INT_MAX

/* Error codes are represented with platform int.  High bits are used
 * for flags and such, so 32 bits are needed.
 */
typedef duk_int_t duk_errcode_t;
#define DUK_ERRCODE_MIN           DUK_INT_MIN
#define DUK_ERRCODE_MAX           DUK_INT_MAX

/* Codepoint type.  Must be 32 bits or more because it is used also for
 * internal codepoints.  The type is signed because negative codepoints
 * are used as internal markers (e.g. to mark EOF or missing argument).
 * (X)UTF-8/CESU-8 encode/decode take and return an unsigned variant to
 * ensure duk_uint32_t casts back and forth nicely.  Almost everything
 * else uses the signed one.
 */
typedef duk_int_t duk_codepoint_t;
typedef duk_uint_t duk_ucodepoint_t;
#define DUK_CODEPOINT_MIN         DUK_INT_MIN
#define DUK_CODEPOINT_MAX         DUK_INT_MAX
#define DUK_UCODEPOINT_MIN        DUK_UINT_MIN
#define DUK_UCODEPOINT_MAX        DUK_UINT_MAX

/* IEEE float/double typedef. */
typedef float duk_float_t;
typedef double duk_double_t;

/* We're generally assuming that we're working on a platform with a 32-bit
 * address space.  If DUK_SIZE_MAX is a typecast value (which is necessary
 * if SIZE_MAX is missing), the check must be avoided because the
 * preprocessor can't do a comparison.
 */
#if !defined(DUK_SIZE_MAX)
#error DUK_SIZE_MAX is undefined, probably missing SIZE_MAX
#elif !defined(DUK_SIZE_MAX_COMPUTED)
#if DUK_SIZE_MAX < 0xffffffffUL
/* On some systems SIZE_MAX can be smaller than max unsigned 32-bit value
 * which seems incorrect if size_t is (at least) an unsigned 32-bit type.
 * However, it doesn't seem useful to error out compilation if this is the
 * case.
 */
#endif
#endif

/* Type for public API calls. */
typedef struct duk_hthread duk_context;

/* Check whether we should use 64-bit integers or not.
 *
 * Quite incomplete now.  Use 64-bit types if detected (C99 or other detection)
 * unless they are known to be unreliable.  For instance, 64-bit types are
 * available on VBCC but seem to misbehave.
 */
#if defined(DUK_F_HAVE_64BIT) && !defined(DUK_F_VBCC)
#define DUK_USE_64BIT_OPS
#else
#undef DUK_USE_64BIT_OPS
#endif

/*
 *  Fill-ins for platform, architecture, and compiler
 */

#if !defined(DUK_SETJMP)
#define DUK_USE_SETJMP
#define DUK_SETJMP(jb)        setjmp((jb))
#define DUK_LONGJMP(jb)       longjmp((jb), 1)
#endif

typedef FILE duk_file;
#if !defined(DUK_STDIN)
#define DUK_STDIN       stdin
#endif
#if !defined(DUK_STDOUT)
#define DUK_STDOUT      stdout
#endif
#if !defined(DUK_STDERR)
#define DUK_STDERR      stderr
#endif

/* Special naming to avoid conflict with e.g. DUK_FREE() in duk_heap.h
 * (which is unfortunately named).  May sometimes need replacement, e.g.
 * some compilers don't handle zero length or NULL correctly in realloc().
 */
#if !defined(DUK_ANSI_MALLOC)
#define DUK_ANSI_MALLOC      malloc
#endif
#if !defined(DUK_ANSI_REALLOC)
#define DUK_ANSI_REALLOC     realloc
#endif
#if !defined(DUK_ANSI_CALLOC)
#define DUK_ANSI_CALLOC      calloc
#endif
#if !defined(DUK_ANSI_FREE)
#define DUK_ANSI_FREE        free
#endif

/* ANSI C (various versions) and some implementations require that the
 * pointer arguments to memset(), memcpy(), and memmove() be valid values
 * even when byte size is 0 (even a NULL pointer is considered invalid in
 * this context).  Zero-size operations as such are allowed, as long as their
 * pointer arguments point to a valid memory area.  The DUK_MEMSET(),
 * DUK_MEMCPY(), and DUK_MEMMOVE() macros require this same behavior, i.e.:
 * (1) pointers must be valid and non-NULL, (2) zero size must otherwise be
 * allowed.  If these are not fulfilled, a macro wrapper is needed.
 *
 *   http://stackoverflow.com/questions/5243012/is-it-guaranteed-to-be-safe-to-perform-memcpy0-0-0
 *   http://lists.cs.uiuc.edu/pipermail/llvmdev/2007-October/011065.html
 *
 * Not sure what's the required behavior when a pointer points just past the
 * end of a buffer, which often happens in practice (e.g. zero size memmoves).
 * For example, if allocation size is 3, the following pointer would not
 * technically point to a valid memory byte:
 *
 *   <-- alloc -->
 *   | 0 | 1 | 2 | .....
 *                 ^-- p=3, points after last valid byte (2)
 */
#if !defined(DUK_MEMCPY)
#if defined(DUK_F_UCLIBC)
/* Old uclibcs have a broken memcpy so use memmove instead (this is overly wide
 * now on purpose): http://lists.uclibc.org/pipermail/uclibc-cvs/2008-October/025511.html
 */
#define DUK_MEMCPY       memmove
#else
#define DUK_MEMCPY       memcpy
#endif
#endif
#if !defined(DUK_MEMMOVE)
#define DUK_MEMMOVE      memmove
#endif
#if !defined(DUK_MEMCMP)
#define DUK_MEMCMP       memcmp
#endif
#if !defined(DUK_MEMSET)
#define DUK_MEMSET       memset
#endif
#if !defined(DUK_STRLEN)
#define DUK_STRLEN       strlen
#endif
#if !defined(DUK_STRCMP)
#define DUK_STRCMP       strcmp
#endif
#if !defined(DUK_STRNCMP)
#define DUK_STRNCMP      strncmp
#endif
#if !defined(DUK_PRINTF)
#define DUK_PRINTF       printf
#endif
#if !defined(DUK_FPRINTF)
#define DUK_FPRINTF      fprintf
#endif
#if !defined(DUK_SPRINTF)
#define DUK_SPRINTF      sprintf
#endif
#if !defined(DUK_SNPRINTF)
/* snprintf() is technically not part of C89 but usually available. */
#define DUK_SNPRINTF     snprintf
#endif
#if !defined(DUK_VSPRINTF)
#define DUK_VSPRINTF     vsprintf
#endif
#if !defined(DUK_VSNPRINTF)
/* vsnprintf() is technically not part of C89 but usually available. */
#define DUK_VSNPRINTF    vsnprintf
#endif
#if !defined(DUK_SSCANF)
#define DUK_SSCANF       sscanf
#endif
#if !defined(DUK_VSSCANF)
#define DUK_VSSCANF      vsscanf
#endif
#if !defined(DUK_FOPEN)
#define DUK_FOPEN        fopen
#endif
#if !defined(DUK_FCLOSE)
#define DUK_FCLOSE       fclose
#endif
#if !defined(DUK_FREAD)
#define DUK_FREAD        fread
#endif
#if !defined(DUK_FWRITE)
#define DUK_FWRITE       fwrite
#endif
#if !defined(DUK_FSEEK)
#define DUK_FSEEK        fseek
#endif
#if !defined(DUK_FTELL)
#define DUK_FTELL        ftell
#endif
#if !defined(DUK_FFLUSH)
#define DUK_FFLUSH       fflush
#endif
#if !defined(DUK_FPUTC)
#define DUK_FPUTC        fputc
#endif
#if !defined(DUK_MEMZERO)
#define DUK_MEMZERO(p,n) DUK_MEMSET((p), 0, (n))
#endif
#if !defined(DUK_ABORT)
#define DUK_ABORT        abort
#endif
#if !defined(DUK_EXIT)
#define DUK_EXIT         exit
#endif

#if !defined(DUK_DOUBLE_2TO32)
#define DUK_DOUBLE_2TO32     4294967296.0
#endif
#if !defined(DUK_DOUBLE_2TO31)
#define DUK_DOUBLE_2TO31     2147483648.0
#endif

#if !defined(DUK_DOUBLE_INFINITY)
#undef DUK_USE_COMPUTED_INFINITY
#if defined(DUK_F_GCC_VERSION) && (DUK_F_GCC_VERSION < 40600)
/* GCC older than 4.6: avoid overflow warnings related to using INFINITY */
#define DUK_DOUBLE_INFINITY  (__builtin_inf())
#elif defined(INFINITY)
#define DUK_DOUBLE_INFINITY  ((double) INFINITY)
#elif !defined(DUK_F_VBCC) && !defined(DUK_F_MSVC) && !defined(DUK_F_BCC)
#define DUK_DOUBLE_INFINITY  (1.0 / 0.0)
#else
/* In VBCC (1.0 / 0.0) results in a warning and 0.0 instead of infinity.
 * Use a computed infinity (initialized when a heap is created at the
 * latest).
 */
#define DUK_USE_COMPUTED_INFINITY
#define DUK_DOUBLE_INFINITY  duk_computed_infinity
#endif
#endif

#if !defined(DUK_DOUBLE_NAN)
#undef DUK_USE_COMPUTED_NAN
#if defined(NAN)
#define DUK_DOUBLE_NAN       NAN
#elif !defined(DUK_F_VBCC) && !defined(DUK_F_MSVC) && !defined(DUK_F_BCC)
#define DUK_DOUBLE_NAN       (0.0 / 0.0)
#else
/* In VBCC (0.0 / 0.0) results in a warning and 0.0 instead of NaN.
 * In MSVC (VS2010 Express) (0.0 / 0.0) results in a compile error.
 * Use a computed NaN (initialized when a heap is created at the
 * latest).
 */
#define DUK_USE_COMPUTED_NAN
#define DUK_DOUBLE_NAN       duk_computed_nan
#endif
#endif

/* Many platforms are missing fpclassify() and friends, so use replacements
 * if necessary.  The replacement constants (FP_NAN etc) can be anything but
 * match Linux constants now.
 */
#undef DUK_USE_REPL_FPCLASSIFY
#undef DUK_USE_REPL_SIGNBIT
#undef DUK_USE_REPL_ISFINITE
#undef DUK_USE_REPL_ISNAN
#undef DUK_USE_REPL_ISINF

/* Complex condition broken into separate parts. */
#undef DUK_F_USE_REPL_ALL
#if !(defined(FP_NAN) && defined(FP_INFINITE) && defined(FP_ZERO) && \
      defined(FP_SUBNORMAL) && defined(FP_NORMAL))
/* Missing some obvious constants. */
#define DUK_F_USE_REPL_ALL
#elif defined(DUK_F_AMIGAOS) && defined(DUK_F_VBCC)
/* VBCC is missing the built-ins even in C99 mode (perhaps a header issue) */
#define DUK_F_USE_REPL_ALL
#elif defined(DUK_F_FREEBSD) && defined(DUK_F_CLANG)
/* Placeholder fix for (detection is wider than necessary):
 * http://llvm.org/bugs/show_bug.cgi?id=17788
 */
#define DUK_F_USE_REPL_ALL
#elif defined(DUK_F_UCLIBC)
/* At least some uclibc versions have broken floating point math.  For
 * example, fpclassify() can incorrectly classify certain NaN formats.
 * To be safe, use replacements.
 */
#define DUK_F_USE_REPL_ALL
#endif

#if defined(DUK_F_USE_REPL_ALL)
#define DUK_USE_REPL_FPCLASSIFY
#define DUK_USE_REPL_SIGNBIT
#define DUK_USE_REPL_ISFINITE
#define DUK_USE_REPL_ISNAN
#define DUK_USE_REPL_ISINF
#define DUK_FPCLASSIFY       duk_repl_fpclassify
#define DUK_SIGNBIT          duk_repl_signbit
#define DUK_ISFINITE         duk_repl_isfinite
#define DUK_ISNAN            duk_repl_isnan
#define DUK_ISINF            duk_repl_isinf
#define DUK_FP_NAN           0
#define DUK_FP_INFINITE      1
#define DUK_FP_ZERO          2
#define DUK_FP_SUBNORMAL     3
#define DUK_FP_NORMAL        4
#else
#define DUK_FPCLASSIFY       fpclassify
#define DUK_SIGNBIT          signbit
#define DUK_ISFINITE         isfinite
#define DUK_ISNAN            isnan
#define DUK_ISINF            isinf
#define DUK_FP_NAN           FP_NAN
#define DUK_FP_INFINITE      FP_INFINITE
#define DUK_FP_ZERO          FP_ZERO
#define DUK_FP_SUBNORMAL     FP_SUBNORMAL
#define DUK_FP_NORMAL        FP_NORMAL
#endif

#if defined(DUK_F_USE_REPL_ALL)
#undef DUK_F_USE_REPL_ALL
#endif

/* Some math functions are C99 only.  This is also an issue with some
 * embedded environments using uclibc where uclibc has been configured
 * not to provide some functions.  For now, use replacements whenever
 * using uclibc.
 */
#undef DUK_USE_MATH_FMIN
#undef DUK_USE_MATH_FMAX
#undef DUK_USE_MATH_ROUND
#if defined(DUK_F_UCLIBC)
/* uclibc may be missing these */
#elif defined(DUK_F_AMIGAOS) && defined(DUK_F_VBCC)
/* vbcc + AmigaOS may be missing these */
#elif !defined(DUK_F_C99) && !defined(DUK_F_CPP11)
/* build is not C99 or C++11, play it safe */
#else
/* C99 or C++11, no known issues */
#define DUK_USE_MATH_FMIN
#define DUK_USE_MATH_FMAX
#define DUK_USE_MATH_ROUND
#endif

/* These functions don't currently need replacement but are wrapped for
 * completeness.  Because these are used as function pointers, they need
 * to be defined as concrete C functions (not macros).
 */
#if !defined(DUK_FABS)
#define DUK_FABS             fabs
#endif
#if !defined(DUK_FMIN)
#define DUK_FMIN             fmin
#endif
#if !defined(DUK_FMAX)
#define DUK_FMAX             fmax
#endif
#if !defined(DUK_FLOOR)
#define DUK_FLOOR            floor
#endif
#if !defined(DUK_CEIL)
#define DUK_CEIL             ceil
#endif
#if !defined(DUK_FMOD)
#define DUK_FMOD             fmod
#endif
#if !defined(DUK_POW)
#define DUK_POW              pow
#endif
#if !defined(DUK_ACOS)
#define DUK_ACOS             acos
#endif
#if !defined(DUK_ASIN)
#define DUK_ASIN             asin
#endif
#if !defined(DUK_ATAN)
#define DUK_ATAN             atan
#endif
#if !defined(DUK_ATAN2)
#define DUK_ATAN2            atan2
#endif
#if !defined(DUK_SIN)
#define DUK_SIN              sin
#endif
#if !defined(DUK_COS)
#define DUK_COS              cos
#endif
#if !defined(DUK_TAN)
#define DUK_TAN              tan
#endif
#if !defined(DUK_EXP)
#define DUK_EXP              exp
#endif
#if !defined(DUK_LOG)
#define DUK_LOG              log
#endif
#if !defined(DUK_SQRT)
#define DUK_SQRT             sqrt
#endif

/* NetBSD 6.0 x86 (at least) has a few problems with pow() semantics,
 * see test-bug-netbsd-math-pow.js.  Use NetBSD specific workaround.
 * (This might be a wider problem; if so, generalize the define name.)
 */
#undef DUK_USE_POW_NETBSD_WORKAROUND
#if defined(DUK_F_NETBSD)
#define DUK_USE_POW_NETBSD_WORKAROUND
#endif

/* Rely as little as possible on compiler behavior for NaN comparison,
 * signed zero handling, etc.  Currently never activated but may be needed
 * for broken compilers.
 */
#undef DUK_USE_PARANOID_MATH

/* There was a curious bug where test-bi-date-canceling.js would fail e.g.
 * on 64-bit Ubuntu, gcc-4.8.1, -m32, and no -std=c99.  Some date computations
 * using doubles would be optimized which then broke some corner case tests.
 * The problem goes away by adding 'volatile' to the datetime computations.
 * Not sure what the actual triggering conditions are, but using this on
 * non-C99 systems solves the known issues and has relatively little cost
 * on other platforms.
 */
#undef DUK_USE_PARANOID_DATE_COMPUTATION
#if !defined(DUK_F_C99)
#define DUK_USE_PARANOID_DATE_COMPUTATION
#endif

/*
 *  Byte order and double memory layout detection
 *
 *  Endianness detection is a major portability hassle because the macros
 *  and headers are not standardized.  There's even variance across UNIX
 *  platforms.  Even with "standard" headers, details like underscore count
 *  varies between platforms, e.g. both __BYTE_ORDER and _BYTE_ORDER are used
 *  (Crossbridge has a single underscore, for instance).
 *
 *  The checks below are structured with this in mind: several approaches are
 *  used, and at the end we check if any of them worked.  This allows generic
 *  approaches to be tried first, and platform/compiler specific hacks tried
 *  last.  As a last resort, the user can force a specific endianness, as it's
 *  not likely that automatic detection will work on the most exotic platforms.
 *
 *  Duktape supports little and big endian machines.  There's also support
 *  for a hybrid used by some ARM machines where integers are little endian
 *  but IEEE double values use a mixed order (12345678 -> 43218765).  This
 *  byte order for doubles is referred to as "mixed endian".
 */

/* For custom platforms allow user to define byteorder explicitly.
 * Since endianness headers are not standardized, this is a useful
 * workaround for custom platforms for which endianness detection
 * is not directly supported.  Perhaps custom hardware is used and
 * user cannot submit upstream patches.
 */
#if defined(DUK_OPT_FORCE_BYTEORDER)
#undef DUK_USE_BYTEORDER
#if (DUK_OPT_FORCE_BYTEORDER == 1)
#define DUK_USE_BYTEORDER 1
#elif (DUK_OPT_FORCE_BYTEORDER == 2)
#define DUK_USE_BYTEORDER 2
#elif (DUK_OPT_FORCE_BYTEORDER == 3)
#define DUK_USE_BYTEORDER 3
#else
#error invalid DUK_OPT_FORCE_BYTEORDER value
#endif
#endif  /* DUK_OPT_FORCE_BYTEORDER */

/* GCC and Clang provide endianness defines as built-in predefines, with
 * leading and trailing double underscores (e.g. __BYTE_ORDER__).  See
 * output of "make gccpredefs" and "make clangpredefs".  Clang doesn't
 * seem to provide __FLOAT_WORD_ORDER__; assume not mixed endian for clang.
 * http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
 */
#if !defined(DUK_USE_BYTEORDER) && defined(__BYTE_ORDER__)
#if defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#if defined(__FLOAT_WORD_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define DUK_USE_BYTEORDER 1
#elif defined(__FLOAT_WORD_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__)
#define DUK_USE_BYTEORDER 2
#elif !defined(__FLOAT_WORD_ORDER__)
/* Float word order not known, assume not a hybrid. */
#define DUK_USE_BYTEORDER 1
#else
/* Byte order is little endian but cannot determine IEEE double word order. */
#endif  /* float word order */
#elif defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#if defined(__FLOAT_WORD_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__)
#define DUK_USE_BYTEORDER 3
#elif !defined(__FLOAT_WORD_ORDER__)
/* Float word order not known, assume not a hybrid. */
#define DUK_USE_BYTEORDER 3
#else
/* Byte order is big endian but cannot determine IEEE double word order. */
#endif  /* float word order */
#else
/* Cannot determine byte order; __ORDER_PDP_ENDIAN__ is related to 32-bit
 * integer ordering and is not relevant.
 */
#endif  /* integer byte order */
#endif  /* !defined(DUK_USE_BYTEORDER) && defined(__BYTE_ORDER__) */

/* More or less standard endianness predefines provided by header files.
 * The ARM hybrid case is detected by assuming that __FLOAT_WORD_ORDER
 * will be big endian, see: http://lists.mysql.com/internals/443.
 * On some platforms some defines may be present with an empty value which
 * causes comparisons to fail: https://github.com/svaarala/duktape/issues/453.
 */
#if !defined(DUK_USE_BYTEORDER)
#if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN) || \
    defined(_BYTE_ORDER) && defined(_LITTLE_ENDIAN) && (_BYTE_ORDER == _LITTLE_ENDIAN) || \
    defined(__LITTLE_ENDIAN__)
#if defined(__FLOAT_WORD_ORDER) && defined(__LITTLE_ENDIAN) && (__FLOAT_WORD_ORDER == __LITTLE_ENDIAN) || \
    defined(_FLOAT_WORD_ORDER) && defined(_LITTLE_ENDIAN) && (_FLOAT_WORD_ORDER == _LITTLE_ENDIAN)
#define DUK_USE_BYTEORDER 1
#elif defined(__FLOAT_WORD_ORDER) && defined(__BIG_ENDIAN) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN) || \
      defined(_FLOAT_WORD_ORDER) && defined(_BIG_ENDIAN) && (_FLOAT_WORD_ORDER == _BIG_ENDIAN)
#define DUK_USE_BYTEORDER 2
#elif !defined(__FLOAT_WORD_ORDER) && !defined(_FLOAT_WORD_ORDER)
/* Float word order not known, assume not a hybrid. */
#define DUK_USE_BYTEORDER 1
#else
/* Byte order is little endian but cannot determine IEEE double word order. */
#endif  /* float word order */
#elif defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN) || \
      defined(_BYTE_ORDER) && defined(_BIG_ENDIAN) && (_BYTE_ORDER == _BIG_ENDIAN) || \
      defined(__BIG_ENDIAN__)
#if defined(__FLOAT_WORD_ORDER) && defined(__BIG_ENDIAN) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN) || \
    defined(_FLOAT_WORD_ORDER) && defined(_BIG_ENDIAN) && (_FLOAT_WORD_ORDER == _BIG_ENDIAN)
#define DUK_USE_BYTEORDER 3
#elif !defined(__FLOAT_WORD_ORDER) && !defined(_FLOAT_WORD_ORDER)
/* Float word order not known, assume not a hybrid. */
#define DUK_USE_BYTEORDER 3
#else
/* Byte order is big endian but cannot determine IEEE double word order. */
#endif  /* float word order */
#else
/* Cannot determine byte order. */
#endif  /* integer byte order */
#endif  /* !defined(DUK_USE_BYTEORDER) */

/* QNX gcc cross compiler seems to define e.g. __LITTLEENDIAN__ or __BIGENDIAN__:
 *  $ /opt/qnx650/host/linux/x86/usr/bin/i486-pc-nto-qnx6.5.0-gcc -dM -E - </dev/null | grep -ni endian
 *  67:#define __LITTLEENDIAN__ 1
 *  $ /opt/qnx650/host/linux/x86/usr/bin/mips-unknown-nto-qnx6.5.0-gcc -dM -E - </dev/null | grep -ni endian
 *  81:#define __BIGENDIAN__ 1
 *  $ /opt/qnx650/host/linux/x86/usr/bin/arm-unknown-nto-qnx6.5.0-gcc -dM -E - </dev/null | grep -ni endian
 *  70:#define __LITTLEENDIAN__ 1
 */
#if !defined(DUK_USE_BYTEORDER)
#if defined(__LITTLEENDIAN__)
#define DUK_USE_BYTEORDER 1
#elif defined(__BIGENDIAN__)
#define DUK_USE_BYTEORDER 3
#endif
#endif

/*
 *  Alignment requirement and support for unaligned accesses
 *
 *  Assume unaligned accesses are not supported unless specifically allowed
 *  in the target platform.  Some platforms may support unaligned accesses
 *  but alignment to 4 or 8 may still be desirable.
 */

/* If not provided, use safe default for alignment. */
#if !defined(DUK_USE_ALIGN_BY)
#define DUK_USE_ALIGN_BY 8
#endif

/* User forced alignment to 4 or 8. */
#if defined(DUK_OPT_FORCE_ALIGN)
#undef DUK_USE_ALIGN_BY
#if (DUK_OPT_FORCE_ALIGN == 4)
#define DUK_USE_ALIGN_BY 4
#elif (DUK_OPT_FORCE_ALIGN == 8)
#define DUK_USE_ALIGN_BY 8
#else
#error invalid DUK_OPT_FORCE_ALIGN value
#endif
#endif

/* Compiler specific hackery needed to force struct size to match aligment,
 * see e.g. duk_hbuffer.h.
 *
 * http://stackoverflow.com/questions/11130109/c-struct-size-alignment
 * http://stackoverflow.com/questions/10951039/specifying-64-bit-alignment
 */
#if !(defined(DUK_USE_PACK_MSVC_PRAGMA) || defined(DUK_USE_PACK_GCC_ATTR) || \
      defined(DUK_USE_PACK_CLANG_ATTR) || defined(DUK_USE_PACK_DUMMY_MEMBER))
#define DUK_USE_PACK_DUMMY_MEMBER
#endif

#if !defined(DUK_VA_COPY)
/* We need va_copy() which is defined in C99 / C++11, so an awkward
 * replacement is needed for pre-C99 / pre-C++11 environments.  This
 * will quite likely need portability hacks for some non-C99
 * environments.
 */
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
/* C99 / C++11 and above: rely on va_copy() which is required.
 * Omit parenthesis on macro right side on purpose to minimize differences
 * to direct use.
 */
#define DUK_VA_COPY(dest,src) va_copy(dest,src)
#else
/* Pre-C99: va_list type is implementation dependent.  This replacement
 * assumes it is a plain value so that a simple assignment will work.
 * This is not the case on all platforms (it may be a single-array element,
 * for instance).
 */
#define DUK_VA_COPY(dest,src) do { (dest) = (src); } while (0)
#endif
#endif

#if !defined(DUK_MACRO_STRINGIFY)
/* Macro hackery to convert e.g. __LINE__ to a string without formatting,
 * see: http://stackoverflow.com/questions/240353/convert-a-preprocessor-token-to-a-string
 */
#define DUK_MACRO_STRINGIFY_HELPER(x)  #x
#define DUK_MACRO_STRINGIFY(x)  DUK_MACRO_STRINGIFY_HELPER(x)
#endif

#if !defined(DUK_CAUSE_SEGFAULT)
/* This is optionally used by panic handling to cause the program to segfault
 * (instead of e.g. abort()) on panic.  Valgrind will then indicate the C
 * call stack leading to the panic.
 */
#define DUK_CAUSE_SEGFAULT()  do { *((volatile duk_uint32_t *) NULL) = (duk_uint32_t) 0xdeadbeefUL; } while (0)
#endif
#if !defined(DUK_UNREF)
/* Macro for suppressing warnings for potentially unreferenced variables.
 * The variables can be actually unreferenced or unreferenced in some
 * specific cases only; for instance, if a variable is only debug printed,
 * it is unreferenced when debug printing is disabled.
 */
#define DUK_UNREF(x)  do { (void) (x); } while (0)
#endif
#if !defined(DUK_NORETURN)
#define DUK_NORETURN(decl)  decl
#endif
#if !defined(DUK_UNREACHABLE)
/* Don't know how to declare unreachable point, so don't do it; this
 * may cause some spurious compilation warnings (e.g. "variable used
 * uninitialized").
 */
#define DUK_UNREACHABLE()  do { } while (0)
#endif
#if !defined(DUK_LOSE_CONST)
/* Convert any input pointer into a "void *", losing a const qualifier.
 * This is not fully portable because casting through duk_uintptr_t may
 * not work on all architectures (e.g. those with long, segmented pointers).
 */
#define DUK_LOSE_CONST(src) ((void *) (duk_uintptr_t) (src))
#endif

#if !defined(DUK_LIKELY)
#define DUK_LIKELY(x)    (x)
#endif
#if !defined(DUK_UNLIKELY)
#define DUK_UNLIKELY(x)  (x)
#endif

#if !defined(DUK_NOINLINE)
#define DUK_NOINLINE       /*nop*/
#endif
#if !defined(DUK_INLINE)
#define DUK_INLINE         /*nop*/
#endif
#if !defined(DUK_ALWAYS_INLINE)
#define DUK_ALWAYS_INLINE  /*nop*/
#endif

#if !defined(DUK_EXTERNAL_DECL)
#define DUK_EXTERNAL_DECL  extern
#endif
#if !defined(DUK_EXTERNAL)
#define DUK_EXTERNAL       /*empty*/
#endif
#if !defined(DUK_INTERNAL_DECL)
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL_DECL  static
#else
#define DUK_INTERNAL_DECL  extern
#endif
#endif
#if !defined(DUK_INTERNAL)
#if defined(DUK_SINGLE_FILE)
#define DUK_INTERNAL       static
#else
#define DUK_INTERNAL       /*empty*/
#endif
#endif
#if !defined(DUK_LOCAL_DECL)
#define DUK_LOCAL_DECL     static
#endif
#if !defined(DUK_LOCAL)
#define DUK_LOCAL          static
#endif

#if !defined(DUK_FILE_MACRO)
#define DUK_FILE_MACRO  __FILE__
#endif
#if !defined(DUK_LINE_MACRO)
#define DUK_LINE_MACRO  __LINE__
#endif
#if !defined(DUK_FUNC_MACRO)
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_FUNC_MACRO  __func__
#elif defined(__FUNCTION__)
#define DUK_FUNC_MACRO  __FUNCTION__
#else
#define DUK_FUNC_MACRO  "unknown"
#endif
#endif

#if !defined(DUK_BSWAP32)
#define DUK_BSWAP32(x) \
	((((duk_uint32_t) (x)) >> 24) | \
	 ((((duk_uint32_t) (x)) >> 8) & 0xff00UL) | \
	 ((((duk_uint32_t) (x)) << 8) & 0xff0000UL) | \
	 (((duk_uint32_t) (x)) << 24))
#endif
#if !defined(DUK_BSWAP16)
#define DUK_BSWAP16(x) \
	((duk_uint16_t) (x) >> 8) | \
	((duk_uint16_t) (x) << 8)
#endif

/* Explicit marker needed; may be 'defined', 'undefined, 'or 'not provided'. */
#if !defined(DUK_F_VARIADIC_MACROS_PROVIDED)
#undef DUK_USE_VARIADIC_MACROS
#if defined(DUK_F_C99) || defined(DUK_F_CPP11)
#define DUK_USE_VARIADIC_MACROS
#endif
#endif

#if !(defined(DUK_USE_FLEX_C99) || defined(DUK_USE_FLEX_ZEROSIZE) || defined(DUK_USE_FLEX_ONESIZE))
#if defined(DUK_F_C99)
#define DUK_USE_FLEX_C99
#else
#define DUK_USE_FLEX_ZEROSIZE  /* Not standard but common enough */
#endif
#endif

#if !(defined(DUK_USE_PACK_GCC_ATTR) || defined(DUK_USE_PACK_CLANG_ATTR) || \
      defined(DUK_USE_PACK_MSVC_PRAGMA) || defined(DUK_USE_PACK_DUMMY_MEMBER))
#define DUK_USE_PACK_DUMMY_MEMBER
#endif

#if 0  /* not defined by default */
#undef DUK_USE_GCC_PRAGMAS
#endif

/* Workaround for GH-323: avoid inlining control when compiling from
 * multiple sources, as it causes compiler portability trouble.
 */
#if !defined(DUK_SINGLE_FILE)
#undef DUK_NOINLINE
#undef DUK_INLINE
#undef DUK_ALWAYS_INLINE
#define DUK_NOINLINE       /*nop*/
#define DUK_INLINE         /*nop*/
#define DUK_ALWAYS_INLINE  /*nop*/
#endif

/*
 *  Check whether or not a packed duk_tval representation is possible.
 *  What's basically required is that pointers are 32-bit values
 *  (sizeof(void *) == 4).  Best effort check, not always accurate.
 *  If guess goes wrong, crashes may result; self tests also verify
 *  the guess.
 */

/* Explicit marker needed; may be 'defined', 'undefined, 'or 'not provided'. */
#if !defined(DUK_F_PACKED_TVAL_PROVIDED)
#undef DUK_F_PACKED_TVAL_POSSIBLE

/* Strict C99 case: DUK_UINTPTR_MAX (= UINTPTR_MAX) should be very reliable */
#if !defined(DUK_F_PACKED_TVAL_POSSIBLE) && defined(DUK_UINTPTR_MAX)
#if (DUK_UINTPTR_MAX <= 0xffffffffUL)
#define DUK_F_PACKED_TVAL_POSSIBLE
#endif
#endif

/* Non-C99 case, still relying on DUK_UINTPTR_MAX, as long as it is not a computed value */
#if !defined(DUK_F_PACKED_TVAL_POSSIBLE) && defined(DUK_UINTPTR_MAX) && !defined(DUK_UINTPTR_MAX_COMPUTED)
#if (DUK_UINTPTR_MAX <= 0xffffffffUL)
#define DUK_F_PACKED_TVAL_POSSIBLE
#endif
#endif

/* DUK_SIZE_MAX (= SIZE_MAX) is often reliable */
#if !defined(DUK_F_PACKED_TVAL_POSSIBLE) && defined(DUK_SIZE_MAX) && !defined(DUK_SIZE_MAX_COMPUTED)
#if (DUK_SIZE_MAX <= 0xffffffffUL)
#define DUK_F_PACKED_TVAL_POSSIBLE
#endif
#endif

#undef DUK_USE_PACKED_TVAL
#if defined(DUK_F_PACKED_TVAL_POSSIBLE)
#define DUK_USE_PACKED_TVAL
#endif

#undef DUK_F_PACKED_TVAL_POSSIBLE
#endif  /* DUK_F_PACKED_TVAL_PROVIDED */

/* Feature option forcing. */
#if defined(DUK_OPT_NO_PACKED_TVAL)
#undef DUK_USE_PACKED_TVAL
#elif defined(DUK_OPT_PACKED_TVAL)
#undef DUK_USE_PACKED_TVAL
#define DUK_USE_PACKED_TVAL
#endif
/* Object property allocation layout has implications for memory and code
 * footprint and generated code size/speed.  The best layout also depends
 * on whether the platform has alignment requirements or benefits from
 * having mostly aligned accesses.
 */
#undef DUK_USE_HOBJECT_LAYOUT_1
#undef DUK_USE_HOBJECT_LAYOUT_2
#undef DUK_USE_HOBJECT_LAYOUT_3
#if (DUK_USE_ALIGN_BY == 1)
/* On platforms without any alignment issues, layout 1 is preferable
 * because it compiles to slightly less code and provides direct access
 * to property keys.
 */
#define DUK_USE_HOBJECT_LAYOUT_1
#else
/* On other platforms use layout 2, which requires some padding but
 * is a bit more natural than layout 3 in ordering the entries.  Layout
 * 3 is currently not used.
 */
#define DUK_USE_HOBJECT_LAYOUT_2
#endif

/* GCC/clang inaccurate math would break compliance and probably duk_tval,
 * so refuse to compile.  Relax this if -ffast-math is tested to work.
 */
#if defined(__FAST_MATH__) && 0
#error __FAST_MATH__ defined, refusing to compile
#endif

/*
 *  Feature option handling
 */

#if !defined(DUK_USE_ALIGN_BY)
#if defined(DUK_OPT_FORCE_ALIGN)
#define DUK_USE_ALIGN_BY DUK_OPT_FORCE_ALIGN
#else
#define DUK_USE_ALIGN_BY 8
#endif
#endif

#if defined(DUK_OPT_ASSERTIONS)
#define DUK_USE_ASSERTIONS
#elif defined(DUK_OPT_NO_ASSERTIONS)
#undef DUK_USE_ASSERTIONS
#else
#undef DUK_USE_ASSERTIONS
#endif

#if defined(DUK_OPT_AUGMENT_ERRORS)
#define DUK_USE_AUGMENT_ERROR_CREATE
#elif defined(DUK_OPT_NO_AUGMENT_ERRORS)
#undef DUK_USE_AUGMENT_ERROR_CREATE
#else
#define DUK_USE_AUGMENT_ERROR_CREATE
#endif

#if defined(DUK_OPT_AUGMENT_ERRORS)
#define DUK_USE_AUGMENT_ERROR_THROW
#elif defined(DUK_OPT_NO_AUGMENT_ERRORS)
#undef DUK_USE_AUGMENT_ERROR_THROW
#else
#define DUK_USE_AUGMENT_ERROR_THROW
#endif

#if defined(DUK_OPT_BROWSER_LIKE)
#define DUK_USE_BROWSER_LIKE
#elif defined(DUK_OPT_NO_BROWSER_LIKE)
#undef DUK_USE_BROWSER_LIKE
#else
#define DUK_USE_BROWSER_LIKE
#endif

#if defined(DUK_OPT_BUFFEROBJECT_SUPPORT)
#define DUK_USE_BUFFEROBJECT_SUPPORT
#elif defined(DUK_OPT_NO_BUFFEROBJECT_SUPPORT)
#undef DUK_USE_BUFFEROBJECT_SUPPORT
#else
#define DUK_USE_BUFFEROBJECT_SUPPORT
#endif

#if defined(DUK_OPT_BUFLEN16)
#define DUK_USE_BUFLEN16
#elif defined(DUK_OPT_NO_BUFLEN16)
#undef DUK_USE_BUFLEN16
#else
#undef DUK_USE_BUFLEN16
#endif

#if defined(DUK_OPT_BYTECODE_DUMP_SUPPORT)
#define DUK_USE_BYTECODE_DUMP_SUPPORT
#elif defined(DUK_OPT_NO_BYTECODE_DUMP_SUPPORT)
#undef DUK_USE_BYTECODE_DUMP_SUPPORT
#else
#define DUK_USE_BYTECODE_DUMP_SUPPORT
#endif

#if defined(DUK_OPT_COMMONJS_MODULES)
#define DUK_USE_COMMONJS_MODULES
#elif defined(DUK_OPT_NO_COMMONJS_MODULES)
#undef DUK_USE_COMMONJS_MODULES
#else
#define DUK_USE_COMMONJS_MODULES
#endif

#if defined(DUK_OPT_CPP_EXCEPTIONS)
#define DUK_USE_CPP_EXCEPTIONS
#elif defined(DUK_OPT_NO_CPP_EXCEPTIONS)
#undef DUK_USE_CPP_EXCEPTIONS
#else
#undef DUK_USE_CPP_EXCEPTIONS
#endif

#if defined(DUK_OPT_DATAPTR16)
#define DUK_USE_DATAPTR16
#elif defined(DUK_OPT_NO_DATAPTR16)
#undef DUK_USE_DATAPTR16
#else
#undef DUK_USE_DATAPTR16
#endif

#if defined(DUK_OPT_DATAPTR_DEC16)
#define DUK_USE_DATAPTR_DEC16(udata,ptr) DUK_OPT_DATAPTR_DEC16((udata),(ptr))
#else
#undef DUK_USE_DATAPTR_DEC16
#endif

#if defined(DUK_OPT_DATAPTR_ENC16)
#define DUK_USE_DATAPTR_ENC16(udata,ptr) DUK_OPT_DATAPTR_ENC16((udata),(ptr))
#else
#undef DUK_USE_DATAPTR_ENC16
#endif

#if defined(DUK_OPT_DDDPRINT)
#define DUK_USE_DDDPRINT
#elif defined(DUK_OPT_NO_DDDPRINT)
#undef DUK_USE_DDDPRINT
#else
#undef DUK_USE_DDDPRINT
#endif

#if defined(DUK_OPT_DDPRINT)
#define DUK_USE_DDPRINT
#elif defined(DUK_OPT_NO_DDPRINT)
#undef DUK_USE_DDPRINT
#else
#undef DUK_USE_DDPRINT
#endif

#if defined(DUK_OPT_DEBUG)
#define DUK_USE_DEBUG
#elif defined(DUK_OPT_NO_DEBUG)
#undef DUK_USE_DEBUG
#else
#undef DUK_USE_DEBUG
#endif

#if defined(DUK_OPT_DEBUGGER_DUMPHEAP)
#define DUK_USE_DEBUGGER_DUMPHEAP
#elif defined(DUK_OPT_NO_DEBUGGER_DUMPHEAP)
#undef DUK_USE_DEBUGGER_DUMPHEAP
#else
#undef DUK_USE_DEBUGGER_DUMPHEAP
#endif

#if defined(DUK_OPT_DEBUGGER_FWD_LOGGING)
#define DUK_USE_DEBUGGER_FWD_LOGGING
#elif defined(DUK_OPT_NO_DEBUGGER_FWD_LOGGING)
#undef DUK_USE_DEBUGGER_FWD_LOGGING
#else
#undef DUK_USE_DEBUGGER_FWD_LOGGING
#endif

#if defined(DUK_OPT_DEBUGGER_FWD_PRINTALERT)
#define DUK_USE_DEBUGGER_FWD_PRINTALERT
#elif defined(DUK_OPT_NO_DEBUGGER_FWD_PRINTALERT)
#undef DUK_USE_DEBUGGER_FWD_PRINTALERT
#else
#undef DUK_USE_DEBUGGER_FWD_PRINTALERT
#endif

#if defined(DUK_OPT_DEBUGGER_PAUSE_UNCAUGHT)
#define DUK_USE_DEBUGGER_PAUSE_UNCAUGHT
#elif defined(DUK_OPT_NO_DEBUGGER_PAUSE_UNCAUGHT)
#undef DUK_USE_DEBUGGER_PAUSE_UNCAUGHT
#else
#undef DUK_USE_DEBUGGER_PAUSE_UNCAUGHT
#endif

#if defined(DUK_OPT_DEBUGGER_SUPPORT)
#define DUK_USE_DEBUGGER_SUPPORT
#elif defined(DUK_OPT_NO_DEBUGGER_SUPPORT)
#undef DUK_USE_DEBUGGER_SUPPORT
#else
#undef DUK_USE_DEBUGGER_SUPPORT
#endif

#if defined(DUK_OPT_DEBUGGER_THROW_NOTIFY)
#define DUK_USE_DEBUGGER_THROW_NOTIFY
#elif defined(DUK_OPT_NO_DEBUGGER_THROW_NOTIFY)
#undef DUK_USE_DEBUGGER_THROW_NOTIFY
#else
#define DUK_USE_DEBUGGER_THROW_NOTIFY
#endif

#if defined(DUK_OPT_DEBUGGER_TRANSPORT_TORTURE)
#define DUK_USE_DEBUGGER_TRANSPORT_TORTURE
#elif defined(DUK_OPT_NO_DEBUGGER_TRANSPORT_TORTURE)
#undef DUK_USE_DEBUGGER_TRANSPORT_TORTURE
#else
#undef DUK_USE_DEBUGGER_TRANSPORT_TORTURE
#endif

#if defined(DUK_OPT_DEBUG_BUFSIZE)
#define DUK_USE_DEBUG_BUFSIZE DUK_OPT_DEBUG_BUFSIZE
#else
#define DUK_USE_DEBUG_BUFSIZE 65536L
#endif

#if defined(DUK_OPT_REFERENCE_COUNTING)
#define DUK_USE_DOUBLE_LINKED_HEAP
#elif defined(DUK_OPT_NO_REFERENCE_COUNTING)
#undef DUK_USE_DOUBLE_LINKED_HEAP
#else
#define DUK_USE_DOUBLE_LINKED_HEAP
#endif

#if defined(DUK_OPT_DPRINT)
#define DUK_USE_DPRINT
#elif defined(DUK_OPT_NO_DPRINT)
#undef DUK_USE_DPRINT
#else
#undef DUK_USE_DPRINT
#endif

#if defined(DUK_OPT_DPRINT_COLORS)
#define DUK_USE_DPRINT_COLORS
#elif defined(DUK_OPT_NO_DPRINT_COLORS)
#undef DUK_USE_DPRINT_COLORS
#else
#undef DUK_USE_DPRINT_COLORS
#endif

#if defined(DUK_OPT_DPRINT_RDTSC)
#define DUK_USE_DPRINT_RDTSC
#elif defined(DUK_OPT_NO_DPRINT_RDTSC)
#undef DUK_USE_DPRINT_RDTSC
#else
#undef DUK_USE_DPRINT_RDTSC
#endif

#if defined(DUK_OPT_AUGMENT_ERRORS)
#define DUK_USE_ERRCREATE
#elif defined(DUK_OPT_NO_AUGMENT_ERRORS)
#undef DUK_USE_ERRCREATE
#else
#define DUK_USE_ERRCREATE
#endif

#if defined(DUK_OPT_AUGMENT_ERRORS)
#define DUK_USE_ERRTHROW
#elif defined(DUK_OPT_NO_AUGMENT_ERRORS)
#undef DUK_USE_ERRTHROW
#else
#define DUK_USE_ERRTHROW
#endif

#if defined(DUK_OPT_ES6_OBJECT_PROTO_PROPERTY)
#define DUK_USE_ES6_OBJECT_PROTO_PROPERTY
#elif defined(DUK_OPT_NO_ES6_OBJECT_PROTO_PROPERTY)
#undef DUK_USE_ES6_OBJECT_PROTO_PROPERTY
#else
#define DUK_USE_ES6_OBJECT_PROTO_PROPERTY
#endif

#if defined(DUK_OPT_ES6_OBJECT_SETPROTOTYPEOF)
#define DUK_USE_ES6_OBJECT_SETPROTOTYPEOF
#elif defined(DUK_OPT_NO_ES6_OBJECT_SETPROTOTYPEOF)
#undef DUK_USE_ES6_OBJECT_SETPROTOTYPEOF
#else
#define DUK_USE_ES6_OBJECT_SETPROTOTYPEOF
#endif

#if defined(DUK_OPT_ES6_PROXY)
#define DUK_USE_ES6_PROXY
#elif defined(DUK_OPT_NO_ES6_PROXY)
#undef DUK_USE_ES6_PROXY
#else
#define DUK_USE_ES6_PROXY
#endif

#undef DUK_USE_EXEC_INDIRECT_BOUND_CHECK
#if defined(DUK_OPT_DEBUG) || defined(DUK_OPT_ASSERTIONS)
/* Enabled with debug/assertions just so that any issues can be caught. */
#define DUK_USE_EXEC_INDIRECT_BOUND_CHECK
#endif

#undef DUK_USE_EXEC_TIMEOUT_CHECK
#if defined(DUK_OPT_EXEC_TIMEOUT_CHECK)
#define DUK_USE_EXEC_TIMEOUT_CHECK(udata)  DUK_OPT_EXEC_TIMEOUT_CHECK((udata))
#endif

#undef DUK_USE_EXTSTR_FREE
#if defined(DUK_OPT_EXTERNAL_STRINGS) && defined(DUK_OPT_EXTSTR_FREE)
#define DUK_USE_EXTSTR_FREE(udata,ptr) DUK_OPT_EXTSTR_FREE((udata), (ptr))
#endif

#undef DUK_USE_EXTSTR_INTERN_CHECK
#if defined(DUK_OPT_EXTERNAL_STRINGS) && defined(DUK_OPT_EXTSTR_INTERN_CHECK)
#define DUK_USE_EXTSTR_INTERN_CHECK(udata,ptr,len) DUK_OPT_EXTSTR_INTERN_CHECK((udata), (ptr), (len))
#endif

/* Support for 48-bit signed integer duk_tval with transparent semantics. */
#undef DUK_USE_FASTINT
#if defined(DUK_OPT_FASTINT)
#if !defined(DUK_F_HAVE_64BIT)
#error DUK_OPT_FASTINT requires 64-bit integer type support at the moment
#endif
#define DUK_USE_FASTINT
#endif

#if defined(DUK_OPT_FILE_IO)
#define DUK_USE_FILE_IO
#elif defined(DUK_OPT_NO_FILE_IO)
#undef DUK_USE_FILE_IO
#else
#define DUK_USE_FILE_IO
#endif

#if defined(DUK_OPT_FUNCPTR16)
#define DUK_USE_FUNCPTR16
#elif defined(DUK_OPT_NO_FUNCPTR16)
#undef DUK_USE_FUNCPTR16
#else
#undef DUK_USE_FUNCPTR16
#endif

#if defined(DUK_OPT_FUNCPTR_DEC16)
#define DUK_USE_FUNCPTR_DEC16(udata,ptr) DUK_OPT_FUNCPTR_DEC16((udata),(ptr))
#else
#undef DUK_USE_FUNCPTR_DEC16
#endif

#if defined(DUK_OPT_FUNCPTR_ENC16)
#define DUK_USE_FUNCPTR_ENC16(udata,ptr) DUK_OPT_FUNCPTR_ENC16((udata),(ptr))
#else
#undef DUK_USE_FUNCPTR_ENC16
#endif

#if defined(DUK_OPT_GC_TORTURE)
#define DUK_USE_GC_TORTURE
#elif defined(DUK_OPT_NO_GC_TORTURE)
#undef DUK_USE_GC_TORTURE
#else
#undef DUK_USE_GC_TORTURE
#endif

#if defined(DUK_OPT_HEAPPTR16)
#define DUK_USE_HEAPPTR16
#elif defined(DUK_OPT_NO_HEAPPTR16)
#undef DUK_USE_HEAPPTR16
#else
#undef DUK_USE_HEAPPTR16
#endif

#if defined(DUK_OPT_HEAPPTR_DEC16)
#define DUK_USE_HEAPPTR_DEC16(udata,ptr) DUK_OPT_HEAPPTR_DEC16((udata),(ptr))
#else
#undef DUK_USE_HEAPPTR_DEC16
#endif

#if defined(DUK_OPT_HEAPPTR_ENC16)
#define DUK_USE_HEAPPTR_ENC16(udata,ptr) DUK_OPT_HEAPPTR_ENC16((udata),(ptr))
#else
#undef DUK_USE_HEAPPTR_ENC16
#endif

/* For now, hash part is dropped if and only if 16-bit object fields are used. */
#define DUK_USE_HOBJECT_HASH_PART
#if defined(DUK_OPT_OBJSIZES16)
#undef DUK_USE_HOBJECT_HASH_PART
#endif

#if defined(DUK_OPT_EXTERNAL_STRINGS)
#define DUK_USE_HSTRING_EXTDATA
#elif defined(DUK_OPT_NO_EXTERNAL_STRINGS)
#undef DUK_USE_HSTRING_EXTDATA
#else
#undef DUK_USE_HSTRING_EXTDATA
#endif

#if defined(DUK_OPT_INTERRUPT_COUNTER)
#define DUK_USE_INTERRUPT_COUNTER
#elif defined(DUK_OPT_NO_INTERRUPT_COUNTER)
#undef DUK_USE_INTERRUPT_COUNTER
#else
#undef DUK_USE_INTERRUPT_COUNTER
#endif

#if defined(DUK_OPT_JC)
#define DUK_USE_JC
#elif defined(DUK_OPT_NO_JC)
#undef DUK_USE_JC
#else
#define DUK_USE_JC
#endif

#if defined(DUK_OPT_JSON_STRINGIFY_FASTPATH)
#define DUK_USE_JSON_STRINGIFY_FASTPATH
#elif defined(DUK_OPT_NO_JSON_STRINGIFY_FASTPATH)
#undef DUK_USE_JSON_STRINGIFY_FASTPATH
#else
#undef DUK_USE_JSON_STRINGIFY_FASTPATH
#endif

#if defined(DUK_OPT_JX)
#define DUK_USE_JX
#elif defined(DUK_OPT_NO_JX)
#undef DUK_USE_JX
#else
#define DUK_USE_JX
#endif

#if defined(DUK_OPT_LIGHTFUNC_BUILTINS)
#define DUK_USE_LIGHTFUNC_BUILTINS
#elif defined(DUK_OPT_NO_LIGHTFUNC_BUILTINS)
#undef DUK_USE_LIGHTFUNC_BUILTINS
#else
#undef DUK_USE_LIGHTFUNC_BUILTINS
#endif

#if defined(DUK_OPT_MARK_AND_SWEEP)
#define DUK_USE_MARK_AND_SWEEP
#elif defined(DUK_OPT_NO_MARK_AND_SWEEP)
#undef DUK_USE_MARK_AND_SWEEP
#else
#define DUK_USE_MARK_AND_SWEEP
#endif

#if defined(DUK_OPT_MS_STRINGTABLE_RESIZE)
#define DUK_USE_MS_STRINGTABLE_RESIZE
#elif defined(DUK_OPT_NO_MS_STRINGTABLE_RESIZE)
#undef DUK_USE_MS_STRINGTABLE_RESIZE
#else
#define DUK_USE_MS_STRINGTABLE_RESIZE
#endif

#if defined(DUK_OPT_NONSTD_ARRAY_CONCAT_TRAILER)
#define DUK_USE_NONSTD_ARRAY_CONCAT_TRAILER
#elif defined(DUK_OPT_NO_NONSTD_ARRAY_CONCAT_TRAILER)
#undef DUK_USE_NONSTD_ARRAY_CONCAT_TRAILER
#else
#define DUK_USE_NONSTD_ARRAY_CONCAT_TRAILER
#endif

#if defined(DUK_OPT_NONSTD_ARRAY_MAP_TRAILER)
#define DUK_USE_NONSTD_ARRAY_MAP_TRAILER
#elif defined(DUK_OPT_NO_NONSTD_ARRAY_MAP_TRAILER)
#undef DUK_USE_NONSTD_ARRAY_MAP_TRAILER
#else
#define DUK_USE_NONSTD_ARRAY_MAP_TRAILER
#endif

#if defined(DUK_OPT_NONSTD_ARRAY_SPLICE_DELCOUNT)
#define DUK_USE_NONSTD_ARRAY_SPLICE_DELCOUNT
#elif defined(DUK_OPT_NO_NONSTD_ARRAY_SPLICE_DELCOUNT)
#undef DUK_USE_NONSTD_ARRAY_SPLICE_DELCOUNT
#else
#define DUK_USE_NONSTD_ARRAY_SPLICE_DELCOUNT
#endif

#if defined(DUK_OPT_NONSTD_FUNC_CALLER_PROPERTY)
#define DUK_USE_NONSTD_FUNC_CALLER_PROPERTY
#elif defined(DUK_OPT_NO_NONSTD_FUNC_CALLER_PROPERTY)
#undef DUK_USE_NONSTD_FUNC_CALLER_PROPERTY
#else
#undef DUK_USE_NONSTD_FUNC_CALLER_PROPERTY
#endif

#if defined(DUK_OPT_NONSTD_FUNC_SOURCE_PROPERTY)
#define DUK_USE_NONSTD_FUNC_SOURCE_PROPERTY
#elif defined(DUK_OPT_NO_NONSTD_FUNC_SOURCE_PROPERTY)
#undef DUK_USE_NONSTD_FUNC_SOURCE_PROPERTY
#else
#undef DUK_USE_NONSTD_FUNC_SOURCE_PROPERTY
#endif

#if defined(DUK_OPT_NONSTD_FUNC_STMT)
#define DUK_USE_NONSTD_FUNC_STMT
#elif defined(DUK_OPT_NO_NONSTD_FUNC_STMT)
#undef DUK_USE_NONSTD_FUNC_STMT
#else
#define DUK_USE_NONSTD_FUNC_STMT
#endif

#if defined(DUK_OPT_NONSTD_ACCESSOR_KEY_ARGUMENT)
#define DUK_USE_NONSTD_GETTER_KEY_ARGUMENT
#elif defined(DUK_OPT_NO_NONSTD_ACCESSOR_KEY_ARGUMENT)
#undef DUK_USE_NONSTD_GETTER_KEY_ARGUMENT
#else
#define DUK_USE_NONSTD_GETTER_KEY_ARGUMENT
#endif

#if defined(DUK_OPT_NONSTD_JSON_ESC_U2028_U2029)
#define DUK_USE_NONSTD_JSON_ESC_U2028_U2029
#elif defined(DUK_OPT_NO_NONSTD_JSON_ESC_U2028_U2029)
#undef DUK_USE_NONSTD_JSON_ESC_U2028_U2029
#else
#define DUK_USE_NONSTD_JSON_ESC_U2028_U2029
#endif

#if defined(DUK_OPT_NONSTD_REGEXP_DOLLAR_ESCAPE)
#define DUK_USE_NONSTD_REGEXP_DOLLAR_ESCAPE
#elif defined(DUK_OPT_NO_NONSTD_REGEXP_DOLLAR_ESCAPE)
#undef DUK_USE_NONSTD_REGEXP_DOLLAR_ESCAPE
#else
#define DUK_USE_NONSTD_REGEXP_DOLLAR_ESCAPE
#endif

#if defined(DUK_OPT_NONSTD_ACCESSOR_KEY_ARGUMENT)
#define DUK_USE_NONSTD_SETTER_KEY_ARGUMENT
#elif defined(DUK_OPT_NO_NONSTD_ACCESSOR_KEY_ARGUMENT)
#undef DUK_USE_NONSTD_SETTER_KEY_ARGUMENT
#else
#define DUK_USE_NONSTD_SETTER_KEY_ARGUMENT
#endif

#if defined(DUK_OPT_NONSTD_STRING_FROMCHARCODE_32BIT)
#define DUK_USE_NONSTD_STRING_FROMCHARCODE_32BIT
#elif defined(DUK_OPT_NO_NONSTD_STRING_FROMCHARCODE_32BIT)
#undef DUK_USE_NONSTD_STRING_FROMCHARCODE_32BIT
#else
#define DUK_USE_NONSTD_STRING_FROMCHARCODE_32BIT
#endif

#if defined(DUK_OPT_OBJSIZES16)
#define DUK_USE_OBJSIZES16
#elif defined(DUK_OPT_NO_OBJSIZES16)
#undef DUK_USE_OBJSIZES16
#else
#undef DUK_USE_OBJSIZES16
#endif

#if defined(DUK_OPT_OCTAL_SUPPORT)
#define DUK_USE_OCTAL_SUPPORT
#elif defined(DUK_OPT_NO_OCTAL_SUPPORT)
#undef DUK_USE_OCTAL_SUPPORT
#else
#define DUK_USE_OCTAL_SUPPORT
#endif

#if defined(DUK_OPT_PACKED_TVAL)
#define DUK_USE_PACKED_TVAL
#elif defined(DUK_OPT_NO_PACKED_TVAL)
#undef DUK_USE_PACKED_TVAL
#else
#undef DUK_USE_PACKED_TVAL
#endif

#undef DUK_USE_PANIC_ABORT
#if !defined(DUK_OPT_SEGFAULT_ON_PANIC)
#define DUK_USE_PANIC_ABORT
#endif

#undef DUK_USE_PANIC_HANDLER
#if defined(DUK_OPT_PANIC_HANDLER)
#define DUK_USE_PANIC_HANDLER(code,msg) DUK_OPT_PANIC_HANDLER((code),(msg))
#endif

#undef DUK_USE_PANIC_SEGFAULT
#if defined(DUK_OPT_SEGFAULT_ON_PANIC)
#define DUK_USE_PANIC_SEGFAULT
#endif

#if defined(DUK_OPT_PARANOID_ERRORS)
#define DUK_USE_PARANOID_ERRORS
#elif defined(DUK_OPT_NO_PARANOID_ERRORS)
#undef DUK_USE_PARANOID_ERRORS
#else
#undef DUK_USE_PARANOID_ERRORS
#endif

#if defined(DUK_OPT_PC2LINE)
#define DUK_USE_PC2LINE
#elif defined(DUK_OPT_NO_PC2LINE)
#undef DUK_USE_PC2LINE
#else
#define DUK_USE_PC2LINE
#endif

#if defined(DUK_OPT_REFCOUNT16)
#define DUK_USE_REFCOUNT16
#elif defined(DUK_OPT_NO_REFCOUNT16)
#undef DUK_USE_REFCOUNT16
#else
#undef DUK_USE_REFCOUNT16
#endif

#if defined(DUK_OPT_REFERENCE_COUNTING)
#define DUK_USE_REFERENCE_COUNTING
#elif defined(DUK_OPT_NO_REFERENCE_COUNTING)
#undef DUK_USE_REFERENCE_COUNTING
#else
#define DUK_USE_REFERENCE_COUNTING
#endif

#if defined(DUK_OPT_REGEXP_CANON_WORKAROUND)
#define DUK_USE_REGEXP_CANON_WORKAROUND
#elif defined(DUK_OPT_NO_REGEXP_CANON_WORKAROUND)
#undef DUK_USE_REGEXP_CANON_WORKAROUND
#else
#undef DUK_USE_REGEXP_CANON_WORKAROUND
#endif

#if defined(DUK_OPT_REGEXP_SUPPORT)
#define DUK_USE_REGEXP_SUPPORT
#elif defined(DUK_OPT_NO_REGEXP_SUPPORT)
#undef DUK_USE_REGEXP_SUPPORT
#else
#define DUK_USE_REGEXP_SUPPORT
#endif

#if defined(DUK_OPT_SECTION_B)
#define DUK_USE_SECTION_B
#elif defined(DUK_OPT_NO_SECTION_B)
#undef DUK_USE_SECTION_B
#else
#define DUK_USE_SECTION_B
#endif

#if defined(DUK_OPT_SELF_TESTS)
#define DUK_USE_SELF_TESTS
#elif defined(DUK_OPT_NO_SELF_TESTS)
#undef DUK_USE_SELF_TESTS
#else
#undef DUK_USE_SELF_TESTS
#endif

#if defined(DUK_OPT_SETJMP)
#define DUK_USE_SETJMP
#elif defined(DUK_OPT_NO_SETJMP)
#undef DUK_USE_SETJMP
#else
#define DUK_USE_SETJMP
#endif

#if defined(DUK_OPT_SHUFFLE_TORTURE)
#define DUK_USE_SHUFFLE_TORTURE
#elif defined(DUK_OPT_NO_SHUFFLE_TORTURE)
#undef DUK_USE_SHUFFLE_TORTURE
#else
#undef DUK_USE_SHUFFLE_TORTURE
#endif

#if defined(DUK_OPT_SIGSETJMP)
#define DUK_USE_SIGSETJMP
#elif defined(DUK_OPT_NO_SIGSETJMP)
#undef DUK_USE_SIGSETJMP
#else
#undef DUK_USE_SIGSETJMP
#endif

#if defined(DUK_OPT_SOURCE_NONBMP)
#define DUK_USE_SOURCE_NONBMP
#elif defined(DUK_OPT_NO_SOURCE_NONBMP)
#undef DUK_USE_SOURCE_NONBMP
#else
#define DUK_USE_SOURCE_NONBMP
#endif

#if defined(DUK_OPT_STRHASH16)
#define DUK_USE_STRHASH16
#elif defined(DUK_OPT_NO_STRHASH16)
#undef DUK_USE_STRHASH16
#else
#undef DUK_USE_STRHASH16
#endif

#if defined(DUK_OPT_STRICT_DECL)
#define DUK_USE_STRICT_DECL
#elif defined(DUK_OPT_NO_STRICT_DECL)
#undef DUK_USE_STRICT_DECL
#else
#define DUK_USE_STRICT_DECL
#endif

#if defined(DUK_OPT_STRICT_UTF8_SOURCE)
#define DUK_USE_STRICT_UTF8_SOURCE
#elif defined(DUK_OPT_NO_STRICT_UTF8_SOURCE)
#undef DUK_USE_STRICT_UTF8_SOURCE
#else
#undef DUK_USE_STRICT_UTF8_SOURCE
#endif

#if defined(DUK_OPT_STRLEN16)
#define DUK_USE_STRLEN16
#elif defined(DUK_OPT_NO_STRLEN16)
#undef DUK_USE_STRLEN16
#else
#undef DUK_USE_STRLEN16
#endif

#undef DUK_USE_STRTAB_CHAIN
#if defined(DUK_OPT_STRTAB_CHAIN) && defined(DUK_OPT_STRTAB_CHAIN_SIZE)
#define DUK_USE_STRTAB_CHAIN
#endif

#undef DUK_USE_STRTAB_CHAIN_SIZE
#if defined(DUK_OPT_STRTAB_CHAIN) && defined(DUK_OPT_STRTAB_CHAIN_SIZE)
/* Low memory algorithm: separate chaining using arrays, fixed size hash */
#define DUK_USE_STRTAB_CHAIN_SIZE  DUK_OPT_STRTAB_CHAIN_SIZE
#endif

#undef DUK_USE_STRTAB_PROBE
#if !(defined(DUK_OPT_STRTAB_CHAIN) && defined(DUK_OPT_STRTAB_CHAIN_SIZE))
#define DUK_USE_STRTAB_PROBE
#endif

#if defined(DUK_OPT_NONSTD_FUNC_CALLER_PROPERTY)
#undef DUK_USE_TAILCALL
#else
#define DUK_USE_TAILCALL
#endif

#if defined(DUK_OPT_TARGET_INFO)
#define DUK_USE_TARGET_INFO DUK_OPT_TARGET_INFO
#else
#define DUK_USE_TARGET_INFO "unknown"
#endif

#if defined(DUK_OPT_NO_AUGMENT_ERRORS)
#undef DUK_USE_TRACEBACKS
#elif defined(DUK_OPT_NO_TRACEBACKS)
#undef DUK_USE_TRACEBACKS
#else
#define DUK_USE_TRACEBACKS
#endif

#if defined(DUK_OPT_TRACEBACK_DEPTH)
#define DUK_USE_TRACEBACK_DEPTH  DUK_OPT_TRACEBACK_DEPTH
#else
#define DUK_USE_TRACEBACK_DEPTH  10
#endif

#if defined(DUK_OPT_UNDERSCORE_SETJMP)
#define DUK_USE_UNDERSCORE_SETJMP
#elif defined(DUK_OPT_NO_UNDERSCORE_SETJMP)
#undef DUK_USE_UNDERSCORE_SETJMP
#else
#undef DUK_USE_UNDERSCORE_SETJMP
#endif

#if defined(DUK_OPT_DECLARE)
#define DUK_USE_USER_DECLARE() DUK_OPT_DECLARE
#else
#define DUK_USE_USER_DECLARE() /* no user declarations */
#endif

/* User provided InitJS. */
#undef DUK_USE_USER_INITJS
#if defined(DUK_OPT_USER_INITJS)
#define DUK_USE_USER_INITJS (DUK_OPT_USER_INITJS)
#endif

#if defined(DUK_OPT_VERBOSE_ERRORS)
#define DUK_USE_VERBOSE_ERRORS
#elif defined(DUK_OPT_NO_VERBOSE_ERRORS)
#undef DUK_USE_VERBOSE_ERRORS
#else
#define DUK_USE_VERBOSE_ERRORS
#endif

#if defined(DUK_OPT_VOLUNTARY_GC)
#define DUK_USE_VOLUNTARY_GC
#elif defined(DUK_OPT_NO_VOLUNTARY_GC)
#undef DUK_USE_VOLUNTARY_GC
#else
#define DUK_USE_VOLUNTARY_GC
#endif

#if defined(DUK_OPT_ZERO_BUFFER_DATA)
#define DUK_USE_ZERO_BUFFER_DATA
#elif defined(DUK_OPT_NO_ZERO_BUFFER_DATA)
#undef DUK_USE_ZERO_BUFFER_DATA
#else
#define DUK_USE_ZERO_BUFFER_DATA
#endif

/*
 *  Autogenerated defaults
 */

#define DUK_USE_AVOID_PLATFORM_FUNCPTRS
#define DUK_USE_BASE64_FASTPATH
#define DUK_USE_BUILTIN_INITJS
#define DUK_USE_COMPILER_RECLIMIT 2500
#undef DUK_USE_DATE_FORMAT_STRING
#undef DUK_USE_DATE_GET_LOCAL_TZOFFSET
#undef DUK_USE_DATE_GET_NOW
#undef DUK_USE_DATE_PARSE_STRING
#undef DUK_USE_DATE_PRS_GETDATE
#define DUK_USE_ESBC_LIMITS
#define DUK_USE_ESBC_MAX_BYTES 2147418112L
#define DUK_USE_ESBC_MAX_LINENUMBER 2147418112L
#undef DUK_USE_EXEC_FUN_LOCAL
#undef DUK_USE_EXPLICIT_NULL_INIT
#define DUK_USE_FAST_REFCOUNT_DEFAULT
#define DUK_USE_HEX_FASTPATH
#undef DUK_USE_INTERRUPT_DEBUG_FIXUP
#define DUK_USE_JSON_DECNUMBER_FASTPATH
#define DUK_USE_JSON_DECSTRING_FASTPATH
#define DUK_USE_JSON_DEC_RECLIMIT 1000
#define DUK_USE_JSON_EATWHITE_FASTPATH
#define DUK_USE_JSON_ENC_RECLIMIT 1000
#define DUK_USE_JSON_QUOTESTRING_FASTPATH
#define DUK_USE_LEXER_SLIDING_WINDOW
#undef DUK_USE_MARKANDSWEEP_FINALIZER_TORTURE
#define DUK_USE_MARK_AND_SWEEP_RECLIMIT 256
#define DUK_USE_MATH_BUILTIN
#define DUK_USE_NATIVE_CALL_RECLIMIT 1000
#undef DUK_USE_PANIC_EXIT
#undef DUK_USE_PREFER_SIZE
#define DUK_USE_PROVIDE_DEFAULT_ALLOC_FUNCTIONS
#undef DUK_USE_REFZERO_FINALIZER_TORTURE
#define DUK_USE_REGEXP_COMPILER_RECLIMIT 10000
#define DUK_USE_REGEXP_EXECUTOR_RECLIMIT 10000
#undef DUK_USE_STRHASH_DENSE
#define DUK_USE_STRHASH_SKIP_SHIFT 5
#undef DUK_USE_VALSTACK_UNSAFE
#define DUK_USE_VERBOSE_EXECUTOR_ERRORS

/*
 *  Alternative customization header
 *
 *  If you want to modify the final DUK_USE_xxx flags directly (without
 *  using the available DUK_OPT_xxx flags), define DUK_OPT_HAVE_CUSTOM_H
 *  and tweak the final flags there.
 */

#if defined(DUK_OPT_HAVE_CUSTOM_H)
#include "duk_custom.h"
#endif

/*
 *  You may add overriding #define/#undef directives below for
 *  customization.  You of course cannot un-#include or un-typedef
 *  anything; these require direct changes above.
 */

/* __OVERRIDE_DEFINES__ */

/*
 *  Date provider selection
 *
 *  User may define DUK_USE_DATE_GET_NOW() etc directly, in which case we'll
 *  rely on an external provider.  If this is not done, revert to previous
 *  behavior and use Unix/Windows built-in provider.
 */

#if defined(DUK_COMPILING_DUKTAPE)

#if defined(DUK_USE_DATE_GET_NOW)
/* External provider already defined. */
#elif defined(DUK_USE_DATE_NOW_GETTIMEOFDAY)
#define DUK_USE_DATE_GET_NOW(ctx)            duk_bi_date_get_now_gettimeofday((ctx))
#elif defined(DUK_USE_DATE_NOW_TIME)
#define DUK_USE_DATE_GET_NOW(ctx)            duk_bi_date_get_now_time((ctx))
#elif defined(DUK_USE_DATE_NOW_WINDOWS)
#define DUK_USE_DATE_GET_NOW(ctx)            duk_bi_date_get_now_windows((ctx))
#else
#error no provider for DUK_USE_DATE_GET_NOW()
#endif

#if defined(DUK_USE_DATE_GET_LOCAL_TZOFFSET)
/* External provider already defined. */
#elif defined(DUK_USE_DATE_TZO_GMTIME_R) || defined(DUK_USE_DATE_TZO_GMTIME)
#define DUK_USE_DATE_GET_LOCAL_TZOFFSET(d)   duk_bi_date_get_local_tzoffset_gmtime((d))
#elif defined(DUK_USE_DATE_TZO_WINDOWS)
#define DUK_USE_DATE_GET_LOCAL_TZOFFSET(d)   duk_bi_date_get_local_tzoffset_windows((d))
#else
#error no provider for DUK_USE_DATE_GET_LOCAL_TZOFFSET()
#endif

#if defined(DUK_USE_DATE_PARSE_STRING)
/* External provider already defined. */
#elif defined(DUK_USE_DATE_PRS_STRPTIME)
#define DUK_USE_DATE_PARSE_STRING(ctx,str)   duk_bi_date_parse_string_strptime((ctx), (str))
#elif defined(DUK_USE_DATE_PRS_GETDATE)
#define DUK_USE_DATE_PARSE_STRING(ctx,str)   duk_bi_date_parse_string_getdate((ctx), (str))
#else
/* No provider for DUK_USE_DATE_PARSE_STRING(), fall back to ISO 8601 only. */
#endif

#if defined(DUK_USE_DATE_FORMAT_STRING)
/* External provider already defined. */
#elif defined(DUK_USE_DATE_FMT_STRFTIME)
#define DUK_USE_DATE_FORMAT_STRING(ctx,parts,tzoffset,flags) \
	duk_bi_date_format_parts_strftime((ctx), (parts), (tzoffset), (flags))
#else
/* No provider for DUK_USE_DATE_FORMAT_STRING(), fall back to ISO 8601 only. */
#endif

#endif  /* DUK_COMPILING_DUKTAPE */

/*
 *  Checks for config option consistency (DUK_USE_xxx)
 */

#if defined(DUK_USE_32BIT_PTRS)
#error unsupported config option used (option has been removed): DUK_USE_32BIT_PTRS
#endif
#if defined(DUK_USE_ALIGN_4)
#error unsupported config option used (option has been removed): DUK_USE_ALIGN_4
#endif
#if defined(DUK_USE_ALIGN_8)
#error unsupported config option used (option has been removed): DUK_USE_ALIGN_8
#endif
#if defined(DUK_USE_BYTEORDER_FORCED)
#error unsupported config option used (option has been removed): DUK_USE_BYTEORDER_FORCED
#endif
#if defined(DUK_USE_DATAPTR_DEC16) && !defined(DUK_USE_DATAPTR16)
#error config option DUK_USE_DATAPTR_DEC16 requires option DUK_USE_DATAPTR16 (which is missing)
#endif
#if defined(DUK_USE_DATAPTR_ENC16) && !defined(DUK_USE_DATAPTR16)
#error config option DUK_USE_DATAPTR_ENC16 requires option DUK_USE_DATAPTR16 (which is missing)
#endif
#if defined(DUK_USE_DEBUGGER_SUPPORT) && !defined(DUK_USE_INTERRUPT_COUNTER)
#error config option DUK_USE_DEBUGGER_SUPPORT requires option DUK_USE_INTERRUPT_COUNTER (which is missing)
#endif
#if defined(DUK_USE_DEEP_C_STACK)
#error unsupported config option used (option has been removed): DUK_USE_DEEP_C_STACK
#endif
#if defined(DUK_USE_DOUBLE_BE)
#error unsupported config option used (option has been removed): DUK_USE_DOUBLE_BE
#endif
#if defined(DUK_USE_DOUBLE_BE) && defined(DUK_USE_DOUBLE_LE)
#error config option DUK_USE_DOUBLE_BE conflicts with option DUK_USE_DOUBLE_LE (which is also defined)
#endif
#if defined(DUK_USE_DOUBLE_BE) && defined(DUK_USE_DOUBLE_ME)
#error config option DUK_USE_DOUBLE_BE conflicts with option DUK_USE_DOUBLE_ME (which is also defined)
#endif
#if defined(DUK_USE_DOUBLE_LE)
#error unsupported config option used (option has been removed): DUK_USE_DOUBLE_LE
#endif
#if defined(DUK_USE_DOUBLE_LE) && defined(DUK_USE_DOUBLE_BE)
#error config option DUK_USE_DOUBLE_LE conflicts with option DUK_USE_DOUBLE_BE (which is also defined)
#endif
#if defined(DUK_USE_DOUBLE_LE) && defined(DUK_USE_DOUBLE_ME)
#error config option DUK_USE_DOUBLE_LE conflicts with option DUK_USE_DOUBLE_ME (which is also defined)
#endif
#if defined(DUK_USE_DOUBLE_ME)
#error unsupported config option used (option has been removed): DUK_USE_DOUBLE_ME
#endif
#if defined(DUK_USE_DOUBLE_ME) && defined(DUK_USE_DOUBLE_LE)
#error config option DUK_USE_DOUBLE_ME conflicts with option DUK_USE_DOUBLE_LE (which is also defined)
#endif
#if defined(DUK_USE_DOUBLE_ME) && defined(DUK_USE_DOUBLE_BE)
#error config option DUK_USE_DOUBLE_ME conflicts with option DUK_USE_DOUBLE_BE (which is also defined)
#endif
#if defined(DUK_USE_DPRINT) && !defined(DUK_USE_DEBUG)
#error config option DUK_USE_DPRINT requires option DUK_USE_DEBUG (which is missing)
#endif
#if defined(DUK_USE_ESBC_MAX_BYTES) && !defined(DUK_USE_ESBC_LIMITS)
#error config option DUK_USE_ESBC_MAX_BYTES requires option DUK_USE_ESBC_LIMITS (which is missing)
#endif
#if defined(DUK_USE_ESBC_MAX_LINENUMBER) && !defined(DUK_USE_ESBC_LIMITS)
#error config option DUK_USE_ESBC_MAX_LINENUMBER requires option DUK_USE_ESBC_LIMITS (which is missing)
#endif
#if defined(DUK_USE_EXEC_TIMEOUT_CHECK) && !defined(DUK_USE_INTERRUPT_COUNTER)
#error config option DUK_USE_EXEC_TIMEOUT_CHECK requires option DUK_USE_INTERRUPT_COUNTER (which is missing)
#endif
#if defined(DUK_USE_EXTSTR_FREE) && !defined(DUK_USE_HSTRING_EXTDATA)
#error config option DUK_USE_EXTSTR_FREE requires option DUK_USE_HSTRING_EXTDATA (which is missing)
#endif
#if defined(DUK_USE_EXTSTR_INTERN_CHECK) && !defined(DUK_USE_HSTRING_EXTDATA)
#error config option DUK_USE_EXTSTR_INTERN_CHECK requires option DUK_USE_HSTRING_EXTDATA (which is missing)
#endif
#if defined(DUK_USE_FULL_TVAL)
#error unsupported config option used (option has been removed): DUK_USE_FULL_TVAL
#endif
#if defined(DUK_USE_FUNCPTR_DEC16) && !defined(DUK_USE_FUNCPTR16)
#error config option DUK_USE_FUNCPTR_DEC16 requires option DUK_USE_FUNCPTR16 (which is missing)
#endif
#if defined(DUK_USE_FUNCPTR_ENC16) && !defined(DUK_USE_FUNCPTR16)
#error config option DUK_USE_FUNCPTR_ENC16 requires option DUK_USE_FUNCPTR16 (which is missing)
#endif
#if defined(DUK_USE_HASHBYTES_UNALIGNED_U32_ACCESS)
#error unsupported config option used (option has been removed): DUK_USE_HASHBYTES_UNALIGNED_U32_ACCESS
#endif
#if defined(DUK_USE_HEAPPTR16) && defined(DUK_USE_DEBUG)
#error config option DUK_USE_HEAPPTR16 conflicts with option DUK_USE_DEBUG (which is also defined)
#endif
#if defined(DUK_USE_HEAPPTR_DEC16) && !defined(DUK_USE_HEAPPTR16)
#error config option DUK_USE_HEAPPTR_DEC16 requires option DUK_USE_HEAPPTR16 (which is missing)
#endif
#if defined(DUK_USE_HEAPPTR_ENC16) && !defined(DUK_USE_HEAPPTR16)
#error config option DUK_USE_HEAPPTR_ENC16 requires option DUK_USE_HEAPPTR16 (which is missing)
#endif
#if defined(DUK_USE_INTEGER_BE)
#error unsupported config option used (option has been removed): DUK_USE_INTEGER_BE
#endif
#if defined(DUK_USE_INTEGER_BE) && defined(DUK_USE_INTEGER_LE)
#error config option DUK_USE_INTEGER_BE conflicts with option DUK_USE_INTEGER_LE (which is also defined)
#endif
#if defined(DUK_USE_INTEGER_BE) && defined(DUK_USE_INTEGER_ME)
#error config option DUK_USE_INTEGER_BE conflicts with option DUK_USE_INTEGER_ME (which is also defined)
#endif
#if defined(DUK_USE_INTEGER_LE)
#error unsupported config option used (option has been removed): DUK_USE_INTEGER_LE
#endif
#if defined(DUK_USE_INTEGER_LE) && defined(DUK_USE_INTEGER_BE)
#error config option DUK_USE_INTEGER_LE conflicts with option DUK_USE_INTEGER_BE (which is also defined)
#endif
#if defined(DUK_USE_INTEGER_LE) && defined(DUK_USE_INTEGER_ME)
#error config option DUK_USE_INTEGER_LE conflicts with option DUK_USE_INTEGER_ME (which is also defined)
#endif
#if defined(DUK_USE_INTEGER_ME)
#error unsupported config option used (option has been removed): DUK_USE_INTEGER_ME
#endif
#if defined(DUK_USE_INTEGER_ME) && defined(DUK_USE_INTEGER_LE)
#error config option DUK_USE_INTEGER_ME conflicts with option DUK_USE_INTEGER_LE (which is also defined)
#endif
#if defined(DUK_USE_INTEGER_ME) && defined(DUK_USE_INTEGER_BE)
#error config option DUK_USE_INTEGER_ME conflicts with option DUK_USE_INTEGER_BE (which is also defined)
#endif
#if defined(DUK_USE_NO_DOUBLE_ALIASING_SELFTEST)
#error unsupported config option used (option has been removed): DUK_USE_NO_DOUBLE_ALIASING_SELFTEST
#endif
#if defined(DUK_USE_PACKED_TVAL_POSSIBLE)
#error unsupported config option used (option has been removed): DUK_USE_PACKED_TVAL_POSSIBLE
#endif
#if defined(DUK_USE_RDTSC)
#error unsupported config option used (option has been removed): DUK_USE_RDTSC
#endif
#if defined(DUK_USE_STRTAB_CHAIN_SIZE) && !defined(DUK_USE_STRTAB_CHAIN)
#error config option DUK_USE_STRTAB_CHAIN_SIZE requires option DUK_USE_STRTAB_CHAIN (which is missing)
#endif
#if defined(DUK_USE_TAILCALL) && defined(DUK_USE_NONSTD_FUNC_CALLER_PROPERTY)
#error config option DUK_USE_TAILCALL conflicts with option DUK_USE_NONSTD_FUNC_CALLER_PROPERTY (which is also defined)
#endif
#if defined(DUK_USE_UNALIGNED_ACCESSES_POSSIBLE)
#error unsupported config option used (option has been removed): DUK_USE_UNALIGNED_ACCESSES_POSSIBLE
#endif

#if defined(DUK_USE_CPP_EXCEPTIONS) && !defined(__cplusplus)
#error DUK_USE_CPP_EXCEPTIONS enabled but not compiling with a C++ compiler
#endif

/*
 *  Convert DUK_USE_BYTEORDER, from whatever source, into currently used
 *  internal defines.  If detection failed, #error out.
 */

#if defined(DUK_USE_BYTEORDER)
#if (DUK_USE_BYTEORDER == 1)
#define DUK_USE_INTEGER_LE
#define DUK_USE_DOUBLE_LE
#elif (DUK_USE_BYTEORDER == 2)
#define DUK_USE_INTEGER_LE  /* integer endianness is little on purpose */
#define DUK_USE_DOUBLE_ME
#elif (DUK_USE_BYTEORDER == 3)
#define DUK_USE_INTEGER_BE
#define DUK_USE_DOUBLE_BE
#else
#error unsupported: byte order invalid
#endif  /* byte order */
#else
#error unsupported: byte order detection failed
#endif  /* defined(DUK_USE_BYTEORDER) */

#endif  /* DUK_CONFIG_H_INCLUDED */
/*==============================================================*/

/*
 *  BEGIN PUBLIC API
 */

#ifndef DUK_API_PUBLIC_H_INCLUDED
#define DUK_API_PUBLIC_H_INCLUDED

/*
 *  Avoid C++ name mangling
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Some defines forwarded from feature detection
 */

#undef DUK_API_VARIADIC_MACROS
#ifdef DUK_USE_VARIADIC_MACROS
#define DUK_API_VARIADIC_MACROS
#endif

#define DUK_API_NORETURN(decl) DUK_NORETURN(decl)

/*
 *  Public API specific typedefs
 *
 *  Many types are wrapped by Duktape for portability to rare platforms
 *  where e.g. 'int' is a 16-bit type.  See practical typing discussion
 *  in Duktape web documentation.
 */

struct duk_memory_functions;
struct duk_function_list_entry;
struct duk_number_list_entry;

/* duk_context is now defined in duk_config.h because it may also be
 * referenced there by prototypes.
 */
typedef struct duk_memory_functions duk_memory_functions;
typedef struct duk_function_list_entry duk_function_list_entry;
typedef struct duk_number_list_entry duk_number_list_entry;

typedef duk_ret_t (*duk_c_function)(duk_context *ctx);
typedef void *(*duk_alloc_function) (void *udata, duk_size_t size);
typedef void *(*duk_realloc_function) (void *udata, void *ptr, duk_size_t size);
typedef void (*duk_free_function) (void *udata, void *ptr);
typedef void (*duk_fatal_function) (duk_context *ctx, duk_errcode_t code, const char *msg);
typedef void (*duk_decode_char_function) (void *udata, duk_codepoint_t codepoint);
typedef duk_codepoint_t (*duk_map_char_function) (void *udata, duk_codepoint_t codepoint);
typedef duk_ret_t (*duk_safe_call_function) (duk_context *ctx);
typedef duk_size_t (*duk_debug_read_function) (void *udata, char *buffer, duk_size_t length);
typedef duk_size_t (*duk_debug_write_function) (void *udata, const char *buffer, duk_size_t length);
typedef duk_size_t (*duk_debug_peek_function) (void *udata);
typedef void (*duk_debug_read_flush_function) (void *udata);
typedef void (*duk_debug_write_flush_function) (void *udata);
typedef void (*duk_debug_detached_function) (void *udata);

struct duk_memory_functions {
	duk_alloc_function alloc_func;
	duk_realloc_function realloc_func;
	duk_free_function free_func;
	void *udata;
};

struct duk_function_list_entry {
	const char *key;
	duk_c_function value;
	duk_idx_t nargs;
};

struct duk_number_list_entry {
	const char *key;
	duk_double_t value;
};

/*
 *  Constants
 */

/* Duktape version, (major * 10000) + (minor * 100) + patch.  Allows C code
 * to #ifdef against Duktape API version.  The same value is also available
 * to Ecmascript code in Duktape.version.  Unofficial development snapshots
 * have 99 for patch level (e.g. 0.10.99 would be a development version
 * after 0.10.0 but before the next official release).
 */
#define DUK_VERSION                       10400L

/* Git commit, describe, and branch for Duktape build.  Useful for
 * non-official snapshot builds so that application code can easily log
 * which Duktape snapshot was used.  Not available in the Ecmascript
 * environment.
 */
#define DUK_GIT_COMMIT                    "cad6f595382a0cc1a7e4207794ade5be11b3e397"
#define DUK_GIT_DESCRIBE                  "v1.4.0"
#define DUK_GIT_BRANCH                    "master"

/* Duktape debug protocol version used by this build. */
#define DUK_DEBUG_PROTOCOL_VERSION        1

/* Used to represent invalid index; if caller uses this without checking,
 * this index will map to a non-existent stack entry.  Also used in some
 * API calls as a marker to denote "no value".
 */
#define DUK_INVALID_INDEX                 DUK_IDX_MIN

/* Indicates that a native function does not have a fixed number of args,
 * and the argument stack should not be capped/extended at all.
 */
#define DUK_VARARGS                       ((duk_int_t) (-1))

/* Number of value stack entries (in addition to actual call arguments)
 * guaranteed to be allocated on entry to a Duktape/C function.
 */
#define DUK_API_ENTRY_STACK               64

/* Value types, used by e.g. duk_get_type() */
#define DUK_TYPE_MIN                      0
#define DUK_TYPE_NONE                     0    /* no value, e.g. invalid index */
#define DUK_TYPE_UNDEFINED                1    /* Ecmascript undefined */
#define DUK_TYPE_NULL                     2    /* Ecmascript null */
#define DUK_TYPE_BOOLEAN                  3    /* Ecmascript boolean: 0 or 1 */
#define DUK_TYPE_NUMBER                   4    /* Ecmascript number: double */
#define DUK_TYPE_STRING                   5    /* Ecmascript string: CESU-8 / extended UTF-8 encoded */
#define DUK_TYPE_OBJECT                   6    /* Ecmascript object: includes objects, arrays, functions, threads */
#define DUK_TYPE_BUFFER                   7    /* fixed or dynamic, garbage collected byte buffer */
#define DUK_TYPE_POINTER                  8    /* raw void pointer */
#define DUK_TYPE_LIGHTFUNC                9    /* lightweight function pointer */
#define DUK_TYPE_MAX                      9

/* Value mask types, used by e.g. duk_get_type_mask() */
#define DUK_TYPE_MASK_NONE                (1 << DUK_TYPE_NONE)
#define DUK_TYPE_MASK_UNDEFINED           (1 << DUK_TYPE_UNDEFINED)
#define DUK_TYPE_MASK_NULL                (1 << DUK_TYPE_NULL)
#define DUK_TYPE_MASK_BOOLEAN             (1 << DUK_TYPE_BOOLEAN)
#define DUK_TYPE_MASK_NUMBER              (1 << DUK_TYPE_NUMBER)
#define DUK_TYPE_MASK_STRING              (1 << DUK_TYPE_STRING)
#define DUK_TYPE_MASK_OBJECT              (1 << DUK_TYPE_OBJECT)
#define DUK_TYPE_MASK_BUFFER              (1 << DUK_TYPE_BUFFER)
#define DUK_TYPE_MASK_POINTER             (1 << DUK_TYPE_POINTER)
#define DUK_TYPE_MASK_LIGHTFUNC           (1 << DUK_TYPE_LIGHTFUNC)
#define DUK_TYPE_MASK_THROW               (1 << 10)  /* internal flag value: throw if mask doesn't match */

/* Coercion hints */
#define DUK_HINT_NONE                     0    /* prefer number, unless input is a Date, in which
                                                * case prefer string (E5 Section 8.12.8)
                                                */
#define DUK_HINT_STRING                   1    /* prefer string */
#define DUK_HINT_NUMBER                   2    /* prefer number */

/* Enumeration flags for duk_enum() */
#define DUK_ENUM_INCLUDE_NONENUMERABLE    (1 << 0)    /* enumerate non-numerable properties in addition to enumerable */
#define DUK_ENUM_INCLUDE_INTERNAL         (1 << 1)    /* enumerate internal properties (regardless of enumerability) */
#define DUK_ENUM_OWN_PROPERTIES_ONLY      (1 << 2)    /* don't walk prototype chain, only check own properties */
#define DUK_ENUM_ARRAY_INDICES_ONLY       (1 << 3)    /* only enumerate array indices */
#define DUK_ENUM_SORT_ARRAY_INDICES       (1 << 4)    /* sort array indices, use with DUK_ENUM_ARRAY_INDICES_ONLY */
#define DUK_ENUM_NO_PROXY_BEHAVIOR        (1 << 5)    /* enumerate a proxy object itself without invoking proxy behavior */

/* Compilation flags for duk_compile() and duk_eval() */
#define DUK_COMPILE_EVAL                  (1 << 0)    /* compile eval code (instead of global code) */
#define DUK_COMPILE_FUNCTION              (1 << 1)    /* compile function code (instead of global code) */
#define DUK_COMPILE_STRICT                (1 << 2)    /* use strict (outer) context for global, eval, or function code */
#define DUK_COMPILE_SAFE                  (1 << 3)    /* (internal) catch compilation errors */
#define DUK_COMPILE_NORESULT              (1 << 4)    /* (internal) omit eval result */
#define DUK_COMPILE_NOSOURCE              (1 << 5)    /* (internal) no source string on stack */
#define DUK_COMPILE_STRLEN                (1 << 6)    /* (internal) take strlen() of src_buffer (avoids double evaluation in macro) */

/* Flags for duk_def_prop() and its variants */
#define DUK_DEFPROP_WRITABLE              (1 << 0)    /* set writable (effective if DUK_DEFPROP_HAVE_WRITABLE set) */
#define DUK_DEFPROP_ENUMERABLE            (1 << 1)    /* set enumerable (effective if DUK_DEFPROP_HAVE_ENUMERABLE set) */
#define DUK_DEFPROP_CONFIGURABLE          (1 << 2)    /* set configurable (effective if DUK_DEFPROP_HAVE_CONFIGURABLE set) */
#define DUK_DEFPROP_HAVE_WRITABLE         (1 << 3)    /* set/clear writable */
#define DUK_DEFPROP_HAVE_ENUMERABLE       (1 << 4)    /* set/clear enumerable */
#define DUK_DEFPROP_HAVE_CONFIGURABLE     (1 << 5)    /* set/clear configurable */
#define DUK_DEFPROP_HAVE_VALUE            (1 << 6)    /* set value (given on value stack) */
#define DUK_DEFPROP_HAVE_GETTER           (1 << 7)    /* set getter (given on value stack) */
#define DUK_DEFPROP_HAVE_SETTER           (1 << 8)    /* set setter (given on value stack) */
#define DUK_DEFPROP_FORCE                 (1 << 9)    /* force change if possible, may still fail for e.g. virtual properties */
#define DUK_DEFPROP_SET_WRITABLE          (DUK_DEFPROP_HAVE_WRITABLE | DUK_DEFPROP_WRITABLE)
#define DUK_DEFPROP_CLEAR_WRITABLE        DUK_DEFPROP_HAVE_WRITABLE
#define DUK_DEFPROP_SET_ENUMERABLE        (DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_ENUMERABLE)
#define DUK_DEFPROP_CLEAR_ENUMERABLE      DUK_DEFPROP_HAVE_ENUMERABLE
#define DUK_DEFPROP_SET_CONFIGURABLE      (DUK_DEFPROP_HAVE_CONFIGURABLE | DUK_DEFPROP_CONFIGURABLE)
#define DUK_DEFPROP_CLEAR_CONFIGURABLE    DUK_DEFPROP_HAVE_CONFIGURABLE

/* Flags for duk_push_thread_raw() */
#define DUK_THREAD_NEW_GLOBAL_ENV         (1 << 0)    /* create a new global environment */

/* Flags for duk_push_string_file_raw() */
#define DUK_STRING_PUSH_SAFE              (1 << 0)    /* no error if file does not exist */

/* Duktape specific error codes */
#define DUK_ERR_NONE                      0    /* no error (e.g. from duk_get_error_code()) */
#define DUK_ERR_UNIMPLEMENTED_ERROR       50   /* UnimplementedError */
#define DUK_ERR_UNSUPPORTED_ERROR         51   /* UnsupportedError */
#define DUK_ERR_INTERNAL_ERROR            52   /* InternalError */
#define DUK_ERR_ALLOC_ERROR               53   /* AllocError */
#define DUK_ERR_ASSERTION_ERROR           54   /* AssertionError */
#define DUK_ERR_API_ERROR                 55   /* APIError */
#define DUK_ERR_UNCAUGHT_ERROR            56   /* UncaughtError */

/* Ecmascript E5 specification error codes */
#define DUK_ERR_ERROR                     100  /* Error */
#define DUK_ERR_EVAL_ERROR                101  /* EvalError */
#define DUK_ERR_RANGE_ERROR               102  /* RangeError */
#define DUK_ERR_REFERENCE_ERROR           103  /* ReferenceError */
#define DUK_ERR_SYNTAX_ERROR              104  /* SyntaxError */
#define DUK_ERR_TYPE_ERROR                105  /* TypeError */
#define DUK_ERR_URI_ERROR                 106  /* URIError */

/* Return codes for C functions (shortcut for throwing an error) */
#define DUK_RET_UNIMPLEMENTED_ERROR       (-DUK_ERR_UNIMPLEMENTED_ERROR)
#define DUK_RET_UNSUPPORTED_ERROR         (-DUK_ERR_UNSUPPORTED_ERROR)
#define DUK_RET_INTERNAL_ERROR            (-DUK_ERR_INTERNAL_ERROR)
#define DUK_RET_ALLOC_ERROR               (-DUK_ERR_ALLOC_ERROR)
#define DUK_RET_ASSERTION_ERROR           (-DUK_ERR_ASSERTION_ERROR)
#define DUK_RET_API_ERROR                 (-DUK_ERR_API_ERROR)
#define DUK_RET_UNCAUGHT_ERROR            (-DUK_ERR_UNCAUGHT_ERROR)
#define DUK_RET_ERROR                     (-DUK_ERR_ERROR)
#define DUK_RET_EVAL_ERROR                (-DUK_ERR_EVAL_ERROR)
#define DUK_RET_RANGE_ERROR               (-DUK_ERR_RANGE_ERROR)
#define DUK_RET_REFERENCE_ERROR           (-DUK_ERR_REFERENCE_ERROR)
#define DUK_RET_SYNTAX_ERROR              (-DUK_ERR_SYNTAX_ERROR)
#define DUK_RET_TYPE_ERROR                (-DUK_ERR_TYPE_ERROR)
#define DUK_RET_URI_ERROR                 (-DUK_ERR_URI_ERROR)

/* Return codes for protected calls (duk_safe_call(), duk_pcall()). */
#define DUK_EXEC_SUCCESS                  0
#define DUK_EXEC_ERROR                    1

/* Log levels */
#define DUK_LOG_TRACE                     0
#define DUK_LOG_DEBUG                     1
#define DUK_LOG_INFO                      2
#define DUK_LOG_WARN                      3
#define DUK_LOG_ERROR                     4
#define DUK_LOG_FATAL                     5

/*
 *  If no variadic macros, __FILE__ and __LINE__ are passed through globals
 *  which is ugly and not thread safe.
 */

#ifndef DUK_API_VARIADIC_MACROS
DUK_EXTERNAL_DECL const char *duk_api_global_filename;
DUK_EXTERNAL_DECL duk_int_t duk_api_global_line;
#endif

/*
 *  Context management
 */

DUK_EXTERNAL_DECL
duk_context *duk_create_heap(duk_alloc_function alloc_func,
                             duk_realloc_function realloc_func,
                             duk_free_function free_func,
                             void *heap_udata,
                             duk_fatal_function fatal_handler);
DUK_EXTERNAL_DECL void duk_destroy_heap(duk_context *ctx);

#define duk_create_heap_default() \
	duk_create_heap(NULL, NULL, NULL, NULL, NULL)

/*
 *  Memory management
 *
 *  Raw functions have no side effects (cannot trigger GC).
 */

DUK_EXTERNAL_DECL void *duk_alloc_raw(duk_context *ctx, duk_size_t size);
DUK_EXTERNAL_DECL void duk_free_raw(duk_context *ctx, void *ptr);
DUK_EXTERNAL_DECL void *duk_realloc_raw(duk_context *ctx, void *ptr, duk_size_t size);
DUK_EXTERNAL_DECL void *duk_alloc(duk_context *ctx, duk_size_t size);
DUK_EXTERNAL_DECL void duk_free(duk_context *ctx, void *ptr);
DUK_EXTERNAL_DECL void *duk_realloc(duk_context *ctx, void *ptr, duk_size_t size);
DUK_EXTERNAL_DECL void duk_get_memory_functions(duk_context *ctx, duk_memory_functions *out_funcs);
DUK_EXTERNAL_DECL void duk_gc(duk_context *ctx, duk_uint_t flags);

/*
 *  Error handling
 */

DUK_API_NORETURN(DUK_EXTERNAL_DECL void duk_throw(duk_context *ctx));
DUK_API_NORETURN(DUK_EXTERNAL_DECL void duk_fatal(duk_context *ctx, duk_errcode_t err_code, const char *err_msg));

DUK_API_NORETURN(DUK_EXTERNAL_DECL void duk_error_raw(duk_context *ctx, duk_errcode_t err_code, const char *filename, duk_int_t line, const char *fmt, ...));

#ifdef DUK_API_VARIADIC_MACROS
#define duk_error(ctx,err_code,...)  \
	duk_error_raw((ctx), (duk_errcode_t) (err_code), (const char *) (__FILE__), (duk_int_t) (__LINE__), __VA_ARGS__)
#else
DUK_API_NORETURN(DUK_EXTERNAL_DECL void duk_error_stash(duk_context *ctx, duk_errcode_t err_code, const char *fmt, ...));
/* One problem with this macro is that expressions like the following fail
 * to compile: "(void) duk_error(...)".  But because duk_error() is noreturn,
 * they make little sense anyway.
 */
#define duk_error  \
	(duk_api_global_filename = (const char *) (__FILE__), \
	 duk_api_global_line = (duk_int_t) (__LINE__), \
	 duk_error_stash)  /* last value is func pointer, arguments follow in parens */
#endif

DUK_API_NORETURN(DUK_EXTERNAL_DECL void duk_error_va_raw(duk_context *ctx, duk_errcode_t err_code, const char *filename, duk_int_t line, const char *fmt, va_list ap));
#define duk_error_va(ctx,err_code,fmt,ap)  \
	duk_error_va_raw((ctx), (duk_errcode_t) (err_code), (const char *) (__FILE__), (duk_int_t) (__LINE__), (fmt), (ap))

/*
 *  Other state related functions
 */

DUK_EXTERNAL_DECL duk_bool_t duk_is_strict_call(duk_context *ctx);
DUK_EXTERNAL_DECL duk_bool_t duk_is_constructor_call(duk_context *ctx);

/*
 *  Stack management
 */

DUK_EXTERNAL_DECL duk_idx_t duk_normalize_index(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_idx_t duk_require_normalize_index(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_valid_index(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_require_valid_index(duk_context *ctx, duk_idx_t index);

DUK_EXTERNAL_DECL duk_idx_t duk_get_top(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_set_top(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_idx_t duk_get_top_index(duk_context *ctx);
DUK_EXTERNAL_DECL duk_idx_t duk_require_top_index(duk_context *ctx);

/* Although extra/top could be an unsigned type here, using a signed type
 * makes the API more robust to calling code calculation errors or corner
 * cases (where caller might occasionally come up with negative values).
 * Negative values are treated as zero, which is better than casting them
 * to a large unsigned number.  (This principle is used elsewhere in the
 * API too.)
 */
DUK_EXTERNAL_DECL duk_bool_t duk_check_stack(duk_context *ctx, duk_idx_t extra);
DUK_EXTERNAL_DECL void duk_require_stack(duk_context *ctx, duk_idx_t extra);
DUK_EXTERNAL_DECL duk_bool_t duk_check_stack_top(duk_context *ctx, duk_idx_t top);
DUK_EXTERNAL_DECL void duk_require_stack_top(duk_context *ctx, duk_idx_t top);

/*
 *  Stack manipulation (other than push/pop)
 */

DUK_EXTERNAL_DECL void duk_swap(duk_context *ctx, duk_idx_t index1, duk_idx_t index2);
DUK_EXTERNAL_DECL void duk_swap_top(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_dup(duk_context *ctx, duk_idx_t from_index);
DUK_EXTERNAL_DECL void duk_dup_top(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_insert(duk_context *ctx, duk_idx_t to_index);
DUK_EXTERNAL_DECL void duk_replace(duk_context *ctx, duk_idx_t to_index);
DUK_EXTERNAL_DECL void duk_copy(duk_context *ctx, duk_idx_t from_index, duk_idx_t to_index);
DUK_EXTERNAL_DECL void duk_remove(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_xcopymove_raw(duk_context *to_ctx, duk_context *from_ctx, duk_idx_t count, duk_bool_t is_copy);

#define duk_xmove_top(to_ctx,from_ctx,count) \
	duk_xcopymove_raw((to_ctx), (from_ctx), (count), 0 /*is_copy*/)
#define duk_xcopy_top(to_ctx,from_ctx,count) \
	duk_xcopymove_raw((to_ctx), (from_ctx), (count), 1 /*is_copy*/)

/*
 *  Push operations
 *
 *  Push functions return the absolute (relative to bottom of frame)
 *  position of the pushed value for convenience.
 *
 *  Note: duk_dup() is technically a push.
 */

DUK_EXTERNAL_DECL void duk_push_undefined(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_null(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_boolean(duk_context *ctx, duk_bool_t val);
DUK_EXTERNAL_DECL void duk_push_true(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_false(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_number(duk_context *ctx, duk_double_t val);
DUK_EXTERNAL_DECL void duk_push_nan(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_int(duk_context *ctx, duk_int_t val);
DUK_EXTERNAL_DECL void duk_push_uint(duk_context *ctx, duk_uint_t val);
DUK_EXTERNAL_DECL const char *duk_push_string(duk_context *ctx, const char *str);
DUK_EXTERNAL_DECL const char *duk_push_lstring(duk_context *ctx, const char *str, duk_size_t len);
DUK_EXTERNAL_DECL void duk_push_pointer(duk_context *ctx, void *p);
DUK_EXTERNAL_DECL const char *duk_push_sprintf(duk_context *ctx, const char *fmt, ...);
DUK_EXTERNAL_DECL const char *duk_push_vsprintf(duk_context *ctx, const char *fmt, va_list ap);

DUK_EXTERNAL_DECL const char *duk_push_string_file_raw(duk_context *ctx, const char *path, duk_uint_t flags);
#define duk_push_string_file(ctx,path) \
	duk_push_string_file_raw((ctx), (path), 0)

DUK_EXTERNAL_DECL void duk_push_this(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_current_function(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_current_thread(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_global_object(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_heap_stash(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_global_stash(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_thread_stash(duk_context *ctx, duk_context *target_ctx);

DUK_EXTERNAL_DECL duk_idx_t duk_push_object(duk_context *ctx);
DUK_EXTERNAL_DECL duk_idx_t duk_push_array(duk_context *ctx);
DUK_EXTERNAL_DECL duk_idx_t duk_push_c_function(duk_context *ctx, duk_c_function func, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_idx_t duk_push_c_lightfunc(duk_context *ctx, duk_c_function func, duk_idx_t nargs, duk_idx_t length, duk_int_t magic);
DUK_EXTERNAL_DECL duk_idx_t duk_push_thread_raw(duk_context *ctx, duk_uint_t flags);

#define duk_push_thread(ctx) \
	duk_push_thread_raw((ctx), 0 /*flags*/)

#define duk_push_thread_new_globalenv(ctx) \
	duk_push_thread_raw((ctx), DUK_THREAD_NEW_GLOBAL_ENV /*flags*/)

DUK_EXTERNAL_DECL duk_idx_t duk_push_error_object_raw(duk_context *ctx, duk_errcode_t err_code, const char *filename, duk_int_t line, const char *fmt, ...);

#ifdef DUK_API_VARIADIC_MACROS
#define duk_push_error_object(ctx,err_code,...)  \
	duk_push_error_object_raw((ctx), (err_code), (const char *) (__FILE__), (duk_int_t) (__LINE__), __VA_ARGS__)
#else
DUK_EXTERNAL_DECL duk_idx_t duk_push_error_object_stash(duk_context *ctx, duk_errcode_t err_code, const char *fmt, ...);
/* Note: parentheses are required so that the comma expression works in assignments. */
#define duk_push_error_object  \
	(duk_api_global_filename = (const char *) (__FILE__), \
	 duk_api_global_line = (duk_int_t) (__LINE__), \
	 duk_push_error_object_stash)  /* last value is func pointer, arguments follow in parens */
#endif

DUK_EXTERNAL_DECL duk_idx_t duk_push_error_object_va_raw(duk_context *ctx, duk_errcode_t err_code, const char *filename, duk_int_t line, const char *fmt, va_list ap);
#define duk_push_error_object_va(ctx,err_code,fmt,ap)  \
	duk_push_error_object_va_raw((ctx), (err_code), (const char *) (__FILE__), (duk_int_t) (__LINE__), (fmt), (ap))

#define DUK_BUF_FLAG_DYNAMIC   (1 << 0)    /* internal flag: dynamic buffer */
#define DUK_BUF_FLAG_EXTERNAL  (1 << 1)    /* internal flag: external buffer */
#define DUK_BUF_FLAG_NOZERO    (1 << 2)    /* internal flag: don't zero allocated buffer */

DUK_EXTERNAL_DECL void *duk_push_buffer_raw(duk_context *ctx, duk_size_t size, duk_small_uint_t flags);

#define duk_push_buffer(ctx,size,dynamic) \
	duk_push_buffer_raw((ctx), (size), (dynamic) ? DUK_BUF_FLAG_DYNAMIC : 0)
#define duk_push_fixed_buffer(ctx,size) \
	duk_push_buffer_raw((ctx), (size), 0 /*flags*/)
#define duk_push_dynamic_buffer(ctx,size) \
	duk_push_buffer_raw((ctx), (size), DUK_BUF_FLAG_DYNAMIC /*flags*/)
#define duk_push_external_buffer(ctx) \
	((void) duk_push_buffer_raw((ctx), 0, DUK_BUF_FLAG_DYNAMIC | DUK_BUF_FLAG_EXTERNAL))

#define DUK_BUFOBJ_CREATE_ARRBUF       (1 << 4)  /* internal flag: create backing ArrayBuffer; keep in one byte */
#define DUK_BUFOBJ_DUKTAPE_BUFFER      0
#define DUK_BUFOBJ_NODEJS_BUFFER       1
#define DUK_BUFOBJ_ARRAYBUFFER         2
#define DUK_BUFOBJ_DATAVIEW            (3 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_INT8ARRAY           (4 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_UINT8ARRAY          (5 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_UINT8CLAMPEDARRAY   (6 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_INT16ARRAY          (7 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_UINT16ARRAY         (8 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_INT32ARRAY          (9 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_UINT32ARRAY         (10 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_FLOAT32ARRAY        (11 | DUK_BUFOBJ_CREATE_ARRBUF)
#define DUK_BUFOBJ_FLOAT64ARRAY        (12 | DUK_BUFOBJ_CREATE_ARRBUF)

DUK_EXTERNAL_DECL void duk_push_buffer_object(duk_context *ctx, duk_idx_t idx_buffer, duk_size_t byte_offset, duk_size_t byte_length, duk_uint_t flags);

DUK_EXTERNAL_DECL duk_idx_t duk_push_heapptr(duk_context *ctx, void *ptr);

/*
 *  Pop operations
 */

DUK_EXTERNAL_DECL void duk_pop(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_pop_n(duk_context *ctx, duk_idx_t count);
DUK_EXTERNAL_DECL void duk_pop_2(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_pop_3(duk_context *ctx);

/*
 *  Type checks
 *
 *  duk_is_none(), which would indicate whether index it outside of stack,
 *  is not needed; duk_is_valid_index() gives the same information.
 */

DUK_EXTERNAL_DECL duk_int_t duk_get_type(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_check_type(duk_context *ctx, duk_idx_t index, duk_int_t type);
DUK_EXTERNAL_DECL duk_uint_t duk_get_type_mask(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_check_type_mask(duk_context *ctx, duk_idx_t index, duk_uint_t mask);

DUK_EXTERNAL_DECL duk_bool_t duk_is_undefined(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_null(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_null_or_undefined(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_boolean(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_number(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_nan(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_string(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_object(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_buffer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_pointer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_lightfunc(duk_context *ctx, duk_idx_t index);

DUK_EXTERNAL_DECL duk_bool_t duk_is_array(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_function(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_c_function(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_ecmascript_function(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_bound_function(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_thread(duk_context *ctx, duk_idx_t index);

#define duk_is_callable(ctx,index) \
	duk_is_function((ctx), (index))
DUK_EXTERNAL_DECL duk_bool_t duk_is_dynamic_buffer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_fixed_buffer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_is_external_buffer(duk_context *ctx, duk_idx_t index);

#define duk_is_primitive(ctx,index) \
	duk_check_type_mask((ctx), (index), DUK_TYPE_MASK_UNDEFINED | \
	                                    DUK_TYPE_MASK_NULL | \
	                                    DUK_TYPE_MASK_BOOLEAN | \
	                                    DUK_TYPE_MASK_NUMBER | \
	                                    DUK_TYPE_MASK_STRING | \
	                                    DUK_TYPE_MASK_BUFFER | \
	                                    DUK_TYPE_MASK_POINTER | \
	                                    DUK_TYPE_MASK_LIGHTFUNC)

#define duk_is_object_coercible(ctx,index) \
	duk_check_type_mask((ctx), (index), DUK_TYPE_MASK_BOOLEAN | \
	                                    DUK_TYPE_MASK_NUMBER | \
	                                    DUK_TYPE_MASK_STRING | \
	                                    DUK_TYPE_MASK_OBJECT | \
	                                    DUK_TYPE_MASK_BUFFER | \
	                                    DUK_TYPE_MASK_POINTER | \
	                                    DUK_TYPE_MASK_LIGHTFUNC)

DUK_EXTERNAL_DECL duk_errcode_t duk_get_error_code(duk_context *ctx, duk_idx_t index);
#define duk_is_error(ctx,index) \
	(duk_get_error_code((ctx), (index)) != 0)
#define duk_is_eval_error(ctx,index) \
	(duk_get_error_code((ctx), (index)) == DUK_ERR_EVAL_ERROR)
#define duk_is_range_error(ctx,index) \
	(duk_get_error_code((ctx), (index)) == DUK_ERR_RANGE_ERROR)
#define duk_is_reference_error(ctx,index) \
	(duk_get_error_code((ctx), (index)) == DUK_ERR_REFERENCE_ERROR)
#define duk_is_syntax_error(ctx,index) \
	(duk_get_error_code((ctx), (index)) == DUK_ERR_SYNTAX_ERROR)
#define duk_is_type_error(ctx,index) \
	(duk_get_error_code((ctx), (index)) == DUK_ERR_TYPE_ERROR)
#define duk_is_uri_error(ctx,index) \
	(duk_get_error_code((ctx), (index)) == DUK_ERR_URI_ERROR)

/*
 *  Get operations: no coercion, returns default value for invalid
 *  indices and invalid value types.
 *
 *  duk_get_undefined() and duk_get_null() would be pointless and
 *  are not included.
 */

DUK_EXTERNAL_DECL duk_bool_t duk_get_boolean(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_double_t duk_get_number(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_int_t duk_get_int(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_uint_t duk_get_uint(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_get_string(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_get_lstring(duk_context *ctx, duk_idx_t index, duk_size_t *out_len);
DUK_EXTERNAL_DECL void *duk_get_buffer(duk_context *ctx, duk_idx_t index, duk_size_t *out_size);
DUK_EXTERNAL_DECL void *duk_get_buffer_data(duk_context *ctx, duk_idx_t index, duk_size_t *out_size);
DUK_EXTERNAL_DECL void *duk_get_pointer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_c_function duk_get_c_function(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_context *duk_get_context(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void *duk_get_heapptr(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_size_t duk_get_length(duk_context *ctx, duk_idx_t index);

/*
 *  Require operations: no coercion, throw error if index or type
 *  is incorrect.  No defaulting.
 */

#define duk_require_type_mask(ctx,index,mask) \
	((void) duk_check_type_mask((ctx), (index), (mask) | DUK_TYPE_MASK_THROW))

DUK_EXTERNAL_DECL void duk_require_undefined(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_require_null(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_require_boolean(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_double_t duk_require_number(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_int_t duk_require_int(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_uint_t duk_require_uint(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_require_string(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_require_lstring(duk_context *ctx, duk_idx_t index, duk_size_t *out_len);
DUK_EXTERNAL_DECL void *duk_require_buffer(duk_context *ctx, duk_idx_t index, duk_size_t *out_size);
DUK_EXTERNAL_DECL void *duk_require_buffer_data(duk_context *ctx, duk_idx_t index, duk_size_t *out_size);
DUK_EXTERNAL_DECL void *duk_require_pointer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_c_function duk_require_c_function(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_context *duk_require_context(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_require_function(duk_context *ctx, duk_idx_t index);
#define duk_require_callable(ctx,index) \
	duk_require_function((ctx), (index))
DUK_EXTERNAL_DECL void *duk_require_heapptr(duk_context *ctx, duk_idx_t index);

#define duk_require_object_coercible(ctx,index) \
	((void) duk_check_type_mask((ctx), (index), DUK_TYPE_MASK_BOOLEAN | \
	                                            DUK_TYPE_MASK_NUMBER | \
	                                            DUK_TYPE_MASK_STRING | \
	                                            DUK_TYPE_MASK_OBJECT | \
	                                            DUK_TYPE_MASK_BUFFER | \
	                                            DUK_TYPE_MASK_POINTER | \
	                                            DUK_TYPE_MASK_LIGHTFUNC | \
	                                            DUK_TYPE_MASK_THROW))

/*
 *  Coercion operations: in-place coercion, return coerced value where
 *  applicable.  If index is invalid, throw error.  Some coercions may
 *  throw an expected error (e.g. from a toString() or valueOf() call)
 *  or an internal error (e.g. from out of memory).
 */

DUK_EXTERNAL_DECL void duk_to_undefined(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_to_null(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_bool_t duk_to_boolean(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_double_t duk_to_number(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_int_t duk_to_int(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_uint_t duk_to_uint(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_int32_t duk_to_int32(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_uint32_t duk_to_uint32(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_uint16_t duk_to_uint16(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_to_string(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_to_lstring(duk_context *ctx, duk_idx_t index, duk_size_t *out_len);
DUK_EXTERNAL_DECL void *duk_to_buffer_raw(duk_context *ctx, duk_idx_t index, duk_size_t *out_size, duk_uint_t flags);
DUK_EXTERNAL_DECL void *duk_to_pointer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_to_object(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_to_defaultvalue(duk_context *ctx, duk_idx_t index, duk_int_t hint);
DUK_EXTERNAL_DECL void duk_to_primitive(duk_context *ctx, duk_idx_t index, duk_int_t hint);

#define DUK_BUF_MODE_FIXED      0   /* internal: request fixed buffer result */
#define DUK_BUF_MODE_DYNAMIC    1   /* internal: request dynamic buffer result */
#define DUK_BUF_MODE_DONTCARE   2   /* internal: don't care about fixed/dynamic nature */

#define duk_to_buffer(ctx,index,out_size) \
	duk_to_buffer_raw((ctx), (index), (out_size), DUK_BUF_MODE_DONTCARE)
#define duk_to_fixed_buffer(ctx,index,out_size) \
	duk_to_buffer_raw((ctx), (index), (out_size), DUK_BUF_MODE_FIXED)
#define duk_to_dynamic_buffer(ctx,index,out_size) \
	duk_to_buffer_raw((ctx), (index), (out_size), DUK_BUF_MODE_DYNAMIC)

/* safe variants of a few coercion operations */
DUK_EXTERNAL_DECL const char *duk_safe_to_lstring(duk_context *ctx, duk_idx_t index, duk_size_t *out_len);
#define duk_safe_to_string(ctx,index) \
	duk_safe_to_lstring((ctx), (index), NULL)

/*
 *  Misc conversion
 */

DUK_EXTERNAL_DECL const char *duk_base64_encode(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_base64_decode(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_hex_encode(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_hex_decode(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL const char *duk_json_encode(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_json_decode(duk_context *ctx, duk_idx_t index);

/*
 *  Buffer
 */

DUK_EXTERNAL_DECL void *duk_resize_buffer(duk_context *ctx, duk_idx_t index, duk_size_t new_size);
DUK_EXTERNAL_DECL void *duk_steal_buffer(duk_context *ctx, duk_idx_t index, duk_size_t *out_size);
DUK_EXTERNAL_DECL void duk_config_buffer(duk_context *ctx, duk_idx_t index, void *ptr, duk_size_t len);

/*
 *  Property access
 *
 *  The basic function assumes key is on stack.  The _string variant takes
 *  a C string as a property name, while the _index variant takes an array
 *  index as a property name (e.g. 123 is equivalent to the key "123").
 */

DUK_EXTERNAL_DECL duk_bool_t duk_get_prop(duk_context *ctx, duk_idx_t obj_index);
DUK_EXTERNAL_DECL duk_bool_t duk_get_prop_string(duk_context *ctx, duk_idx_t obj_index, const char *key);
DUK_EXTERNAL_DECL duk_bool_t duk_get_prop_index(duk_context *ctx, duk_idx_t obj_index, duk_uarridx_t arr_index);
DUK_EXTERNAL_DECL duk_bool_t duk_put_prop(duk_context *ctx, duk_idx_t obj_index);
DUK_EXTERNAL_DECL duk_bool_t duk_put_prop_string(duk_context *ctx, duk_idx_t obj_index, const char *key);
DUK_EXTERNAL_DECL duk_bool_t duk_put_prop_index(duk_context *ctx, duk_idx_t obj_index, duk_uarridx_t arr_index);
DUK_EXTERNAL_DECL duk_bool_t duk_del_prop(duk_context *ctx, duk_idx_t obj_index);
DUK_EXTERNAL_DECL duk_bool_t duk_del_prop_string(duk_context *ctx, duk_idx_t obj_index, const char *key);
DUK_EXTERNAL_DECL duk_bool_t duk_del_prop_index(duk_context *ctx, duk_idx_t obj_index, duk_uarridx_t arr_index);
DUK_EXTERNAL_DECL duk_bool_t duk_has_prop(duk_context *ctx, duk_idx_t obj_index);
DUK_EXTERNAL_DECL duk_bool_t duk_has_prop_string(duk_context *ctx, duk_idx_t obj_index, const char *key);
DUK_EXTERNAL_DECL duk_bool_t duk_has_prop_index(duk_context *ctx, duk_idx_t obj_index, duk_uarridx_t arr_index);
DUK_EXTERNAL_DECL void duk_def_prop(duk_context *ctx, duk_idx_t obj_index, duk_uint_t flags);

DUK_EXTERNAL_DECL duk_bool_t duk_get_global_string(duk_context *ctx, const char *key);
DUK_EXTERNAL_DECL duk_bool_t duk_put_global_string(duk_context *ctx, const char *key);

/*
 *  Object prototype
 */

DUK_EXTERNAL_DECL void duk_get_prototype(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_set_prototype(duk_context *ctx, duk_idx_t index);

/*
 *  Object finalizer
 */

DUK_EXTERNAL_DECL void duk_get_finalizer(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_set_finalizer(duk_context *ctx, duk_idx_t index);

/*
 *  Global object
 */

DUK_EXTERNAL_DECL void duk_set_global_object(duk_context *ctx);

/*
 *  Duktape/C function magic value
 */

DUK_EXTERNAL_DECL duk_int_t duk_get_magic(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL void duk_set_magic(duk_context *ctx, duk_idx_t index, duk_int_t magic);
DUK_EXTERNAL_DECL duk_int_t duk_get_current_magic(duk_context *ctx);

/*
 *  Module helpers: put multiple function or constant properties
 */

DUK_EXTERNAL_DECL void duk_put_function_list(duk_context *ctx, duk_idx_t obj_index, const duk_function_list_entry *funcs);
DUK_EXTERNAL_DECL void duk_put_number_list(duk_context *ctx, duk_idx_t obj_index, const duk_number_list_entry *numbers);

/*
 *  Variable access
 */

/* XXX: These calls are incomplete and not usable now.  They are not (yet)
 * part of the public API.
 */
DUK_EXTERNAL_DECL void duk_get_var(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_put_var(duk_context *ctx);
DUK_EXTERNAL_DECL duk_bool_t duk_del_var(duk_context *ctx)
	DUK_GNUC_NORETURN;
DUK_EXTERNAL_DECL duk_bool_t duk_has_var(duk_context *ctx)
	DUK_GNUC_NORETURN;

/*
 *  Object operations
 */

DUK_EXTERNAL_DECL void duk_compact(duk_context *ctx, duk_idx_t obj_index);
DUK_EXTERNAL_DECL void duk_enum(duk_context *ctx, duk_idx_t obj_index, duk_uint_t enum_flags);
DUK_EXTERNAL_DECL duk_bool_t duk_next(duk_context *ctx, duk_idx_t enum_index, duk_bool_t get_value);

/*
 *  String manipulation
 */

DUK_EXTERNAL_DECL void duk_concat(duk_context *ctx, duk_idx_t count);
DUK_EXTERNAL_DECL void duk_join(duk_context *ctx, duk_idx_t count);
DUK_EXTERNAL_DECL void duk_decode_string(duk_context *ctx, duk_idx_t index, duk_decode_char_function callback, void *udata);
DUK_EXTERNAL_DECL void duk_map_string(duk_context *ctx, duk_idx_t index, duk_map_char_function callback, void *udata);
DUK_EXTERNAL_DECL void duk_substring(duk_context *ctx, duk_idx_t index, duk_size_t start_char_offset, duk_size_t end_char_offset);
DUK_EXTERNAL_DECL void duk_trim(duk_context *ctx, duk_idx_t index);
DUK_EXTERNAL_DECL duk_codepoint_t duk_char_code_at(duk_context *ctx, duk_idx_t index, duk_size_t char_offset);

/*
 *  Ecmascript operators
 */

DUK_EXTERNAL_DECL duk_bool_t duk_equals(duk_context *ctx, duk_idx_t index1, duk_idx_t index2);
DUK_EXTERNAL_DECL duk_bool_t duk_strict_equals(duk_context *ctx, duk_idx_t index1, duk_idx_t index2);
DUK_EXTERNAL_DECL duk_bool_t duk_instanceof(duk_context *ctx, duk_idx_t index1, duk_idx_t index2);

/*
 *  Function (method) calls
 */

DUK_EXTERNAL_DECL void duk_call(duk_context *ctx, duk_idx_t nargs);
DUK_EXTERNAL_DECL void duk_call_method(duk_context *ctx, duk_idx_t nargs);
DUK_EXTERNAL_DECL void duk_call_prop(duk_context *ctx, duk_idx_t obj_index, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_int_t duk_pcall(duk_context *ctx, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_int_t duk_pcall_method(duk_context *ctx, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_int_t duk_pcall_prop(duk_context *ctx, duk_idx_t obj_index, duk_idx_t nargs);
DUK_EXTERNAL_DECL void duk_new(duk_context *ctx, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_int_t duk_pnew(duk_context *ctx, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_int_t duk_safe_call(duk_context *ctx, duk_safe_call_function func, duk_idx_t nargs, duk_idx_t nrets);

/*
 *  Thread management
 */

/* There are currently no native functions to yield/resume, due to the internal
 * limitations on coroutine handling.  These will be added later.
 */

/*
 *  Compilation and evaluation
 */

DUK_EXTERNAL_DECL duk_int_t duk_eval_raw(duk_context *ctx, const char *src_buffer, duk_size_t src_length, duk_uint_t flags);
DUK_EXTERNAL_DECL duk_int_t duk_compile_raw(duk_context *ctx, const char *src_buffer, duk_size_t src_length, duk_uint_t flags);

/* plain */
#define duk_eval(ctx)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL))

#define duk_eval_noresult(ctx)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL | DUK_COMPILE_NORESULT))

#define duk_peval(ctx)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL | DUK_COMPILE_SAFE))

#define duk_peval_noresult(ctx)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL | DUK_COMPILE_SAFE | DUK_COMPILE_NORESULT))

#define duk_compile(ctx,flags)  \
	((void) duk_compile_raw((ctx), NULL, 0, (flags)))

#define duk_pcompile(ctx,flags)  \
	(duk_compile_raw((ctx), NULL, 0, (flags) | DUK_COMPILE_SAFE))

/* string */
#define duk_eval_string(ctx,src)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_eval_raw((ctx), (src), 0, DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN))

#define duk_eval_string_noresult(ctx,src)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_eval_raw((ctx), (src), 0, DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN | DUK_COMPILE_NORESULT))

#define duk_peval_string(ctx,src)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_eval_raw((ctx), (src), 0, DUK_COMPILE_EVAL | DUK_COMPILE_SAFE | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN))

#define duk_peval_string_noresult(ctx,src)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_eval_raw((ctx), (src), 0, DUK_COMPILE_EVAL | DUK_COMPILE_SAFE | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN | DUK_COMPILE_NORESULT))

#define duk_compile_string(ctx,flags,src)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_compile_raw((ctx), (src), 0, (flags) | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN))

#define duk_compile_string_filename(ctx,flags,src)  \
	((void) duk_compile_raw((ctx), (src), 0, (flags) | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN))

#define duk_pcompile_string(ctx,flags,src)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_compile_raw((ctx), (src), 0, (flags) | DUK_COMPILE_SAFE | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN))

#define duk_pcompile_string_filename(ctx,flags,src)  \
	(duk_compile_raw((ctx), (src), 0, (flags) | DUK_COMPILE_SAFE | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN))

/* lstring */
#define duk_eval_lstring(ctx,buf,len)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_eval_raw((ctx), buf, len, DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE))

#define duk_eval_lstring_noresult(ctx,buf,len)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_eval_raw((ctx), buf, len, DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_NORESULT))

#define duk_peval_lstring(ctx,buf,len)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_eval_raw((ctx), buf, len, DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_SAFE))

#define duk_peval_lstring_noresult(ctx,buf,len)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_eval_raw((ctx), buf, len, DUK_COMPILE_EVAL | DUK_COMPILE_SAFE | DUK_COMPILE_NOSOURCE | DUK_COMPILE_NORESULT))

#define duk_compile_lstring(ctx,flags,buf,len)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 (void) duk_compile_raw((ctx), buf, len, (flags) | DUK_COMPILE_NOSOURCE))

#define duk_compile_lstring_filename(ctx,flags,buf,len)  \
	((void) duk_compile_raw((ctx), buf, len, (flags) | DUK_COMPILE_NOSOURCE))

#define duk_pcompile_lstring(ctx,flags,buf,len)  \
	((void) duk_push_string((ctx), (const char *) (__FILE__)), \
	 duk_compile_raw((ctx), buf, len, (flags) | DUK_COMPILE_SAFE | DUK_COMPILE_NOSOURCE))

#define duk_pcompile_lstring_filename(ctx,flags,buf,len)  \
	(duk_compile_raw((ctx), buf, len, (flags) | DUK_COMPILE_SAFE | DUK_COMPILE_NOSOURCE))

/* file */
#define duk_eval_file(ctx,path)  \
	((void) duk_push_string_file_raw((ctx), (path), 0), \
	 (void) duk_push_string((ctx), (path)), \
	 (void) duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL))

#define duk_eval_file_noresult(ctx,path)  \
	((void) duk_push_string_file_raw((ctx), (path), 0), \
	 (void) duk_push_string((ctx), (path)), \
	 (void) duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL | DUK_COMPILE_NORESULT))

#define duk_peval_file(ctx,path)  \
	((void) duk_push_string_file_raw((ctx), (path), DUK_STRING_PUSH_SAFE), \
	 (void) duk_push_string((ctx), (path)), \
	 duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL | DUK_COMPILE_SAFE))

#define duk_peval_file_noresult(ctx,path)  \
	((void) duk_push_string_file_raw((ctx), (path), DUK_STRING_PUSH_SAFE), \
	 (void) duk_push_string((ctx), (path)), \
	 duk_eval_raw((ctx), NULL, 0, DUK_COMPILE_EVAL | DUK_COMPILE_SAFE | DUK_COMPILE_NORESULT))

#define duk_compile_file(ctx,flags,path)  \
	((void) duk_push_string_file_raw((ctx), (path), 0), \
	 (void) duk_push_string((ctx), (path)), \
	 (void) duk_compile_raw((ctx), NULL, 0, (flags)))

#define duk_pcompile_file(ctx,flags,path)  \
	((void) duk_push_string_file_raw((ctx), (path), DUK_STRING_PUSH_SAFE), \
	 (void) duk_push_string((ctx), (path)), \
	 duk_compile_raw((ctx), NULL, 0, (flags) | DUK_COMPILE_SAFE))

/*
 *  Bytecode load/dump
 */

DUK_EXTERNAL_DECL void duk_dump_function(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_load_function(duk_context *ctx);

/*
 *  Logging
 */

DUK_EXTERNAL_DECL void duk_log(duk_context *ctx, duk_int_t level, const char *fmt, ...);
DUK_EXTERNAL_DECL void duk_log_va(duk_context *ctx, duk_int_t level, const char *fmt, va_list ap);

/*
 *  Debugging
 */

DUK_EXTERNAL_DECL void duk_push_context_dump(duk_context *ctx);

#if defined(DUK_USE_FILE_IO)
/* internal use */
#define duk_dump_context_filehandle(ctx,fh) \
	(duk_push_context_dump((ctx)), \
	 DUK_FPRINTF((fh), "%s\n", duk_safe_to_string(ctx, -1)), \
	 duk_pop(ctx))

/* external use */
#define duk_dump_context_stdout(ctx) \
	duk_dump_context_filehandle((ctx), DUK_STDOUT)
#define duk_dump_context_stderr(ctx) \
	duk_dump_context_filehandle((ctx), DUK_STDERR)
#else  /* DUK_USE_FILE_IO */
#define duk_dump_context_stdout(ctx)  ((void) 0)
#define duk_dump_context_stderr(ctx)  ((void) 0)
#endif  /* DUK_USE_FILE_IO */

/*
 *  Debugger (debug protocol)
 */

DUK_EXTERNAL_DECL void duk_debugger_attach(duk_context *ctx,
                                           duk_debug_read_function read_cb,
                                           duk_debug_write_function write_cb,
                                           duk_debug_peek_function peek_cb,
                                           duk_debug_read_flush_function read_flush_cb,
                                           duk_debug_write_flush_function write_flush_cb,
                                           duk_debug_detached_function detached_cb,
                                           void *udata)
	DUK_GNUC_NORETURN;
DUK_EXTERNAL_DECL void duk_debugger_detach(duk_context *ctx)
	DUK_GNUC_NORETURN;
DUK_EXTERNAL_DECL void duk_debugger_cooperate(duk_context *ctx)
	DUK_GNUC_CONST;

/*
 *  Date provider related constants
 *
 *  NOTE: These are "semi public" - you should only use these if you write
 *  your own platform specific Date provider, see doc/datetime.rst.
 */

/* Millisecond count constants. */
#define DUK_DATE_MSEC_SECOND          1000L
#define DUK_DATE_MSEC_MINUTE          (60L * 1000L)
#define DUK_DATE_MSEC_HOUR            (60L * 60L * 1000L)
#define DUK_DATE_MSEC_DAY             (24L * 60L * 60L * 1000L)

/* Ecmascript date range is 100 million days from Epoch:
 * > 100e6 * 24 * 60 * 60 * 1000  // 100M days in millisecs
 * 8640000000000000
 * (= 8.64e15)
 */
#define DUK_DATE_MSEC_100M_DAYS         (8.64e15)
#define DUK_DATE_MSEC_100M_DAYS_LEEWAY  (8.64e15 + 24 * 3600e3)

/* Ecmascript year range:
 * > new Date(100e6 * 24 * 3600e3).toISOString()
 * '+275760-09-13T00:00:00.000Z'
 * > new Date(-100e6 * 24 * 3600e3).toISOString()
 * '-271821-04-20T00:00:00.000Z'
 */
#define DUK_DATE_MIN_ECMA_YEAR     (-271821L)
#define DUK_DATE_MAX_ECMA_YEAR     275760L

/* Part indices for internal breakdowns.  Part order from DUK_DATE_IDX_YEAR
 * to DUK_DATE_IDX_MILLISECOND matches argument ordering of Ecmascript API
 * calls (like Date constructor call).  Some functions in duk_bi_date.c
 * depend on the specific ordering, so change with care.  16 bits are not
 * enough for all parts (year, specifically).
 *
 * (Must be in-sync with genbuiltins.py.)
 */
#define DUK_DATE_IDX_YEAR           0  /* year */
#define DUK_DATE_IDX_MONTH          1  /* month: 0 to 11 */
#define DUK_DATE_IDX_DAY            2  /* day within month: 0 to 30 */
#define DUK_DATE_IDX_HOUR           3
#define DUK_DATE_IDX_MINUTE         4
#define DUK_DATE_IDX_SECOND         5
#define DUK_DATE_IDX_MILLISECOND    6
#define DUK_DATE_IDX_WEEKDAY        7  /* weekday: 0 to 6, 0=sunday, 1=monday, etc */
#define DUK_DATE_IDX_NUM_PARTS      8

/* Internal API call flags, used for various functions in this file.
 * Certain flags are used by only certain functions, but since the flags
 * don't overlap, a single flags value can be passed around to multiple
 * functions.
 *
 * The unused top bits of the flags field are also used to pass values
 * to helpers (duk__get_part_helper() and duk__set_part_helper()).
 *
 * (Must be in-sync with genbuiltins.py.)
 */

/* NOTE: when writing a Date provider you only need a few specific
 * flags from here, the rest are internal.  Avoid using anything you
 * don't need.
 */

#define DUK_DATE_FLAG_NAN_TO_ZERO          (1 << 0)  /* timeval breakdown: internal time value NaN -> zero */
#define DUK_DATE_FLAG_NAN_TO_RANGE_ERROR   (1 << 1)  /* timeval breakdown: internal time value NaN -> RangeError (toISOString) */
#define DUK_DATE_FLAG_ONEBASED             (1 << 2)  /* timeval breakdown: convert month and day-of-month parts to one-based (default is zero-based) */
#define DUK_DATE_FLAG_EQUIVYEAR            (1 << 3)  /* timeval breakdown: replace year with equivalent year in the [1971,2037] range for DST calculations */
#define DUK_DATE_FLAG_LOCALTIME            (1 << 4)  /* convert time value to local time */
#define DUK_DATE_FLAG_SUB1900              (1 << 5)  /* getter: subtract 1900 from year when getting year part */
#define DUK_DATE_FLAG_TOSTRING_DATE        (1 << 6)  /* include date part in string conversion result */
#define DUK_DATE_FLAG_TOSTRING_TIME        (1 << 7)  /* include time part in string conversion result */
#define DUK_DATE_FLAG_TOSTRING_LOCALE      (1 << 8)  /* use locale specific formatting if available */
#define DUK_DATE_FLAG_TIMESETTER           (1 << 9)  /* setter: call is a time setter (affects hour, min, sec, ms); otherwise date setter (affects year, month, day-in-month) */
#define DUK_DATE_FLAG_YEAR_FIXUP           (1 << 10) /* setter: perform 2-digit year fixup (00...99 -> 1900...1999) */
#define DUK_DATE_FLAG_SEP_T                (1 << 11) /* string conversion: use 'T' instead of ' ' as a separator */
#define DUK_DATE_FLAG_VALUE_SHIFT          12        /* additional values begin at bit 12 */

/*
 *  C++ name mangling
 */

#ifdef __cplusplus
/* end 'extern "C"' wrapper */
}
#endif

#endif  /* DUK_API_PUBLIC_H_INCLUDED */

/*
 *  END PUBLIC API
 */

/*
 *  Union to access IEEE double memory representation, indexes for double
 *  memory representation, and some macros for double manipulation.
 *
 *  Also used by packed duk_tval.  Use a union for bit manipulation to
 *  minimize aliasing issues in practice.  The C99 standard does not
 *  guarantee that this should work, but it's a very widely supported
 *  practice for low level manipulation.
 *
 *  IEEE double format summary:
 *
 *    seeeeeee eeeeffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff
 *       A        B        C        D        E        F        G        H
 *
 *    s       sign bit
 *    eee...  exponent field
 *    fff...  fraction
 *
 *  See http://en.wikipedia.org/wiki/Double_precision_floating-point_format.
 *
 *  NaNs are represented as exponent 0x7ff and mantissa != 0.  The NaN is a
 *  signaling NaN when the highest bit of the mantissa is zero, and a quiet
 *  NaN when the highest bit is set.
 *
 *  At least three memory layouts are relevant here:
 *
 *    A B C D E F G H    Big endian (e.g. 68k)           DUK_USE_DOUBLE_BE
 *    H G F E D C B A    Little endian (e.g. x86)        DUK_USE_DOUBLE_LE
 *    D C B A H G F E    Mixed/cross endian (e.g. ARM)   DUK_USE_DOUBLE_ME
 *
 *  ARM is a special case: ARM double values are in mixed/cross endian
 *  format while ARM duk_uint64_t values are in standard little endian
 *  format (H G F E D C B A).  When a double is read as a duk_uint64_t
 *  from memory, the register will contain the (logical) value
 *  E F G H A B C D.  This requires some special handling below.
 *
 *  Indexes of various types (8-bit, 16-bit, 32-bit) in memory relative to
 *  the logical (big endian) order:
 *
 *  byte order      duk_uint8_t    duk_uint16_t     duk_uint32_t
 *    BE             01234567         0123               01
 *    LE             76543210         3210               10
 *    ME (ARM)       32107654         1032               01
 *
 *  Some processors may alter NaN values in a floating point load+store.
 *  For instance, on X86 a FLD + FSTP may convert a signaling NaN to a
 *  quiet one.  This is catastrophic when NaN space is used in packed
 *  duk_tval values.  See: misc/clang_aliasing.c.
 */

#ifndef DUK_DBLUNION_H_INCLUDED
#define DUK_DBLUNION_H_INCLUDED

/*
 *  Union for accessing double parts, also serves as packed duk_tval
 */

union duk_double_union {
	double d;
	float f[2];
#if defined(DUK_USE_64BIT_OPS)
	duk_uint64_t ull[1];
#endif
	duk_uint32_t ui[2];
	duk_uint16_t us[4];
	duk_uint8_t uc[8];
#if defined(DUK_USE_PACKED_TVAL)
	void *vp[2];  /* used by packed duk_tval, assumes sizeof(void *) == 4 */
#endif
};

typedef union duk_double_union duk_double_union;

/*
 *  Indexes of various types with respect to big endian (logical) layout
 */

#if defined(DUK_USE_DOUBLE_LE)
#ifdef DUK_USE_64BIT_OPS
#define DUK_DBL_IDX_ULL0   0
#endif
#define DUK_DBL_IDX_UI0    1
#define DUK_DBL_IDX_UI1    0
#define DUK_DBL_IDX_US0    3
#define DUK_DBL_IDX_US1    2
#define DUK_DBL_IDX_US2    1
#define DUK_DBL_IDX_US3    0
#define DUK_DBL_IDX_UC0    7
#define DUK_DBL_IDX_UC1    6
#define DUK_DBL_IDX_UC2    5
#define DUK_DBL_IDX_UC3    4
#define DUK_DBL_IDX_UC4    3
#define DUK_DBL_IDX_UC5    2
#define DUK_DBL_IDX_UC6    1
#define DUK_DBL_IDX_UC7    0
#define DUK_DBL_IDX_VP0    DUK_DBL_IDX_UI0  /* packed tval */
#define DUK_DBL_IDX_VP1    DUK_DBL_IDX_UI1  /* packed tval */
#elif defined(DUK_USE_DOUBLE_BE)
#ifdef DUK_USE_64BIT_OPS
#define DUK_DBL_IDX_ULL0   0
#endif
#define DUK_DBL_IDX_UI0    0
#define DUK_DBL_IDX_UI1    1
#define DUK_DBL_IDX_US0    0
#define DUK_DBL_IDX_US1    1
#define DUK_DBL_IDX_US2    2
#define DUK_DBL_IDX_US3    3
#define DUK_DBL_IDX_UC0    0
#define DUK_DBL_IDX_UC1    1
#define DUK_DBL_IDX_UC2    2
#define DUK_DBL_IDX_UC3    3
#define DUK_DBL_IDX_UC4    4
#define DUK_DBL_IDX_UC5    5
#define DUK_DBL_IDX_UC6    6
#define DUK_DBL_IDX_UC7    7
#define DUK_DBL_IDX_VP0    DUK_DBL_IDX_UI0  /* packed tval */
#define DUK_DBL_IDX_VP1    DUK_DBL_IDX_UI1  /* packed tval */
#elif defined(DUK_USE_DOUBLE_ME)
#ifdef DUK_USE_64BIT_OPS
#define DUK_DBL_IDX_ULL0   0  /* not directly applicable, byte order differs from a double */
#endif
#define DUK_DBL_IDX_UI0    0
#define DUK_DBL_IDX_UI1    1
#define DUK_DBL_IDX_US0    1
#define DUK_DBL_IDX_US1    0
#define DUK_DBL_IDX_US2    3
#define DUK_DBL_IDX_US3    2
#define DUK_DBL_IDX_UC0    3
#define DUK_DBL_IDX_UC1    2
#define DUK_DBL_IDX_UC2    1
#define DUK_DBL_IDX_UC3    0
#define DUK_DBL_IDX_UC4    7
#define DUK_DBL_IDX_UC5    6
#define DUK_DBL_IDX_UC6    5
#define DUK_DBL_IDX_UC7    4
#define DUK_DBL_IDX_VP0    DUK_DBL_IDX_UI0  /* packed tval */
#define DUK_DBL_IDX_VP1    DUK_DBL_IDX_UI1  /* packed tval */
#else
#error internal error
#endif

/*
 *  Helper macros for reading/writing memory representation parts, used
 *  by duk_numconv.c and duk_tval.h.
 */

#define DUK_DBLUNION_SET_DOUBLE(u,v)  do {  \
		(u)->d = (v); \
	} while (0)

#define DUK_DBLUNION_SET_HIGH32(u,v)  do {  \
		(u)->ui[DUK_DBL_IDX_UI0] = (duk_uint32_t) (v); \
	} while (0)

#ifdef DUK_USE_64BIT_OPS
#ifdef DUK_USE_DOUBLE_ME
#define DUK_DBLUNION_SET_HIGH32_ZERO_LOW32(u,v)  do { \
		(u)->ull[DUK_DBL_IDX_ULL0] = (duk_uint64_t) (v); \
	} while (0)
#else
#define DUK_DBLUNION_SET_HIGH32_ZERO_LOW32(u,v)  do { \
		(u)->ull[DUK_DBL_IDX_ULL0] = ((duk_uint64_t) (v)) << 32; \
	} while (0)
#endif
#else  /* DUK_USE_64BIT_OPS */
#define DUK_DBLUNION_SET_HIGH32_ZERO_LOW32(u,v)  do { \
		(u)->ui[DUK_DBL_IDX_UI0] = (duk_uint32_t) (v); \
		(u)->ui[DUK_DBL_IDX_UI1] = (duk_uint32_t) 0; \
	} while (0)
#endif  /* DUK_USE_64BIT_OPS */

#define DUK_DBLUNION_SET_LOW32(u,v)  do {  \
		(u)->ui[DUK_DBL_IDX_UI1] = (duk_uint32_t) (v); \
	} while (0)

#define DUK_DBLUNION_GET_DOUBLE(u)  ((u)->d)
#define DUK_DBLUNION_GET_HIGH32(u)  ((u)->ui[DUK_DBL_IDX_UI0])
#define DUK_DBLUNION_GET_LOW32(u)   ((u)->ui[DUK_DBL_IDX_UI1])

#ifdef DUK_USE_64BIT_OPS
#ifdef DUK_USE_DOUBLE_ME
#define DUK_DBLUNION_SET_UINT64(u,v)  do { \
		(u)->ui[DUK_DBL_IDX_UI0] = (duk_uint32_t) ((v) >> 32); \
		(u)->ui[DUK_DBL_IDX_UI1] = (duk_uint32_t) (v); \
	} while (0)
#define DUK_DBLUNION_GET_UINT64(u) \
	((((duk_uint64_t) (u)->ui[DUK_DBL_IDX_UI0]) << 32) | \
	 ((duk_uint64_t) (u)->ui[DUK_DBL_IDX_UI1]))
#else
#define DUK_DBLUNION_SET_UINT64(u,v)  do { \
		(u)->ull[DUK_DBL_IDX_ULL0] = (duk_uint64_t) (v); \
	} while (0)
#define DUK_DBLUNION_GET_UINT64(u)  ((u)->ull[DUK_DBL_IDX_ULL0])
#endif
#define DUK_DBLUNION_SET_INT64(u,v) DUK_DBLUNION_SET_UINT64((u), (duk_uint64_t) (v))
#define DUK_DBLUNION_GET_INT64(u)   ((duk_int64_t) DUK_DBLUNION_GET_UINT64((u)))
#endif  /* DUK_USE_64BIT_OPS */

/*
 *  Double NaN manipulation macros related to NaN normalization needed when
 *  using the packed duk_tval representation.  NaN normalization is necessary
 *  to keep double values compatible with the duk_tval format.
 *
 *  When packed duk_tval is used, the NaN space is used to store pointers
 *  and other tagged values in addition to NaNs.  Actual NaNs are normalized
 *  to a specific quiet NaN.  The macros below are used by the implementation
 *  to check and normalize NaN values when they might be created.  The macros
 *  are essentially NOPs when the non-packed duk_tval representation is used.
 *
 *  A FULL check is exact and checks all bits.  A NOTFULL check is used by
 *  the packed duk_tval and works correctly for all NaNs except those that
 *  begin with 0x7ff0.  Since the 'normalized NaN' values used with packed
 *  duk_tval begin with 0x7ff8, the partial check is reliable when packed
 *  duk_tval is used.  The 0x7ff8 prefix means the normalized NaN will be a
 *  quiet NaN regardless of its remaining lower bits.
 *
 *  The ME variant below is specifically for ARM byte order, which has the
 *  feature that while doubles have a mixed byte order (32107654), unsigned
 *  long long values has a little endian byte order (76543210).  When writing
 *  a logical double value through a ULL pointer, the 32-bit words need to be
 *  swapped; hence the #ifdefs below for ULL writes with DUK_USE_DOUBLE_ME.
 *  This is not full ARM support but suffices for some environments.
 */

#ifdef DUK_USE_64BIT_OPS
#ifdef DUK_USE_DOUBLE_ME
#define DUK__DBLUNION_SET_NAN_FULL(u)  do { \
		(u)->ull[DUK_DBL_IDX_ULL0] = 0x000000007ff80000ULL; \
	} while (0)
#else
#define DUK__DBLUNION_SET_NAN_FULL(u)  do { \
		(u)->ull[DUK_DBL_IDX_ULL0] = 0x7ff8000000000000ULL; \
	} while (0)
#endif
#else  /* DUK_USE_64BIT_OPS */
#define DUK__DBLUNION_SET_NAN_FULL(u)  do { \
		(u)->ui[DUK_DBL_IDX_UI0] = (duk_uint32_t) 0x7ff80000UL; \
		(u)->ui[DUK_DBL_IDX_UI1] = (duk_uint32_t) 0x00000000UL; \
	} while (0)
#endif  /* DUK_USE_64BIT_OPS */

#define DUK__DBLUNION_SET_NAN_NOTFULL(u)  do { \
		(u)->us[DUK_DBL_IDX_US0] = 0x7ff8UL; \
	} while (0)

#ifdef DUK_USE_64BIT_OPS
#ifdef DUK_USE_DOUBLE_ME
#define DUK__DBLUNION_IS_NAN_FULL(u) \
	/* E == 0x7ff, F != 0 => NaN */ \
	((((u)->us[DUK_DBL_IDX_US0] & 0x7ff0UL) == 0x7ff0UL) && \
	 ((((u)->ull[DUK_DBL_IDX_ULL0]) & 0xffffffff000fffffULL) != 0))
#else
#define DUK__DBLUNION_IS_NAN_FULL(u) \
	/* E == 0x7ff, F != 0 => NaN */ \
	((((u)->us[DUK_DBL_IDX_US0] & 0x7ff0UL) == 0x7ff0UL) && \
	 ((((u)->ull[DUK_DBL_IDX_ULL0]) & 0x000fffffffffffffULL) != 0))
#endif
#else  /* DUK_USE_64BIT_OPS */
#define DUK__DBLUNION_IS_NAN_FULL(u) \
	/* E == 0x7ff, F != 0 => NaN */ \
	((((u)->ui[DUK_DBL_IDX_UI0] & 0x7ff00000UL) == 0x7ff00000UL) && \
	 (((u)->ui[DUK_DBL_IDX_UI0] & 0x000fffffUL) != 0 || \
          (u)->ui[DUK_DBL_IDX_UI1] != 0))
#endif  /* DUK_USE_64BIT_OPS */

#define DUK__DBLUNION_IS_NAN_NOTFULL(u) \
	/* E == 0x7ff, topmost four bits of F != 0 => assume NaN */ \
	((((u)->us[DUK_DBL_IDX_US0] & 0x7ff0UL) == 0x7ff0UL) && \
	 (((u)->us[DUK_DBL_IDX_US0] & 0x000fUL) != 0x0000UL))

#ifdef DUK_USE_64BIT_OPS
#ifdef DUK_USE_DOUBLE_ME
#define DUK__DBLUNION_IS_NORMALIZED_NAN_FULL(u) \
	((u)->ull[DUK_DBL_IDX_ULL0] == 0x000000007ff80000ULL)
#else
#define DUK__DBLUNION_IS_NORMALIZED_NAN_FULL(u) \
	((u)->ull[DUK_DBL_IDX_ULL0] == 0x7ff8000000000000ULL)
#endif
#else  /* DUK_USE_64BIT_OPS */
#define DUK__DBLUNION_IS_NORMALIZED_NAN_FULL(u) \
	(((u)->ui[DUK_DBL_IDX_UI0] == 0x7ff80000UL) && \
	 ((u)->ui[DUK_DBL_IDX_UI1] == 0x00000000UL))
#endif  /* DUK_USE_64BIT_OPS */

#define DUK__DBLUNION_IS_NORMALIZED_NAN_NOTFULL(u) \
	/* E == 0x7ff, F == 8 => normalized NaN */ \
	((u)->us[DUK_DBL_IDX_US0] == 0x7ff8UL)

#define DUK__DBLUNION_NORMALIZE_NAN_CHECK_FULL(u)  do { \
		if (DUK__DBLUNION_IS_NAN_FULL((u))) { \
			DUK__DBLUNION_SET_NAN_FULL((u)); \
		} \
	} while (0)

#define DUK__DBLUNION_NORMALIZE_NAN_CHECK_NOTFULL(u)  do { \
		if (DUK__DBLUNION_IS_NAN_NOTFULL((u))) { \
			DUK__DBLUNION_SET_NAN_NOTFULL((u)); \
		} \
	} while (0)

/* Concrete macros for NaN handling used by the implementation internals.
 * Chosen so that they match the duk_tval representation: with a packed
 * duk_tval, ensure NaNs are properly normalized; with a non-packed duk_tval
 * these are essentially NOPs.
 */

#if defined(DUK_USE_PACKED_TVAL)
#if defined(DUK_USE_FULL_TVAL)
#define DUK_DBLUNION_NORMALIZE_NAN_CHECK(u)  DUK__DBLUNION_NORMALIZE_NAN_CHECK_FULL((u))
#define DUK_DBLUNION_IS_NAN(u)               DUK__DBLUNION_IS_NAN_FULL((u))
#define DUK_DBLUNION_IS_NORMALIZED_NAN(u)    DUK__DBLUNION_IS_NORMALIZED_NAN_FULL((u))
#define DUK_DBLUNION_SET_NAN(d)              DUK__DBLUNION_SET_NAN_FULL((d))
#else
#define DUK_DBLUNION_NORMALIZE_NAN_CHECK(u)  DUK__DBLUNION_NORMALIZE_NAN_CHECK_NOTFULL((u))
#define DUK_DBLUNION_IS_NAN(u)               DUK__DBLUNION_IS_NAN_NOTFULL((u))
#define DUK_DBLUNION_IS_NORMALIZED_NAN(u)    DUK__DBLUNION_IS_NORMALIZED_NAN_NOTFULL((u))
#define DUK_DBLUNION_SET_NAN(d)              DUK__DBLUNION_SET_NAN_NOTFULL((d))
#endif
#define DUK_DBLUNION_IS_NORMALIZED(u) \
	(!DUK_DBLUNION_IS_NAN((u)) ||  /* either not a NaN */ \
	 DUK_DBLUNION_IS_NORMALIZED_NAN((u)))  /* or is a normalized NaN */
#else  /* DUK_USE_PACKED_TVAL */
#define DUK_DBLUNION_NORMALIZE_NAN_CHECK(u)  /* nop: no need to normalize */
#define DUK_DBLUNION_IS_NAN(u)               (DUK_ISNAN((u)->d))
#define DUK_DBLUNION_IS_NORMALIZED_NAN(u)    (DUK_ISNAN((u)->d))
#define DUK_DBLUNION_IS_NORMALIZED(u)        1  /* all doubles are considered normalized */
#define DUK_DBLUNION_SET_NAN(u)  do { \
		/* in non-packed representation we don't care about which NaN is used */ \
		(u)->d = DUK_DOUBLE_NAN; \
	} while (0)
#endif  /* DUK_USE_PACKED_TVAL */

/* XXX: native 64-bit byteswaps when available */

/* 64-bit byteswap, same operation independent of target endianness. */
#define DUK_DBLUNION_BSWAP64(u) do { \
		duk_uint32_t duk__bswaptmp1, duk__bswaptmp2; \
		duk__bswaptmp1 = (u)->ui[0]; \
		duk__bswaptmp2 = (u)->ui[1]; \
		duk__bswaptmp1 = DUK_BSWAP32(duk__bswaptmp1); \
		duk__bswaptmp2 = DUK_BSWAP32(duk__bswaptmp2); \
		(u)->ui[0] = duk__bswaptmp2; \
		(u)->ui[1] = duk__bswaptmp1; \
	} while (0)

/* Byteswap an IEEE double in the duk_double_union from host to network
 * order.  For a big endian target this is a no-op.
 */
#if defined(DUK_USE_DOUBLE_LE)
#define DUK_DBLUNION_DOUBLE_HTON(u) do { \
		duk_uint32_t duk__bswaptmp1, duk__bswaptmp2; \
		duk__bswaptmp1 = (u)->ui[0]; \
		duk__bswaptmp2 = (u)->ui[1]; \
		duk__bswaptmp1 = DUK_BSWAP32(duk__bswaptmp1); \
		duk__bswaptmp2 = DUK_BSWAP32(duk__bswaptmp2); \
		(u)->ui[0] = duk__bswaptmp2; \
		(u)->ui[1] = duk__bswaptmp1; \
	} while (0)
#elif defined(DUK_USE_DOUBLE_ME)
#define DUK_DBLUNION_DOUBLE_HTON(u) do { \
		duk_uint32_t duk__bswaptmp1, duk__bswaptmp2; \
		duk__bswaptmp1 = (u)->ui[0]; \
		duk__bswaptmp2 = (u)->ui[1]; \
		duk__bswaptmp1 = DUK_BSWAP32(duk__bswaptmp1); \
		duk__bswaptmp2 = DUK_BSWAP32(duk__bswaptmp2); \
		(u)->ui[0] = duk__bswaptmp1; \
		(u)->ui[1] = duk__bswaptmp2; \
	} while (0)
#elif defined(DUK_USE_DOUBLE_BE)
#define DUK_DBLUNION_DOUBLE_HTON(u) do { } while (0)
#else
#error internal error, double endianness insane
#endif

/* Reverse operation is the same. */
#define DUK_DBLUNION_DOUBLE_NTOH(u) DUK_DBLUNION_DOUBLE_HTON((u))

#endif  /* DUK_DBLUNION_H_INCLUDED */

#endif  /* DUKTAPE_H_INCLUDED */
