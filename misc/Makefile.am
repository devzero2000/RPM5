# Makefile for misc library.

AUTOMAKE_OPTIONS = 1.4 foreign

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir)

EXTRA_DIST = \
	alloca.c	basename.c	err.c		error.c		\
	fakefork.c	fnmatch.c	getcwd.c	getmntent.c	\
	getwd.c		glob.c				memcmp.c	\
	mktime.c	myrealloc.c	putenv.c	realpath.c	\
	setenv.c	stpcpy.c	stpncpy.c			\
			strcspn.c	strdup.c	strerror.c	\
	strftime.c	strcspn.c	strstr.c	strtol.c	\
	strtoul.c

noinst_HEADERS = \
	err.h		error.h		fnmatch.h	glob.h		

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmmisc.la

librpmmisc_la_SOURCES = librpmmisc.c
librpmmisc_la_LDFLAGS = -release $(LT_CURRENT).$(LT_REVISION)
if HAVE_LD_VERSION_SCRIPT
librpmmisc_la_LDFLAGS += -Wl,--version-script=$(srcdir)/librpmmisc.vers
endif
librpmmisc_la_LIBADD = \
	@ALLOCA@ \
	@WITH_DB_LDFLAGS@   @WITH_DB_LIBS@ \
	@WITH_ZLIB_LDFLAGS@ @WITH_ZLIB_LIBS@ \
	@WITH_LUA_LDFLAGS@  @WITH_LUA_LIBS@ \
	@WITH_FILE_LDFLAGS@ @WITH_FILE_LIBS@
librpmmisc_la_DEPENDENCIES = \
	@ALLOCA@
librpmmisc.la: $(librpmmisc_la_OBJECTS) $(librpmmisc_la_DEPENDENCIES) 
	$(librpmmisc_la_LINK) -rpath $(usrlibdir) $(librpmmisc_la_OBJECTS) $(librpmmisc_la_LIBADD)

