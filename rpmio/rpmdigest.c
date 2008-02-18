#include "system.h"
#include <rpmio_internal.h>
#include <poptIO.h>
#include "debug.h"

static int initDigests(FD_t fd, pgpHashAlgo algo)
{
    switch (algo) {
    default:
	fdInitDigest(fd, algo, 0);
	break;
    case 256:		/* --all digests requested. */
      {	struct poptOption * opt = rpmioDigestPoptTable;
	for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
	    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
		continue;
	    algo = opt->val;
	    if (!(algo > 0 && algo < 256))
		continue;
	    fdInitDigest(fd, algo, 0);
	}
      }	break;
    }
    return 0;
}

static int printDigest(FD_t fd, pgpHashAlgo algo, const char * algoName)
{
    const char * digest = NULL;
    size_t digestlen = 0;
    static int asAscii = 1;

    fdFiniDigest(fd, algo, &digest, &digestlen, asAscii);
    if (digest) {
	const char * fn = fdGetOPath(fd);
	if (algoName)
	    fprintf(stdout, "%s:\t", algoName);
	fprintf(stdout, "%s     %s\n", digest, fn);
	fflush(stdout);
	digest = _free(digest);
    }
    return 0;
}

static int finiDigests(FD_t fd, pgpHashAlgo algo)
{
    
    switch (algo) {
    default:
	printDigest(fd, algo, NULL);
	break;
    case 256:		/* --all digests requested. */
      {	struct poptOption * opt = rpmioDigestPoptTable;
	for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
	    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
		continue;
	    if (opt->arg != (void *)&rpmioDigestHashAlgo)
		continue;
	    algo = opt->val;
	    if (!(algo > 0 && algo < 256))
		continue;
	    printDigest(fd, algo, opt->longName);
	}
      }	break;
    }
    return 0;
}

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioDigestPoptTable, 0,
	N_("Digest options:"), NULL },
  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    const char ** args;
    const char * fn;
    int rc = 0;

    if ((int)rpmioDigestHashAlgo < 0)
	rpmioDigestHashAlgo = PGPHASHALGO_MD5;

    if ((args = poptGetArgs(optCon)) != NULL)
    while ((fn = *args++) != NULL) {
	FD_t fd;
	unsigned char buf[BUFSIZ];
	ssize_t nb;

	fd = Fopen(fn, "r");
	if (fd == NULL || Ferror(fd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), fn, Fstrerror(fd));
	    if (fd) Fclose(fd);
	    rc++;
	    continue;
	}

	initDigests(fd, rpmioDigestHashAlgo);
	while ((nb = Fread(buf, 1, sizeof(buf), fd)) > 0)
	    ;
	finiDigests(fd, rpmioDigestHashAlgo);

	Fclose(fd);
    }

    optCon = rpmioFini(optCon);

    return rc;
}
