# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@WITH_LIBELF_INCLUDE@ \
	@WITH_ZLIB_INCLUDE@ \
	-I$(top_srcdir)/file \
	@INCPATH@ \
	-I$(top_srcdir)/misc

EXTRA_DIST =	rpminject.c rpmsort.c sections.h utils.h

EXTRA_PROGRAMS = rpminject rpmsort

LDADD = \
	$(top_builddir)/lib/librpm.la \
	@WITH_LIBELF_LIB@ \
	@INTLLIBS@

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

noinst_PROGRAMS = \
	dump dumpdb rpmarchive rpmheader rpmlead rpmsignature

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = javadeps rpmcache rpmfile striptofile unstripfile

bin_PROGRAMS =	rpmgraph

javadeps_SOURCES =	javadeps.c

rpmcache_SOURCES =	rpmcache.c
#rpmcache_LDFLAGS =	@LDFLAGS_STATIC@

rpmfile.c: $(top_builddir)/file/file.c
	rm -f $@
	ln -s $< $@

rpmfile_SOURCES =	rpmfile.c
rpmfile_LDADD =		$(top_builddir)/rpmio/librpmio.la
#rpmfile_LDFLAGS =	$(staticLDFLAGS)

rpmgraph_SOURCES =	rpmgraph.c
#rpmgraph_LDFLAGS =	$(staticLDFLAGS)

rpminject_SOURCES =	rpminject.c
#rpminject_LDFLAGS =	$(staticLDFLAGS)

rpmsort_SOURCES =	rpmsort.c
#rpmsort_LDFLAGS =	$(staticLDFLAGS)

striptofile_SOURCES =	striptofile.c sections.c utils.c
striptofile_LDFLAGS =	-lelf

unstripfile_SOURCES =	unstripfile.c sections.c utils.c
unstripfile_LDFLAGS =	-lelf

$(PROGRAMS): $(LDADD)
