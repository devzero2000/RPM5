#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/messages.h"
#include "install.h"
#include "intl.h"
#include "query.h"
#include "rpmlib.h"
#include "url.h"
#include "verify.h"

static int verifyHeader(char * prefix, Header h, int verifyFlags);
static int verifyMatches(char * prefix, rpmdb db, dbiIndexSet matches,
			  int verifyFlags);
static int verifyDependencies(rpmdb db, Header h);

static int verifyHeader(char * prefix, Header h, int verifyFlags) {
    char ** fileList;
    int count, type;
    int verifyResult;
    int i, ec, rc;
    int_32 * fileFlagsList;
    int omitMask = 0;

    ec = 0;
    if (!(verifyFlags & VERIFY_MD5)) omitMask = RPMVERIFY_MD5;

    headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlagsList, NULL);

    if (headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
			&count)) {
	for (i = 0; i < count; i++) {
	    if ((rc = rpmVerifyFile(prefix, h, i, &verifyResult, omitMask)) != 0) {
		fprintf(stdout, _("missing    %s\n"), fileList[i]);
	    } else {
		const char * size, * md5, * link, * mtime, * mode;
		const char * group, * user, * rdev;
		static const char * aok = ".";
		static const char * unknown = "?";

		if (!verifyResult) continue;
	    
		rc = 1;

#define	_verify(_RPMVERIFY_F, _C)	\
	((verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifylink(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READLINKFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifyfile(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
	
		md5 = _verifyfile(RPMVERIFY_MD5, "5");
		size = _verify(RPMVERIFY_FILESIZE, "S");
		link = _verifylink(RPMVERIFY_LINKTO, "L");
		mtime = _verify(RPMVERIFY_MTIME, "T");
		rdev = _verify(RPMVERIFY_RDEV, "D");
		user = _verify(RPMVERIFY_USER, "U");
		group = _verify(RPMVERIFY_GROUP, "G");
		mode = _verify(RPMVERIFY_MODE, "M");

#undef _verify
#undef _verifylink
#undef _verifyfile

		fprintf(stdout, "%s%s%s%s%s%s%s%s %c %s\n",
		       size, mode, md5, rdev, link, user, group, mtime, 
		       fileFlagsList[i] & RPMFILE_CONFIG ? 'c' : ' ', 
		       fileList[i]);
	    }
	    if (rc)
		ec = rc;
	}
	
	free(fileList);
    }
    return ec;
}

static int verifyDependencies(rpmdb db, Header h) {
    rpmDependencies rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    char * name, * version, * release;
    int type, count, i;

    rpmdep = rpmdepDependencies(db);
    rpmdepAddPackage(rpmdep, h, NULL);

    rpmdepCheck(rpmdep, &conflicts, &numConflicts);
    rpmdepDone(rpmdep);

    if (numConflicts) {
	headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
	headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
	headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);
	fprintf(stdout, _("Unsatisfied dependencies for %s-%s-%s: "),
		name, version, release);
	for (i = 0; i < numConflicts; i++) {
	    if (i) fprintf(stdout, ", ");
	    fprintf(stdout, "%s", conflicts[i].needsName);
	    if (conflicts[i].needsFlags) {
		printDepFlags(stdout, conflicts[i].needsVersion, 
			      conflicts[i].needsFlags);
	    }
	}
	fprintf(stdout, "\n");
	rpmdepFreeConflicts(conflicts, numConflicts);
	return 1;
    }
    return 0;
}

static int verifyPackage(char * root, rpmdb db, Header h, int verifyFlags) {
    int ec, rc;
    ec = 0;
    if ((verifyFlags & VERIFY_DEPS) &&
	(rc = verifyDependencies(db, h)) != 0)
	    ec = rc;
    if ((verifyFlags & VERIFY_FILES) &&
	(rc = verifyHeader(root, h, verifyFlags)) != 0)
	    ec = rc;;
    if ((verifyFlags & VERIFY_SCRIPT) &&
	(rc = rpmVerifyScript(root, h, 1)) != 0)
	    ec = rc;
    return ec;
}

static int verifyMatches(char * prefix, rpmdb db, dbiIndexSet matches,
			  int verifyFlags) {
    int ec, rc;
    int i;
    Header h;

    ec = 0;
    for (i = 0; i < matches.count; i++) {
	if (matches.recs[i].recOffset == 0)
	    continue;
	rpmMessage(RPMMESS_DEBUG, _("verifying record number %d\n"),
		matches.recs[i].recOffset);
	    
	h = rpmdbGetRecord(db, matches.recs[i].recOffset);
	if (!h) {
		fprintf(stderr, _("error: could not read database record\n"));
		ec = 1;
	} else {
		if ((rc = verifyPackage(prefix, db, h, verifyFlags)) != 0)
		    ec = rc;
		headerFree(h);
	}
    }
    return ec;
}

int doVerify(char * prefix, enum verifysources source, char ** argv,
	      int verifyFlags) {
    Header h;
    int offset;
    int fd;
    int ec, rc;
    int isSource;
    rpmdb db;
    dbiIndexSet matches;
    char * arg;
    void *context;
    char path[PATH_MAX];

    ec = 0;
    if (source == VERIFY_RPM && !(verifyFlags & VERIFY_DEPS)) {
	db = NULL;
    } else {
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
	    return 1;	/* XXX was exit(1) */
	}
    }

    if (source == VERIFY_EVERY) {
	for (offset = rpmdbFirstRecNum(db);
	     offset != 0;
	     offset = rpmdbNextRecNum(db, offset)) {
		h = rpmdbGetRecord(db, offset);
		if (!h) {
		    fprintf(stderr, _("could not read database record!\n"));
		    return 1;	/* XXX was exit(1) */
		}
		if ((rc = verifyPackage(prefix, db, h, verifyFlags)) != 0)
		    ec = rc;
		headerFree(h);
	}
    } else {
	while (*argv) {
	    arg = *argv++;

	    context = NULL;
	    rc = 0;
	    switch (source) {
	      case VERIFY_RPM:
		if (urlIsURL(arg)) {
		    if ((fd = urlGetFd(arg, &context)) < 0) {
			fprintf(stderr, _("open of %s failed: %s\n"), arg, 
				ftpStrerror(fd));
		    }
		} else if (!strcmp(arg, "-")) {
		    fd = 0;
		} else {
		    if ((fd = open(arg, O_RDONLY)) < 0) {
			fprintf(stderr, _("open of %s failed: %s\n"), arg, 
				strerror(errno));
		    }
		}

		if (fd >= 0) {
		    rc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);

		    if (context != NULL)
			urlAbortFd(context, fd);
		    else
			close(fd);

		    switch (rc) {
			case 0:
			    rc = verifyPackage(prefix, db, h, verifyFlags);
			    headerFree(h);
			    break;
			case 1:
			    fprintf(stderr, _("%s is not an RPM\n"), arg);
		    }
		}
			
		break;

	      case VERIFY_GRP:
		if (rpmdbFindByGroup(db, arg, &matches)) {
		    fprintf(stderr, 
				_("group %s does not contain any packages\n"), 
				arg);
		} else {
		    rc = verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	      case VERIFY_PATH:
	      if (*arg != '/') {
		/* Using realpath on the arg isn't correct if the arg is a symlink,
		 * especially if the symlink is a dangling link.  What we should
		 * instead do is use realpath() on `.' and then append arg to
		 * it.
		 */
	       if (realpath(".", path) != NULL) {
		if (path[strlen(path)] != '/') {
		    if (strncat(path, "/", sizeof(path) - strlen(path) - 1) == NULL) {
	    		fprintf(stderr, _("maximum path length exceeded\n"));
	    		return 1;
		    }
		}
		/* now append the original file name to the real path */
		if (strncat(path, arg, sizeof(path) - strlen(path) - 1) == NULL) {
	    	    fprintf(stderr, _("maximum path length exceeded\n"));
	    	    return 1;
		}
		arg = path;
	       }
	      }

		if (rpmdbFindByFile(db, arg, &matches)) {
		    fprintf(stderr, _("file %s is not owned by any package\n"), 
				arg);
		} else {
		    rc = verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	      case VERIFY_PACKAGE:
		rc = rpmdbFindByLabel(db, arg, &matches);
		if (rc == 1) 
		    fprintf(stderr, _("package %s is not installed\n"), arg);
		else if (rc == 2) {
		    fprintf(stderr, _("error looking for package %s\n"), arg);
		} else {
		    rc = verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	      case VERIFY_EVERY:
		break;
	    }
	    if (rc)
		ec = rc;
	}
    }
   
    if (source != VERIFY_RPM) {
	rpmdbClose(db);
    }
    return ec;
}
