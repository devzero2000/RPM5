## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

# Note: *.src.rpm's cannot be added here because of suffix rules.
EXTRA_DIST =	genpgp.sh tpgp.c ref/[^C]* ref/.alldigests

EXTRA_PROGRAMS = tpgp

SUBDIRS =

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
	@WITH_SYCK_CPPFLAGS@ \
	@WITH_XAR_CPPFLAGS@

RPM_LDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/librpmmisc.la \
	@LTLIBINTL@

macros =	$(abs_top_builddir)/macros:$(testdir)/macros

rpm =		$(abs_top_builddir)/rpm --target i386 --macros $(macros)
rpm2cpio =	$(abs_top_builddir)/tools/rpm2cpio
rpmcache =	$(abs_top_builddir)/tools/rpmcache --macros $(macros)
rpmdigest =	$(abs_top_builddir)/tools/rpmdigest
rpmrepo =	$(abs_top_builddir)/tools/rpmrepo
rpmsolverA =	${rpm} -i -D '_dbpath %{_solve_A}' --justdb --nodeps
rpmsolverB =	${rpm} -i -D '_dbpath %{_solve_B}' --justdb --nodeps

poky_topdir =	/X/src/poky
poky_deploy =	$(poky_topdir)/build/tmp/deploy/rpm
poky_pkgsdirA =	$(poky_deploy)/i586
poky_pkgsdirB =	$(poky_deploy)/all
poky_pkgsdirC =	$(poky_deploy)/qemux86
poky_options =	--noaid --nodeps --noorder --noscripts --notriggers --noparentdirs --nolinktos --stats
poky_force =	--replacepkgs --replacefiles --oldpackage
poky_solverA =	${rpm} -i $(poky_force) -D '_dbpath %{_solve_A}' --justdb $(poky_options)
poky_solverB =	${rpm} -i $(poky_force) -D '_dbpath %{_solve_B}' --justdb $(poky_options)
poky_solverC =	${rpm} -i $(poky_force) -D '_dbpath %{_solve_C}' --justdb $(poky_options)

tpgp =		./tpgp --macros $(macros)

mtree =		$(abs_top_builddir)/tools/rpmmtree
wget =		/usr/bin/wget

sudo =		sudo
caosroot =	$(abs_top_builddir)/tests/tmp/CAOS
centosroot =	$(abs_top_builddir)/tests/tmp/CENTOS
suseroot =	$(abs_top_builddir)/tests/tmp/SUSE
mandrivaroot =	$(abs_top_builddir)/tests/tmp/MANDRIVA
caixaroot =	$(abs_top_builddir)/tests/tmp/CAIXA
idmsroot =	$(abs_top_builddir)/tests/tmp/IDMS
unityroot =	$(abs_top_builddir)/tests/tmp/UNITY

rpmdb =		$(abs_top_builddir)/tests/tmp/rpmdb

cp =		@__CP@
cpio =		@__CPIO@
diff =		@__DIFF@
echo =		/bin/echo
file =		@__FILE@
gpg =		@__GPG@
mkdir =		@__MKDIR@
mv =		@__MV@
rm =		@__RM@
tar =		@__TAR@

tpgp_SOURCES = genpgp.h tpgp.c
tpgp_LDADD = $(RPM_LDADD)

tpgp.c: genpgp.h

genpgp.h: genpgp.sh
	-sh $< > $@

.PHONY:	check-init
check-init: macros
	@echo "=== $@ ==="
	@mkdir -p tmp/hrmib/ tmp/repackage/
	@${rpm} -v --version	> tmp/version
	@-diff -u {tmp,ref}/version || cp {tmp,ref}/version
	@${rpm} -v --querytags	> tmp/querytags
	@-diff -u {tmp,ref}/querytags || cp {tmp,ref}/querytags
	@${rpm} -v --showrc | sed -e "s,$(abs_top_builddir),..,g" > tmp/showrc
	@-diff -u {tmp,ref}/showrc || cp {tmp,ref}/showrc
	@${rpmdigest} --alldigests ref/[^C]* > tmp/.alldigests
	@-diff -u {tmp,ref}/.alldigests || cp {tmp,ref}/.alldigests
	@-${mtree} -c -p ref | ${mtree} -p ref

