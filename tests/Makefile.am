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

macros =	$(abs_top_builddir)/macros/macros:$(testdir)/macros
#XXX rpmpopt =	$(abs_top_builddir)/rpmpopt

rpm =		$(abs_top_builddir)/rpm --macros $(macros)
rpm2cpio =	$(abs_top_builddir)/tools/rpm2cpio
rpmcache =	$(abs_top_builddir)/tools/rpmcache --macros $(macros)
rpmdigest =	$(abs_top_builddir)/tools/rpmdigest
rpmrepo =	$(abs_top_builddir)/tools/rpmrepo

tpgp =		./tpgp --macros $(macros)

chroot =	$(abs_top_builddir)/tools/chroot
cp =		$(abs_top_builddir)/tools/cp
find =		$(abs_top_builddir)/tools/find
grep =		$(abs_top_builddir)/tools/grep
lua =		$(abs_top_builddir)/lua/lua
mtree =		$(abs_top_builddir)/tools/mtree
rc =		$(abs_top_builddir)/tools/rc
wget =		$(abs_top_builddir)/tools/wget

sudo =		sudo
caosroot =	$(abs_top_builddir)/tests/tmp/CAOS
centosroot =	$(abs_top_builddir)/tests/tmp/CENTOS

rpmdb =		$(abs_top_builddir)/tests/tmp/rpmdb
rpmdb_Tables = \
	$(rpmdb)/Arch	\
	$(rpmdb)/Basenames	\
	$(rpmdb)/Conflictname	\
	$(rpmdb)/Dirnames	\
	$(rpmdb)/Filedigests	\
	$(rpmdb)/Filepaths	\
	$(rpmdb)/Group	\
	$(rpmdb)/Installtid	\
	$(rpmdb)/Name	\
	$(rpmdb)/Nvra	\
	$(rpmdb)/Obsoletename	\
	$(rpmdb)/Os	\
	$(rpmdb)/Packagecolor	\
	$(rpmdb)/Providename	\
	$(rpmdb)/Pubkeys	\
	$(rpmdb)/Release	\
	$(rpmdb)/Requirename	\
	$(rpmdb)/Seqno	\
	$(rpmdb)/Sha1header	\
	$(rpmdb)/Sigmd5	\
	$(rpmdb)/Sourcepkgid	\
	$(rpmdb)/Triggername	\
	$(rpmdb)/Version

db_tool =	$(abs_top_builddir)/tools/db_tool
db_archive =	${db_tool} db_archive	-h $(rpmdb) -v
db_checkpoint =	${db_tool} db_checkpoint -h $(rpmdb) -v
db_dump =	${db_tool} db_dump	-h $(rpmdb)
db_load =	${db_tool} db_load	-h $(rpmdb)
db_printlog =	${db_tool} db_printlog	-h $(rpmdb)
db_recover =	${db_tool} db_recover	-h $(rpmdb) -v
db_stat =	${db_tool} db_stat	-h $(rpmdb)
db_upgrade =	${db_tool} db_upgrade	-h $(rpmdb) -v
db_verify =	${db_tool} db_verify	-h $(rpmdb)

cpio =		@__CPIO@
diff =		@__DIFF@
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
	@-diff -u tmp/version ref/version || cp tmp/version ref/version
	@${rpm} -v --querytags	> tmp/querytags
	@-diff -u tmp/querytags ref/querytags || cp tmp/querytags ref/querytags
	@${rpm} -v --showrc | sed -e "s,$(abs_top_builddir),..,g" > tmp/showrc
	@-diff -u tmp/showrc ref/showrc || cp tmp/showrc ref/showrc
	@${rpmdigest} --alldigests ref/[^C]* > tmp/.alldigests
	@-diff -u tmp/.alldigests ref/.alldigests || cp tmp/.alldigests ref/.alldigests
	@${mtree} -c -p ref | ${mtree} -p ref	# XXX mtime usecs need fixing

POPTURI =	http://rpm5.org/files/popt/
POPTPKGS =	\
	popt-1.14-1.src.rpm \
	popt-1.14-1.i386.rpm

$(POPTPKGS):
	${wget} $(POPTURI)$@

