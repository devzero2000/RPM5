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
    RPMDC_FLAGS_STATUS	= _DFB( 2),	/*!<    --status ... */
    RPMDC_FLAGS_0INSTALL= _DFB( 3)	/*!< -0 --0install ... */
};

struct rpmdc_s {
    enum dcFlags_e flags;
    uint32_t algo;		/*!< default digest algorithm. */
    uint32_t dalgo;		/*!< digest algorithm. */
/*@observer@*/ /*@null@*/
    const char * dalgoName;	/*!< digest algorithm name. */
    const char * digest;
    size_t digestlen;
    const char * fn;
    FD_t fd;
    struct stat sb;
    int (*parse) (rpmdc dc);
    const char * (*print) (rpmdc dc, int rc);
    const char * ofn;		/*!< output file name */
    FD_t ofd;			/*!< output file handle */
    uint32_t oalgo;		/*!< output digest algorithm. */
    const char * oalgoName;	/*!< output digest algorithm name. */
    ARGV_t manifests;		/*!< array of file manifests to verify. */
    ARGI_t algos;		/*!< array of file digest algorithms. */
    ARGV_t digests;		/*!< array of file digests. */
    ARGV_t paths;		/*!< array of file paths. */
    unsigned char buf[BUFSIZ];
    ssize_t nb;
    int ix;
    size_t ncomputed;		/*!< no. of digests computed. */
    size_t nchecked;		/*!< no. of digests checked. */
    size_t nmatched;		/*!< no. of digests matched. */
    size_t nfailed;		/*!< no. of digests failed. */
    struct rpmop_s totalops;
    struct rpmop_s readops;
    struct rpmop_s digestops;
};

/* ================================================= */
static int rpmdcParseCoreutils(rpmdc dc)
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
	    int c, xx;

	    while (se > buf && xisspace((int)se[-1]))
		se--;
	    *se = '\0';

	    /* Skip blank lines */
	    if (buf[0] == '\0')	/*@innercontinue@*/ continue;
	    /* Skip comment lines */
	    if (buf[0] == '#')	/*@innercontinue@*/ continue;

	    /* Parse "[algo:]digest [* ]path" line. */
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
		dc->dalgo = 0xffffffff;
		dc->dalgoName = NULL;
		for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
		    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
			continue;
		    if (opt->longName == NULL)
			continue;
		    if (!(opt->val > 0 && opt->val < 256))
			continue;
		    if (strcmp(opt->longName, dname))
			continue;
		    dc->dalgo = (uint32_t) opt->val;
		    dc->dalgoName = opt->longName;
		    break;
		}
		if (dc->dalgo == 0xffffffff) {
		    fprintf(stderr, _("%s: Unknown digest name \"%s\"\n"),
				__progname, dname);
		    rc = 2;
		    goto exit;
		}
	    } else
		dc->dalgo = dc->algo;

	    /* Save {algo, digest, path} for processing. */
	    xx = argiAdd(&dc->algos, -1, dc->dalgo);
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

/*@null@*/
static const char * rpmdcPrintCoreutils(rpmdc dc, int rc)
{
    const char *msg = (rc ? "FAILED" : "OK");
    char * t, * te;
    size_t nb = 0;

    /* Don't bother formatting if noone cares. */
    if (rc == 0 && F_ISSET(dc, STATUS))
	return NULL;

    /* Calculate size of message. */
    if (dc->dalgoName != NULL)
	nb += strlen(dc->dalgoName) + sizeof(":") - 1;
    if (dc->digest != NULL && dc->digestlen > 0)
	nb += dc->digestlen;
    nb += sizeof(" *") - 1;
    if (dc->fn != NULL)
	nb += strlen(dc->fn);
    nb += strlen(msg);
    nb += sizeof("\n");		/* XXX trailing NUL */

    /* Compose the message. */
    te = t = xmalloc(nb);
    *te = '\0';

    if (dc->manifests) {
	if (rc || !F_ISSET(dc, STATUS)) {
	    if (dc->fn)
		te = stpcpy( stpcpy(te, dc->fn), ": ");
	    te = stpcpy(te, msg);
	    *te++ = '\n';
	}
    } else {
	if (dc->dalgoName)
	    te = stpcpy( stpcpy(te, dc->dalgoName), ":");
	te = stpcpy(te, dc->digest);
	*te++ = ' ';
	*te++ = (F_ISSET(dc, BINARY) ? '*' : ' ');
	te = stpcpy(te, dc->fn);
	*te++ = '\n';
    }
    *te = '\0';

    return t;
}