POPTURI =	http://rpm5.org/files/popt/
POPTPKGS =	\
	popt-1.14-1.src.rpm \
	popt-1.14-1.i386.rpm

$(POPTPKGS):
	-${wget} $(POPTURI)$@

.PHONY:	check-pubkeys
check-pubkeys: tpgp $(POPTPKGS)
	@echo "=== $@ ==="
	@${rm} -rf tmp/rpmdb
	@${mkdir} -p tmp/rpmdb/log tmp/rpmdb/tmp
	@${cp} ref/DB_CONFIG tmp/rpmdb
	@${rpm} --import tmp/DSA.pubpem
	@${rpm} --import tmp/RSA.pubpem
	@${echo} -n "--> default:"
	@${tpgp}
	@${echo} -n "--> beecrypt:"
	@${tpgp} --usecrypto bc
	@echo "--> gcrypt(note: MPI padding is BROKEN atm):"
	@-${tpgp} --usecrypto gc
	@${echo} -n "--> nss:"
	@-${tpgp} --usecrypto nss
	@${echo} -n "--> openssl:"
	@-${tpgp} --usecrypto ssl
	@${rpm} --import $(top_srcdir)/pubkeys/JBJ-GPG-KEY
	@${rpm} -Kv $(POPTPKGS) > tmp/popt.Kv
	@diff -u {tmp,ref}/popt.Kv
	@${rpm} -Kvv --usecrypto bc $(POPTPKGS) > /dev/null 2>&1
	@-${rpm} -Kvv --usecrypto gc $(POPTPKGS) > /dev/null 2>&1
	@-${rpm} -Kvv --usecrypto nss $(POPTPKGS) > /dev/null 2>&1
	@-${rpm} -Kvv --usecrypto ssl $(POPTPKGS) > /dev/null 2>&1

