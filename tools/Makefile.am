# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc \
	@WITH_XAR_CPPFLAGS@

EXTRA_DIST =	hashtab.h

EXTRA_PROGRAMS = debugedit rpmkey txar

myLDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

pkglibdir =		@USRLIBRPM@
pkglib_PROGRAMS =	rpmcache rpmcmp rpmdeps rpmdigest @WITH_LIBELF_DEBUGEDIT@

debugedit_SOURCES =	debugedit.c hashtab.c
debugedit_LDADD =	$(myLDADD)

rpmcache_SOURCES =	rpmcache.c
rpmcache_LDADD =	$(myLDADD)

rpmcmp_SOURCES =	rpmcmp.c
rpmcmp_LDADD =		$(myLDADD)

rpmdeps_SOURCES =	rpmdeps.c
rpmdeps_LDADD =		$(myLDADD)

rpmdigest_SOURCES =	rpmdigest.c
rpmdigest_LDADD =	$(myLDADD)

##
##  keyctl(1) clone
##
rpmkey_SOURCES =        rpmkey.c
rpmkey_LDADD =          $(myLDADD) -lkeyutils

##
## XAR package format work-in-progress.
##
txar_SOURCES =		txar.c
txar_LDADD =		$(myLDADD) @WITH_XAR_LDFLAGS@
txar_LIBS =		@WITH_XAR_LIBS@

##
##  provide Berkeley-DB tools
##

if WITH_DB_INTERNAL
if WITH_DB_TOOLS_INTEGRATED
pkglib_PROGRAMS += db_tool
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
	$(top_builddir)/db3/rep_autop.lo \
	$(top_builddir)/db3/txn_autop.lo
if WITH_DB_RPC
#   for db_svc
db_tool_LDADD += \
	$(top_builddir)/db3/db_server_proc.lo \
	$(top_builddir)/db3/db_server_svc.lo \
	$(top_builddir)/db3/gen_db_server.lo
endif
db_tool_LDADD += \
	$(top_builddir)/db3/libdb-4.6.la
endif
endif

if WITH_DB_INTERNAL
my_DB_TOOLS = db_stat db_load db_dump db_archive db_checkpoint db_deadlock db_recover db_upgrade db_verify db_hotbackup db_printlog
if WITH_DB_RPC
my_DB_TOOLS += db_svc
endif
endif

install-data-local:
if WITH_DB_INTERNAL
if WITH_DB_TOOLS_INTEGRATED
	$(INSTALL_PROGRAM_ENV) $(LIBTOOL) --mode=install $(pkglibPROGRAMS_INSTALL) \
		$(builddir)/db_tool $(DESTDIR)$(pkglibdir)/db_tool
	for tool in . $(my_DB_TOOLS); do \
		test ".$$tool" = .. && continue; \
		rm -f $(DESTDIR)$(pkglibdir)/$$tool; \
		ln $(DESTDIR)$(pkglibdir)/db_tool $(DESTDIR)$(pkglibdir)/$$tool; \
	done
else
	for tool in . $(my_DB_TOOLS); do \
		test ".$$tool" = .. && continue; \
		$(INSTALL_PROGRAM_ENV) $(LIBTOOL) --mode=install $(pkglibPROGRAMS_INSTALL) \
		$(top_builddir)/$(WITH_DB_SUBDIR)/$$tool $(DESTDIR)$(pkglibdir)/$$tool; \
	done
endif
endif

