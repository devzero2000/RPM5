#include "system.h"

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;
static int _printing = 1;

extern int noNeon;

#if 0
#define	HKPPATH		"hkp://pgp.mit.edu:11371/pks/lookup?op=get&search=0xF5C75256"
#else
#define	HKPPATH		"hkp://pgp.mit.edu"
#endif
static char * hkppath = HKPPATH;

static unsigned int keyids[] = {
	0xF5C75256,
	0xe418e3aa,
	0x897da07a,
	0xdb42a60e,
	0x4f2a6fd2,
	0xcba29bf9,
	0x2039b291,
	0x30c9ecf8,
	0x8df56d05,
	0x1cddbca9,
	0
};

static int readKeys(const char * uri)
{
    unsigned int * kip;
    const byte * pkt;
    ssize_t pktlen;
    char fn[BUFSIZ];
    pgpDig dig;
    int rc;
    int ec = 0;

    for (kip = keyids; *kip; kip++) {
	pgpArmor pa;

	sprintf(fn, "%s/pks/lookup?op=get&search=0x%x", uri, *kip);
fprintf(stderr, "======================= %s\n", fn);
	pkt = NULL;
	pktlen = 0;
	pa = pgpReadPkts(fn, &pkt, &pktlen);
	if (pa == PGPARMOR_ERROR || pa == PGPARMOR_NONE
         || pkt == NULL || pktlen <= 0)
        {
            ec++;
            continue;
        }

	dig = pgpNewDig();
	rc = pgpPrtPkts(pkt, pktlen, dig, _printing);
	if (rc)
	    ec++;
	dig = pgpFreeDig(dig);

	free((void *)pkt);
	pkt = NULL;
    }

    return ec;
}

static struct poptOption optionsTable[] = {
 { "print", 'p', POPT_ARG_VAL,  &_printing, 1,		NULL, NULL },
 { "noprint", 'n', POPT_ARG_VAL, &_printing, 0,		NULL, NULL },
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "davdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "noneon", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noNeon, 1,
	N_("disable use of libneon for HTTP"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    int rc;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    readKeys(hkppath);

/*@i@*/ urlFreeCache();

    return 0;
}
