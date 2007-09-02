# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = \
	RPM.xs

installprefix = $(DESTDIR)

configdir = ${prefix}/lib/rpm
config_SCRIPTS = 

all install clean: PMakefile
	make -f $< $@
	
PMakefile: Makefile.PL
	perl Makefile.PL
