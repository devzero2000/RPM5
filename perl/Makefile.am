# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = \
	MANIFEST \
	typemap \
	Makefile.PL.in \
	rpmxs.c rpmxs.h \
	RPM.xs RPM.pm t/01.rpm.t \
	RPM/Constant.pm RPM_Constant.xs t/02.rpmconstant.t \
	RPM_Header.xs RPM/Header.pm t/03.header.t \
	RPM_Ts.xs \
	t/00.pod.coverage.t t/00.pod.t \
	hdlist-test.hdr test-rpm-1.0-1.noarch.rpm test-rpm-1.0-1.src.rpm

noinst_DATA = .made_mod

.made_mod: Makefile.perl $(EXTRA_DIST)
	$(MAKE) -f Makefile.perl || $(MAKE) -f Makefile.perl

test: .made_mod
	$(MAKE) -f Makefile.perl test

MANIFEST: Makefile
	echo -n > MANIFEST
	for i in $(EXTRA_DIST); do \
		echo $$i >> MANIFEST; \
	done

Makefile.perl: Makefile.PL
	$(PERL) Makefile.PL MAKEFILE=$@

install-data-local:
	$(MAKE) -f Makefile.perl PREFIX=$(prefix) install || \
	$(MAKE) -f Makefile.perl PREFIX=$(prefix) install

CLEANFILES = *~

.PHONY: test

clean-local:
	if [ -f Makefile.perl ]; then $(MAKE) -f Makefile.perl realclean; fi
	rm -f Makefile.perl Makefile.perl.old

