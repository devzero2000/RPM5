#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include "miscfn.h"
#include "install.h"
#include "intl.h"
#include "lib/rpmlib.h"
#include "messages.h"
#include "query.h"
#include "url.h"

static int hashesPrinted = 0;

static void printHash(const unsigned long amount, const unsigned long total);
static void printPercent(const unsigned long amount, const unsigned long total);
static void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts);

static void printHash(const unsigned long amount, const unsigned long total) {
    int hashesNeeded;

    if (hashesPrinted != 50) {
	hashesNeeded = 50 * (total ? (((float) amount) / total) : 1);
	while (hashesNeeded > hashesPrinted) {
	    fprintf(stdout, "#");
	    hashesPrinted++;
	}
	fflush(stdout);
	hashesPrinted = hashesNeeded;

	if (hashesPrinted == 50)
	    fprintf(stdout, "\n");
    }
}

static void printPercent(const unsigned long amount, const unsigned long total) 
{
    fprintf(stdout, "%%%% %f\n", (total
                        ? ((float) ((((float) amount) / total) * 100))
                        : 100.0));
    fflush(stdout);
}

static int installPackages(const char * rootdir, const char ** packages, 
			    int numPackages, int installFlags, 
			    int interfaceFlags, rpmdb db,
			    struct rpmRelocation * relocations) {
    int i, fd;
    int numFailed = 0;
    const char ** filename;
    const char * printFormat = NULL;
    const char * chptr;
    int rc;
    rpmNotifyFunction fn;

    if (interfaceFlags & INSTALL_PERCENT)
	fn = printPercent;
    else if (interfaceFlags & INSTALL_HASH)
	fn = printHash;
    else
	fn = NULL;

    for (i = 0, filename = packages; i < numPackages; i++, filename++) {
	if (!*filename) continue;

	hashesPrinted = 0;

	fd = open(*filename, O_RDONLY);
	if (fd < 0) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open file %s\n"), *filename);
	    numFailed++;
	    *filename = NULL;
	    continue;
	} 

	if (interfaceFlags & INSTALL_PERCENT) 
	    printFormat = "%%f %s:%s:%s\n";
	else if (rpmIsVerbose() && (interfaceFlags & INSTALL_HASH)) {
	    chptr = strrchr(*filename, '/');
	    if (!chptr)
		chptr = *filename;
	    else
		chptr++;

	    printFormat = "%-28s";
	} else if (rpmIsVerbose())
	    fprintf(stdout, _("Installing %s\n"), *filename);

	if (db) {
	    rc = rpmInstallPackage(rootdir, db, fd, relocations, installFlags, 
				   fn, printFormat);
	} else {
	    if (installFlags &= RPMINSTALL_TEST) {
		rpmMessage(RPMMESS_DEBUG, _("stopping source install as we're "
			"just testing\n"));
		rc = 0;
	    } else {
		rc = rpmInstallSourcePackage(rootdir, fd, NULL, fn,
					     printFormat, NULL);
	    }
	} 

	switch (rc) {
	case 1:
	    rpmMessage(RPMMESS_ERROR, 
		    _("%s does not appear to be a RPM package\n"), 
		    *filename);
	    /* fall thru */
	default:
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *filename);
	    numFailed++;
	    break;
	case 0:
	    break;
	}

	close(fd);
    }

    return numFailed;
}