/* ================================================= */
/*@null@*/
static const char * rpmdcPrintZeroInstall(rpmdc dc, int rc)
{
    char * t, * te;
    size_t nb = 0;
    char _mtime[32];
    char _size[32];
    const char * _bn;

    /* Don't bother formatting if noone cares. */
    if (rc == 0 && F_ISSET(dc, STATUS))
	return NULL;

    snprintf(_mtime, sizeof(_mtime), "%llu",
		(unsigned long long) dc->sb.st_mtime);
    _mtime[sizeof(_mtime)-1] = '\0';
    snprintf(_size, sizeof(_size), "%llu",
		(unsigned long long)dc->sb.st_size);
    _size[sizeof(_size)-1] = '\0';
    if ((_bn = strrchr(dc->fn, '/')) != NULL)
	_bn++;
    else
	_bn = dc->fn;

    /* Calculate size of message. */
    nb += sizeof("F");
    if (dc->digest != NULL && dc->digestlen > 0)
	nb += 1 + dc->digestlen;
    nb += 1 + strlen(_mtime);
    nb += 1 + strlen(_size);
    nb += 1 + strlen(_bn);
    nb += sizeof("\n");		/* XXX trailing NUL */
    
    /* Compose the message. */
    te = t = xmalloc(nb);
    *te = '\0';

    if (dc->manifests) {
#ifdef NOTYET
	const char *msg = (rc ? "FAILED" : "OK");
	if (rc || !F_ISSET(dc, STATUS)) {
	    if (dc->fn)
		te = stpcpy( stpcpy(te, dc->fn), ": ");
	    te = stpcpy(te, msg);
	    *te++ = '\n';
	}
#endif
    } else {
	*te++ = 'F';
	*te++ = ' ';
	te = stpcpy(te, dc->digest);
	*te++ = ' ';
	te = stpcpy(te, _mtime);
	*te++ = ' ';
	te = stpcpy(te, _size);
	*te++ = ' ';
	te = stpcpy(te, _bn);
	*te++ = '\n';
    }
    *te = '\0';

    return t;
}

/* ================================================= */
static struct rpmdc_s _dc = {
	.parse = rpmdcParseCoreutils,
	.print = rpmdcPrintCoreutils
};

static rpmdc dc = &_dc;

static int rpmdcPrintFile(rpmdc dc)
{
    static int asAscii = 1;
    int rc = 0;

    fdFiniDigest(dc->fd, dc->dalgo, &dc->digest, &dc->digestlen, asAscii);
assert(dc->digest != NULL);
    dc->ncomputed++;

    if (dc->manifests) {
	dc->nchecked++;
	if ((rc = strcmp(dc->digest, dc->digests[dc->ix])) != 0)
	    dc->nfailed++;
	else
	    dc->nmatched++;
    }

    {	const char * t = (*dc->print) (dc, rc);
	if (dc->ofd && t && *t) {
	    size_t nb = strlen(t);
	    nb = Fwrite(t, nb, sizeof(*t), dc->ofd);
	    (void) Fflush(dc->ofd);
	}
	t = _free(t);
    }

    dc->digest = _free(dc->digest);
    return rc;
}

static int rpmdcFiniFile(rpmdc dc)
{
    uint32_t dalgo = (dc->manifests ? dc->algos->vals[dc->ix] : dc->algo);
    int rc = 0;
    int xx;

    switch (dalgo) {
    default:
	dc->dalgo = dalgo;
	dc->dalgoName = NULL;
	xx = rpmdcPrintFile(dc);
	if (xx) rc = xx;
	break;
    case 256:		/* --all digests requested. */
      {	struct poptOption * opt = rpmioDigestPoptTable;
	for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
	    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
		continue;
	    if (opt->arg != (void *)&rpmioDigestHashAlgo)
		continue;
	    dc->dalgo = opt->val;
	    if (!(dc->dalgo > 0 && dc->dalgo < 256))
		continue;
	    dc->dalgoName = opt->longName;
	    xx = rpmdcPrintFile(dc);
	    if (xx) rc = xx;
	}
      }	break;
    }
    (void) rpmswAdd(&dc->readops, fdstat_op(dc->fd, FDSTAT_READ));
    (void) rpmswAdd(&dc->digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
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
    int rc;

    /* XXX Stat(2) to insure files only? */
    if ((rc = Lstat(dc->fn, &dc->sb)) != 0) {
	memset(&dc->sb, 0, sizeof(dc->sb));
	goto exit;
    }

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
	dc->dalgo = dc->algo;
	fdInitDigest(dc->fd, dc->dalgo, 0);
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
	    dc->dalgo = opt->val;
	    dc->dalgoName = opt->longName;
	    fdInitDigest(dc->fd, dc->dalgo, 0);
	}
      }	break;
    }

