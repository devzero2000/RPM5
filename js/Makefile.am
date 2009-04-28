# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc \
	@WITH_DB_CPPFLAGS@ \
	@WITH_FILE_CPPFLAGS@ \
	@WITH_JS_CPPFLAGS@ \
	@WITH_LUA_CPPFLAGS@ \
	@WITH_SYCK_CPPFLAGS@ \
	@WITH_XAR_CPPFLAGS@ \
	@WITH_ZLIB_CPPFLAGS@

EXTRA_DIST =

EXTRA_PROGRAMS = tjs

RPMMISC_LDADD_COMMON = \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

RPMIO_LDADD_COMMON = \
	$(top_builddir)/rpmio/librpmio.la \
	$(RPMMISC_LDADD_COMMON)

RPMDB_LDADD_COMMON = \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(RPMIO_LDADD_COMMON)

RPM_LDADD_COMMON = \
	$(top_builddir)/lib/librpm.la \
	$(RPMDB_LDADD_COMMON)

RPMBUILD_LDADD_COMMON = \
	$(top_builddir)/build/librpmbuild.la \
	$(RPM_LDADD_COMMON)

noinst_LTLIBRARIES = rpmjsm.la

rpmjsm_la_CFLAGS = -fno-strict-aliasing
rpmjsm_la_LDFLAGS = -module -avoid-version
rpmjsm_la_LIBADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

noinst_HEADERS = \
	rpmjsfile.h \
	rpmds-js.h rpmfi-js.h rpmhdr-js.h rpmmc-js.h rpmmi-js.h rpmps-js.h \
	rpmte-js.h rpmts-js.h \
	syck-js.h uuid-js.h

rpmjsm_la_SOURCES = \
	rpmjsfile.c \
	rpmds-js.c rpmfi-js.c rpmhdr-js.c rpmmc-js.c rpmmi-js.c rpmps-js.c \
	rpmte-js.c rpmts-js.c \
	syck-js.c uuid-js.c

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(rpmjs_la_sources)

tjs_SOURCES = tjs.c rpmjsm.la
tjs_LDADD = rpmjsm.la $(RPM_LDADD_COMMON)

