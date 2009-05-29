#include "system.h"
/*@unchecked@*/
extern const char * __progname;

#include <rpmio_internal.h>
#include <poptIO.h>
#include "debug.h"

typedef struct rpmdc_s * rpmdc;

#define _DFB(n) ((1 << (n)) | 0x40000000)
#define F_ISSET(_dc, _FLAG) ((_dc)->flags & ((RPMDC_FLAGS_##_FLAG) & ~0x40000000))

enum dcFlags_e {
    RPMDC_FLAGS_BINARY	= _DFB( 0),	/*!< -b,--binary ... */
    RPMDC_FLAGS_WARN	= _DFB( 1),	/*!< -w,--warn ... */
    RPMDC_FLAGS_STATUS	= _DFB( 2)	/*!<    --status ... */
};

struct rpmdc_s {
    enum dcFlags_e flags;
    rpmuint32_t algo;		/*!< default digest algorithm. */
    const char * digest;
    size_t digestlen;
    const char * fn;
    FD_t fd;
    ARGV_t manifests;		/*!< array of file manifests to verify. */
    ARGI_t algos;		/*!< array of file digest algorithms. */
    ARGV_t digests;		/*!< array of file digests. */
    ARGV_t paths;		/*!< array of file paths. */
    unsigned char buf[BUFSIZ];
    ssize_t nb;
    int ix;
    int nfails;
};

static struct rpmdc_s _dc;
static rpmdc dc = &_dc;

static struct rpmop_s dc_totalops;
static struct rpmop_s dc_readops;
static struct rpmop_s dc_digestops;

static int rpmdcPrintFile(rpmdc dc, int algo, const char * algoName)
{
    static int asAscii = 1;
    int rc = 0;

    fdFiniDigest(dc->fd, algo, &dc->digest, &dc->digestlen, asAscii);
assert(dc->digest != NULL);
    if (dc->manifests) {
	const char * msg = "OK";
	if ((rc = strcmp(dc->digest, dc->digests[dc->ix])) != 0) {
	    msg = "FAILED";
	    dc->nfails++;
	}
	if (rc || !F_ISSET(dc, STATUS))
	    fprintf(stdout, "%s: %s\n", dc->fn, msg);
    } else {
	if (!F_ISSET(dc, STATUS)) {
	    if (algoName) fprintf(stdout, "%s:", algoName);
	    fprintf(stdout, "%s %c%s\n", dc->digest,
		(F_ISSET(dc, BINARY) ? '*' : ' '), dc->fn);
	    fflush(stdout);
	}
	dc->digest = _free(dc->digest);
    }
    return rc;
}

static int rpmdcFiniFile(rpmdc dc)
{
    rpmuint32_t algo = (dc->manifests ? dc->algos->vals[dc->ix] : dc->algo);
    int rc = 0;
    int xx;

    switch (algo) {
    default:
	xx = rpmdcPrintFile(dc, algo, NULL);
	if (xx) rc = xx;
	break;
    case 256:		/* --all digests requested. */
      {	struct poptOption * opt = rpmioDigestPoptTable;
	for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
	    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
		continue;
	    if (opt->arg != (void *)&rpmioDigestHashAlgo)
		continue;
	    dc->algo = opt->val;
	    if (!(dc->algo > 0 && dc->algo < 256))
		continue;
	    xx = rpmdcPrintFile(dc, dc->algo, opt->longName);
	    if (xx) rc = xx;
	}
      }	break;
    }
    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
    Fclose(dc->fd);
    dc->fd = NULL;
    return rc;
}

static int rpmdcCalcFile(rpmdc dc)
{
    int rc = 0;

    do {
	dc->nb = Fread(dc->buf, sizeof(dc->buf[0]), sizeof(dc->buf), dc->fd);
	if (Ferror(dc->fd)) {
	    rc = 2;
	    break;
	}
    } while (dc->nb > 0);

    return rc;
}

static int rpmdcInitFile(rpmdc dc)
{
    rpmuint32_t algo = (dc->manifests ? dc->algos->vals[dc->ix] : dc->algo);
    int rc = 0;

    /* XXX Stat(2) to insure files only? */
    dc->fd = Fopen(dc->fn, "r.ufdio");
    if (dc->fd == NULL || Ferror(dc->fd)) {
	fprintf(stderr, _("open of %s failed: %s\n"), dc->fn, Fstrerror(dc->fd));
	if (dc->fd != NULL) Fclose(dc->fd);
	dc->fd = NULL;
	rc = 2;
	goto exit;
    }

    switch (dc->algo) {
    default:
	/* XXX TODO: instantiate verify digests for all identical paths. */
	fdInitDigest(dc->fd, algo, 0);
	break;
    case 256:		/* --all digests requested. */
      {	struct poptOption * opt = rpmioDigestPoptTable;
	for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
	    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
		continue;
	    if (opt->longName == NULL)
		continue;
	    if (!(opt->val > 0 && opt->val < 256))
		continue;
	    algo = opt->val;
	    fdInitDigest(dc->fd, algo, 0);
	}
      }	break;
    }

exit:
    return rc;
}