.PHONY:	check-sign
check-sign:
	@echo "=== $@ ==="
	@echo "--> DSA:"
	@-${rpm} -D'_gpg_name Donald' --addsign --nopassword cudf-test/*.rpm > /dev/null
	@-${rpm} -Kv cudf-test/*.rpm	> /dev/null
	@echo "--> RSA:"
	@-${rpm} -D'_gpg_name Ronald' --addsign --nopassword edos-test/*.rpm > /dev/null
	@-${rpm} -Kv edos-test/*.rpm	> /dev/null

.PHONY:	check-markup
check-markup: $(POPTPKGS)
	@echo "=== $@ ==="
	@${rpm} -qp --nosignature --qf '[%{*:yaml}\n]' popt*.rpm > tmp/popt.yaml
	@diff -u {tmp,ref}/popt.yaml
	@${rpm} -qp --nosignature --qf '[%{*:xml}\n]' popt*.rpm > tmp/popt.xml
	@diff -u {tmp,ref}/popt.xml
	-@${rpmrepo} --database --gzip .
	-@${rpmrepo} --database --bzip2 .
	-@${rpmrepo} --database --lzma .
	-@${rpmrepo} --gzip .
	-@${rpmrepo} --bzip2 .
	-@${rpmrepo} --lzma .
	@${rpmrepo} --uncompressed .
	@-diff -u {repodata,ref}/primary.xml
	@-diff -u {repodata,ref}/filelists.xml
	@-diff -u {repodata,ref}/other.xml
# XXX repomd.xml always differs because st->st_ctime is used.
	@-diff -u {repodata,ref}/repomd.xml > /dev/null 2>&1

.PHONY:	check-macros
check-macros:
	@echo "=== $@ ==="
	@${rpm} -D 'foo bar' -E '%dump%trace%foo' > /dev/null 2>&1
	@${rpm} -D 'foo %foo' -E '%foo' > /dev/null 2>&1
	@${rpm} -D 'foo bing' -D 'foo bang' -D 'foo boom' -E '%{@foo}' > /dev/null 2>&1
	@${rpm}  -E '%(/bin/echo "-->       sh: Portable Shar!")'
	@-${rpm}  -E '%{lua:print("-->      lua: Hard Rocks!")}'
#	@-${rpm}  -E '%{js:print("-->       js: Use GPSEE!")}'
#	@-${rpm} -E '%{ruby:print "-->     ruby: Puppet Gems!"}'
#	@-${rpm} -E '%{tcl:puts "-->      tcl: Porticus Ports!"}'
#	@-${rpm} -E '%{squirrel:print("--> squirrel: Eat Nuts!")}'
#	@-${rpm} -E '%{perl:print "-->     perl: Artistic Scribbles!"}'
#	@-${rpm} -E '%{python:print "-->   python: Snake Eggs!",}'
#	@-${rpm} -E '%{ficl:.( -->     ficl: Das OpenBoot!)}'

%.spec: $(wildcard %-*.src.rpm)
	@${rpm2cpio} $(wildcard $*-*.src.rpm) | ${cpio} -i --quiet $@

%.src.rpm: $(wildcard *.spec)
	@${rpm} -bs --nodeps $^

BUILD_DIRS = $(shell ${rpm} -qp --qf '%{NAME} ' --nosignature $(wildcard *.src.rpm))

$(BUILD_DIRS): $(wildcard %-*.src.rpm)
	@echo "=== build $@ ==="
	${rpm} -i --nosignature --nodeps $@-*.src.rpm
	(cd $@ && ${rpm} -q --specfile $@.spec && ${rpm} -q --specsrpm $@.spec && ${rpm} -q --specfile --specedit $@.spec && ${rpm} -bb --nodeps $@.spec) > /dev/null

.PHONY:	check-build
check-build: $(BUILD_DIRS)
	@echo "=== $@ ==="
	@ls -1 */*.rpm > tmp/manifest
	@diff -u {tmp,ref}/manifest || ${cp} {tmp,ref}/manifest
	@rm -rf tmp/cachedb
	@${rpm} -i -D '_dbpath $(testdir)/tmp/cachedb' --justdb --nodeps edos-test/*.rpm
	@mkdir -p tmp/hrmib/ tmp/repackage/
	@${rpm} -U -- edos-test/glass-1-*
	@${rpm} -U -- +car +turbo edos-test/{engine-1-*,wheel-2-*,door-1-*,window-0-*}
	@${rpm} -qaT | grep -v gpg-pubkey > tmp/edos.qa.1
	@diff -u {tmp,ref}/edos.qa.1 || ${cp} {tmp,ref}/edos.qa.1
	@${rpm} -U -- edos-test/engine-2-* -turbo
	@${rpm} -U -- edos-test/{wheel-3,tyre-1}-*
	@${rpm} -F -- edos-test/{door-2,window-1,glass-2}-*
	@${rpm} -qaT | grep -v gpg-pubkey > tmp/edos.qa.2
	@diff -u {tmp,ref}/edos.qa.2 || ${cp} {tmp,ref}/edos.qa.2
	@${rpm} -e car engine wheel door tyre window glass

check-depsolver:
	@echo "=== $@ ==="
	rm -rf tmp/solveA tmp/solveB tmp/solveC
	${rpmsolverA} edos-test/turbo-1* edos-test/wheel-2* edos-test/door-1*
	${rpmsolverB} edos-test/engine-2*
	rm -rf tmp/hrmib tmp/repackage
	mkdir -p tmp/hrmib/ tmp/repackage/
	-${rpm} -e --nodeps turbo
	-${rpm} -e --nodeps car
	-${rpm} -e --nodeps engine
	-${rpm} -e --nodeps wheel
	-${rpm} -e --nodeps door
	-${rpm} -U edos-test/car-1-*
	-${rpm} -qa
	-${rpm} -e --nodeps turbo
	-${rpm} -e --nodeps car
	-${rpm} -e --nodeps engine
	-${rpm} -e --nodeps wheel
	-${rpm} -e --nodeps door

#check-poky:
#	-${rpm} -U -D "_solve_pkgsdir $(poky_pkgsdirA)/" --justdb --noscripts --notriggers --noparentdirs --nolinktos --stats $(poky_pkgsdirA)/rpm-5* $(poky_pkgsdirA)/perl-dev-5* $(poky_pkgsdirA)/python-core-2* $(poky_pkgsdirA)/bash-4* $(poky_pkgsdirB}/update-rc.d-0.7-r3.i586.rpm

check-poky:
	@echo "=== $@ ==="
	@rm -rf tmp/solveA tmp/solveB tmp/solveC
	${poky_solverA} $(poky_pkgsdirA)/*.rpm
	${poky_solverB} $(poky_pkgsdirB)/*.rpm
	${poky_solverC} $(poky_pkgsdirC)/*.rpm
	@rm -rf tmp/hrmib tmp/repackage
	@mkdir -p tmp/hrmib/ tmp/repackage/
	find $(poky_pkgsdirA) $(poky_pkgsdirB) $(poky_pkgsdirC) -name '*.rpm' | sort -u > poky_manifest
	-for F in `cat poky_manifest`; do \
	    echo "-----> " $$F; \
	    ${rpm} -U --justdb --noscripts --notriggers --noparentdirs --nolinktos --stats $$F || : ; \
	    ${rpm} -Va --nofiles --noaid --noparentdirs --nolinktos || : ; \
	    ${rpm} -e --justdb --nodeps --noscripts --notriggers `${rpm} -qa | grep -v gpg-pubkey` || : ; \
	done

check-install:
	@echo "=== $@ ==="
	${rpm} -U --relocate /tmp/=$(testdir)/tmp/root/ --nodeps devtool-sanity/*.rpm
	${rpm} -U probes-test/probes-2*.rpm

.PHONY:	check-IDMS
check-IDMS:
	@echo "=== $@ ==="
	$(sudo) rm -rf $(idmsroot) tmp/repackage
	@rm -rf tmp/idms-pkgs
	@mkdir -p $(idmsroot) tmp/idms-pkgs tmp/repackage
	@cd tmp/idms-pkgs && $(wget) -i ../../ref/idms-minimal.i586.manifest
	-$(sudo) $(rpm) -Uvh --noparentdirs --nolinktos -r $(idmsroot) tmp/idms-pkgs/*.rpm

.PHONY:	check-triggers
check-triggers:
	@echo "=== $@ ==="
	@-${rpm} -e --noscripts --notriggers --allmatches `${rpm} -qa | grep '^triggers-*'` > /dev/null 2>&1
	@echo "--> N:"
	@${rpm} -U --noparentdirs --nodeps triggers-N/triggers-N*.rpm
	@${rpm} -e --noparentdirs --nodeps triggers-N-a triggers-N-b
	@echo "--> NA:"
	@${rpm} -U --noparentdirs --nodeps triggers-NA/triggers-NA*.rpm
	@${rpm} -e --noparentdirs --nodeps triggers-NA-a triggers-NA-b
	@echo "--> P:"
	@${rpm} -U --noparentdirs --nodeps triggers-P/triggers-P*.rpm
	@${rpm} -e --noparentdirs --nodeps triggers-P-a triggers-P-b
	@echo "--> F:"
	@${rpm} -U --noparentdirs --nodeps triggers-F/triggers-F*.rpm
	@${rpm} -e --noparentdirs --nodeps triggers-F-a triggers-F-b
	@echo "--> FP:"
	@${rpm} -U --noparentdirs --nodeps triggers-FP/triggers-FP*.rpm
	@${rpm} -e --noparentdirs --nodeps triggers-FP-a triggers-FP-b
	@echo "--> D:"
	@${rpm} -U --noparentdirs --nodeps triggers-D/triggers-D*.rpm
	@${rpm} -e --noparentdirs --nodeps triggers-D-a triggers-D-b
	@echo "--> DP:"
	@${rpm} -U --noparentdirs --nodeps triggers-DP/triggers-DP*.rpm
	@${rpm} -e --noparentdirs --nodeps triggers-DP-a triggers-DP-b

check-query:
	@echo "=== $@ ==="
	@${rpm} -qW . > /dev/null 2>&1
	-@${rpm} -U --justdb */*.rpm > /dev/null 2>&1
	-@${rpm} -Uvh --justdb --nodeps */*.rpm > /dev/null 2>&1
	-@${rpm} -i --justdb --nodeps */*.rpm > /dev/null 2>&1
	@${rpm} -qa > /dev/null
	@${rpm} -qa 'arch=noarch' > /dev/null
	@${rpm} -qa 'arch=!noarch' > /dev/null
	@${rpm} -qa -c > /dev/null
	@${rpm} -qa -d > /dev/null
	@${rpm} -qa -s > /dev/null
	@${rpm} -qa --dump > /dev/null
	@${rpm} -q --querybynumber 1 > /dev/null
	@${rpm} -qal > /dev/null
	@${rpm} -qalv > /dev/null
	@${rpm} -qa --qf '%{INSTALLTIME} %{INSTALLTIME:date} %{INSTALLTIME:day} %{INSTALLTIME:dec} %{INSTALLTIME:hex} %{INSTALLTIME:oct}\n' > /dev/null
	@${rpm} -qa --qf '%{INSTALLTIDUUID} %{INSTALLTIMEUUID} %{ORIGINTIDUUID} %{ORIGINTIMEUUID} %{BUILDTIMEUUID}\n' > /dev/null
	@${rpm} -qa --qf '%{DBINSTANCE} %{HDRID} %{PKGID} %{SOURCEPKGID}\n' > /dev/null
	@${rpm} -qa --qf '%{HDRUUID} %{PKGUUID} %{SOURCEPKGUUID}\n' > /dev/null
	@${rpm} -qa --qf '%{PACKAGESIZE:rpn(511,+,512,/)}\n' > /dev/null
	@${rpm} -qa --qf '%{PACKAGESTAT:base64}\n' > /dev/null
	@${rpm} -qa --qf '%{PACKAGESTAT:stat(atime,ctime,blksize,blocks,dev,gid,gname,ino,link,mode,mtime,nlink,rdev,size,uid,uname)}\n' > /dev/null
	@${rpm} -qa --qf '[%{providesqlentry}\n] [%{requiresqlentry}\n] [%{conflictsqlentry}\n] [%{obsoletesqlentry}\n] [%{filessqlentry1}\n] [%{filessqlentry2}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES:shescape}: %{FILEMTIMES:shescape} %{FILEFLAGS:perms} %{FILEMODES:fflags}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FILECLASS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILEPATHS}: %{FILECONTEXTS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FSCONTEXTS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{RECONTEXTS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FILEPROVIDE}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FILEREQUIRE}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FSNAMES}: %{FSSIZES}\n]' > /dev/null
