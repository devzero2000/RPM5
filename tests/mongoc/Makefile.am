## Process this file with automake to produce Makefile.in.
# export TSAN_OPTIONS="history_size=2 suppressions=tsan.suppress exit_code=0 set die_after_fork=0 atexit_sleep_ms=0 flush_memory_ms=1000 force_seq_cst_atomics=1"

AUTOMAKE_OPTIONS = 1.4 foreign

DISTCLEANFILES =
EXTRA_DIST = \
	abicheck.sh \
	binary/delete1.dat \
	binary/get_more1.dat \
	binary/gridfs.dat \
	binary/insert1.dat \
	binary/kill_cursors1.dat \
	binary/msg1.dat \
	binary/query1.dat \
	binary/query2.dat \
	binary/reply1.dat \
	binary/reply2.dat \
	binary/update1.dat \
	certificates/ca.pem \
	certificates/client.pem \
	mock-server.c \
	mock-server.h \
	mongoc-tests.c \
	mongoc-tests.h \
	trust_dir.cnf \
	make_ca.pl \
	c_rehash

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

noinst_PROGRAMS = \
	echo-server \
	test-libmongoc \
	test-load \
	test-replica-set \
	test-replica-set-ssl \
	test-secondary \
	test-sharded-cluster


TEST_PROGS = test-libmongoc


TEST_CFLAGS = \
	$(AM_CPPFLAGS) \
	-DMONGOC_COMPILATION \
	-DBINARY_DIR="\"$(srcdir)/binary\""

TEST_LIBS = $(RPMIO_LDADD)	# libmongoc-priv.la $(BSON_LIBS) $(SHM_LIB)
#if OS_WIN32
#TEST_LIBS += -lshlwapi
#endif

test_load_SOURCES = \
	test-load.c \
	mongoc-tests.c
test_load_CFLAGS = $(TEST_CFLAGS)
test_load_LDADD = $(TEST_LIBS)


echo_server_SOURCES = echo-server.c
echo_server_CFLAGS = $(TEST_CFLAGS)
echo_server_LDADD = $(TEST_LIBS)


test_secondary_SOURCES = \
	test-secondary.c \
	mongoc-tests.c
test_secondary_CFLAGS = $(TEST_CFLAGS)
test_secondary_LDADD = $(TEST_LIBS)

test_replica_set_SOURCES = \
	test-replica-set.c \
	ha-test.c \
	ha-test.h \
	mongoc-tests.c
test_replica_set_CFLAGS = $(TEST_CFLAGS)
test_replica_set_LDADD = $(TEST_LIBS)


test_replica_set_ssl_SOURCES = \
	test-replica-set-ssl.c \
	ha-test.c \
	ha-test.h \
	mongoc-tests.c
test_replica_set_ssl_CFLAGS = $(TEST_CFLAGS)
test_replica_set_ssl_LDADD = $(TEST_LIBS)


test_libmongoc_SOURCES = \
	mock-server.c \
	mock-server.h \
	test-libmongoc.c \
	test-bulk.c \
	test-mongoc-array.c \
	test-mongoc-buffer.c \
	test-mongoc-client.c \
	test-mongoc-client-pool.c \
	test-mongoc-collection.c \
	test-mongoc-cursor.c \
	test-mongoc-database.c \
	test-mongoc-gridfs.c \
	test-mongoc-gridfs-file-page.c \
	test-mongoc-list.c \
	test-mongoc-matcher.c \
	test-mongoc-queue.c \
	test-mongoc-read-prefs.c \
	test-mongoc-rpc.c \
	test-mongoc-stream.c \
	test-mongoc-uri.c \
	test-mongoc-write-concern.c \
	test-libmongoc.h \
	test-write-commands.c \
	TestSuite.c \
	TestSuite.h
test_libmongoc_SOURCES += \
	test-x509.c \
	test-mongoc-stream-tls.c \
	ssl-test.c \
	ssl-test.h
test_libmongoc_CFLAGS = $(TEST_CFLAGS)
test_libmongoc_LDADD = $(TEST_LIBS)


test_sharded_cluster_SOURCES = \
	test-sharded-cluster.c \
	ha-test.c \
	ha-test.h \
	mongoc-tests.c
test_sharded_cluster_CFLAGS = $(TEST_CFLAGS)
test_sharded_cluster_LDADD = $(TEST_LIBS)

test_certs: trust_dir/done

trust_dir/done: $(srcdir)/make_ca.pl $(srcdir)/trust_dir.cnf
	$(srcdir)/make_ca.pl $(builddir)/trust_dir $(srcdir)/trust_dir.cnf
	touch $(builddir)/trust_dir/done

check: test

TEST_ARGS = -f -p

test: $(TEST_PROGS) test_certs
	@ for TEST_PROG in $(TEST_PROGS) ; do \
		./$$TEST_PROG $(TEST_ARGS) -F test.log; \
	done

DISTCLEANFILES += \
	test.log \
	trust_dir/ca.db.serial \
	trust_dir/done \
	trust_dir/keys/rev.mongodb.com.pem \
	trust_dir/keys/mongodb.com.pem \
	trust_dir/keys/pass.mongodb.com.pem \
	trust_dir/keys/127.0.0.1.pem \
	trust_dir/keys/alt.mongodb.com.pem \
	trust_dir/ca.db.certs/01.pem \
	trust_dir/ca.db.certs/04.pem \
	trust_dir/ca.db.certs/05.pem \
	trust_dir/ca.db.certs/02.pem \
	trust_dir/ca.db.certs/03.pem \
	trust_dir/ca.db.index.attr \
	trust_dir/ca.db.index \
	trust_dir/build/mongodb.com.crt \
	trust_dir/build/127.0.0.1.req \
	trust_dir/build/mongodb.com.key \
	trust_dir/build/rev.mongodb.com.req \
	trust_dir/build/alt.mongodb.com.req \
	trust_dir/build/pass.mongodb.com.key \
	trust_dir/build/rev.mongodb.com.key \
	trust_dir/build/127.0.0.1.crt \
	trust_dir/build/pass.mongodb.com.req \
	trust_dir/build/rev.mongodb.com.crt \
	trust_dir/build/pass.mongodb.com.crt \
	trust_dir/build/mongodb.com.req \
	trust_dir/build/127.0.0.1.key \
	trust_dir/build/alt.mongodb.com.crt \
	trust_dir/build/alt.mongodb.com.key \
	trust_dir/verify/mongo_root.pem \
	trust_dir/verify/rev.mongodb.com.pem \
	trust_dir/verify/mongodb.com.pem \
	trust_dir/verify/pass.mongodb.com.pem \
	trust_dir/verify/127.0.0.1.pem \
	trust_dir/verify/alt.mongodb.com.pem \
	trust_dir/ca.db.index.old \
	trust_dir/ca.db.rand \
	trust_dir/signing-ca.key \
	trust_dir/signing-ca.crt \
	trust_dir/ca.db.index.attr.old \
	trust_dir/ca.db.serial.old \
	trust_dir/crl/root.crl.pem

.PHONY: test_certs

