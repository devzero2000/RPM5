# $Id$

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/misc

bin_PROGRAMS = rpmconstant
lib_LTLIBRARIES     = librpmconstant.la
librpmconstant_la_LDFLAGS = -release $(LT_CURRENT).$(LT_REVISION)
librpmconstant_la_SOURCES = rpmconstant.c \
                      	    rpmconstant.h \
						    rpmconstanttbl.c \
							rpmconstant_internal.c \
							rpmh2tbl

rpmconstant_SOURCES = constant.c
rpmconstant_LDADD   = librpmconstant.la
pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmconstant.h

BUILT_SOURCES = rpmconstanttbl.c
CLEANFILES = rpmconstanttbl.c

rpmconstanttbl.c: Makefile.am rpmh2tbl
	$(srcdir)/rpmh2tbl $(top_srcdir) $(top_srcdir)/{lib,build,rpmdb,rpmio,misc}/*.h > $@

rpmh2tbl:

