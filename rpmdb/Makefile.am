# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

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

EXTRA_DIST = db3.c sqlite.c db_emu.h librpmdb.vers

EXTRA_PROGRAMS = tjfn

tjfn_SOURCES = tjfn.c
tjfn_LDFLAGS = @LDFLAGS_STATIC@
tjfn_LDADD = librpmdb.la

pkgincdir = $(pkgincludedir)$(WITH_PATH_VERSIONED_SUFFIX)
pkginc_HEADERS = header.h hdrinline.h rpmdb.h
noinst_HEADERS = fprint.h header_internal.h legacy.h

pkglibdir =		@USRLIBRPM@
pkglib_PROGRAMS =

mylibs = \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/misc/librpmmisc.la

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = \
	dbconfig.c fprint.c \
	hdrNVR.c header.c header_internal.c legacy.c merge.c \
	poptDB.c rpmdb.c \
	tagname.c tagtbl.c
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

tagtbl.c: Makefile.am $(top_srcdir)/lib/rpmlib.h 
	@echo '/*@-bounds@*/' > $@
	@echo '#include "system.h"' >> $@
	@echo '#include <rpmlib.h>' >> $@
	@echo '#include "debug.h"' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'static const struct headerTagTableEntry_s rpmTagTbl[] = {' >> $@
	${AWK} '/(RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ {tt = "NULL"; ta = "ANY"; if ($$5 == "c") {tt = "CHAR"; ta = "SCALAR"} if ($$5 == "c[]") {tt = "CHAR"; ta = "ARRAY"} if ($$5 == "h") {tt = "INT16"; ta = "SCALAR"} if ($$5 == "h[]") {tt = "INT16"; ta = "ARRAY"} if ($$5 == "i") {tt = "INT32"; ta = "SCALAR"} if ($$5 == "i[]") {tt = "INT32"; ta = "ARRAY"} if ($$5 == "l") {tt = "INT64"; ta = "SCALAR"} if ($$5 == "l[]") {tt = "INT64"; ta = "ARRAY"} if ($$5 == "s") {tt = "STRING"; ta = "SCALAR"} if ($$5 == "s[]") {tt = "STRING_ARRAY"; ta = "ARRAY"} if ($$5 == "s{}") {tt = "I18NSTRING"; ta = "SCALAR"} if ($$5 == "x") {tt = "BIN"; ta = "SCALAR"} if ($$2 == "=") { printf("\t{ \"%s\",	%s	RPM_%s_TYPE + RPM_%s_RETURN_TYPE },\n", $$1, $$3, tt, ta) } else { printf("\t{ \"%s\",	%s,	RPM_%s_TYPE + RPM_%s_RETURN_TYPE  },\n", $$2, $$3, tt, ta) } }' < ${top_srcdir}/lib/rpmlib.h | sort >> $@
	@echo '	{ NULL, 0, 0 }' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'const struct headerTagTableEntry_s * rpmTagTable = rpmTagTbl;' >> $@
	@echo '' >> $@
	@echo '/*@unchecked@*/' >> $@
	@echo 'const int rpmTagTableSize = sizeof(rpmTagTbl) / sizeof(rpmTagTbl[0]) - 1;' >> $@
	@echo '/*@=bounds@*/' >> $@

if WITH_DB_INTERNAL
my_DB_HEADER = $(top_builddir)/$(WITH_DB_SUBDIR)/db.h
else
my_DB_HEADER = $(srcdir)/db_emu.h
endif

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(pkgincdir)
	$(pkgincHEADERS_INSTALL) $(my_DB_HEADER) $(DESTDIR)$(pkgincdir)/db.h

clean-local:
	rm -f *.o

BUILT_SOURCES = tagtbl.c

rpmdb.lcd: Makefile.am ${librpmdb_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-splint ${DEFS} ${INCLUDES} ${librpmdb_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmdb_la_SOURCES:%=rpmdb/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpmdb_la_SOURCES) $(DBLIBSRCS)

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
