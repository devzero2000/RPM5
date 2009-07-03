#include "system.h"

#include <rpmio.h>
#include <rpmdir.h>
#include <rpmdav.h>
#include <poptIO.h>

#include "debug.h"

static void printDir(struct dirent * dp, int nentry)
{
    if (rpmIsDebug()) {
	unsigned d_off = 0;
#if !(defined(hpux) || defined(__hpux) || defined(sun) || defined(RPM_OS_AIX)) && \
    !defined(__APPLE__) && !defined(__FreeBSD_kernel__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__DragonFly__)
	d_off = (unsigned) dp->d_off,
#endif
	fprintf(stderr, "%5d (0x%08x,0x%08x) 0x%04x ", nentry,
		(unsigned) dp->d_ino, d_off, (unsigned) dp->d_reclen);
    }
    if (rpmIsVerbose()) {
	if (!rpmIsDebug())
	    fprintf(stderr, "\t");
	fprintf(stderr, "%s%s\n", dp->d_name,
	    (dp->d_type == 0x04 ? "/" : ""));
    }
}

static int dirWalk(const char * dn)
{
    rpmop op = memset(alloca(sizeof(*op)), 0, sizeof(*op));
    struct dirent * dp;
    DIR * dir;
    int nentries;
    int rc = 1;
    int xx;

    xx = rpmswEnter(op, 0);
    nentries = 0;
    if ((dir = Opendir(dn)) == NULL)
	goto exit;
    while ((dp = Readdir(dir)) != NULL)
	printDir(dp, nentries++);
    rc = Closedir(dir);

exit:
    xx = rpmswExit(op, nentries);

fprintf(stderr, "===== %s: %d entries\n", dn, nentries);
    if (_rpmsw_stats)
	rpmswPrint("opendir:", op, NULL);
    return rc;
}

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    ARGV_t av = NULL;
    int ac;
    const char * dn;
    int rc = 0;

    if (__debug) {
_av_debug = -1;
_dav_debug = -1;
_ftp_debug = -1;
_url_debug = -1;
_rpmio_debug = -1;
    }

    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    while (rc == 0 && (dn = *av++) != NULL) {
#ifdef	DYING	/* XXX davOpendir() is adding pesky trailing '/'. */
	/* XXX Add pesky trailing '/' to http:// URI's */
	size_t nb = strlen(dn);
	dn = rpmExpand(dn, (dn[nb-1] != '/' ? "/" : NULL), NULL);
	rc = dirWalk(dn);
	dn = _free(dn);
#else
	rc = dirWalk(dn);
#endif
    }

exit:

    optCon = rpmioFini(optCon);

    return rc;
}
