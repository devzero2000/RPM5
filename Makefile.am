# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign
ACLOCAL_AMFLAGS = -I m4

LINT = splint
MCCABE = pmccabe

EXTRA_DIST = CHANGES CREDITS Doxyheader INSTALL INSTALL.developer cpuinfo.yaml \
	autodeps autogen.sh \
	syck js \
	lua \
	ruby \
	macros \
	perl \
	pubkeys/JBJ-GPG-KEY \
	rpm.magic rpmqv.c \
	python \
	tests/arbitrarytag-1.0-1.src.rpm \
	tests/cudf-test-1-0.src.rpm \
	tests/devtool-sanity-1.0-1.src.rpm \
	tests/edos-test-1-0.src.rpm \
	tests/probes-test-1-0.src.rpm \
	tests/triggers-D-1.0-1.src.rpm \
	tests/triggers-DP-1.0-1.src.rpm \
	tests/triggers-F-1.0-1.src.rpm \
	tests/triggers-FP-1.0-1.src.rpm \
	tests/triggers-N-1.0-1.src.rpm \
	tests/triggers-NA-1.0-1.src.rpm \
	tests/triggers-P-1.0-1.src.rpm

SUBDIRS =
if USE_NLS
SUBDIRS += \
	po
endif
SUBDIRS += \
	@WITH_DB_SUBDIR@ \
	@WITH_ZLIB_SUBDIR@ \
	@WITH_PCRE_SUBDIR@ \
	@WITH_POPT_SUBDIR@ \
	@WITH_LUA_SUBDIR@ \
	@WITH_BEECRYPT_SUBDIR@ \
	@WITH_NEON_SUBDIR@ \
	@WITH_SPIDERMONKEY_SUBDIR@ \
	@WITH_FILE_SUBDIR@ \
	@WITH_BASH_SUBDIR@ \
	@WITH_RC_SUBDIR@ \
	@WITH_SYCK_SUBDIR@ \
	@WITH_XAR_SUBDIR@ \
	@WITH_XZ_SUBDIR@ \
	misc \
	rpmio \
	rpmdb \
	lib \
	build \
	rpmconstant \
	@WITH_JS_SUBDIR@ \
	@WITH_RUBY_SUBDIR@ \
	@WITH_PYTHON_SUBDIR@ \
	@WITH_PERL_SUBDIR@ \
	tools \
	scripts \
	doc \
	. \
	tests

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc \
	@WITH_DB_CPPFLAGS@ \
	@WITH_ZLIB_CPPFLAGS@ \
	@WITH_LUA_CPPFLAGS@ \
	@WITH_FILE_CPPFLAGS@ \
	@WITH_PCRE_CPPFLAGS@ \
	@WITH_POPT_CPPFLAGS@ \
	@WITH_XAR_CPPFLAGS@ \
	@WITH_XZ_CPPFLAGS@

AM_CFLAGS = $(OPENMP_CFLAGS)

myLDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

bin_PROGRAMS = 		rpm rpmbuild

install-exec-hook:
if WITH_PATH_VERSIONED
	-for p in $(bin_PROGRAMS); do \
	    mv $(DESTDIR)$(bindir)/$${p}$(EXEEXT) \
	       $(DESTDIR)$(bindir)/$${p}-$(VERSION)$(EXEEXT); \
	done
endif

pkglibdir =		@USRLIBRPM@
pkglib_DATA = rpmpopt macros/macros macros/macros.rpmbuild cpuinfo.yaml

pkgbindir =	$(pkglibdir)/bin
pkgbin_SCRIPTS = install-sh mkinstalldirs

pkgcfgdir =	$(pkglibdir)/macros.d
pkgcfg_DATA = \
	macros/cmake macros/java macros/libtool macros/mandriva macros/mono \
	macros/perl macros/pkgconfig macros/php macros/python macros/ruby \
	macros/selinux macros/tcl

noinst_HEADERS = build.h debug.h system.h

rpm_SOURCES =		$(top_srcdir)/build.c
rpm_LDFLAGS =		@LDFLAGS_STATIC@ $(LDFLAGS)
rpm_LDADD =		rpm.o $(myLDADD)
rpm.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMBT -DIAM_RPMDB -DIAM_RPMEIU -DIAM_RPMK -DIAM_RPMQV -o $@ -c $(top_srcdir)/rpmqv.c

rpmbuild_SOURCES =	$(top_srcdir)/build.c
rpmbuild_LDFLAGS =	@LDFLAGS_STATIC@ $(LDFLAGS)
rpmbuild_LDADD =	rpmbuild.o $(myLDADD)
rpmbuild.o: $(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMBT -o $@ -c $(top_srcdir)/rpmqv.c

.PHONY:	splint
splint:
	splint \
	    -load build/rpmbuild.lcd \
	    -load lib/rpmlib.lcd \
	    -load rpmdb/rpmdb.lcd \
	    -load rpmio/rpmio.lcd \
	    -load popt/popt.lcd \
		$(DEFS) $(INCLUDES) rpmqv.c $(rpmbuild_SOURCES)

.PHONY:	lint
lint:
	$(LINT) -Dlint $(DEFS) $(INCLUDES) rpmqv.c $(rpmbuild_SOURCES) \
		`make -s sources -C build` \
		`make -s sources -C lib` \
		`make -s sources -C rpmdb` \
		`make -s sources -C rpmio` \
		`make -s sources -C file/src`

.PHONY:	mccabe
mccabe:
	@$(MCCABE) rpmqv.c $(rpmbuild_SOURCES) \
		`make -s sources -C build` \
		`make -s sources -C lib` \
		`make -s sources -C rpmdb` \
		`make -s sources -C rpmio` | sort -nr | head -n 30

.PHONY: updatepo
updatepo:
	rsync -Lrtvz  translationproject.org::tp/latest/rpm/  po

CVSTAG = r$(subst .,-,$(VERSION))

pkgsrcdir = @PKGSRCDIR@

DBPATH = @DBPATH@

install-data-local:
	@$(MKDIR_P) $(DESTDIR)$(DBPATH)
	@$(MKDIR_P) $(DESTDIR)$(varprefix)/spool/repackage
	@for dir in BUILD RPMS SOURCES SPECS SRPMS; do\
	    $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/$$dir;\
	done
	@case "@host_cpu@" in \
	*86)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i386 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i486 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i586 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i686 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/athlon ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/pentium3 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/pentium4 ;;\
	armv3l)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;;\
	armv4b)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4b ;;\
	armv4l)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4l ;;\
	armv5teb) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4b ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5teb ;;\
	armv5tel) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4l ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5tel ;;\
	alpha*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/alpha ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/alphaev6 ;;\
	sparc*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv8 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv9 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc64 ;;\
	ia64*)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ia64 ;;\
	s390*)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/s390 ;;\
	mipsel*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/mipsel ;;\
	mips*)  $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/mips ;;\
	powerpc*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppciseries ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppcpseries ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64iseries ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64pseries ;;\
	*)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/@host_cpu@ ;;\
	esac
	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/noarch

check-local:

.PHONY: tar
tar:
	rm -rf /tmp/rpm-$(VERSION)
	$(MAKE) DESTDIR=/tmp/rpm-$(VERSION) install
	cd /tmp/rpm-$(VERSION) ; tar cvf /tmp/rpm-$(VERSION).tar .

.PHONY: noconfig
noconfig:
	find . -name "Makefile" -exec rm {} \; 
	rm -f *gz *rpm config.*

.PHONY: archive
archive: 
	@cvs -d `cat CVS/Root` diff 2>&1 > /dev/null || { \
	    echo " " ; \
	    echo "==> Please resolve differences between the repository and" ; \
	    echo "==> your rpm check out before tagging." ; \
	    echo " " ; \
	    cvs -n up ; \
	    echo " " ; \
	    exit 1 ; \
	}
	@cvs -d `cat CVS/Root` -Q tag -F $(CVSTAG) .
	@make dist
	@echo " "
	@echo "The final archive is ./rpm-$(VERSION).tar.gz."

.PHONY:	nextsrpm
nextsrpm: all archive
	@sudo ./rpm -ta rpm-$(VERSION).tar.gz

.PHONY:	doxygen
doxygen @WITH_APIDOCS_TARGET@: Doxyfile rpmpopt macros/macros
	rm -rf $@
	mkdir -p $@
	- [ X"@__DOXYGEN@" != Xno ] && @__DOXYGEN@

.PHONY:	lcov-reset	# run lcov from scratch, always
lcov-reset:
	make lcov-run
	make lcov-report

# run lcov from scratch if the dir is not there
lcov:
	make lcov-reset

.PHONY:	lcov-run	# reset run coverage tests
lcov-run:
	@-rm -rf lcov
	find . -name "*.gcda" -exec rm {} \;
	make check

.PHONY:	lcov-report	# generate report based on current coverage data
lcov-report:
	mkdir lcov
	-lcov -d rpmio -d rpmdb -d lib -d build -d tools -d . --capture --output-file lcov/lcov.info
	lcov -l lcov/lcov.info | grep -v "`cd $(top_srcdir) && pwd`" | cut -d: -f1 > lcov/remove
	lcov -r lcov/lcov.info `cat lcov/remove` > lcov/lcov.cleaned.info
	rm lcov/remove
	mv lcov/lcov.cleaned.info lcov/lcov.info
	genhtml -t "$(PACKAGE_STRING)" -o lcov lcov/lcov.info

.PHONY:	lcov-upload
lcov-upload: lcov
	mkdir -p /var/ftp/pub/jbj/rpm-$(VERSION)-lcov
	tar -C lcov -cf - . | (cd /var/ftp/pub/jbj/rpm-$(VERSION)-lcov; tar xf -)
#	rsync -rvz -e ssh --delete lcov/* ???

cscope:
	cscope -b -R

cref: ctags cscope
