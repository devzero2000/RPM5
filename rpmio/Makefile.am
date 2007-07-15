# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = tax.c tdir.c tficl.c tfts.c tget.c thkp.c tput.c tglob.c tinv.c tkey.c trpmio.c lookup3.c librpmio.vers

EXTRA_PROGRAMS = tax tdir tfts tget thkp tmacro tput tglob tinv tkey trpmio tsw dumpasn1 lookup3

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
	@WITH_FILE_CPPFLAGS@

RPM_LDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	argv.h envvar.h fts.h mire.h rpmdav.h rpmhash.h \
	rpmio-stub.h \
	rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h rpmpgp.h \
	rpmsq.h rpmsw.h ugid.h
noinst_HEADERS = \
	md2.h md4.h rmd128.h rmd160.h rmd256.h rmd320.h sha224.h \
	salsa10.h salsa20.h tiger.h \
	LzmaDecode.h rpmio_internal.h rpmlua.h rpmhook.h

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c digest.c fts.c getpass.c macro.c mire.c \
	md2.c md4.c rmd128.c rmd160.c rmd256.c rmd320.c sha224.c \
	salsa10.c salsa20.c tiger.c \
	rpmio-stub.c \
	rpmdav.c rpmhash.c rpmhook.c rpmio.c rpmlog.c rpmlua.c rpmmalloc.c \
	rpmpgp.c rpmrpc.c rpmsq.c rpmsw.c strcasecmp.c strtolocale.c \
	url.c ugid.c \
	LzmaDecode.c
librpmio_la_LDFLAGS = -release $(LT_CURRENT).$(LT_REVISION)
if HAVE_LD_VERSION_SCRIPT
librpmio_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpmio.vers
endif
librpmio_la_LIBADD =
librpmio.la: $(librpmio_la_OBJECTS) $(librpmio_la_DEPENDENCIES) 
	$(librpmio_la_LINK) -rpath $(usrlibdir) $(librpmio_la_OBJECTS) $(librpmio_la_LIBADD)

#BUILT_SOURCES = rpmio.lcd

rpmio.lcd: Makefile.am ${librpmio_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint -load ../beecrypt/beecrypt.lcd $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES) -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmio_la_SOURCES:%=rpmio/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES)

tax_SOURCES = tax.c
tax_LDADD = $(RPM_LDADD)

tdir_SOURCES = tdir.c
tdir_LDADD = $(RPM_LDADD)

tfts_SOURCES = tfts.c
tfts_LDADD = $(RPM_LDADD)

tget_SOURCES = tget.c
tget_LDADD = $(RPM_LDADD)

thkp_SOURCES = thkp.c
thkp_LDADD = $(RPM_LDADD)

tmacro_SOURCES =
tmacro_LDADD = tmacro.o $(RPM_LDADD)
tmacro.o:  macro.c
	$(COMPILE) -DDEBUG_MACROS -o $@ -c $<

tput_SOURCES = tput.c
tput_LDADD = $(RPM_LDADD)

tglob_SOURCES = tglob.c
tglob_LDADD = $(RPM_LDADD)

tinv_SOURCES = tinv.c
tinv_LDADD = $(RPM_LDADD)

tkey_SOURCES = tkey.c
tkey_LDADD = $(RPM_LDADD)

trpmio_SOURCES = trpmio.c
trpmio_LDADD = $(RPM_LDADD)

tsw_SOURCES = tsw.c
tsw_LDFLAGS = $(RPM_LDADD)

dumpasn1_SOURCES = dumpasn1.c

lookup3_SOURCES = lookup3.c
lookup3_CFLAGS	= $(CFLAGS) -D_JLU3_SELFTEST
lookup3_LDADD = $(RPM_LDADD)

