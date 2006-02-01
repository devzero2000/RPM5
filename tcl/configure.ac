#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.58)

AC_INIT(RPM for TCL, 0.4.4, david.hagood@aeroflex.com,RPM_for_TCL)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([include/config.h])
AC_LANG_CPLUSPLUS

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
AC_CHECK_LIB(rpm,rpmReadConfigFiles,,AC_MSG_ERROR([cannot find RPM library]))

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([tcl.h],,AC_MSG_ERROR([Need tcl.h to build TCL extensions]))
AC_CHECK_HEADERS([rpm/rpmlib.h],,AC_MSG_ERROR([Need rpmlib.h to access the RPM functions]))

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

AC_CONFIG_FILES([Makefile objs/Makefile rpm_for_tcl.spec src/pkgIndex.tcl include/version.hpp ])

AC_OUTPUT
