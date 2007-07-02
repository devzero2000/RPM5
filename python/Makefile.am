# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

pylibdir = ${prefix}/lib@MARK64@/python@WITH_PYTHON_VERSION@
pyincdir = $(prefix)/include/python@WITH_PYTHON_VERSION@

SUBDIRS = rpm

EXTRA_DIST = system.h rpmdebug-py.c

AM_CPPFLAGS = -I. \
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
	-I$(pyincdir)

noinst_HEADERS = header-py.h \
	rpmal-py.h rpmds-py.h rpmdb-py.h rpmfd-py.h rpmfts-py.h \
	rpmfi-py.h rpmmi-py.h rpmps-py.h rpmrc-py.h rpmte-py.h rpmts-py.h \
	spec-py.h

mylibs = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

#LDADD =
#LIBS =

rpmdir = $(pylibdir)/site-packages/rpm
rpm_LTLIBRARIES = _rpmmodule.la

_rpmmodule_la_LDFLAGS = $(mylibs) -module -avoid-version
_rpmmodule_la_LIBADD =

_rpmmodule_la_SOURCES = rpmmodule.c header-py.c \
		       rpmal-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfts-py.c \
		       rpmfi-py.c rpmmi-py.c rpmps-py.c rpmrc-py.c rpmte-py.c \
		       rpmts-py.c spec-py.c

# rpmmodule.c header-py.c \
# 	rpmal-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfi-py.c rpmmi-py.c \
# 	rpmps-py.c rpmrc-py.c rpmte-py.c rpmts-py.c
# rpmmodule.c header-py.c
splint_srcs = \
 	rpmal-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfts-py.c \
	rpmfi-py.c rpmmi-py.c rpmps-py.c rpmrc-py.c rpmte-py.c rpmts-py.c \
	spec-py.c

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(splint_srcs)
