# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I. \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	@WITH_ZLIB_INCLUDE@ \
	@INCPATH@ \
	-I$(top_srcdir)/misc

EXTRA_DIST =	rpminject.c

EXTRA_PROGRAMS = rpminject

LDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la \
	@INTLLIBS@

noinst_PROGRAMS = \
	dump dumpdb rpmarchive rpmheader rpmlead rpmsignature rpmsort

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = javadeps

javadeps_SOURCES =	javadeps.c

rpmsort_SOURCES =	rpmsort.c
rpmsort_LDFLAGS =	@LDFLAGS_STATIC@

$(PROGRAMS): $(LDADD)