int doInstall(const char * rootdir, const char ** argv, int installFlags, 
	      int interfaceFlags, struct rpmRelocation * relocations) {
    rpmdb db;
    int fd, i;
    int mode, rc;
    const char ** packages;
    const char ** tpkgs, ** spkgs, ** bpkgs;
    const char ** filename;
    int numPackages;
    int numTmpPackages = 0, numBinaryPackages = 0, numSourcePackages = 0;
    int numFailed = 0;
    Header * binaryHeaders;
    int isSource;
    int tmpnum = 0;
    rpmDependencies rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int stopInstall = 0;
    size_t nb;
    const char *tmppath = rpmGetVar(RPMVAR_TMPPATH);
    const char *myroot;

    if (installFlags & RPMINSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_CREAT;

    rpmMessage(RPMMESS_DEBUG, _("counting packages to install\n"));
    for (filename = argv, numPackages = 0; *filename; filename++, numPackages++)
	;

    rpmMessage(RPMMESS_DEBUG, _("found %d packages\n"), numPackages);

    nb = (numPackages + 1) * sizeof(char *);
    packages = alloca(nb);
    memset(packages, 0, nb);
    tpkgs = alloca(nb);
    memset(tpkgs, 0, nb);
    spkgs = alloca(nb);
    memset(spkgs, 0, nb);
    bpkgs = alloca(nb);
    memset(bpkgs, 0, nb);
    nb = (numPackages + 1) * sizeof(Header);
    binaryHeaders = alloca(nb);
    memset(binaryHeaders, 0, nb);

    myroot = rootdir;
    if (myroot[0] == '/' && myroot[1] == '\0' && tmppath[0] == '/')
	myroot = "";

    rpmMessage(RPMMESS_DEBUG, _("looking for packages to download\n"));
    for (filename = argv, i = 0; *filename; filename++) {

	if (urlIsURL(*filename)) {
	    char *tfn;

	    if (rpmIsVerbose())
		fprintf(stdout, _("Retrieving %s\n"), *filename);
	    
	    nb = strlen(myroot) + strlen(tmppath) +
		sizeof("/rpm-ftp-12345-12345.tmp");
	    tfn = malloc(nb);
	    sprintf(tfn, "%s%s/rpm-ftp-%.5u-%.5u.tmp", myroot, 
		    tmppath, tmpnum++, (int) getpid());
	    rpmMessage(RPMMESS_DEBUG, _(" ... as %s\n"), tfn);
	    rc = urlGetFile(*filename, tfn);
	    if (rc < 0) {
		rpmMessage(RPMMESS_ERROR, 
			_("skipping %s - transfer failed - %s\n"), 
			*filename, ftpStrerror(rc));
		numFailed++;
		packages[i] = NULL;
		free(tfn);
	    } else {
		tpkgs[numTmpPackages++] = packages[i++] = tfn;
	    }
	} else {
	    packages[i++] = *filename;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("retrieved %d packages\n"), numTmpPackages);

    rpmMessage(RPMMESS_DEBUG, _("finding source and binary packages\n"));
    for (filename = packages, i = 0; *filename; filename++, i++) {
	fd = open(*filename, O_RDONLY);
	if (fd < 0) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open file %s\n"), *filename);
	    numFailed++;
	    packages[i] = NULL;
	    continue;
	}

	rc = rpmReadPackageHeader(fd, &binaryHeaders[numBinaryPackages], 
					&isSource, NULL, NULL);

	close(fd);
	
	switch (rc) {
	case 1:
	    rpmMessage(RPMMESS_ERROR, 
			_("%s does not appear to be a RPM package\n"), 
			*filename);
	    /* fall thru */
	default:
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *filename);
	    numFailed++;
	    packages[i] = NULL;
	    break;
	case 0:
	    if (isSource) {
	/* the header will be NULL if this is a v1 source package */
		if (binaryHeaders[numBinaryPackages])
		    headerFree(binaryHeaders[numBinaryPackages]);
		spkgs[numSourcePackages++] = *filename;
	    } else {
		bpkgs[numBinaryPackages++] = *filename;
	    }
	    break;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("found %d source and %d binary packages\n"), 
		numSourcePackages, numBinaryPackages);

    if (numBinaryPackages) {
	rpmMessage(RPMMESS_DEBUG, _("opening database mode: 0%o\n"), mode);
	if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open %s%s/packages.rpm\n"), 
			rootdir, rpmGetVar(RPMVAR_DBPATH));
	    exit(1);
	}

	rpmdep = rpmdepDependencies(db);
	for (i = 0; i < numBinaryPackages; i++)
	    if (installFlags & RPMINSTALL_UPGRADE)
		rpmdepUpgradePackage(rpmdep, binaryHeaders[i], bpkgs[i]);
	    else
		rpmdepAddPackage(rpmdep, binaryHeaders[i], bpkgs[i]);

	if (!(interfaceFlags & INSTALL_NODEPS)) {
	    if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
		numFailed = numPackages;
		stopInstall = 1;
	    }

	    if (!stopInstall && conflicts) {
		rpmMessage(RPMMESS_ERROR, _("failed dependencies:\n"));
		printDepProblems(stderr, conflicts, numConflicts);
		rpmdepFreeConflicts(conflicts, numConflicts);
		numFailed = numPackages;
		stopInstall = 1;
	    }
	}

	if (!(interfaceFlags & INSTALL_NOORDER)) {
	    if (rpmdepOrder(rpmdep, (void ***) &bpkgs)) {
		numFailed = numPackages;
		stopInstall = 1;
	    }
	}

	rpmdepDone(rpmdep);

	if (!stopInstall) {
	    rpmMessage(RPMMESS_DEBUG, _("installing binary rpms\n"));
	    numFailed += installPackages(rootdir, bpkgs, numBinaryPackages, 
				     installFlags, interfaceFlags, db,
				     relocations);
	}
	if (db) rpmdbClose(db);
    }

    if (numSourcePackages) {
	db = NULL;
	if (!stopInstall) {
	    rpmMessage(RPMMESS_DEBUG, _("installing source rpms\n"));
	    numFailed += installPackages(rootdir, spkgs, numSourcePackages, 
				     installFlags, interfaceFlags, db,
				     relocations);
	}
    }

    for (i = 0; i < numTmpPackages; i++) {
	unlink(tpkgs[i]);
	free((void *)tpkgs[i]);
    }

    for (i = 0; i < numBinaryPackages; i++) 
	headerFree(binaryHeaders[i]);

    return numFailed;
}

