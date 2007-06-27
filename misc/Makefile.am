# Makefile for misc library.

AUTOMAKE_OPTIONS = 1.4 foreign

AM_CPPFLAGS = -I. -I$(top_srcdir)

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
librpmmisc_la_LIBADD = @ALLOCA@
librpmmisc_la_DEPENDENCIES = $(librpmmisc_la_LIBADD)

