## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST =
SUBDIRS =

macros =	$(abs_top_builddir)/macros:@testdir@/macros
rpm =		$(abs_top_builddir)/rpm --macros $(macros)
rpm2cpio =	$(abs_top_builddir)/tools/rpm2cpio
cpio =		cpio

%.spec: $(wildcard %-*.src.rpm)
	@${rpm2cpio} $(wildcard $*-*.src.rpm) | ${cpio} -i --quiet $@

%.src.rpm: $(wildcard *.spec)
	@${rpm} -bs --nodeps $^

BUILD_DIRS = $(shell ${rpm} -qp --qf '%{NAME} ' --nosignature $(wildcard *.src.rpm))

$(BUILD_DIRS): $(wildcard %-*.src.rpm)
	${rpm} -i --nosignature --nodeps $@-*.src.rpm
	(cd $@ && ${rpm} -bb --nodeps $@.spec)

check-local: $(BUILD_DIRS)
	@${rpm} -v --version > /dev/null
	@${rpm} -v --showrc > /dev/null
	@-rm -rf rpm
	@${rpm} --initdb
	@${rpm} --import $(abs_top_builddir)/pubkeys/JBJ-GPG-KEY
	@${rpm} -qW . > /dev/null 2>&1
	@${rpm} -Kv *.src.rpm */*.rpm > /dev/null
	@${rpm} -Kv --usecrypto beecrypt *.src.rpm */*.rpm > /dev/null
	@${rpm} -U --justdb */*.rpm
	@${rpm} -qa > /dev/null
	@${rpm} -qal > /dev/null
	@@${rpm} -qa --qf '[%{*:xml}\n]' > /dev/null
	@${rpm} --rebuilddb
	@${rpm} -qa --qf '[%{*:yaml}\n]' > /dev/null
	@${rpm} -Va
	@${rpm} -e --justdb `${rpm} -qa`
	@-${rpm} -qp http://rpm5.org/files/popt/popt-1.14-1.i386.rpm > /dev/null
	@-${rpm} -qW http://rpm5.org/files/popt/ > /dev/null 2>&1
	@-${rpm} -qW ftp://localhost/pub/i386-linux/RPMS > /dev/null 2>&1

clean-local:
	rm -rf rpm tmp $(BUILD_DIRS) $(patsubst %,%.spec,$(BUILD_DIRS))
