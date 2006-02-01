SUBDIRS = objs
EXTRA_DIST=rpm_for_tcl.spec rpm_for_tcl.spec.in bootstrap include/*hpp.in include/*hpp src/*tcl

.PHONY: rpm

@PACKAGE_TARNAME@-@PACKAGE_VERSION@.tar.gz: dist

rpm: @PACKAGE_TARNAME@-@PACKAGE_VERSION@.tar.gz
	rpmbuild --target i586 -tb $<
