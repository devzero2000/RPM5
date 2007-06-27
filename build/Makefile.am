# Makefile for rpmbuild library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = librpmbuild.vers

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
	@WITH_FILE_CPPFLAGS@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmbuild.h rpmspec.h
noinst_HEADERS = buildio.h

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c expression.c files.c misc.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c poptBT.c reqprov.c spec.c
librpmbuild_la_LDFLAGS = -release $(LT_CURRENT).$(LT_REVISION)
if HAVE_LD_VERSION_SCRIPT
librpmbuild_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpmbuild.vers
endif
librpmbuild_la_LIBADD =
librpmbuild.la: $(librpmbuild_la_OBJECTS) $(librpmbuild_la_DEPENDENCIES) 
	$(librpmbuild_la_LINK) -rpath $(usrlibdir) $(librpmbuild_la_OBJECTS) $(librpmbuild_la_LIBADD)

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
