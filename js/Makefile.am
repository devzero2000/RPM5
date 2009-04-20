# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST =

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
	@WITH_XAR_CPPFLAGS@ \
	@WITH_ZLIB_CPPFLAGS@

noinst_HEADERS =

noinst_LTLIBRARIES = rpmjsm.la

rpmjsm_la_CFLAGS = -fno-strict-aliasing
rpmjsm_la_LDFLAGS = -module -avoid-version
rpmjsm_la_LIBADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	$(top_builddir)/$(WITH_JS_SUBDIR)/Linux_All_DBG.OBJ/libjs.a
	@LTLIBINTL@

rpmjsm_la_SOURCES = \
	rpmts-js.c

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(rpmjs_la_sources)
