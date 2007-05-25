# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_MAGIC_INCLUDE@ \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc \
	@INCPATH@

EXTRA_DIST = getdate.y librpm.vers

EXTRA_PROGRAMS =

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	fs.h misc.h rpmcli.h rpmlib.h \
	rpmal.h rpmdpkg.h rpmds.h rpmevr.h rpmfc.h rpmfi.h rpmgi.h \
	rpmns.h rpmps.h rpmsx.h rpmte.h rpmts.h stringbuf.h
noinst_HEADERS = \
	cpio.h fsm.h manifest.h psm.h rpmlead.h rpmlock.h signature.h tar.h

mylibs = librpm.la
LIBS =

LDFLAGS = -L$(RPM_BUILD_ROOT)$(usrlibdir) -L$(DESTDIR)$(usrlibdir)

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c depends.c formats.c fs.c fsm.c getdate.c \
	manifest.c misc.c package.c \
	poptALL.c poptI.c poptQV.c psm.c query.c \
	rpmal.c rpmchecksig.c rpmdpkg.c rpmds.c rpmevr.c rpmfc.c \
	rpmfi.c rpmgi.c rpminstall.c rpmrollback.c \
	rpmlead.c rpmlock.c rpmns.c rpmps.c rpmrc.c rpmsx.c rpmte.c rpmts.c \
	signature.c stringbuf.c transaction.c \
	verify.c tar.c
librpm_la_LDFLAGS = -no-undefined -release $(LT_CURRENT).$(LT_REVISION) $(LDFLAGS) \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_SELINUX_LIB@ \
	@LTLIBINTL@

if HAVE_LD_VERSION_SCRIPT
librpm_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpm.vers
endif

getdate.c: getdate.y
	@echo expect 10 shift/reduce conflicts
	$(YACC) $(srcdir)/getdate.y
	-@if test -f y.tab.c; then \
	 { echo "/*@-globstate -statictrans -unqualifiedtrans -noparams @*/";\
	   echo "/*@-retvalint -usedef -varuse -nullderef -nullassign @*/";\
	   echo "/*@-readonlytrans -modunconnomods -compdef -noeffectuncon @*/";\
	   echo "/*@-globs -evalorderuncon -modobserveruncon -modnomods @*/";\
	   echo "/*@-noeffect -branchstate -sizeoftype -usereleased @*/";\
	   echo "/*@unused@*/";\
	   sed	-e 's,y.tab.c,getdate.c,' y.tab.c \
		-e 's,^YYSTYPE ,static &,' \
		-e 's,^short ,static &,' \
		-e 's,^const short ,static &,' \
		-e 's,^int yydebug,/*@unused@*/ static &,' \
		-e 's,^int ,static &,' ;\
	   echo "/*@=noeffect =branchstate =sizeoftype =usereleased @*/";\
	   echo "/*@=globs =evalorderuncon =modobserveruncon =modnomods @*/";\
	   echo "/*@=readonlytrans =modunconnomods =compdef =noeffectuncon @*/";\
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

BUILT_SOURCES = getdate.c # rpmlib.lcd

rpmlib.lcd: Makefile.am ${librpm_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint ${DEFS} ${INCLUDES} ${librpm_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpm_la_SOURCES:%=lib/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpm_la_SOURCES)
