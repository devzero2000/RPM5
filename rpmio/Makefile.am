# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = tdir.c tfts.c tget.c tglob.c thkp.c thtml.c tinv.c tkey.c tmire.c tput.c trpmio.c tsw.c ttar.c lookup3.c tpw.c librpmio.vers testit.sh rpmgrep.1

EXTRA_PROGRAMS = tdir tfts tget tglob thkp thtml tinv tkey tmacro tmagic tmire tput tpw trpmio tsw ttar dumpasn1 lookup3

bin_PROGRAMS =
man_MANS =

TESTS =
check_PROGRAMS =
check_SCRIPTS =
dist_noinst_SCRIPTS = gengpg.sh testit.sh

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc \
	@WITH_DB_CPPFLAGS@ \
	@WITH_ZLIB_CPPFLAGS@ \
	@WITH_LUA_CPPFLAGS@ \
	@WITH_FILE_CPPFLAGS@ \
	@WITH_XAR_CPPFLAGS@

RPMIO_LDADD = \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

RPM_LDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(RPMIO_LDADD)

pkgincdir = $(pkgincludedir)$(WITH_PATH_VERSIONED_SUFFIX)
pkginc_HEADERS = \
	argv.h envvar.h fts.h mire.h rpmbc.h rpmcb.h rpmdav.h \
	rpmhash.h rpmio.h rpmio-stub.h rpmlog.h rpmmacro.h rpmmg.h \
	rpmnss.h rpmpgp.h rpmsq.h rpmssl.h rpmsw.h rpmurl.h rpmxar.h \
	stringbuf.h ugid.h rpmuuid.h
noinst_HEADERS = \
	md2.h md4.h poptIO.h rmd128.h rmd160.h rmd256.h rmd320.h sha224.h \
	salsa10.h salsa20.h tiger.h \
	LzmaDecode.h rpmhook.h rpmio_internal.h rpmlua.h

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c digest.c fts.c getpass.c macro.c mire.c mount.c \
	md2.c md4.c poptIO.c rmd128.c rmd160.c rmd256.c rmd320.c sha224.c \
	salsa10.c salsa20.c tiger.c LzmaDecode.c \
	rpmbc.c rpmdav.c rpmhash.c rpmhook.c rpmio.c rpmio-stub.c \
	rpmlog.c rpmlua.c rpmmalloc.c rpmmg.c rpmnss.c rpmpgp.c \
	rpmrpc.c rpmsq.c rpmssl.c rpmsw.c rpmxar.c \
	strcasecmp.c stringbuf.c strtolocale.c url.c ugid.c rpmuuid.c
librpmio_la_LDFLAGS = -release $(LT_CURRENT).$(LT_REVISION)
if HAVE_LD_VERSION_SCRIPT
librpmio_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpmio.vers
endif
librpmio_la_LIBADD =
if ENABLE_BUILD_INTLIBDEP
librpmio_la_LIBADD += \
	$(top_builddir)/misc/librpmmisc.la
endif
if ENABLE_BUILD_MAXEXTLIBDEP
librpmio_la_LDFLAGS += $(LDFLAGS)
librpmio_la_LIBADD  += $(LIBS)
endif
librpmio.la: $(librpmio_la_OBJECTS) $(librpmio_la_DEPENDENCIES) 
	$(librpmio_la_LINK) -rpath $(usrlibdir) $(librpmio_la_OBJECTS) $(librpmio_la_LIBADD)

install-data-hook:
if !ENABLE_BUILD_LAFILES
	-for l in $(usrlib_LTLIBRARIES); do \
	    rm -f $(DESTDIR)$(usrlibdir)/$${l}; \
	done
endif
if WITH_PATH_VERSIONED
	-for l in $(usrlib_LTLIBRARIES); do \
	    base=`echo "$${l}" | sed -e 's;\.la$$;;'`; \
	    if [ -f $(DESTDIR)$(usrlibdir)/$${base}.a ]; then \
	        mv $(DESTDIR)$(usrlibdir)/$${base}.a \
	           $(DESTDIR)$(usrlibdir)/$${base}-$(VERSION).a; \
	    fi; \
	done
