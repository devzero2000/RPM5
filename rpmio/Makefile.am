# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = tax.c tdir.c tfts.c tget.c thkp.c tput.c tglob.c tinv.c tkey.c trpmio.c lookup3.c tpw.c librpmio.vers

EXTRA_PROGRAMS = tax tdir tfts tget thkp tmacro tput tglob tinv tkey tmagic trpmio tsw dumpasn1 lookup3 tpw

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

RPM_LDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

pkgincdir = $(pkgincludedir)$(WITH_PATH_VERSIONED_SUFFIX)
pkginc_HEADERS = \
	argv.h envvar.h fts.h mire.h rpmbeecrypt.h rpmcb.h rpmdav.h rpmhash.h \
	rpmio.h rpmio-stub.h rpmlog.h rpmmacro.h rpmmg.h rpmpgp.h \
	rpmsq.h rpmsw.h rpmurl.h rpmxar.h ugid.h
noinst_HEADERS = \
	md2.h md4.h rmd128.h rmd160.h rmd256.h rmd320.h sha224.h \
	salsa10.h salsa20.h tiger.h \
	LzmaDecode.h rpmhook.h rpmio_internal.h rpmlua.h

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c digest.c fts.c getpass.c macro.c mire.c mount.c \
	md2.c md4.c rmd128.c rmd160.c rmd256.c rmd320.c sha224.c \
	salsa10.c salsa20.c tiger.c LzmaDecode.c \
	rpmdav.c rpmhash.c rpmhook.c rpmio.c rpmio-stub.c \
	rpmlog.c rpmlua.c rpmmalloc.c rpmmg.c rpmpgp.c \
	rpmrpc.c rpmsq.c rpmsw.c rpmxar.c \
	strcasecmp.c strtolocale.c url.c ugid.c
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

tmagic_SOURCES = tmagic.c
tmagic_LDADD = $(RPM_LDADD)

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

tpw_SOURCES = tpw.c
tpw_LDFLAGS = $(RPM_LDADD)

tsw_SOURCES = tsw.c
tsw_LDFLAGS = $(RPM_LDADD)

dumpasn1_SOURCES = dumpasn1.c

lookup3_SOURCES = lookup3.c
lookup3_CFLAGS	= $(CFLAGS) -D_JLU3_SELFTEST
lookup3_LDADD = $(RPM_LDADD)

