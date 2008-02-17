# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = CHANGES CREDITS Doxyheader INSTALL \
	autodeps/none autodeps/*.prov autodeps/*.req autogen.sh \
	db db3/configure xar \
	lua/[A-Z]* lua/*.[ch] lua/local/l* \
	file/src/[A-Z]* file/src/*.[ch] file/python file/magic file/doc file/[A-Z]* file/acinclude.m4 file/aclocal.m4 \
	file/autogen.sh file/config* file/depcomp file/install-sh file/ltmain.sh file/missing file/mkinstalldirs \
	zlib/[A-Z]* zlib/*.[ch] zlib/autogen.sh zlib/configure.ac zlib/config.guess zlib/config.sub \
	zlib/ltmain.sh zlib/aclocal.m4 zlib/config.h.in zlib/missing zlib/install-sh zlib/depcomp \
	zlib/Makefile.in zlib/configure zlib/stamp-h.in \
    perl/typemap \
	perl/Makefile.PL.in \
	perl/t/00.pod.t perl/t/00.pod.coverage.t \
	perl/rpmxs.c perl/rpmxs.h \
	perl/RPM.xs perl/RPM.pm perl/t/01.rpm.t \
	perl/RPM/Constant.pm perl/RPM_Constant.xs perl/t/02.rpmconstant.t \
	perl/RPM_Header.xs perl/RPM/Header.pm perl/t/03.header.t \
	perl/RPM_Transaction.xs perl/RPM/Transaction.pm perl/t/04.transaction.t \
	perl/RPM_PackageIterator.xs perl/RPM/PackageIterator.pm perl/t/05.packageiterator.t \
	perl/RPM_Problems.xs perl/RPM/Problems.pm perl/t/06.problems.t \
	perl/RPM_Files.xs perl/RPM/Files.pm perl/t/07.files.t \
	perl/RPM_Dependencies.xs perl/RPM/Dependencies.pm perl/t/08.dependencies.t \
	perl/RPM_Spec.xs perl/RPM/Spec.pm perl/t/09.spec.t \
	perl/hdlist-test.hdr perl/test-rpm-1.0-1.noarch.rpm perl/test-rpm-1.0-1.src.rpm \
	perl/META.yml perl/Makefile.PL.in perl/Makefile.am perl/Makefile.in \
	perl/README \
	pubkeys/JBJ-GPG-KEY \
	po/*.in po/*.po po/rpm.pot \
	rpm.magic rpmqv.c \
	python/mpw/Makefile.am python/mpw/test/* python/test/resources/* python/test/dscmp \
	python/test/psTest.py python/test/testhdr python/test/testit python/test/testmi \
	python/test/testrollback.py python/test/dsMerge.py python/rpm python/rpm/Makefile.in \
	python/rpm/Makefile.am python/rpm/__init__.py python/[a-z]* python/Makefile.in python/Makefile.am

SUBDIRS =
if USE_NLS
SUBDIRS += \
	po
endif
SUBDIRS += \
	@WITH_DB_SUBDIR@ \
	@WITH_ZLIB_SUBDIR@ \
	@WITH_LUA_SUBDIR@ \
	@WITH_FILE_SUBDIR@ \
	@WITH_XAR_SUBDIR@ \
	misc \
	rpmio \
	rpmdb \
	lib \
	build \
	rpmconstant \
	@WITH_PYTHON_SUBDIR@ \
	@WITH_PERL_SUBDIR@ \
	tools \
	scripts \
	doc \
	.

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
pkglib_DATA =	rpmpopt macros
pkglib_SCRIPTS = install-sh mkinstalldirs

noinst_HEADERS = build.h debug.h system.h

rpm_SOURCES =		$(top_srcdir)/build.c
rpm_LDFLAGS =		@LDFLAGS_STATIC@
rpm_LDADD =		rpm.o $(myLDADD)
rpm.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMBT -DIAM_RPMDB -DIAM_RPMEIU -DIAM_RPMK -DIAM_RPMQV -o $@ -c $(top_srcdir)/rpmqv.c

rpmbuild_SOURCES =	$(top_srcdir)/build.c
rpmbuild_LDFLAGS =	@LDFLAGS_STATIC@
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

.PHONY:	setperms
setperms:
	@for f in $(bin_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(bindir)/$$f ;\
	done
	@for f in $(pkglib_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)/$$f ;\
	    $(__CHMOD) g+s $(DESTDIR)$(pkglibdir)/$$f ;\
	done
	@for f in $(pkglib_SCRIPTS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)/$$f ;\
	done
	@$(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)
	@$(__CHOWN) -R ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(varprefix)/lib/rpm
	@$(__CHOWN) -R ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(varprefix)/spool/repackage
	-@$(__CHMOD) 0664 $(DESTDIR)$(varprefix)/lib/rpm/[A-Z]*
	-@$(__CHMOD) 0775 $(DESTDIR)$(varprefix)/lib/rpm
	-@$(__CHMOD) 0664 $(DESTDIR)$(varprefix)/lib/rpm/__db.*
	-@$(__CHMOD) 0755 $(DESTDIR)$(varprefix)/spool/repackage

.PHONY:	unsetgid
unsetgid:
	@for f in $(pkglib_PROGRAMS) ; do\
	    $(__CHMOD) g-s $(DESTDIR)$(pkglibdir)/$$f ;\
	done

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
doxygen @WITH_APIDOCS_TARGET@: Doxyfile rpmpopt macros
	rm -rf $@
	mkdir -p $@
	- [ X"@__DOXYGEN@" != Xno ] && @__DOXYGEN@

cscope:
	cscope -b -R

cref: ctags cscope

ACLOCAL_AMFLAGS = -I m4
