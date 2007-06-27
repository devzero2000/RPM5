# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc

EXTRA_DIST =	hashtab.h

EXTRA_PROGRAMS = debugedit convertdb1

myLDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@WITH_SELINUX_LIB@ \
	@LTLIBINTL@

pkglibdir =		@USRLIBRPM@
versionlibdir =		$(pkglibdir)/@VERSION@
versionlib_PROGRAMS =	rpmdeps @WITH_LIBELF_DEBUGEDIT@

convertdb1_SOURCES =	convertdb1.c
convertdb1_LDADD =	$(myLDADD)

debugedit_SOURCES =	debugedit.c hashtab.c
debugedit_LDADD =	$(myLDADD)

rpmdeps_SOURCES =	rpmdeps.c
rpmdeps_LDADD =		$(myLDADD)

