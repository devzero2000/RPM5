SUBDIRS = objs
EXTRA_DIST= \
	bootstrap \
	include/*.hpp.in \
	include/*.hpp \
	rpm_for_tcl.spec \
	rpm_for_tcl.spec.in \
	src/*.tcl

@PACKAGE_TARNAME@-@PACKAGE_VERSION@.tar.gz: dist

.PHONY: rpm
rpm: @PACKAGE_TARNAME@-@PACKAGE_VERSION@.tar.gz
	rpmbuild --target i586 -tb $<
