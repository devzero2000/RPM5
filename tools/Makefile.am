# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

SUBDIRS = # tests

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc \
	-I$(top_srcdir)/build \
	@WITH_DB_CPPFLAGS@ \
	@WITH_PCRE_CPPFLAGS@ \
	@WITH_XAR_CPPFLAGS@

EXTRA_DIST =	augtool.h hashtab.h

EXTRA_PROGRAMS = augtool debugedit rpmkey

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

bin_PROGRAMS =		rpm2cpio

pkgbindir =		@USRLIBRPM@/bin
pkgbin_PROGRAMS =	\
	rpmcache rpmdigest grep mtree rpmrepo rpmspecdump wget \
	rpmcmp rpmdeps @WITH_KEYUTILS_RPMKEY@ @WITH_LIBELF_DEBUGEDIT@
dist_man_MANS =		rpmgrep.1

augtool_SOURCES =	augtool.c
augtool_LDADD =		$(RPM_LDADD_COMMON) -lreadline

debugedit_SOURCES =	debugedit.c hashtab.c
debugedit_LDADD =	$(RPM_LDADD_COMMON)

rpmcache_SOURCES =	rpmcache.c
rpmcache_LDADD =	$(RPM_LDADD_COMMON)

rpmcmp_SOURCES =	rpmcmp.c
rpmcmp_LDADD =		$(RPM_LDADD_COMMON)

rpmdeps_SOURCES =	rpmdeps.c
rpmdeps_LDADD =		$(RPM_LDADD_COMMON)

rpmdigest_SOURCES =	rpmdigest.c
rpmdigest_LDADD =	$(RPMIO_LDADD_COMMON)

grep_SOURCES =		rpmgrep.c
grep_LDADD =		$(RPMIO_LDADD_COMMON)

mtree_SOURCES =		rpmmtree.c
mtree_LDADD =		$(RPM_LDADD_COMMON)

rpmrepo_SOURCES =	rpmrepo.c
rpmrepo_CPPFLAGS =	$(AM_CPPFLAGS) -I$(top_srcdir)/scripts -I$(top_builddir)/scripts $(CPPFLAGS)
rpmrepo_LDADD =		$(RPM_LDADD_COMMON)

rpmspecdump_SOURCES =	rpmspecdump.c
rpmspecdump_LDADD =	$(RPMBUILD_LDADD_COMMON)

wget_SOURCES =		rpmwget.c
wget_LDADD =		$(RPMIO_LDADD_COMMON)

##
## Traditional rpm2cpio
##
rpm2cpio_SOURCES =	rpm2cpio.c
rpm2cpio_LDADD =	$(RPM_LDADD_COMMON)

##
##  keyctl(1) clone
##
rpmkey_SOURCES =        rpmkey.c
rpmkey_LDADD =          $(RPM_LDADD_COMMON) -lkeyutils

##
## XAR <-> RPM package converter
##

if WITH_XAR
pkgbin_PROGRAMS += txar
txar_SOURCES =		txar.c
txar_LDADD =		$(RPM_LDADD_COMMON) @WITH_XAR_LDFLAGS@
txar_LIBS =		@WITH_XAR_LIBS@
endif

##
##  provide Berkeley-DB tools
##

if WITH_DB_INTERNAL
if WITH_DB_TOOLS_INTEGRATED
pkgbin_PROGRAMS += db_tool
db_tool_SOURCES = db_tool.c
db_tool_CPPFLAGS = \
	-I$(top_builddir)/db3 \
	-I$(top_builddir)/db
if WITH_DB_RPC
db_tool_CPPFLAGS += \
    -DWITH_DB_RPC
endif
db_tool_LDFLAGS =
#   for all db_xxx
db_tool_LDADD = \
    $(top_builddir)/db3/util_cache.lo \
    $(top_builddir)/db3/util_log.lo \
	$(top_builddir)/db3/util_sig.lo
#   for db_printlog
db_tool_LDADD += \
	$(top_builddir)/db3/btree_autop.lo \
	$(top_builddir)/db3/crdel_autop.lo \
	$(top_builddir)/db3/db_autop.lo \
	$(top_builddir)/db3/dbreg_autop.lo \
	$(top_builddir)/db3/fileops_autop.lo \
	$(top_builddir)/db3/hash_autop.lo \
	$(top_builddir)/db3/qam_autop.lo \
	$(top_builddir)/db3/rep_auto.lo \
	$(top_builddir)/db3/txn_autop.lo
if WITH_DB_RPC
#   for db_svc
db_tool_LDADD += \
	$(top_builddir)/db3/db_server_proc.lo \
	$(top_builddir)/db3/db_server_svc.lo \
	$(top_builddir)/db3/gen_db_server.lo
endif
db_tool_LDADD += \
	$(top_builddir)/db3/libdb-4.7.la
endif
endif

if WITH_DB_INTERNAL
my_DB_TOOLS = db_stat db_load db_dump db_archive db_checkpoint db_deadlock db_recover db_upgrade db_verify db_hotbackup db_printlog
if WITH_DB_RPC
my_DB_TOOLS += db_svc
endif
endif

install-data-local:
	@[ -d $(DESTDIR)$(pkgbindir) ] || $(mkinstalldirs) $(DESTDIR)$(pkgbindir)
if WITH_DB_INTERNAL
if WITH_DB_TOOLS_INTEGRATED
	$(INSTALL_PROGRAM_ENV) $(LIBTOOL) --mode=install $(INSTALL_PROGRAM) \
		$(builddir)/db_tool $(DESTDIR)$(pkgbindir)/db_tool
	for tool in . $(my_DB_TOOLS); do \
		test ".$$tool" = .. && continue; \
		rm -f $(DESTDIR)$(pkgbindir)/$$tool; \
		ln $(DESTDIR)$(pkgbindir)/db_tool $(DESTDIR)$(pkgbindir)/$$tool; \
	done
else
	for tool in . $(my_DB_TOOLS); do \
		test ".$$tool" = .. && continue; \
		$(INSTALL_PROGRAM_ENV) $(LIBTOOL) --mode=install $(INSTALL_PROGRAM) \
		$(top_builddir)/$(WITH_DB_SUBDIR)/$$tool $(DESTDIR)$(pkgbindir)/$$tool; \
	done
endif
endif

check-local:
	-./rpmdeps --perl > /dev/null 2>&1

.PHONY: lint_rpmgrep
lint_rpmgrep:
	$(LINT) -f .splintrc_rpmgrep $(DEFS) $(INCLUDES) rpmgrep.c

.PHONY:	lint_rpmmtree
lint_rpmmtree:
	$(LINT) -f .splintrc_rpmmtree $(DEFS) $(INCLUDES) rpmmtree.c

.PHONY:	lint_rpmrepo
lint_rpmrepo:
	$(LINT) -f .splintrc_rpmrepo $(DEFS) $(INCLUDES) rpmrepo.c

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
