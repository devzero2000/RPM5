# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

INCLUDES = -I. \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST = getdate.y

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	misc.h rpmcli.h rpmlib.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h fsm.h \
	manifest.h psm.h \
	rpmlead.h signature.h

mylibs = librpm.la
LIBS =

LDFLAGS = -L$(RPM_BUILD_ROOT)$(usrlibdir) -L$(DESTDIR)$(usrlibdir)

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c depends.c formats.c fs.c fsm.c getdate.c \
	manifest.c misc.c package.c \
	problems.c poptI.c poptK.c poptQV.c psm.c query.c \
	rpmchecksig.c rpminstall.c rpmlead.c rpmlibprov.c rpmrc.c rpmvercmp.c \
	signature.c stringbuf.c transaction.c \
	verify.c
librpm_la_LDFLAGS = -release 4.0.4 $(LDFLAGS) \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la

getdate.c: getdate.y
	@echo expect 10 shift/reduce conflicts
	$(YACC) $(srcdir)/getdate.y
	-@if test -f y.tab.c; then \
	 { echo "/*@-globstate -statictrans -unqualifiedtrans -noparams @*/";\
	   echo "/*@-retvalint -usedef -varuse -nullderef -nullassign @*/";\
	   echo "/*@-readonlytrans -modunconnomods -compdef @*/";\
	   sed	-e 's,y.tab.c,getdate.c,' y.tab.c \
		-e 's,^YYSTYPE ,static &,' \
		-e 's,^short ,static &,' \
		-e 's,^const short ,static &,' \
		-e 's,^int yydebug,/*@unused@*/ static &,' \
		-e 's,^int ,static &,' ;\
	   echo "/*@=readonlytrans =modunconnomods =compdef @*/";\
	   echo "/*@=retvalint =usedef =varuse =nullderef =nullassign @*/";\
	   echo "/*@=globstate =statictrans =unqualifiedtrans =noparams @*/";\
	 } > getdate.c ;\
	   rm -f y.tab.c; \
	else \
	   if test -f getdate.tab.c ; then \
	       mv getdate.tab.c getdate.c ; \
	   else \
	       echo '*** Unable to create getdate.c' ;\
	   fi ;\
	fi

BUILT_SOURCES = getdate.c

.PHONY:	sources
sources:
	@echo $(librpm_la_SOURCES:%=lib/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpm_la_SOURCES)

th: th.c librpm.la
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibs)

trb: trb.o librpm.la
	$(LINK) @LDFLAGS_STATIC@ $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ trb.o $(mylibs)

trhn: trhn.o librpm.la
	$(LINK) @LDFLAGS_STATIC@ $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ trhn.o $(mylibs)
