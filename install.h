#ifndef _H_INSTALL
#define _H_INSTALL

#include <stdio.h>

#include "lib/rpmlib.h"

#define INSTALL_PERCENT         (1 << 0)
#define INSTALL_HASH            (1 << 1)
#define INSTALL_NODEPS          (1 << 2)
#define INSTALL_NOORDER         (1 << 3)

#define UNINSTALL_NODEPS        (1 << 0)
#define UNINSTALL_ALLMATCHES    (1 << 1)

int doInstall(const char * rootdir, const char ** argv, int installFlags, 
	      int interfaceFlags, struct rpmRelocation * relocations);
int doSourceInstall(const char * rootdir, const char * arg, char ** specFile,
		    char ** cookie);
int doUninstall(const char * rootdir, const char ** argv, int uninstallFlags, 
		 int interfaceFlags);
void printDepFlags(FILE * f, char * version, int flags);

#endif