endif

#BUILT_SOURCES = rpmio.lcd

rpmio.lcd: Makefile.am ${librpmio_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint -load ../beecrypt/beecrypt.lcd $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES) -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmio_la_SOURCES:%=rpmio/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES)

.PHONY:	lint_rpmgrep
lint_rpmgrep:
	$(LINT) -f .splintrc_rpmgrep $(DEFS) $(INCLUDES) rpmgrep.c

TESTS += RunGrepTest
dist_noinst_SCRIPTS += RunGrepTest
bin_PROGRAMS += rpmgrep
man_MANS +=	rpmgrep.1

rpmgrep_SOURCES = rpmgrep.c
rpmgrep_LDADD = $(RPMIO_LDADD)

EXTRA_DIST += \
  testdata/grepinput \
  testdata/grepinput8 \
  testdata/grepinputv \
  testdata/grepinputx \
  testdata/greplist \
  testdata/grepoutput \
  testdata/grepoutput8 \
  testdata/grepoutputN \
  testdata/testinput1 \
  testdata/testinput2 \
  testdata/testinput3 \
  testdata/testinput4 \
  testdata/testinput5 \
  testdata/testinput6 \
  testdata/testinput7 \
  testdata/testinput8 \
  testdata/testinput9 \
  testdata/testinput10 \
  testdata/testoutput1 \
  testdata/testoutput2 \
  testdata/testoutput3 \
  testdata/testoutput4 \
  testdata/testoutput5 \
  testdata/testoutput6 \
  testdata/testoutput7 \
  testdata/testoutput8 \
  testdata/testoutput9 \
  testdata/testoutput10 \
  testdata/wintestinput3 \
  testdata/wintestoutput3

tdir_SOURCES = tdir.c
tdir_LDADD = $(RPMIO_LDADD)

tfts_SOURCES = tfts.c
tfts_LDADD = $(RPMIO_LDADD)

tget_SOURCES = tget.c
tget_LDADD = $(RPMIO_LDADD)

tglob_SOURCES = tglob.c
tglob_LDADD = $(RPMIO_LDADD)

thkp_SOURCES = thkp.c
thkp_LDADD = $(RPMIO_LDADD)

thtml_SOURCES = thtml.c
thtml_CFLAGS = $(CFLAGS) -I/usr/include/libxml2/
thtml_LDADD = $(RPMIO_LDADD)

tinv_SOURCES = tinv.c
tinv_LDADD = $(RPMIO_LDADD)

tkey_SOURCES = gengpg.h tkey.c rpmgc.c rpmnss.c rpmssl.c
tkey_CFLAGS  = $(CFLAGS)
tkey_LDADD = $(RPMIO_LDADD) -lgcrypt
gengpg.h:
	-sh ./gengpg.sh > gengpg.h

tmacro_SOURCES =
tmacro_LDADD = tmacro.o $(RPM_LDADD)
tmacro.o:  macro.c
	$(COMPILE) -DDEBUG_MACROS -o $@ -c $<

tmagic_SOURCES = tmagic.c
tmagic_LDADD = $(RPMIO_LDADD)

tmire_SOURCES = tmire.c
tmire_LDADD = $(RPMIO_LDADD)

tput_SOURCES = tput.c
tput_LDADD = $(RPMIO_LDADD)

tpw_SOURCES = tpw.c
tpw_LDFLAGS = $(RPMIO_LDADD)

trpmio_SOURCES = trpmio.c
trpmio_LDADD = $(RPMIO_LDADD)

tsw_SOURCES = tsw.c
tsw_LDFLAGS = $(RPMIO_LDADD)

ttar_SOURCES = ttar.c
ttar_LDFLAGS = $(RPM_LDADD)

dumpasn1_SOURCES = dumpasn1.c

lookup3_SOURCES = lookup3.c
lookup3_CFLAGS	= $(CFLAGS) -D_JLU3_SELFTEST
lookup3_LDADD = $(RPMIO_LDADD)

