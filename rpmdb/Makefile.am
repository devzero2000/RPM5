# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I. \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST = db3.c db1.c falloc.c

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = db.h rpmdb.h
noinst_HEADERS = falloc.h fprint.h rpmhash.h

mylibpaths = \
	-L$(top_builddir)/lib/.libs \
	-L$(top_builddir)/rpmio/.libs \
	-L$(top_builddir)/popt/.libs

mylibs = -lrpm -lrpmio -lpopt @LIBS@ @INTLLIBS@ @LIBMISC@
LIBS =

DB3LOBJS = $(shell cat $(top_builddir)/$(WITH_DB_SUBDIR)/db3lobjs)

lib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = \
	dbconfig.c fprint.c merge.c poptDB.c rpmhash.c rpmdb.c
librpmdb_la_LDFLAGS = -release @VERSION@ @libdb3@
librpmdb_la_LIBADD = $(DBLIBOBJS) $(DB3LOBJS)
librpmdb_la_DEPENDENCIES = $(DBLIBOBJS) .created

# XXX make sure that db.h symlink is generated
dbconfig.c: db.h

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lrpmio|" < librpmdb.la > .librpmdb.la && \
	mv .librpmdb.la librpmdb.la

falloc.lo: falloc.c $(top_srcdir)/system.h $(top_srcdir)/rpmio/rpmio.h falloc.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

db.h:
	@ln -sf ../$(WITH_DB_SUBDIR)/db.h $@

.created: db.h
	for lo in $(DB3LOBJS); do \
	  [ -f $$lo ] || $(LN_S) $(top_builddir)/$(WITH_DB_SUBDIR)/$$lo $$lo ; \
	done
	touch $@

clean-local:
	rm -f $(DB3LOBJS) *.o .created db.h

.PHONY:	sources
sources:
	@echo $(librpmdb_la_SOURCES:%=rpmdb/%)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmdb_la_SOURCES)

tdbi: librpmdb.la tdbi.o
	$(LINK) -all-static $@.o $< $(mylibpaths) $(mylibs) $(LIBS)

tfalloc: librpmdb.la tfalloc.o
	$(LINK) -all-static $@.o $< $(mylibpaths) $(mylibs) $(LIBS)
