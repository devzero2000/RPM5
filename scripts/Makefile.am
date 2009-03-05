# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

HELPERS = \
	makeshlibs

EXTRA_DIST = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-nobuildrootpath \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	brp-implant-ident-static brp-java-repack-jars \
	check-files cross-build \
	deb_Packages deb_Sources \
	deb_control deb_md5sums deb_postinst deb_postrm deb_preinst deb_prerm \
	executabledeps.sh \
	find-debuginfo.sh find-lang.sh find-prov.pl find-req.pl \
	find-provides.ksyms find-requires.ksyms kmodtool \
	cpanflute cpanflute2 Specfile.pm find-provides.perl \
	find-requires.perl freshen.sh gendiff getpo.sh http.req \
	check-java-closure.sh java.prov.sh java.req.sh \
	javadeps.sh libtooldeps.sh \
	mono-find-provides mono-find-requires \
	osgideps.pl pkgconfigdeps.sh \
	perldeps.pl perl.prov perl.req pythondeps.sh \
	php.prov php.req rpm2cpio \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd \
	rpmsort rt-reloc-wrapper.sh symset-table \
	sql.prov sql.req symclash.py symclash.sh tcl.req tgpg trpm u_pkg.sh \
	vcheck vpkg-provides.sh vpkg-provides2.sh \
	wnh_filelists_yaml wnh_other_yaml wnh_primary_yaml \
	yum_filelists_sqlite yum_other_sqlite yum_primary_sqlite \
	yum_filelists_xml yum_other_xml yum_primary_xml \
	macros.perl* macros.python* \
	macros.php* find-*.php find-php-* ${HELPERS}

all:

bin_SCRIPTS = gendiff

pkglibdir =		@USRLIBRPM@
pkglib_SCRIPTS = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-nobuildrootpath \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files cross-build executabledeps.sh \
	find-debuginfo.sh find-lang.sh find-prov.pl find-req.pl \
	find-provides.perl \
	find-requires.perl getpo.sh http.req \
	javadeps.sh libtooldeps.sh \
	mono-find-provides mono-find-requires \
	osgideps.pl pkgconfigdeps.sh \
	perldeps.pl perl.prov perl.req pythondeps.sh \
	php.prov php.req \
	rpm2cpio rpmdb_loadcvt \
	rpm.daily rpm.log rpm.xinetd \
	tgpg u_pkg.sh \
	vcheck vpkg-provides.sh vpkg-provides2.sh

pkglibhelperdir =	@USRLIBRPM@/helpers
pkglibhelper_SCRIPTS = 	${HELPERS}

pkglibqfdir =		@USRLIBRPM@/qf
pkglibqf_DATA = \
	deb_Packages deb_Sources \
	deb_control deb_md5sums deb_postinst deb_postrm deb_preinst deb_prerm \
	wnh_filelists_yaml wnh_other_yaml wnh_primary_yaml \
	yum_filelists_sqlite yum_other_sqlite yum_primary_sqlite \
	yum_filelists_xml yum_other_xml yum_primary_xml

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rpm.pc

install-exec-hook:
if WITH_PATH_VERSIONED
	-for p in $(bin_SCRIPTS); do \
	    mv $(DESTDIR)$(bindir)/$${p}$(EXEEXT) \
	       $(DESTDIR)$(bindir)/$${p}-$(VERSION)$(EXEEXT); \
	done
endif
install-data-hook:
if WITH_PATH_VERSIONED
	-for p in $(pkgconfig_DATA); do \
	    mv $(DESTDIR)$(pkgconfigdir)/$${p} \
	       `echo "$(DESTDIR)$(pkgconfigdir)/$${p}" | sed 's/\.\([^.]*\)$$/-$(VERSION).\1/'`; \
	done
endif