exit:
    return rc;
}

static int rpmdcLoadManifests(rpmdc dc)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies dc, h_errno, fileSystem, internalState @*/
{
    return (dc->manifests != NULL ? (*dc->parse) (dc) : 0);
}

#if !defined(POPT_ARG_ARGV)
static int _poptSaveString(const char ***argvp, unsigned int argInfo, const char * val)
	/*@*/
{
    ARGV_t argv;
    int argc = 0;
    if (argvp == NULL)
	return -1;
    if (*argvp)
    while ((*argvp)[argc] != NULL)
	argc++;
    *argvp = xrealloc(*argvp, (argc + 1 + 1) * sizeof(**argvp));
    if ((argv = *argvp) != NULL) {
	argv[argc++] = xstrdup(val);
	argv[argc  ] = NULL;
    }
    return 0;
}

/**
 */
static void rpmdcArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, /*@unused@*/ const char * arg,
                /*@unused@*/ void * data)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
	int xx;
    case 'c':
assert(arg != NULL);
	xx = _poptSaveString(&_dc.manifests, opt->argInfo, arg);
	break;

    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
/*@-exitarg@*/
	exit(2);
/*@=exitarg@*/
	/*@notreached@*/ break;
    }
}
#endif	/* POPT_ARG_ARGV */

static struct poptOption _optionsTable[] = {
#if !defined(POPT_ARG_ARGV)
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmdcArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* POPT_ARG_ARGV */

  { "0install", '0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&_dc.flags, RPMDC_FLAGS_0INSTALL,
	N_("print 0install manifest"), NULL },

  { "binary", 'b', POPT_BIT_SET,	&_dc.flags, RPMDC_FLAGS_BINARY,
	N_("read in binary mode"), NULL },

#if !defined(POPT_ARG_ARGV)
  { "check", 'c', POPT_ARG_STRING,	NULL, 'c',
	N_("read digests from MANIFEST file and verify (may be used more than once)"),
	N_("MANIFEST") },
#else
  { "check", 'c', POPT_ARG_ARGV,	&_dc.manifests, 0,
	N_("read digests from MANIFEST file and verify (may be used more than once)"),
	N_("MANIFEST") },
#endif

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

static struct poptOption *optionsTable = &_optionsTable[0];

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    ARGV_t av;
    int ac;
    int rc = 0;
    int xx;

    rpmswEnter(&dc->totalops, -1);

    if ((int)rpmioDigestHashAlgo < 0)
	rpmioDigestHashAlgo = PGPHASHALGO_MD5;

    dc->algo = rpmioDigestHashAlgo;
    if (dc->ofn == NULL)
	dc->ofn = "-";
    dc->ofd = Fopen(dc->ofn, "w.ufdio");

    if (F_ISSET(dc, 0INSTALL)) {
	dc->print = rpmdcPrintZeroInstall;
	dc->algo = PGPHASHALGO_SHA1;
	dc->oalgo = PGPHASHALGO_SHA1;
	dc->oalgoName = "sha1";
	fdInitDigest(dc->ofd, dc->oalgo, 0);
    }

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
    if (dc->nfailed)
	fprintf(stderr, "%s: WARNING: %u of %d computed checksums did NOT match\n",
		__progname, dc->nfailed, dc->ncomputed);

    if (dc->ofd) {
	if (F_ISSET(dc, 0INSTALL)) {
	    static int asAscii = 1;
	    char *t;
	    fdFiniDigest(dc->ofd, dc->oalgo, &dc->digest, &dc->digestlen, asAscii);
assert(dc->digest != NULL);
	    t = rpmExpand(dc->oalgoName, "=", dc->digest, "\n", NULL);
	    (void) Fwrite(t, strlen(t), sizeof(*t), dc->ofd);
	    t = _free(t);
	    dc->digest = _free(dc->digest);
	}
	(void) Fclose(dc->ofd);
	dc->ofd = NULL;
    }

#ifdef	NOTYET
    dc->manifests = argvFree(dc->manifests);
#endif
    dc->algos = argiFree(dc->algos);
    dc->digests = argvFree(dc->digests);
    dc->paths = argvFree(dc->paths);

    rpmswExit(&dc->totalops, 0);
    if (_rpmsw_stats) {
	rpmswPrint(" total:", &dc->totalops);
	rpmswPrint("  read:", &dc->readops);
	rpmswPrint("digest:", &dc->digestops);
    }

    optCon = rpmioFini(optCon);

    return rc;
}
