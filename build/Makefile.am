# Makefile for rpmbuild library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@INCPATH@

LIBS =

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmbuild.h rpmfile.h rpmspec.h
noinst_HEADERS = buildio.h myftw.h

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c expression.c files.c misc.c myftw.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c poptBT.c reqprov.c spec.c
librpmbuild_la_LDFLAGS = -release 4.1 \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la

rpmfile.h:
	@ln -sf $(top_builddir)/file/file.h $@ 

clean-local:
	rm -f *.o rpmfile.h # .created $(FILELOBJS)

#BUILT_SOURCES = rpmbuild.lcd

rpmbuild.lcd: Makefile.am ${librpmbuild_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmbuild_la_SOURCES:%=build/%)

.PHONY:	lint
lint:
	$(LINT) ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES}