# assertion failure
#	@${rpm} -qa --qf '[%{SUGGESTS}\n]' > /dev/null
	@${rpm} --rebuilddb
	-@${rpm} -Va > /dev/null
	-@${rpm} -Vp tmp/manifest > /dev/null
	@${rpm} -e probes
	-@${rpm} -e devtool-sanity
#	@${rpm} -e --justdb `${rpm} -qa` --allmatches

check-rpmv3:
	@echo "=== $@ ==="
	@${rpm} -qlp simplestRPMv3-1.0-2.aix5.3.noarch.rpm

check-verify:
	@echo "=== $@ ==="
	@${rpm} -e probes devtool-sanity
	@-${rpm} -U --justdb --nodeps tmp/manifest
	@-${rpm} -V probes devtool-sanity > /dev/null
	@-${rpm} -Va > /dev/null
#XXX	@-${rpm} -Vp tmp/manifest
	@-${rpm} -e --noscripts --notriggers --allmatches `${rpm} -qa | grep '^triggers-*'` > /dev/null 2>&1
	@${rpm} -e car engine wheel door tyre window glass
	@${rpm} -e battery bicycle electric-engine gasoline-engine
	@${rpm} -e solar-collector tire turbo
	@-${rpm} -e --justdb popt
	@${rpm} -e probes
	@${rpm} -e devtool-sanity
check-jbj:
	@${rpm} -qp ref/cAos.i386.manifest > /dev/null
	@-${rpm} -qp $(POPTURI)popt-1.14-1.i386.rpm > /dev/null
	@-${rpm} -qW $(POPTURI) > /dev/null 2>&1
	@-${rpm} -qW ftp://localhost/pub/i386-linux/RPMS > /dev/null 2>&1

check-local: check-init check-pubkeys check-markup check-macros \
	check-build check-sign check-install \
	check-query \
	check-triggers # check-jbj

clean-local:
	rm -f genpgp.h
	$(sudo) rm -rf $(caosroot) $(centosroot) $(suseroot) $(mandrivaroot) $(caixaroot) $(idmsroot) $(unityroot) tmp/repackage
	rm -rf tmp $(BUILD_DIRS)
	rm -rf repodata/