int doUninstall(const char * rootdir, const char ** argv, int uninstallFlags,
		 int interfaceFlags) {
    rpmdb db;
    dbiIndexSet matches;
    int i, j;
    int mode;
    int rc;
    int count;
    int numPackages, packageOffsetsAlloced;
    int * packageOffsets;
    const char ** arg;
    int numFailed = 0;
    rpmDependencies rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int stopUninstall = 0;

    rpmMessage(RPMMESS_DEBUG, _("counting packages to uninstall\n"));
    for (arg = argv, numPackages = 0; *arg; arg++, numPackages++)
	;

    packageOffsetsAlloced = numPackages;
    packageOffsets = malloc(sizeof(int *) * packageOffsetsAlloced);

    if (uninstallFlags & RPMUNINSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	rpmMessage(RPMMESS_ERROR, _("cannot open %s%s/packages.rpm\n"), 
		rootdir, rpmGetVar(RPMVAR_DBPATH));
	exit(1);
    }

    j = 0;
    numPackages = 0;
    for (arg = argv; *arg; arg++) {
	rc = rpmdbFindByLabel(db, *arg, &matches);
	if (rc == 1) {
	    rpmMessage(RPMMESS_ERROR, _("package %s is not installed\n"), *arg);
	    numFailed++;
	} else if (rc == 2) {
	    rpmMessage(RPMMESS_ERROR, _("error searching for package %s\n"), *arg);
	    numFailed++;
	} else {
	    count = 0;
	    for (i = 0; i < matches.count; i++)
		if (matches.recs[i].recOffset) count++;

	    if (count > 1 && !(interfaceFlags & UNINSTALL_ALLMATCHES)) {
		rpmMessage(RPMMESS_ERROR, _("\"%s\" specifies multiple packages\n"), 
			*arg);
		numFailed++;
	    }
	    else { 
		numPackages += count;
		if (numPackages > packageOffsetsAlloced) {
		    packageOffsetsAlloced = numPackages + 5;
		    packageOffsets = realloc(packageOffsets, 
				sizeof(int *) * packageOffsetsAlloced);
		}
		for (i = 0; i < matches.count; i++) {
		    if (matches.recs[i].recOffset) {
			packageOffsets[j++] = matches.recs[i].recOffset;
		    }
		}
	    }

	    dbiFreeIndexRecord(matches);
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("found %d packages to uninstall\n"), numPackages);

    if (!(interfaceFlags & UNINSTALL_NODEPS)) {
	rpmdep = rpmdepDependencies(db);
	for (i = 0; i < numPackages; i++)
	    rpmdepRemovePackage(rpmdep, packageOffsets[i]);

	if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
	    numFailed = numPackages;
	    stopUninstall = 1;
	}

	rpmdepDone(rpmdep);

	if (!stopUninstall && conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("removing these packages would break "
			      "dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    rpmdepFreeConflicts(conflicts, numConflicts);
	    numFailed += numPackages;
	    stopUninstall = 1;
	}
    }

    if (!stopUninstall) {
	for (i = 0; i < numPackages; i++) {
	    rpmMessage(RPMMESS_DEBUG, _("uninstalling record number %d\n"),
			packageOffsets[i]);
	    if (rpmRemovePackage(rootdir, db, packageOffsets[i], 
				 uninstallFlags))
		numFailed++;
	}
    }

    rpmdbClose(db);

    free(packageOffsets);

    return numFailed;
}

int doSourceInstall(const char * rootdir, const char * arg, char ** specFile,
		    char ** cookie) {
    int fd;
    int rc;

    fd = open(arg, O_RDONLY);
    if (fd < 0) {
	rpmMessage(RPMMESS_ERROR, _("cannot open %s\n"), arg);
	return 1;
    }

    if (rpmIsVerbose())
	fprintf(stdout, _("Installing %s\n"), arg);

    rc = rpmInstallSourcePackage(rootdir, fd, specFile, NULL, NULL, cookie);
    if (rc == 1) {
	rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), arg);
	if (specFile) free(*specFile);
	if (cookie) free(*cookie);
    }

    close(fd);

    return rc;
}

void printDepFlags(FILE * f, char * version, int flags) {
    if (flags)
	fprintf(f, " ");

    if (flags & RPMSENSE_LESS) 
	fprintf(f, "<");
    if (flags & RPMSENSE_GREATER)
	fprintf(f, ">");
    if (flags & RPMSENSE_EQUAL)
	fprintf(f, "=");
    if (flags & RPMSENSE_SERIAL)
	fprintf(f, "S");

    if (flags)
	fprintf(f, " %s", version);
}

static void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts) {
    int i;

    for (i = 0; i < numConflicts; i++) {
	fprintf(f, "\t%s", conflicts[i].needsName);
	if (conflicts[i].needsFlags) {
	    printDepFlags(stderr, conflicts[i].needsVersion, 
			  conflicts[i].needsFlags);
	}

	if (conflicts[i].sense == RPMDEP_SENSE_REQUIRES) 
	    fprintf(f, _(" is needed by %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
	else
	    fprintf(f, _(" conflicts with %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
    }
}
