# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = \
	MANIFEST \
	Makefile.PL.in \
	RPM.xs RPM.pm t/01.rpm.t\
	RPM_Header.xs RPM_Ts.xs \
	RPM/Constant.pm RPM_Constant.xs t/02.rpmconstant.t

noinst_DATA = .made_mod

.made_mod: Makefile.perl $(EXTRA_DIST)
	$(MAKE) -f Makefile.perl || $(MAKE) -f Makefile.perl

test:
	$(MAKE) -f Makefile.perl test || $(MAKE) -f Makefile.perl test

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