static int rpmdcLoadManifests(rpmdc dc)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies h_errno, fileSystem, internalState @*/
{
    int rc = -1;	/* assume failure */

    if (dc->manifests != NULL)	/* note rc=0 return with no files to load. */
    while ((dc->fn = *dc->manifests++) != NULL) {
	char buf[BUFSIZ];
	FILE *fp;

	if (strcmp(dc->fn, "-") == 0) {
	    dc->fd = NULL;
	    fp = stdin;
	} else {
	    /* XXX .fpio is needed because of fgets(3) usage. */
	    dc->fd = Fopen(dc->fn, "r.fpio");
	    if (dc->fd == NULL || Ferror(dc->fd) || (fp = fdGetFILE(dc->fd)) == NULL) {
		fprintf(stderr, _("%s: Failed to open %s: %s\n"),
				__progname, dc->fn, Fstrerror(dc->fd));
		if (dc->fd != NULL) (void) Fclose(dc->fd);
		dc->fd = NULL;
		fp = NULL;
		goto exit;
	    }
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
	    const char * dname, * digest, * path;
	    char *se = buf + (int)strlen(buf);
	    int algo;
	    int c, xx;

	    while (se > buf && xisspace((int)se[-1]))
		se--;
	    *se = '\0';

	    /* Skip blank lines */
	    if (buf[0] == '\0')	/*@innercontinue@*/ continue;
	    /* Skip comment lines */
	    if (buf[0] == '#')	/*@innercontinue@*/ continue;

	    dname = NULL; path = NULL;
	    for (digest = se = buf; (c = (int)*se) != 0; se++)
	    switch (c) {
	    default:
		/*@switchbreak@*/ break;
	    case ':':
		*se++ = '\0';
		dname = digest;
		digest = se;
		/*@switchbreak@*/ break;
	    case ' ':
		se[0] = '\0';	/* loop will terminate */
		if (se[1] == ' ' || se[1] == '*')
		    se[1] = '\0';
		path = se + 2;
		/*@switchbreak@*/ break;
	    }

	    /* Map name to algorithm number. */
	    if (dname) {
		struct poptOption * opt = rpmioDigestPoptTable;
		algo = -1;
		for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
		    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
			continue;
		    if (opt->longName == NULL)
			continue;
		    if (!(opt->val > 0 && opt->val < 256))
			continue;
		    if (strcmp(opt->longName, dname))
			continue;
		    algo = opt->val;
		    break;
		}
		if (algo == -1) {
		    fprintf(stderr, _("%s: Unknown digest name \"%s\"\n"),
				__progname, dname);
		    rc = 2;
		    goto exit;
		}
	    } else
		algo = dc->algo;

	    /* Save {algo, digest, path} for processing. */
	    xx = argiAdd(&dc->algos, -1, algo);
	    xx = argvAdd(&dc->digests, digest);
	    xx = argvAdd(&dc->paths, path);
	}

	if (dc->fd != NULL) {
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	}
    }
    rc = 0;

exit:
    return rc;
}

static struct poptOption optionsTable[] = {

  { "binary", 'b', POPT_BIT_SET,	&_dc.flags, RPMDC_FLAGS_BINARY,
	N_("read in binary mode"), NULL },

  { "check", 'c', POPT_ARG_ARGV,	&_dc.manifests, 0,
	N_("read digests from MANIFEST file and verify (may be used more than once)"),
	N_("MANIFEST") },

  { "text", 't', POPT_BIT_CLR,		&_dc.flags, RPMDC_FLAGS_BINARY,
	N_("read in text mode (default)"), NULL },

#ifdef	NOTYET		/* XXX todo for popt-1.15 */
  { NULL, -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        N_("\
The following two options are useful only when verifying digests:\
"), NULL },
#endif

  { "status", '\0', POPT_BIT_SET,	&_dc.flags, RPMDC_FLAGS_STATUS,
	N_("no output when verifying"), NULL },
  { "warn", 'w', POPT_BIT_SET,		&_dc.flags, RPMDC_FLAGS_WARN,
	N_("warn about improperly formatted checksum lines"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioDigestPoptTable, 0,
	N_("Available digests:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        N_("\
When checking, the input should be a former output of this program.  The\n\
default mode is to print a line with digest, a character indicating type\n\
(`*' for binary, ` ' for text), and name for each FILE.\n\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    ARGV_t av;
    int ac;
    int rc = 0;
    int xx;

    rpmswEnter(&dc_totalops, -1);

    if ((int)rpmioDigestHashAlgo < 0)
	rpmioDigestHashAlgo = PGPHASHALGO_MD5;

    dc->algo = rpmioDigestHashAlgo;

    av = poptGetArgs(optCon);
    ac = argvCount(av);

    if (dc->manifests != NULL) {
	if (ac != 0) {
	    poptPrintUsage(optCon, stderr, 0);
	    rc = 2;
	    goto exit;
	}
	xx = rpmdcLoadManifests(dc);
	av = dc->paths;
    }
    dc->ix = 0;

    if (av != NULL)
    while ((dc->fn = *av++) != NULL) {
	/* XXX TODO: instantiate verify digests for all identical paths. */
	if ((xx = rpmdcInitFile(dc)) != 0) {
	    rc = xx;
	} else {
	    if ((xx = rpmdcCalcFile(dc)) != 0)
		rc = xx;
	    if ((xx = rpmdcFiniFile(dc)) != 0)
		rc = xx;
	}
	dc->ix++;
    }

exit:
    if (dc->nfails)
	fprintf(stderr, "%s: WARNING: %d of %d computed checksums did NOT match\n",
		__progname, dc->nfails, dc->ix);

#ifdef	NOTYET
    dc->manifests = argvFree(dc->manifests);
#endif
    dc->algos = argiFree(dc->algos);
    dc->digests = argvFree(dc->digests);
    dc->paths = argvFree(dc->paths);

    rpmswExit(&dc_totalops, 0);
    if (_rpmsw_stats) {
	rpmswPrint(" total:", &dc_totalops, NULL);
	rpmswPrint("  read:", &dc_readops, NULL);
	rpmswPrint("digest:", &dc_digestops, NULL);
    }

    optCon = rpmioFini(optCon);

    return rc;
}