.PHONY:	check-pubkeys
check-pubkeys: tpgp $(POPTPKGS)
	@echo "=== $@ ==="
	@${rm} -rf tmp/rpmdb
	@${mkdir} -p tmp/rpmdb/log tmp/rpmdb/tmp
	@${cp} ref/DB_CONFIG tmp/rpmdb
	@${rpm} --import tmp/DSA.pubpem
	@${rpm} --import tmp/RSA.pubpem
	@echo -n "--> default:"
	@${tpgp}
	@echo -n "--> beecrypt:"
	@${tpgp} --usecrypto bc
	@echo "--> gcrypt(note: MPI padding is BROKEN atm):"
	@-${tpgp} --usecrypto gc
	@echo -n "--> nss:"
	@-${tpgp} --usecrypto nss
	@echo -n "--> openssl:"
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
	@${rpm} -D'_gpg_name Donald' --addsign --nopassword cudf-test/*.rpm > /dev/null
	@${rpm} -Kv cudf-test/*.rpm	> /dev/null
	@echo "--> RSA:"
	@${rpm} -D'_gpg_name Ronald' --addsign --nopassword edos-test/*.rpm > /dev/null
	@${rpm} -Kv edos-test/*.rpm	> /dev/null

.PHONY:	check-markup
check-markup: $(POPTPKGS)
	@echo "=== $@ ==="
	@${rpm} -qp --nosignature --qf '[%{*:yaml}\n]' popt*.rpm > tmp/popt.yaml
	@diff -u {tmp,ref}/popt.yaml
	@${rpm} -qp --nosignature --qf '[%{*:xml}\n]' popt*.rpm > tmp/popt.xml
	@diff -u {tmp,ref}/popt.xml

.PHONY:	check-macros
check-macros:
	@echo "=== $@ ==="
	@${rpm} -D 'foo bar' -E '%dump%trace%foo' > /dev/null 2>&1
	@${rpm} -D 'foo %foo' -E '%foo' > /dev/null 2>&1
	@${rpm} -D 'foo bing' -D 'foo bang' -D 'foo boom' -E '%{@foo}' > /dev/null 2>&1
	@${rpm}  -E '%(/bin/echo "-->       sh: Portable Shar!")'
	@-${rpm}  -E '%{lua:print("-->      lua: Hard Rocks!")}'
	@-${rpm}  -E '%{js:print("-->       js: Use GPSEE!")}'
	@-${rpm} -E '%{ruby:print "-->     ruby: Puppet Gems!"}'
	@-${rpm} -E '%{tcl:puts "-->      tcl: Porticus Ports!"}'
	@-${rpm} -E '%{squirrel:print("--> squirrel: Eat Nuts!")}'
	@-${rpm} -E '%{perl:print "-->     perl: Artistic Scribbles!"}'
	@-${rpm} -E '%{python:print "-->   python: Snake Eggs!",}'
	@-${rpm} -E '%{ficl:.( -->     ficl: Das OpenBoot!)}'

%.spec: $(wildcard %-*.src.rpm)
	@${rpm2cpio} $(wildcard $*-*.src.rpm) | ${cpio} -i --quiet $@

%.src.rpm: $(wildcard *.spec)
	@${rpm} -bs --nodeps $^

BUILD_DIRS = $(shell ${rpm} -qp --qf '%{NAME} ' --nosignature $(wildcard *.src.rpm))

$(BUILD_DIRS): $(wildcard %-*.src.rpm)
	@echo "=== build $@ ==="
	${rpm} -i --nosignature --nodeps $@-*.src.rpm
	(cd $@ && ${rpm} -q --specfile $@.spec && ${rpm} -q --specsrpm $@.spec && ${rpm} -q --specfile --specedit $@.spec && ${rpm} -bb --nodeps $@.spec || :) > /dev/null

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

check-ACID:
	@echo "=== $@ ==="
	@-${db_upgrade} Packages
	@-${db_checkpoint} -1	> /dev/null 2>&1
	@${rpm} -Uvh --stats ref/edos.manifest
	@-${db_dump} Packages | ${db_load} Packages
	@-${db_verify} Packages
	@-${db_stat} -CA	> /dev/null 2>&1
	@rm -f tmp/rpmdb/Nvra
	@${rpm} -q tyre > /dev/null 2>&1
	@[ -f tmp/rpmdb/Nvra ] || echo "NACK: Nvra not re-created!"
	@rm -f tmp/rpmdb/Conflictname
	@${rpm} -q --whatconflicts tyre > /dev/null 2>&1
	@[ -f tmp/rpmdb/Conflictname ] || echo "NACK: Conflictname not re-created!"
	@rm -f $(rpmdb_Tables)
	@-${db_recover} -e
	@${rpm} -e car engine wheel door tyre window glass
	@-${db_printlog} -r	> /dev/null 2>&1
	@-${db_checkpoint} -1	> /dev/null 2>&1
	@-${db_archive} -d

check-install:
	@echo "=== $@ ==="
	${rpm} -U --relocate /tmp/=$(testdir)/tmp/root/ --nodeps devtool-sanity/*.rpm
	${rpm} -U probes-test/probes-2*.rpm

check-cAos:
	@echo "=== $@ ==="
	$(sudo) rm -rf $(caosroot) tmp/repackage
	@rm -rf tmp/cAos-2 tmp/cAos-3
	@mkdir -p $(caosroot) tmp/cAos-2 tmp/cAos-3 tmp/repackage
	@cd tmp/cAos-2 && $(wget) -i ../../ref/cAos2-bash.i386.manifest
	$(sudo) $(rpm) -Uvh -r $(caosroot) tmp/cAos-2/*.rpm cAos2-stub/*.noarch.rpm
	@cd tmp/cAos-3 && $(wget) -i ../../ref/cAos3-bash.i386.manifest
	$(sudo) $(rpm) -Uvh -r $(caosroot) tmp/cAos-3/ncurses-libs-*.rpm
	-$(sudo) $(rpm) -Uvh -r $(caosroot) `echo tmp/cAos-3/*.rpm | sed -e 's,tmp/cAos-3/ncurses-libs[^ ]*,,'`

check-CentOS:
	@echo "=== $@ ==="
	$(sudo) rm -rf $(centosroot) tmp/repackage
	@rm -rf tmp/CentOS5.1 tmp/CentOS5.2
	@mkdir -p $(centosroot) tmp/CentOS5.1 tmp/CentOS5.2 tmp/repackage
	@cd tmp/CentOS5.1 && $(wget) -i ../../ref/CentOS5.1-bash.i386.manifest
	$(sudo) $(rpm) -Uvh -r $(centosroot) tmp/CentOS5.1/*.rpm
	@cd tmp/CentOS5.2 && $(wget) -i ../../ref/CentOS5.2-bash.i386.manifest
	-$(sudo) $(rpm) -Uvh -r $(centosroot) tmp/CentOS5.2/*.rpm

.PHONY:	check-triggers
check-triggers:
	@echo "=== $@ ==="
	@-${rpm} -e --noscripts --notriggers --allmatches '^triggers-.*' > /dev/null 2>&1
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
#XXX	-${rpm} -Uvv --justdb */*.rpm > /dev/null 2>&1
#XXX	-${rpm} -Uvh --justdb --nodeps */*.rpm > /dev/null 2>&1
#XXX	-${rpm} -i --justdb --nodeps */*.rpm > /dev/null 2>&1
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
	@${rpm} -qa --qf '%{DBINSTANCE} %{HDRID} %{PKGID} %{SOURCEPKGID}\n' > /dev/null
	@${rpm} -qa --qf '%{PACKAGESIZE:rpn(511,+,512,/)}\n' > /dev/null
	@${rpm} -qa --qf '%{PACKAGESTAT:base64}\n' > /dev/null
	@${rpm} -qa --qf '%{PACKAGESTAT:stat(atime,ctime,blksize,blocks,dev,gid,gname,ino,link,mode,mtime,nlink,rdev,size,uid,uname)}\n' > /dev/null
	@${rpm} -qa --qf '[%{SUGGESTS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{providesqlentry}\n] [%{requiresqlentry}\n] [%{conflictsqlentry}\n] [%{obsoletesqlentry}\n] [%{filessqlentry1}\n] [%{filessqlentry2}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES:shescape}: %{FILEMTIMES:shescape} %{FILEFLAGS:perms} %{FILEMODES:fflags}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FILECLASS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILEPATHS}: %{FILECONTEXTS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FSCONTEXTS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{RECONTEXTS}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FILEPROVIDE}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FILENAMES}: %{FILEREQUIRE}\n]' > /dev/null
	@${rpm} -qa --qf '[%{FSNAMES}: %{FSSIZES}\n]' > /dev/null

