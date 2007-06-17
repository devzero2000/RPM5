# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-nobuildrootpath \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files cross-build executabledeps.sh \
	find-debuginfo.sh find-lang.sh find-prov.pl find-req.pl \
	cpanflute cpanflute2 Specfile.pm find-provides.perl \
	find-requires.perl freshen.sh gendiff getpo.sh http.req \
	check-java-closure.sh java.prov.sh java.req.sh \
	javadeps.sh libtooldeps.sh pkgconfigdeps.sh \
	perldeps.pl perl.prov perl.req pythondeps.sh \
	php.prov php.req rpm2cpio \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd \
	sql.prov sql.req symclash.py symclash.sh tcl.req tgpg trpm u_pkg.sh \
	vpkg-provides.sh vpkg-provides2.sh \
	macros.perl* macros.python* \
	macros.php* find-*.php find-php-*

installprefix = $(DESTDIR)

all:

bin_SCRIPTS = gendiff rpm2cpio

pkglibdir =		@USRLIBRPM@
versionlibdir =		$(pkglibdir)/@VERSION@
versionlib_SCRIPTS = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-nobuildrootpath \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files cross-build executabledeps.sh \
	find-debuginfo.sh find-lang.sh find-prov.pl find-req.pl \
	find-provides.perl \
	find-requires.perl getpo.sh http.req \
	javadeps.sh libtooldeps.sh pkgconfigdeps.sh \
	perldeps.pl perl.prov perl.req pythondeps.sh \
	php.prov php.req \
	rpmdb_loadcvt \
	rpm.daily rpm.log rpm.xinetd \
	symclash.py symclash.sh tgpg u_pkg.sh \
	vpkg-provides.sh vpkg-provides2.sh
