# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc \
	@WITH_DB_CPPFLAGS@ \
	@WITH_ZLIB_CPPFLAGS@ \
	@WITH_LUA_CPPFLAGS@ \
	@WITH_FILE_CPPFLAGS@ \
	@WITH_XAR_CPPFLAGS@

RPM_LDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

EXTRA_DIST = genpgp.sh getdate.y librpm.vers tpgp.c

EXTRA_PROGRAMS = tgi tpgp tsbt

pkgincdir = $(pkgincludedir)$(WITH_PATH_VERSIONED_SUFFIX)
pkginc_HEADERS = \
	fs.h misc.h rpmcli.h rpmlib.h rpmversion.h \
	rpmal.h rpmdpkg.h rpmds.h rpmevr.h rpmfc.h rpmfi.h rpmgi.h \
	rpmns.h rpmps.h rpmsx.h rpmte.h rpmts.h
noinst_HEADERS = \
	cpio.h fsm.h manifest.h psm.h rpmlock.h tar.h rpmluaext.h

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c depends.c formats.c fs.c fsm.c getdate.c \
	manifest.c misc.c package.c \
	poptALL.c poptI.c poptQV.c psm.c query.c \
	rpmal.c rpmchecksig.c rpmdpkg.c rpmds.c rpmevr.c rpmfc.c \
	rpmfi.c rpmgi.c rpminstall.c rpmrollback.c rpmversion.c \
	rpmlock.c rpmns.c rpmps.c rpmrc.c rpmsx.c rpmte.c rpmts.c \
	transaction.c verify.c tar.c rpmluaext.c
librpm_la_LDFLAGS = -release $(LT_CURRENT).$(LT_REVISION)
if HAVE_LD_VERSION_SCRIPT
librpm_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpm.vers
endif
librpm_la_LIBADD =
if ENABLE_BUILD_INTLIBDEP
librpm_la_LIBADD += \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la
endif
if ENABLE_BUILD_MAXEXTLIBDEP
librpm_la_LDFLAGS += $(LDFLAGS)
librpm_la_LIBADD  += $(LIBS)
endif
librpm.la: $(librpm_la_OBJECTS) $(librpm_la_DEPENDENCIES) 
	$(librpm_la_LINK) -rpath $(usrlibdir) $(librpm_la_OBJECTS) $(librpm_la_LIBADD)

install-data-hook:
if !ENABLE_BUILD_LAFILES
	-for l in $(usrlib_LTLIBRARIES); do \
	    rm -f $(DESTDIR)$(usrlibdir)/$${l}; \
	done
endif
if WITH_PATH_VERSIONED
	-for l in $(usrlib_LTLIBRARIES); do \
	    base=`echo "$${l}" | sed -e 's;\.la$$;;'`; \
	    if [ -f $(DESTDIR)$(usrlibdir)/$${base}.a ]; then \
	        mv $(DESTDIR)$(usrlibdir)/$${base}.a \
	           $(DESTDIR)$(usrlibdir)/$${base}-$(VERSION).a; \
	    fi; \
	done
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

tgi_SOURCES = tgi.c
tgi_LDADD = $(RPM_LDADD)

tpgp_SOURCES = genpgp.h tpgp.c
tpgp_LDADD = $(RPM_LDADD)

tpgp.c: genpgp.h

genpgp.h: genpgp.sh
	-sh ./genpgp.sh > genpgp.h

tsbt_SOURCES = tsbt.c
tsbt_LDADD = $(RPM_LDADD)