check-verify:
	@echo "=== $@ ==="
	@${rpm} -e probes devtool-sanity
	@-${rpm} -U --justdb --nodeps tmp/manifest
	@-${rpm} -V probes devtool-sanity > /dev/null
	@-${rpm} -Va > /dev/null
#XXX	@-${rpm} -Vp tmp/manifest
	@${rpm} -e --noscripts --notriggers --allmatches '^triggers-.*'
	@${rpm} -e car engine wheel door tyre window glass
	@${rpm} -e battery bicycle electric-engine gasoline-engine
	@${rpm} -e solar-collector tire turbo
	@-${rpm} -e --justdb popt
	@${rpm} -e probes
	@${rpm} -e devtool-sanity

check-tools:
	@echo "=== $@ ==="
	@${find} $(POPTURI) -type f -maxdepth 1 > tmp/find
	@mkdir -p tmp/wget && ${wget} -i tmp/find -P tmp/wget > /dev/null 2>&1
	@mkdir -p tmp/cp && ${cp} -R $(POPTURI) tmp/cp
	@diff -ru tmp/{wget,cp}
	@${grep} -i HREF $(POPTURI) > tmp/grep

check-local: check-init check-pubkeys check-markup check-macros \
	check-build check-sign check-ACID check-install \
	check-query check-verify \
	check-triggers # check-tools # check-repo

clean-local:
	rm -f genpgp.h
	$(sudo) rm -rf $(caosroot) $(centosroot) tmp/repackage
	rm -rf tmp $(BUILD_DIRS)
	rm -rf repodata/
