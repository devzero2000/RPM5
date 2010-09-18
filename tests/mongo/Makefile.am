## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST =

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

mongoserver =		198.178.231.189		# XXX mongo.rpm5.org

all_types_SOURCES =	all_types.c
all_types_LDADD =	$(RPMIO_LDADD)

auth_SOURCES =		auth.c
auth_LDADD =		$(RPMIO_LDADD)

benchmark_SOURCES =	benchmark.c
benchmark_LDADD =	$(RPMIO_LDADD)

count_delete_SOURCES =	count_delete.c
count_delete_LDADD =	$(RPMIO_LDADD)

endian_swap_SOURCES =	endian_swap.c
endian_swap_LDADD =	$(RPMIO_LDADD)

errors_SOURCES =	errors.c
errors_LDADD =		$(RPMIO_LDADD)

json_SOURCES =		json.c
json_LDADD =		$(RPMIO_LDADD)

pair_SOURCES =		pair.c
pair_LDADD =		$(RPMIO_LDADD)

resize_SOURCES =	resize.c
resize_LDADD =		$(RPMIO_LDADD)

simple_SOURCES =	simple.c
simple_LDADD =		$(RPMIO_LDADD)

sizes_SOURCES =		sizes.c
sizes_LDADD =		$(RPMIO_LDADD)

update_SOURCES =	update.c
update_LDADD =		$(RPMIO_LDADD)

check-local:
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

