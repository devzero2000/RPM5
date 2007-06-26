# Makefile for rpmbuild library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = librpmbuild.vers

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc

LIBS =

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmbuild.h rpmspec.h
noinst_HEADERS = buildio.h

LDFLAGS = -L$(RPM_BUILD_ROOT)$(usrlibdir) -L$(DESTDIR)$(usrlibdir)

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c expression.c files.c misc.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c poptBT.c reqprov.c spec.c
librpmbuild_la_LDFLAGS = -no-undefined -release $(LT_CURRENT).$(LT_REVISION) $(LDFLAGS) \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la

if HAVE_LD_VERSION_SCRIPT
librpmbuild_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpmbuild.vers
endif

clean-local:
	rm -f *.o # .created $(FILELOBJS)

#BUILT_SOURCES = rpmbuild.lcd

rpmbuild.lcd: Makefile.am ${librpmbuild_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmbuild_la_SOURCES:%=build/%)

.PHONY:	lint
lint:
	$(LINT) ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES}
