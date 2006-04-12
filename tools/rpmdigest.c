#include "system.h"
#include <rpmio.h>
#include <rpmpgp.h>
#include "debug.h"

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmDigestPoptTable, 0,
	N_("Digest options:"),
	NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    const char ** args;
    const char * fn;
    FD_t fd;
    unsigned char buf[BUFSIZ];
    ssize_t nb;
    DIGEST_CTX ctx = NULL;
    const char * digest = NULL;
    size_t digestlen = 0;
    int asAscii = 1;
    int rc;

    optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    while ((rc = poptGetNextOpt(optCon)) > 0)
	;

    if ((args = poptGetArgs(optCon)) != NULL)
    while ((fn = *args++) != NULL) {

	fd = Fopen(fn, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), fn, Fstrerror(fd));
	    if (fd) Fclose(fd);
	    rc++;
	    continue;
	}

	ctx = rpmDigestInit(rpmDigestHashAlgo, RPMDIGEST_NONE);
	while ((nb = Fread(buf, 1, sizeof(buf), fd)) > 0)
	    rpmDigestUpdate(ctx, buf, nb);
	Fclose(fd);
	rpmDigestFinal(ctx, (void **)&digest, &digestlen, asAscii);

	if (digest) {
	    fprintf(stdout, "%s     %s\n", digest, fn);
	    fflush(stdout);
	    free((void *)digest);
	    digest = NULL;
	}
    }

    optCon = poptFreeContext(optCon);

    return rc;
}
