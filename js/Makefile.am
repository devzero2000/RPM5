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

AM_CFLAGS = $(OPENMP_CFLAGS)

EXTRA_DIST = \
	src \
	tscripts \
	words \
	legacy.cudf legacy-sol.cudf
EXTRA_PROGRAMS =

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

pkglibdir =     @USRLIBRPM@/lib
pkgbindir =     @USRLIBRPM@/bin

pkgbin_PROGRAMS =	tjs
pkglib_LTLIBRARIES =	librpmjsm.la

librpmjsm_la_CFLAGS = -fno-strict-aliasing
librpmjsm_la_LDFLAGS = -module -avoid-version
librpmjsm_la_LIBADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

noinst_HEADERS = \
	rpmjsfile.h \
	rpmaug-js.h rpmbc-js.h rpmbf-js.h rpmcudf-js.h rpmdb-js.h rpmdbc-js.h rpmdbe-js.c rpmdc-js.h rpmdig-js.h \
	rpmdir-js.h rpmds-js.h rpmfc-js.h rpmfi-js.h rpmfts-js.h rpmgi-js.h \
	rpmhdr-js.h rpmio-js.h rpmiob-js.h rpmmc-js.h rpmmg-js.h rpmmi-js.h \
	rpmmpf-js.h rpmmpw-js.h rpmps-js.h rpmseq-js.h rpmsm-js.h rpmsp-js.h rpmst-js.h rpmsw-js.h \
	rpmsx-js.h rpmsys-js.h rpmte-js.h rpmts-js.h rpmtxn-js.h rpmxar-js.h \
	syck-js.h uuid-js.h

librpmjsm_la_SOURCES = \
	rpmjsfile.c \
	rpmaug-js.c rpmbc-js.c rpmbf-js.c rpmcudf-js.c rpmdb-js.c rpmdbc-js.c rpmdbe-js.c rpmdc-js.c rpmdig-js.c \
	rpmdir-js.c rpmds-js.c rpmfc-js.c rpmfi-js.c rpmfts-js.c rpmgi-js.c \
	rpmhdr-js.c rpmio-js.c rpmiob-js.c rpmmc-js.c rpmmg-js.c rpmmi-js.c \
	rpmmpf-js.c rpmmpw-js.c rpmps-js.c rpmseq-js.c rpmsm-js.c rpmsp-js.c rpmst-js.c rpmsw-js.c \
	rpmsx-js.c rpmsys-js.c rpmte-js.c rpmts-js.c rpmtxn-js.c rpmxar-js.c \
	syck-js.c uuid-js.c

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpmjs_la_sources)

tjs_SOURCES = tjs.c
tjs_LDADD = librpmjsm.la $(RPM_LDADD_COMMON)

check-local:	tjs
	-./tjs

.PHONY:	lcov-reset	# run lcov from scratch, always
lcov-reset:
	make lcov-run
	make lcov-report

# run lcov from scratch if the dir is not there
lcov:
	make lcov-reset

.PHONY:	lcov-run	# reset run coverage tests
lcov-run:
	@-rm -rf lcov
	find .. -name "*.gcda" -exec rm {} \;
	make check

.PHONY:	lcov-report	# generate report based on current coverage data
lcov-report:
	mkdir lcov
	-lcov -d ./src -d . --capture --output-file lcov/lcov.info
	lcov -l lcov/lcov.info | grep -v "`cd $(top_srcdir) && pwd`" | cut -d: -f1 > lcov/remove
	lcov -r lcov/lcov.info `cat lcov/remove` > lcov/lcov.cleaned.info
	rm lcov/remove
	mv lcov/lcov.cleaned.info lcov/lcov.info
	genhtml -t "$(PACKAGE_STRING)" -o lcov lcov/lcov.info

.PHONY:	lcov-upload
lcov-upload: lcov
	mkdir -p /var/ftp/pub/jbj/rpm-$(VERSION)-lcov
	tar -C lcov -cf - . | (cd /var/ftp/pub/jbj/rpm-$(VERSION)-lcov; tar xf -)
#	rsync -rvz -e ssh --delete lcov/* ???
