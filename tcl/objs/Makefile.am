pkglib_PROGRAMS =  rpm.so
pkglib_SCRIPTS  = ../src/pkgIndex.tcl ../src/rpm.tcl
rpm_so_LDFLAGS = -shared -export-dynamic
EXTRA_DIST = ../src/*.tcl ../src/*.tcl.in
rpm_so_SOURCES = ../src/header_obj.cpp ../src/rpm_for_tcl.cpp ../src/transaction_set.cpp ../src/cmd_base.cpp ../src/problem.cpp ../src/dependency.cpp
AM_CFLAGS=-Wall -Werror
AM_CXXFLAGS=-Wall -Werror
