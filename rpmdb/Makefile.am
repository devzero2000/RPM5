# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc

EXTRA_DIST = db3.c sqlite.c db_emu.h librpmdb.vers

EXTRA_PROGRAMS = tjfn

tjfn_SOURCES = tjfn.c
tjfn_LDFLAGS = @LDFLAGS_STATIC@
tjfn_LDADD = librpmdb.la

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = db.h header.h hdrinline.h rpmdb.h
noinst_HEADERS = fprint.h header_internal.h legacy.h

pkglibdir =		@USRLIBRPM@
versionlibdir = 	$(pkglibdir)/@VERSION@
versionlib_PROGRAMS = \
	db_archive db_checkpoint db_deadlock db_dump \
	db_hotbackup db_load db_printlog db_recover \
	db_stat db_upgrade db_verify 
if WITH_DB_RPC
versionlib_PROGRAMS += db_svc
endif

mylibs = librpmdb.la

LIBS =

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = \
	dbconfig.c fprint.c \
	hdrNVR.c header.c header_internal.c legacy.c merge.c \
	poptDB.c rpmdb.c \
	tagname.c tagtbl.c
librpmdb_la_LDFLAGS = -no-undefined -release $(LT_CURRENT).$(LT_REVISION)
librpmdb_la_LIBADD = \
	$(DBLIBOBJS) \
	$(top_builddir)/$(WITH_DB_SUBDIR)/libdb-4.5.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la
librpmdb_la_DEPENDENCIES = $(DBLIBOBJS)

if HAVE_LD_VERSION_SCRIPT
librpmdb_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpmdb.vers
endif

# XXX make sure that db.h symlink is generated
dbconfig.c $(DBLIBSRCS): db.h

dbconfig.o: $(top_srcdir)/dbconfig.c
	$(COMPILE) -o $@ -c $<

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

db.h:
	@ln -sf ../$(WITH_DB_SUBDIR)/db.h $@

db_archive_SOURCES =
db_archive_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_archive.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_checkpoint_SOURCES =
db_checkpoint_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_checkpoint.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_deadlock_SOURCES =
db_deadlock_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_deadlock.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_dump_SOURCES =
db_dump_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_dump.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_hotbackup_SOURCES =
db_hotbackup_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_hotbackup.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_load_SOURCES =
db_load_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_load.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_printlog_SOURCES =
db_printlog_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_printlog.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/btree_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/crdel_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/dbreg_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/fileops_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/hash_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/qam_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/rep_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/txn_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_recover_SOURCES =
db_recover_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_recover.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_stat_SOURCES =
db_stat_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_stat.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_svc_SOURCES =
db_svc_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_proc.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_svc.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_util.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_xdr.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/gen_db_server.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	librpmdb.la

db_upgrade_SOURCES =
db_upgrade_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_upgrade.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

db_verify_SOURCES =
db_verify_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_verify.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

clean-local:
	rm -f *.o db.h

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
