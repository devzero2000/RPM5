%define	with_python_subpackage	1 %{nil}
%define	with_bzip2		1 %{nil}
%define	with_apidocs		1 %{nil}
%define with_internal_db	1 %{nil}
%define strip_binaries		1

# XXX legacy requires './' payload prefix to be omitted from rpm packages.
%define	_noPayloadPrefix	1

%define	__prefix	/usr
%{expand:%%define __share %(if [ -d %{__prefix}/share/man ]; then echo /share ; else echo %%{nil} ; fi)}

Summary: The Red Hat package management system.
Name: rpm
%define version 4.0.3
Version: %{version}
Release: 0.26
Group: System Environment/Base
Source: ftp://ftp.rpm.org/pub/rpm/dist/rpm-4.0.x/rpm-%{version}.tar.gz
Copyright: GPL
Conflicts: patch < 2.5
%ifos linux
Prereq: gawk fileutils textutils mktemp shadow-utils
Requires: popt
%endif

%if !%{with_internal_db}
BuildRequires: db3-devel

# XXX glibc-2.1.92 has incompatible locale changes that affect statically
# XXX linked binaries like /bin/rpm.
%ifnarch ia64
Requires: glibc >= 2.1.92
# XXX needed to avoid libdb.so.2 satisfied by compat/libc5 provides.
Requires: db1 = 1.85
%endif
%endif

# XXX Red Hat 5.2 has not bzip2 or python
%if %{with_bzip2}
BuildRequires: bzip2 >= 0.9.0c-2
%endif
%if %{with_python_subpackage}
BuildRequires: python-devel >= 1.5.2
%endif

BuildRoot: %{_tmppath}/%{name}-root

%description
The RPM Package Manager (RPM) is a powerful command line driven
package management system capable of installing, uninstalling,
verifying, querying, and updating software packages.  Each software
package consists of an archive of files along with information about
the package like its version, a description, etc.

%package devel
Summary: Development files for applications which will manipulate RPM packages.
Group: Development/Libraries
Requires: rpm = %{version}, popt

%description devel
This package contains the RPM C library and header files.  These
development files will simplify the process of writing programs which
manipulate RPM packages and databases. These files are intended to
simplify the process of creating graphical package managers or any
other tools that need an intimate knowledge of RPM packages in order
to function.

This package should be installed if you want to develop programs that
will manipulate RPM packages and databases.

%package build
Summary: Scripts and executable programs used to build packages.
Group: Development/Tools
Requires: rpm = %{version}

%description build
This package contains scripts and executable programs that are used to
build packages using RPM.

%if %{with_python_subpackage}
%package python
Summary: Python bindings for apps which will manipulate RPM packages.
Group: Development/Libraries
BuildRequires: popt >= 1.5
Requires: rpm = %{version}
Requires: popt >= 1.5
Requires: python >= 1.5.2

%description python
The rpm-python package contains a module which permits applications
written in the Python programming language to use the interface
supplied by RPM (RPM Package Manager) libraries.

This package should be installed if you want to develop Python
programs that will manipulate RPM packages and databases.
%endif

%package -n popt
Summary: A C library for parsing command line parameters.
Group: Development/Libraries
Version: 1.6.3

%description -n popt
Popt is a C library for parsing command line parameters.  Popt was
heavily influenced by the getopt() and getopt_long() functions, but it
improves on them by allowing more powerful argument expansion.  Popt
can parse arbitrary argv[] style arrays and automatically set
variables based on command line arguments.  Popt allows command line
arguments to be aliased via configuration files and includes utility
functions for parsing arbitrary strings into argv[] arrays using
shell-like rules.

Install popt if you're a C programmer and you'd like to use its
capabilities.

%prep
%setup -q

%build
%ifos linux
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{__prefix} --sysconfdir=/etc --localstatedir=/var --infodir='${prefix}%{__share}/info' --mandir='${prefix}%{__share}/man' # --enable-db1
%else
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{__prefix} # --enable-db1
%endif

make

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR="$RPM_BUILD_ROOT" install

%ifos linux

