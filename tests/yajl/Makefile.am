AUTOMAKE_OPTIONS = 1.4 foreign

CLEANFILES = *.gcov .libs/*.gcda .libs/*.gcno *.gcno *.gcda
DISTCLEANFILES =
EXTRA_DIST = \
	parse_tests.sh
	cases/*

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rpmio

RPMIO_LDADD = \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

vg =		$(LIBTOOL) --mode=execute vg
VALGRIND =

noinst_PROGRAMS = yajl_test

yajl_test_SOURCES = \
	yajl_test.c

yajl_test_CPPFLAGS = \
	$(AM_CPPFLAGS)

yajl_test_LDADD = $(RPMIO_LDADD)

TEST_PROGS = yajl_test

check:	test

test: $(TEST_PROGS)
	@sh parse_tests.sh
