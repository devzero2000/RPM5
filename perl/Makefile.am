# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = 

noinst_DATA = .made_mod

.made_mod: Makefile.perl
	$(MAKE) -fMakefile.perl
	touch $@

Makefile.perl: Makefile.PL
	$(PERL) $< MAKEFILE=$@

install-data-local:
	$(MAKE) -fMakefile.perl PREFIX=$(prefix) install

CLEANFILES = *~
clean-local:
	if test -f Makefile.perl; them $(MAKE) -fMakefile.perl realclean; fi
	rm -f Makefile.perl Makefile.perl.old
