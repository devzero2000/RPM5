## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST =	ref/*
SUBDIRS =

macros =	$(abs_top_builddir)/macros:$(testdir)/macros
rpm =		$(abs_top_builddir)/rpm --macros $(macros)
rpm2cpio =	$(abs_top_builddir)/tools/rpm2cpio
rpmdigest =	$(abs_top_builddir)/tools/rpmdigest
rpmmtree =	$(abs_top_builddir)/tools/rpmmtree
rpmrepo =	$(abs_top_builddir)/tools/rpmrepo
cpio =		cpio

.PHONY:	check-init
check-init: macros
	@mkdir -p tmp/hrmib/ tmp/repackage/
	@${rpm} -v --version	> tmp/version
	@diff -u {tmp,ref}/version
	@${rpm} -v --querytags	> tmp/querytags
	@diff -u {tmp,ref}/querytags
	@${rpm} -v --showrc | sed -e "s,$(abs_top_builddir),..,g" > tmp/showrc
	-@diff -u {tmp,ref}/showrc
	@${rpmdigest} --alldigests ref/[^C]* > tmp/.alldigests
	@diff -u {tmp,ref}/.alldigests
	@${rpmmtree} -c -p ref | ${rpmmtree} -p ref

POPTURI =	http://rpm5.org/files/popt/
POPTPKGS =	\
	popt-1.14-1.src.rpm \
	popt-1.14-1.i386.rpm

$(POPTPKGS):
	-wget $(POPTURI)$@

.PHONY:	check-pubkeys
check-pubkeys: $(POPTPKGS)
	@rm -rf tmp/rpmdb
	@${rpm} --import $(top_srcdir)/pubkeys/JBJ-GPG-KEY
#	-@${rpm} -Kv --usecrypto gcrypt $(POPTPKGS) > tmp/popt.Kv-gcrypt
	-@${rpm} -Kv --usecrypto ssl $(POPTPKGS) > tmp/popt.Kv-ssl
	-@${rpm} -Kv --usecrypto nss $(POPTPKGS) > tmp/popt.Kv-nss
	-@${rpm} -Kv --usecrypto beecrypt $(POPTPKGS) > tmp/popt.Kv-beecrypt
	@${rpm} -Kv $(POPTPKGS) > tmp/popt.Kv
	@diff -u {tmp,ref}/popt.Kv

.PHONY:	check-markup
check-markup: $(POPTPKGS)
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
	-@${rpmrepo} --uncompressed .
	@diff -u {repodata,ref}/primary.xml
	@diff -u {repodata,ref}/filelists.xml
	@diff -u {repodata,ref}/other.xml
	@-diff -u {repodata,ref}/repomd.xml > /dev/null 2>&1

%.spec: $(wildcard %-*.src.rpm)
	@${rpm2cpio} $(wildcard $*-*.src.rpm) | ${cpio} -i --quiet $@

%.src.rpm: $(wildcard *.spec)
	@${rpm} -bs --nodeps $^

BUILD_DIRS = $(shell ${rpm} -qp --qf '%{NAME} ' --nosignature $(wildcard *.src.rpm))

$(BUILD_DIRS): $(wildcard %-*.src.rpm)
	${rpm} -i --nosignature --nodeps $@-*.src.rpm
	(cd $@ && ${rpm} -q --specfile $@.spec && ${rpm} -q --specsrpm $@.spec && ${rpm} -q --specfile --specedit $@.spec && ${rpm} -bb --nodeps $@.spec) > /dev/null

check-build: $(BUILD_DIRS)
	@ls -1 */*.rpm > tmp/manifest
	-@diff -u {tmp,ref}/manifest

check-install:
	${rpm} -U --relocate /tmp/=$(testdir)/tmp/root/ --nodeps devtool-sanity/*.rpm

check-local: check-init check-pubkeys check-markup check-build check-install
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
#	@${rpm} -qa --qf '[%{SUGGESTS}\n]' > /dev/null
	@${rpm} --rebuilddb
	@-${rpm} -Va > /dev/null
	@-${rpm} -Vp tmp/manifest > /dev/null
	@${rpm} -e devtool-sanity
	@${rpm} -e --justdb `${rpm} -qa` --allmatches
	@-${rpm} -qp $(POPTURI)popt-1.14-1.i386.rpm > /dev/null
	@-${rpm} -qW $(POPTURI) > /dev/null 2>&1
	@-${rpm} -qW ftp://localhost/pub/i386-linux/RPMS > /dev/null 2>&1

clean-local:
	rm -rf tmp $(BUILD_DIRS) $(patsubst %,%.spec,$(BUILD_DIRS))
	rm -rf $(POPTPKGS) repodata/
