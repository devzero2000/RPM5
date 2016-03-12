## Process this file with automake to produce Makefile.in.
# export TSAN_OPTIONS="history_size=2 suppressions=tsan.suppress exit_code=0 set die_after_fork=0 atexit_sleep_ms=0 flush_memory_ms=1000 force_seq_cst_atomics=1"

AUTOMAKE_OPTIONS = 1.4 foreign

CLEANFILES = *.gcov .libs/*.gcda .libs/*.gcno *.gcno *.gcda
DISTCLEANFILES =
EXTRA_DIST = \
	tests/abicheck.sh \
	tests/bson-tests.h \
	tests/binary/binary_deprecated.bson \
	tests/binary/codewscope.bson \
	tests/binary/dollarquery.bson \
	tests/binary/dotkey.bson \
	tests/binary/dotquery.bson \
	tests/binary/eurokey.bson \
	tests/binary/overflow1.bson \
	tests/binary/overflow2.bson \
	tests/binary/overflow3.bson \
	tests/binary/overflow4.bson \
	tests/binary/readergrow.bson \
	tests/binary/stackoverflow.bson \
	tests/binary/stream.bson \
	tests/binary/stream_corrupt.bson \
	tests/binary/test10.bson \
	tests/binary/test11.bson \
	tests/binary/test12.bson \
	tests/binary/test13.bson \
	tests/binary/test14.bson \
	tests/binary/test15.bson \
	tests/binary/test16.bson \
	tests/binary/test17.bson \
	tests/binary/test18.bson \
	tests/binary/test19.bson \
	tests/binary/test1.bson \
	tests/binary/test20.bson \
	tests/binary/test21.bson \
	tests/binary/test22.bson \
	tests/binary/test23.bson \
	tests/binary/test24.bson \
	tests/binary/test25.bson \
	tests/binary/test26.bson \
	tests/binary/test27.bson \
	tests/binary/test28.bson \
	tests/binary/test29.bson \
	tests/binary/test2.bson \
	tests/binary/test30.bson \
	tests/binary/test31.bson \
	tests/binary/test32.bson \
	tests/binary/test33.bson \
	tests/binary/test34.bson \
	tests/binary/test35.bson \
	tests/binary/test36.bson \
	tests/binary/test37.bson \
	tests/binary/test38.bson \
	tests/binary/test39.bson \
	tests/binary/test3.bson \
	tests/binary/test4.bson \
	tests/binary/test40.bson \
	tests/binary/test41.bson \
	tests/binary/test42.bson \
	tests/binary/test43.bson \
	tests/binary/test44.bson \
	tests/binary/test45.bson \
	tests/binary/test46.bson \
	tests/binary/test47.bson \
	tests/binary/test48.bson \
	tests/binary/test49.bson \
	tests/binary/test5.bson \
	tests/binary/test50.bson \
	tests/binary/test51.bson \
	tests/binary/test52.bson \
	tests/binary/test53.bson \
	tests/binary/test54.bson \
	tests/binary/test55.bson \
	tests/binary/test56.bson \
	tests/binary/test57.bson \
	tests/binary/test6.bson \
	tests/binary/test7.bson \
	tests/binary/test8.bson \
	tests/binary/test9.bson \
	tests/binary/trailingnull.bson \
	tests/json/test.json

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
