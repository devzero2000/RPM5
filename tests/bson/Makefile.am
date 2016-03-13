## Process this file with automake to produce Makefile.in.
# export TSAN_OPTIONS="history_size=2 suppressions=tsan.suppress exit_code=0 set die_after_fork=0 atexit_sleep_ms=0 flush_memory_ms=1000 force_seq_cst_atomics=1"

AUTOMAKE_OPTIONS = 1.4 foreign

CLEANFILES = *.gcov .libs/*.gcda .libs/*.gcno *.gcno *.gcda
DISTCLEANFILES =
EXTRA_DIST = \
	abicheck.sh \
	bson-tests.h \
	binary/binary_deprecated.bson \
	binary/codewscope.bson \
	binary/dollarquery.bson \
	binary/dotkey.bson \
	binary/dotquery.bson \
	binary/eurokey.bson \
	binary/overflow1.bson \
	binary/overflow2.bson \
	binary/overflow3.bson \
	binary/overflow4.bson \
	binary/readergrow.bson \
	binary/stackoverflow.bson \
	binary/stream.bson \
	binary/stream_corrupt.bson \
	binary/test10.bson \
	binary/test11.bson \
	binary/test12.bson \
	binary/test13.bson \
	binary/test14.bson \
	binary/test15.bson \
	binary/test16.bson \
	binary/test17.bson \
	binary/test18.bson \
	binary/test19.bson \
	binary/test1.bson \
	binary/test20.bson \
	binary/test21.bson \
	binary/test22.bson \
	binary/test23.bson \
	binary/test24.bson \
	binary/test25.bson \
	binary/test26.bson \
	binary/test27.bson \
	binary/test28.bson \
	binary/test29.bson \
	binary/test2.bson \
	binary/test30.bson \
	binary/test31.bson \
	binary/test32.bson \
	binary/test33.bson \
	binary/test34.bson \
	binary/test35.bson \
	binary/test36.bson \
	binary/test37.bson \
	binary/test38.bson \
	binary/test39.bson \
	binary/test3.bson \
	binary/test4.bson \
	binary/test40.bson \
	binary/test41.bson \
	binary/test42.bson \
	binary/test43.bson \
	binary/test44.bson \
	binary/test45.bson \
	binary/test46.bson \
	binary/test47.bson \
	binary/test48.bson \
	binary/test49.bson \
	binary/test5.bson \
	binary/test50.bson \
	binary/test51.bson \
	binary/test52.bson \
	binary/test53.bson \
	binary/test54.bson \
	binary/test55.bson \
	binary/test56.bson \
	binary/test57.bson \
	binary/test6.bson \
	binary/test7.bson \
	binary/test8.bson \
	binary/test9.bson \
	binary/trailingnull.bson \
	json/test.json

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio

RPMIO_LDADD = \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

#RPMDB_LDADD = \
#	$(top_builddir)/rpmdb/librpmdb.la \
#	$(RPMIO_LDADD)

#RPM_LDADD = \
#	$(top_builddir)/build/librpmbuild.la \
#	$(top_builddir)/lib/librpm.la \
#	$(RPMDB_LDADD)

vg =		$(LIBTOOL) --mode=execute vg 
VALGRIND =

noinst_PROGRAMS = test-libbson

test_libbson_SOURCES = \
	TestSuite.c \
	TestSuite.h \
	test-libbson.c \
	test-atomic.c \
	test-bson.c \
	test-endian.c \
	test-clock.c \
	test-error.c \
	test-iso8601.c \
	test-iter.c \
	test-json.c \
	test-oid.c \
	test-reader.c \
	test-string.c \
	test-utf8.c \
	test-value.c \
	test-version.c \
	test-writer.c \
	test-bcon-basic.c \
	test-bcon-extract.c

test_libbson_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DBSON_COMPILATION \
	-DBINARY_DIR="\"$(srcdir)/binary\""

test_libbson_LDADD = $(RPMIO_LDADD) # libbson-1.0.la

#PYTEST_DEPS = cbson.la
PYTEST_DEPS =

TEST_PROGS = test-libbson

check: test

test: $(TEST_PROGS) $(PYTEST_DEPS)
	@for TEST_PROG in $(TEST_PROGS) ; do \
	  $(LIBTOOL) --mode=execute $$VALGRIND ./$$TEST_PROG -F test.log; \
	done
#	@LD_LIBRARY_PATH=.libs DYLD_LIBRARY_PATH=.libs PYTHONPATH=.libs ./test_cbson.py
#	@$(srcdir)/abicheck.sh "$(srcdir)/src/libbson.symbols"

DISTCLEANFILES += test.log