# Save list of packages through cron
mkdir -p ${RPM_BUILD_ROOT}/etc/cron.daily
install -m 755 scripts/rpm.daily ${RPM_BUILD_ROOT}/etc/cron.daily/rpm

mkdir -p ${RPM_BUILD_ROOT}/etc/logrotate.d
install -m 755 scripts/rpm.log ${RPM_BUILD_ROOT}/etc/logrotate.d/rpm

mkdir -p $RPM_BUILD_ROOT/etc/rpm
#cat << E_O_F > $RPM_BUILD_ROOT/etc/rpm/macros.db1
#%%_dbapi		1
#E_O_F

%endif

%if %{with_apidocs}
gzip -9n apidocs/man/man*/* || :
%endif

%if %{strip_binaries}
{ cd $RPM_BUILD_ROOT
  strip ./bin/rpm
  strip .%{__prefix}/bin/rpm2cpio
}
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%pre
%ifos linux
if [ -f /var/lib/rpm/packages.rpm ]; then
    echo "

This version of rpm does not have db1 support, install rpm-4.0.2 and
do rpm --rebuilddb to convert your database from db1 to db3 format.

"
    exit 1
fi
/usr/sbin/groupadd -g 37 rpm
/usr/sbin/useradd  -d /var/lib/rpm -u 37 -g 37 rpm
%endif
exit 0

%post
%ifos linux
/sbin/ldconfig
%endif
# initialize db3 database
/bin/rpm --initdb
/bin/chown rpm.rpm /var/lib/rpm/[A-Z]*

%ifos linux
%postun
/sbin/ldconfig
if [ $1 == 0 ]; then
    /usr/sbin/userdel rpm
    /usr/sbin/groupdel rpm
fi


%post devel -p /sbin/ldconfig
%postun devel -p /sbin/ldconfig

%post -n popt -p /sbin/ldconfig
%postun -n popt -p /sbin/ldconfig
%endif

%if %{with_python_subpackage}
%post python -p /sbin/ldconfig
%postun python -p /sbin/ldconfig
%endif

%files
%defattr(-,root,root)
%doc RPM-PGP-KEY RPM-GPG-KEY CHANGES GROUPS doc/manual/[a-z]*
%attr(0755, rpm, rpm)	/bin/rpm

%ifos linux
%config(missingok)	/etc/cron.daily/rpm
%config(missingok)	/etc/logrotate.d/rpm
%dir			/etc/rpm
#%config(missingok)	/etc/rpm/macros.db1
%endif

%attr(0755, rpm, rpm)	%{__prefix}/bin/rpm2cpio
%attr(0755, rpm, rpm)	%{__prefix}/bin/gendiff
%attr(0755, rpm, rpm)	%{__prefix}/bin/rpmdb
%attr(0755, rpm, rpm)	%{__prefix}/bin/rpm[eiukqv]
%attr(0755, rpm, rpm)	%{__prefix}/bin/rpmsign
%attr(0755, rpm, rpm)	%{__prefix}/bin/rpmquery
%attr(0755, rpm, rpm)	%{__prefix}/bin/rpmverify

%{__prefix}/lib/librpm.so.*
%{__prefix}/lib/librpmdb.so.*
%{__prefix}/lib/librpmio.so.*
%{__prefix}/lib/librpmbuild.so.*

%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/config.guess
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/config.sub
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/convertrpmrc.sh
%attr(0644, rpm, rpm)	%{__prefix}/lib/rpm/macros
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/mkinstalldirs
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/rpm.*
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/rpm[deiukqv]
%attr(0644, rpm, rpm)	%{__prefix}/lib/rpm/rpmpopt*
%attr(0644, rpm, rpm)	%{__prefix}/lib/rpm/rpmrc

%attr(0755, rpm, rpm)	%dir /var/lib/rpm
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Basenames
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Conflictname
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/__db.001
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Dirnames
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Group
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Installtid
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Name
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Packages
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Providename
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Provideversion
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Removetid
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Requirename
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Requireversion
#%attr(0755, rpm, rpm)	%ghost /var/lib/rpm/Triggername

%ifarch i386 i486 i586 i686 athlon
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/i[3456]86*
%endif
%ifarch alpha
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/alpha*
%endif
%ifarch sparc sparc64
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/sparc*
%endif
%ifarch ia64
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/ia64*
%endif
%ifarch powerpc ppc
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/ppc*
%endif
%ifarch s390 s390x
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/s390*
%endif
%ifarch armv3l armv4l
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/armv[34][lb]*
%endif

%lang(cs)	%{__prefix}/*/locale/cs/LC_MESSAGES/rpm.mo
%lang(da)	%{__prefix}/*/locale/da/LC_MESSAGES/rpm.mo
%lang(de)	%{__prefix}/*/locale/de/LC_MESSAGES/rpm.mo
%lang(fi)	%{__prefix}/*/locale/fi/LC_MESSAGES/rpm.mo
%lang(fr)	%{__prefix}/*/locale/fr/LC_MESSAGES/rpm.mo
%lang(is)	%{__prefix}/*/locale/is/LC_MESSAGES/rpm.mo
%lang(ja)	%{__prefix}/*/locale/ja/LC_MESSAGES/rpm.mo
%lang(no)	%{__prefix}/*/locale/no/LC_MESSAGES/rpm.mo
%lang(pl)	%{__prefix}/*/locale/pl/LC_MESSAGES/rpm.mo
%lang(pt)	%{__prefix}/*/locale/pt/LC_MESSAGES/rpm.mo
%lang(pt_BR)	%{__prefix}/*/locale/pt_BR/LC_MESSAGES/rpm.mo
%lang(ro)	%{__prefix}/*/locale/ro/LC_MESSAGES/rpm.mo
%lang(ru)	%{__prefix}/*/locale/ru/LC_MESSAGES/rpm.mo
%lang(sk)	%{__prefix}/*/locale/sk/LC_MESSAGES/rpm.mo
%lang(sl)	%{__prefix}/*/locale/sl/LC_MESSAGES/rpm.mo
%lang(sr)	%{__prefix}/*/locale/sr/LC_MESSAGES/rpm.mo
%lang(sv)	%{__prefix}/*/locale/sv/LC_MESSAGES/rpm.mo
%lang(tr)	%{__prefix}/*/locale/tr/LC_MESSAGES/rpm.mo

%{__prefix}%{__share}/man/man[18]/*.[18]*
%lang(pl)	%{__prefix}%{__share}/man/pl/man[18]/*.[18]*
%lang(ru)	%{__prefix}%{__share}/man/ru/man[18]/*.[18]*
%lang(sk)	%{__prefix}%{__share}/man/sk/man[18]/*.[18]*

%files build
%defattr(-,root,root)
%dir %{__prefix}/src/redhat
%dir %{__prefix}/src/redhat/BUILD
%dir %{__prefix}/src/redhat/SPECS
%dir %{__prefix}/src/redhat/SOURCES
%dir %{__prefix}/src/redhat/SRPMS
%dir %{__prefix}/src/redhat/RPMS
%{__prefix}/src/redhat/RPMS/*
%attr(0755, rpm, rpm)	%{__prefix}/bin/rpmbuild
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/brp-*
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/check-prereqs
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/cpanflute
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/find-lang.sh
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/find-prov.pl
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/find-provides
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/find-provides.perl
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/find-req.pl
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/find-requires
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/find-requires.perl
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/get_magic.pl
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/getpo.sh
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/http.req
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/javadeps
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/magic.prov
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/magic.req
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/perl.prov
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/perl.req
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/rpm[bt]
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/rpmdiff
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/rpmdiff.cgi
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/u_pkg.sh
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/vpkg-provides.sh
%attr(0755, rpm, rpm)	%{__prefix}/lib/rpm/vpkg-provides2.sh

%if %{with_python_subpackage}
%files python
%defattr(-,root,root)
%{__prefix}/lib/python1.5/site-packages/rpmmodule.so
%endif

%files devel
%defattr(-,root,root)
%if %{with_apidocs}
%doc apidocs
%endif
%{__prefix}/include/rpm
%{__prefix}/lib/librpm.a
%{__prefix}/lib/librpm.la
%{__prefix}/lib/librpm.so
%{__prefix}/lib/librpmdb.a
%{__prefix}/lib/librpmdb.la
%{__prefix}/lib/librpmdb.so
%{__prefix}/lib/librpmio.a
%{__prefix}/lib/librpmio.la
%{__prefix}/lib/librpmio.so
%{__prefix}/lib/librpmbuild.a
%{__prefix}/lib/librpmbuild.la
%{__prefix}/lib/librpmbuild.so

%files -n popt
%defattr(-,root,root)
%{__prefix}/lib/libpopt.so.*
%{__prefix}%{__share}/man/man3/popt.3*
%lang(cs)	%{__prefix}/*/locale/cs/LC_MESSAGES/popt.mo
%lang(da)	%{__prefix}/*/locale/da/LC_MESSAGES/popt.mo
%lang(gl)	%{__prefix}/*/locale/gl/LC_MESSAGES/popt.mo
%lang(hu)	%{__prefix}/*/locale/hu/LC_MESSAGES/popt.mo
%lang(is)	%{__prefix}/*/locale/is/LC_MESSAGES/popt.mo
%lang(no)	%{__prefix}/*/locale/no/LC_MESSAGES/popt.mo
%lang(pt)	%{__prefix}/*/locale/pt/LC_MESSAGES/popt.mo
%lang(ro)	%{__prefix}/*/locale/ro/LC_MESSAGES/popt.mo
%lang(ru)	%{__prefix}/*/locale/ru/LC_MESSAGES/popt.mo
%lang(sk)	%{__prefix}/*/locale/sk/LC_MESSAGES/popt.mo
%lang(sl)	%{__prefix}/*/locale/sl/LC_MESSAGES/popt.mo
%lang(sv)	%{__prefix}/*/locale/sv/LC_MESSAGES/popt.mo
%lang(tr)	%{__prefix}/*/locale/tr/LC_MESSAGES/popt.mo
%lang(uk)	%{__prefix}/*/locale/uk/LC_MESSAGES/popt.mo
%lang(wa)	%{__prefix}/*/locale/wa/LC_MESSAGES/popt.mo
%lang(zh_CN)	%{__prefix}/*/locale/zh_CN.GB2312/LC_MESSAGES/popt.mo

# XXX These may end up in popt-devel but it hardly seems worth the effort now.
%{__prefix}/lib/libpopt.a
%{__prefix}/lib/libpopt.la
%{__prefix}/lib/libpopt.so
%{__prefix}/include/popt.h

%changelog
* Wed May 23 2001 Jeff Johnson <jbj@redhat.com>
- headerFree() returns NULL, _free is C++ safe.

* Mon May 21 2001 Jeff Johnson <jbj@redhat.com>
- fix: skip %ghost files when building packages (#38218).
- refuse to install on systems using db1.

* Sun May 20 2001 Jeff Johnson <jbj@redhat.com>
- fix: i18n strings need 1 on sucess return code (#41313).

* Wed May 16 2001 Jeff Johnson <jbj@redhat.com>
- fix: filter duplicate package removals (#35828).
- add armv3l arch.

* Mon May 14 2001 Jeff Johnson <jbj@redhat.com>
- upgrade to db-3.3.4.

* Sun May 13 2001 Jeff Johnson <jbj@redhat.com>
- add cron/logrotate scripts to save installed package filenames.

* Thu May 10 2001 Jeff Johnson <jbj@redhat.com>
- rpm database has rpm.rpm g+w permissions to share db3 mutexes.
- expose more db3 macro configuration tokens.
- move fprint.[ch] and hash.[ch] to rpmdb directory.
- detect and fiddle incompatible mixtures of db3 env/open flags.
- add DBI_WRITECURSOR to map to db3 flags with CDB database model.
- add rpmdbSetIteratorRewrite to warn of pending lazy (re-)writes.
- harden rpmdb iterators from damaged header instance segfaults.

* Mon May  7 2001 Jeff Johnson <jbj@redhat.com>
- use internal db-3.2.9 sources to build by default.
- don't build db1 support by default.
- create rpmdb.la so that linkage against rpm's db-3.2.9 is possible.

* Sun May  6 2001 Jeff Johnson <jbj@redhat.com>
- fix: specfile queries with BuildArch: (#27589).

* Sat May  5 2001 Jeff Johnson <jbj@redhat.com>
- enough lclint annotations and fiddles already.

* Thu May  3 2001 Jeff Johnson <jbj@redhat.com>
- still more boring lclint annotations and fiddles.

* Sun Apr 29 2001 Jeff Johnson <jbj@redhat.com>
- transaction iterator(s) need to run in reverse order on pure erasures.
- erasures not yet strict, warn & chug on unlink(2)/rmdir(2) failure.
- more boring lclint annotations and fiddles.

* Sat Apr 28 2001 Jeff Johnson <jbj@redhat.com>
- globalize _free(3) wrapper in rpmlib.h, consistent usage throughout.
- internalize locale insensitive ctype(3) in rpmio.h
- boring lclint annotations and fiddles.

* Thu Apr 26 2001 Jeff Johnson <jbj@redhat.com>
- fix: ineeded count wrong for overlapped, created files.

* Wed Apr 25 2001 Jeff Johnson <jbj@redhat.com>
- fix: readlink return value clobbered by header write.

* Mon Apr 23 2001 Jeff Johnson <jbj@redhat.com>
- regenerate rpm.8 man page from docbook glop (in max-rpm).
- lib/depends.c: diddle debugging messages.

* Sat Apr 21 2001 Jeff Johnson <jbj@redhat.com>
- fix: s390 (and ppc?) could return CPIOERR_BAD_HEADER (#28645).
- fix: Fwrite's are optimized out by aggressive compiler(irix) (#34711).
- portability: vsnprintf/snprintf wrappers for those without (#34657).
- more info provided by rpmdepOrder() debugging messages.
- merge (compatible) changes from top-of-stack into rpmlib.h.
- cpio mappings carry dirname/basename, not absolute path.
- fix: check waitpid return code.
- remove support for v1 src rpm's.
- re-position callbacks with ts/fi in cpio payload layer.
- state machines for packages (psm.c) and payloads (fsm.c)
- add --repackage option to put erased bits back into a package.

* Tue Apr 17 2001 Jeff Johnson <jbj@redhat.com>
- fix: s390 (and ppc?) could return CPIOERR_BAD_HEADER (#28645).
- fix: Fwrite's are optimized out by aggressive compiler(irix) (#34711).
- portability: vsnprintf/snprintf wrappers for those without (#34657).
- don't build with db1 support, don't install with packages.rpm present.

* Wed Apr  4 2001 Jeff Johnson <jbj@redhat.com>
- fix: parameterized macro segfault (Jakub Bogusz <qboosh@pld.org.pl>)
- fix: i18n tags in rpm-2.5.x had wrong offset/length (#33478).
- fix: AIX has sizeof(uint_16) != sizeof(mode_t) verify cast needed.
- fix: zero length hard links unpacked incorrectly (#34211).
- fix: --relocate missing trailing slash (#28874,#25876).
- fix: --excludedoc shouldn't create empty doc dir (#14531).
- fix: %_netsharedpath needs to look at basenames (#26561).
- fix: --excludepath was broken (#24434).

* Thu Mar 22 2001 Jeff Johnson <jbj@redhat.com>
- update per-interpreter dependency scripts, add sql/tcl (#20295).
- fix: rpmvercmp("1.a", "1.") returned -1, not +1 (#21392).
- add %exclude support (i.e. "everything but") to %files.
	(Michael (Micksa) Slade" <micksa@knobbits.org>)
- add --with/--without popt glue for conditional builds(Tomasz Kloczko).
- python: strip header regions during unload.
- add -g to optflags in per-platform config.
- permit confgure/compile with db3-3.2.9.
- permit manifest files as args to query/verify modes.

* Thu Mar 15 2001 Jeff Johnson <jbj@redhat.com>
- start rpm-4.0.3.
- add cpuid asm voodoo to detect athlon processors.
