# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = config.rpath m4/ChangeLog  CHANGES CREDITS Doxyheader GROUPS INSTALL \
	autodeps/none autodeps/*.prov autodeps/*.req autogen.sh \
	config.site db db3/configure installplatform platform* \
	perl/Changes perl/MANIFEST perl/META.yml perl/Makefile.PL \
	perl/Makefile.PL.in perl/Makefile.am perl/Makefile.in perl/README \
	perl/RPM.pm perl/RPM.xs perl/foo.i386.rpm perl/hdlist-test.hdr \
	perl/test-rpm-1.0-1.noarch.rpm \
	perl/test-rpm-1.0-1.src.rpm \
	perl/test.pl \
	perl/typemap \
	pubkeys/JBJ-GPG-KEY \
	po/*.in po/*.po po/rpm.pot \
	rpm.magic rpmqv.c rpm.c

SUBDIRS = po misc @WITH_ZLIB_SUBDIR@ @WITH_ELFUTILS_SUBDIR@ @WITH_MAGIC_SUBDIR@ @WITH_DB_SUBDIR@ @WITH_SQLITE3_SUBDIR@ @WITH_POPT_SUBDIR@ @WITH_BEECRYPT_SUBDIR@ @WITH_NEON_SUBDIR@ @WITH_SYCK_SUBDIR@ @WITH_LUA_SUBDIR@ rpmio rpmdb lib build @WITH_PYTHON_SUBDIR@ @WITH_PERL_SUBDIR@ tools scripts doc .

INCLUDES = \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc \
	@WITH_LIBELF_INCLUDE@ \
	@INCPATH@

staticLDFLAGS = # @LDFLAGS_STATIC@ # @LDFLAGS_NPTL@

myLDFLAGS = @WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@

myLDADD = \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_ZLIB_LIB@ \
	@LIBMISC@

bin_PROGRAMS = 		rpm

pkglibdir =		@USRLIBRPM@
versionlibdir =		$(pkglibdir)/@VERSION@
versionlib_DATA =	rpmpopt macros

noinst_HEADERS = build.h debug.h system.h

rpm_SOURCES =		$(top_srcdir)/build.c
rpm_LDFLAGS =		$(myLDFLAGS) $(staticLDFLAGS)
rpm_LDADD =		rpm.o $(top_builddir)/build/.libs/librpmbuild.a $(myLDADD)
rpm.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -fpie -DIAM_RPMBT -DIAM_RPMDB -DIAM_RPMEIU -DIAM_RPMK -DIAM_RPMQV -o $@ -c $<

$(PROGRAMS): 		$(myLDADD) @WITH_APIDOCS_TARGET@

.PHONY:	splint
splint:
	splint \
	    -load build/rpmbuild.lcd \
	    -load lib/rpmlib.lcd \
	    -load rpmdb/rpmdb.lcd \
	    -load rpmio/rpmio.lcd \
	    -load popt/popt.lcd \
		$(DEFS) $(INCLUDES) rpmqv.c $(rpmb_SOURCES)

.PHONY:	lint
lint:
	$(LINT) -Dlint $(DEFS) $(INCLUDES) rpmqv.c $(rpmb_SOURCES) \
		`make -s sources -C build` \
		`make -s sources -C lib` \
		`make -s sources -C rpmdb` \
		`make -s sources -C rpmio` \
		`make -s sources -C beecrypt` \
		`make -s sources -C file/src` \
		`make -s sources -C popt`

CVSTAG = r$(subst .,-,$(VERSION))

pkgsrcdir = $(prefix)/src/rpm

install-data-local:
	@$(mkinstalldirs) $(DESTDIR)$(varprefix)/lib/rpm
	@for dir in BUILD RPMS SOURCES SPECS SRPMS ; do\
	    $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/$$dir;\
	done
	@case "@host_cpu@" in \
	*86)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i386 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i486 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i586 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i686 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/athlon ;;\
	armv3l)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;;\
	armv4b)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4b ;;\
	armv4l)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4l ;;\
	armv5teb) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4b ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5teb ;;\
	armv5tel) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4l ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5tel ;;\
	alpha*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/alpha ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/alphaev6 ;;\
	sparc*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv8 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv9 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc64 ;;\
	ia64*)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ia64 ;;\
	s390*)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/s390 ;;\
	mipsel*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/mipsel ;;\
	mips*)  $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/mips ;;\
	powerpc*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppciseries ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppcpseries ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64iseries ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64pseries ;;\
	*)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/@host_cpu@ ;;\
	esac
	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/noarch
	@case "@host_os@" in \
	mint) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/m68kmint ;;\
	solaris*|linux*|darwin*) \
	   chmod u+x $(top_srcdir)/installplatform; DESTDIR="$(DESTDIR)" pkglibdir="$(pkglibdir)" $(top_srcdir)/installplatform rpmrc macros platform ;; \
	esac
	@$(mkinstalldirs) $(DESTDIR)/var/tmp

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
	-@$(__CHMOD) 0664 $(DESTDIR)$(varprefix)/lib/rpm/[A-Z]*
	-@$(__CHMOD) 0775 $(DESTDIR)$(varprefix)/lib/rpm
	-@$(__CHMOD) 0664 $(DESTDIR)$(varprefix)/lib/rpm/__db.*

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

ctags:
	find . -type f -name "*.[ch]*" | xargs @CTAGS@

cscope:
	@CSCOPE@ -b -R

cref: ctags cscope

ACLOCAL_AMFLAGS = -I m4
