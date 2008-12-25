# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

SUBDIRS = # tests

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc \
	@WITH_DB_CPPFLAGS@ \
	@WITH_ZLIB_CPPFLAGS@ \
	@WITH_LUA_CPPFLAGS@ \
	@WITH_FILE_CPPFLAGS@ \
	@WITH_XAR_CPPFLAGS@

EXTRA_DIST = db3.c sqlite.c db_emu.h librpmdb.vers

EXTRA_PROGRAMS = tjfn

tjfn_SOURCES = tjfn.c
tjfn_LDFLAGS = @LDFLAGS_STATIC@
tjfn_LDADD = $(mylibs)

pkgincdir = $(pkgincludedir)$(WITH_PATH_VERSIONED_SUFFIX)
pkginc_HEADERS = pkgio.h rpmdb.h rpmevr.h rpmns.h rpmtag.h rpmtypes.h
noinst_HEADERS = \
	fprint.h header_internal.h legacy.h rpmdpkg.h \
	rpmtd.h rpmwf.h signature.h

pkglibdir =		@USRLIBRPM@
pkglib_PROGRAMS =

mylibs = \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/misc/librpmmisc.la

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = \
	dbconfig.c fprint.c \
	hdrfmt.c hdrNVR.c header.c header_internal.c legacy.c merge.c \
	pkgio.c poptDB.c rpmdb.c rpmdpkg.c rpmevr.c rpmns.c rpmtd.c rpmwf.c \
	signature.c tagname.c tagtbl.c
librpmdb_la_LDFLAGS = -release $(LT_CURRENT).$(LT_REVISION)
if HAVE_LD_VERSION_SCRIPT
librpmdb_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpmdb.vers
endif
librpmdb_la_LIBADD = $(DBLIBOBJS)
if ENABLE_BUILD_INTLIBDEP
librpmdb_la_LIBADD += \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la
endif
if ENABLE_BUILD_MAXEXTLIBDEP
librpmdb_la_LDFLAGS += $(LDFLAGS)
librpmdb_la_LIBADD  += $(LIBS)
endif
librpmdb_la_DEPENDENCIES = $(DBLIBOBJS)
librpmdb.la: $(librpmdb_la_OBJECTS) $(librpmdb_la_DEPENDENCIES) 
	$(librpmdb_la_LINK) -rpath $(usrlibdir) $(librpmdb_la_OBJECTS) $(librpmdb_la_LIBADD)

install-data-hook:
if !ENABLE_BUILD_LAFILES
	-for l in $(usrlib_LTLIBRARIES); do \
	    rm -f $(DESTDIR)$(usrlibdir)/$${l}; \
	done
endif
if WITH_PATH_VERSIONED
	-for l in $(usrlib_LTLIBRARIES); do \
	    base=`echo "$${l}" | sed -e 's;\.la$$;;'`; \
	    if [ -f $(DESTDIR)$(usrlibdir)/$${base}.a ]; then \
	        mv $(DESTDIR)$(usrlibdir)/$${base}.a \
	           $(DESTDIR)$(usrlibdir)/$${base}-$(VERSION).a; \
	    fi; \
	done
endif

