# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_NEON_INCLUDE@ \
	@WITH_LIBELF_INCLUDE@ \
	@WITH_MAGIC_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc \
	@INCPATH@

EXTRA_DIST =	hashtab.h

EXTRA_PROGRAMS = debugedit convertdb1

LDADD = $(top_builddir)/lib/librpm.la

bin_PROGRAMS =	rpmdeps rpmdigest rpmmtree rpmrepo @WITH_LIBDWARF_DEBUGEDIT@

convertdb1_SOURCES =	convertdb1.c

debugedit_SOURCES =	debugedit.c hashtab.c
debugedit_LDADD =	\
	@WITH_LIBELF_LIB@\
	@WITH_BEECRYPT_LIB@\
	@WITH_POPT_LIB@

rpmdeps_SOURCES =	rpmdeps.c

rpmdigest_SOURCES =	rpmdigest.c

rpmmtree_SOURCES =	rpmmtree.c

rpmrepo_SOURCES =	rpmrepo.c
rpmrepo_LDADD =		$(LDADD) -lsqlite3

$(PROGRAMS): $(LDADD)
