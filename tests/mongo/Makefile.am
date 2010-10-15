## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST =	tutorial.js

EXTRA_PROGRAMS = \
	endian_swap \
	json

noinst_PROGRAMS = \
	all_types \
	auth \
	benchmark \
	count_delete \
	errors \
	pair \
	resize \
	simple \
	sizes \
	update

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

mongo =			@__MONGO@
mongoserver =		198.178.231.189		# XXX mongo.rpm5.org

all_types_SOURCES =	test.h all_types.c
all_types_LDADD =	$(RPMIO_LDADD)

auth_SOURCES =		test.h auth.c
auth_LDADD =		$(RPMIO_LDADD)

benchmark_SOURCES =	test.h benchmark.c
benchmark_LDADD =	$(RPMIO_LDADD)

count_delete_SOURCES =	test.h count_delete.c
count_delete_LDADD =	$(RPMIO_LDADD)

endian_swap_SOURCES =	test.h endian_swap.c
endian_swap_LDADD =	$(RPMIO_LDADD)

errors_SOURCES =	test.h errors.c
errors_LDADD =		$(RPMIO_LDADD)

json_SOURCES =		test.h json.c
json_LDADD =		$(RPMIO_LDADD)

pair_SOURCES =		test.h pair.c
pair_LDADD =		$(RPMIO_LDADD)

resize_SOURCES =	test.h resize.c
resize_LDADD =		$(RPMIO_LDADD)

simple_SOURCES =	test.h simple.c
simple_LDADD =		$(RPMIO_LDADD)

sizes_SOURCES =		test.h sizes.c
sizes_LDADD =		$(RPMIO_LDADD)

update_SOURCES =	test.h update.c
update_LDADD =		$(RPMIO_LDADD)

check-shell:
	@echo "=== mongo-shell ==="
	@-${mongo} ${mongoserver} < tutorial.js

check-local:	check-shell
	@echo "=== mongo-c-driver ==="
	@echo "--> all_types"
	@-./all_types
	@echo "--> auth"
	@-./auth $(mongoserver)
#	@echo "--> benchmark"
#	@-./benchmark $(mongoserver)
	@echo "--> count_delete"
	@-./count_delete $(mongoserver)
#	@echo "--> endian_swap"
#	@-./endian_swap
	@echo "--> errors"
	@-./errors $(mongoserver)
#	@echo "--> json"
#	@-./json
	@echo "--> pair"
	@-./pair $(mongoserver)
	@echo "--> resize"
	@-./resize
	@echo "--> simple"
	@-./simple $(mongoserver)
	@echo "--> sizes"
	@-./sizes
	@echo "--> update"
	@-./update $(mongoserver)