tagtbl.c: Makefile $(top_srcdir)/rpmdb/rpmtag.h 
	@echo '#include "system.h"' > $@
	@echo '#define _RPMTAG_INTERNAL' >> $@
	@echo '#include <rpmtag.h>' >> $@
	@echo '#include "debug.h"' >> $@
	@echo '' >> $@
	@echo '/*@access headerTagTableEntry @*/' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'static const struct headerTagTableEntry_s _rpmTagTable[] = {' >> $@
	${AWK} '/[ \t](RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ {tt = "NULL"; ta = "ANY"; if ($$5 == "c") {tt = "UINT8"; ta = "SCALAR"} if ($$5 == "c[]") {tt = "UINT8"; ta = "ARRAY"} if ($$5 == "h") {tt = "UINT16"; ta = "SCALAR"} if ($$5 == "h[]") {tt = "UINT16"; ta = "ARRAY"} if ($$5 == "i") {tt = "UINT32"; ta = "SCALAR"} if ($$5 == "i[]") {tt = "UINT32"; ta = "ARRAY"} if ($$5 == "l") {tt = "UINT64"; ta = "SCALAR"} if ($$5 == "l[]") {tt = "UINT64"; ta = "ARRAY"} if ($$5 == "s") {tt = "STRING"; ta = "SCALAR"} if ($$5 == "s[]") {tt = "STRING_ARRAY"; ta = "ARRAY"} if ($$5 == "s{}") {tt = "I18NSTRING"; ta = "SCALAR"} if ($$5 == "x") {tt = "BIN"; ta = "SCALAR"} if ($$2 == "=") { printf("\t{ \"%s\",	%s	RPM_%s_TYPE + RPM_%s_RETURN_TYPE },\n", $$1, $$3, tt, ta) } else { printf("\t{ \"%s\",	%s,	RPM_%s_TYPE + RPM_%s_RETURN_TYPE  },\n", $$2, $$3, tt, ta) } }' < ${top_srcdir}/rpmdb/rpmtag.h | LC_ALL=C sort >> $@
	@echo '	{ NULL, 0, 0 }' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'headerTagTableEntry rpmTagTable = _rpmTagTable;' >> $@
	@echo '' >> $@
	@echo '/*@unchecked@*/' >> $@
	@echo 'int rpmTagTableSize = sizeof(_rpmTagTable) / sizeof(_rpmTagTable[0]) - 1;' >> $@

if WITH_DB_INTERNAL
my_DB_HEADER = $(top_builddir)/$(WITH_DB_SUBDIR)/db.h
else
my_DB_HEADER = $(srcdir)/db_emu.h
endif

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(pkgincdir)
	$(pkgincHEADERS_INSTALL) $(my_DB_HEADER) $(DESTDIR)$(pkgincdir)/db.h

check-local:

clean-local:
	rm -f *.o tagtbl.c

BUILT_SOURCES = tagtbl.c

rpmdb.lcd: Makefile.am ${librpmdb_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-splint ${DEFS} ${INCLUDES} ${librpmdb_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmdb_la_SOURCES:%=rpmdb/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpmdb_la_SOURCES) $(DBLIBSRCS)

.PHONY:	lcov-reset	# run lcov from scratch, always
lcov-reset:
	make lcov-run
	make lcov-report

.PHONY:	lcov		# run lcov from scratch if the dir is not there
lcov:
	make lcov-reset

.PHONY:	lcov-run	# reset run coverage tests
lcov-run:
	@-rm -rf lcov
	find . -name "*.gcda" -exec rm {} \;
	make check

.PHONY:	lcov-report	# generate report based on current coverage data
lcov-report:
	mkdir lcov
	lcov --directory . --capture --output-file lcov/lcov.info
	lcov -l lcov/lcov.info | grep -v "`cd $(top_srcdir) && pwd`" | cut -d: -f1 > lcov/remove
	lcov -r lcov/lcov.info `cat lcov/remove` > lcov/lcov.cleaned.info
	rm lcov/remove
	mv lcov/lcov.cleaned.info lcov/lcov.info
	genhtml -t "$(PACKAGE_STRING)" -o lcov lcov/lcov.info

#.PHONY:	lcov-upload
#lcov-upload: lcov
#	rsync -rvz -e ssh --delete lcov/* ???

tdbi: tdbi.o librpmdb.la
	$(LINK) @LDFLAGS_STATIC@ -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)

tdpkg: tdpkg.o librpmdb.la
	$(LINK) @LDFLAGS_STATIC@ -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)

tmpkg: tmpkg.o librpmdb.la
	$(LINK) @LDFLAGS_STATIC@ -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)

tsql: tsql.o librpmdb.la
	$(LINK) @LDFLAGS_STATIC@ -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)

tpkgadd: tpkgadd.o librpmdb.la
	$(LINK) @LDFLAGS_STATIC@ -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)

trepomd: trepomd.o librpmdb.la
	$(LINK) @LDFLAGS_STATIC@ -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)

ttag: ttag.o librpmdb.la
	$(LINK) @LDFLAGS_STATIC@ -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)
