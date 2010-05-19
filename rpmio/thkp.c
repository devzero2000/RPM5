#include "system.h"

#define	_RPMHKP_INTERNAL
#include <rpmhkp.h>

#ifdef	DYING
#define	_RPMPGP_INTERNAL
#include <rpmpgp.h>
#endif

#include <rpmdir.h>
#include <rpmdav.h>
#include <rpmmacro.h>
#include <rpmcb.h>

#include <poptIO.h>

#include "debug.h"

static int _debug = 0;
static int _printing = 0;

int noNeon;

/* XXX renaming work-in-progress */
#define	SUM	_rpmhkp_stats

extern int _rpmhkp_spew;

static uint64_t _keyids[] = {
#if 0
	0xc2b079fcf5c75256ULL,	/* Coker */
	0x94cd5742e418e3aaULL,	/* RH 2003 */
	0xb44269d04f2a6fd2ULL,	/* fedora@redhat.com */
	0xda84cbd430c9ecf8ULL,	/* rawhide@redhat.com */
	0x29d5ba248df56d05ULL,	/* security@fedora.us */
	0x1CFC22F3363DEAE3ULL,	/* XXX DSASHA224/DSA-SHA256 */
	0x87CC9EC7F9033421ULL,	/* XXX rpmhkpHashKey: 98 */
	0xa520e8f1cba29bf9ULL,	/* V2 revocations (and V3 RSA) */
	0x58e727c4c621be0fULL,	/* ensc */
	0x0D001429,		/* ensc */
	0x5D385582001AE622ULL,	/* key revoke */
#endif
	0xD5CA9B04F2C423BCULL,	/* zack: subkey revoke */
#if 0
	0x219180cddb42a60eULL,
	0xfd372689897da07aULL,
	0xe1385d4e1cddbca9ULL,
	0x6bddfe8e54a2acf1ULL,
	0xb873641b2039b291ULL,
/* --- RHEL6 */
	0x2fa658e0,
	0x37017186,
	0x42193e6b,
	0x897da07a,
	0xdb42a60e,
	0xf21541eb,
	0xfd431d51,
/* --- Fedorable */
	0xd22e77f2,	/* F11 */
	0x57bbccba,	/* F12 */
	0xe8e40fde,	/* F13 */
#endif
	0,0
};

/*==============================================================*/

static int rpmhkpReadKeys(uint64_t * keyids)
{
    rpmhkp hkp;
    uint64_t * kip;
    rpmuint8_t keyid[8];
    int rc;
    int ec = 0;

    for (kip = keyids; *kip; kip++) {

	keyid[0] = (kip[0] >> 56);
	keyid[1] = (kip[0] >> 48);
	keyid[2] = (kip[0] >> 40);
	keyid[3] = (kip[0] >> 32);
	keyid[4] = (kip[0] >> 24);
	keyid[5] = (kip[0] >> 16);
	keyid[6] = (kip[0] >>  8);
	keyid[7] = (kip[0]      );

fprintf(stderr, "===============\n");
	hkp = rpmhkpLookup(keyid);
	if (hkp == NULL) {
	    ec++;
	    continue;
	}

	rc = rpmhkpValidate(hkp);
	if (rc)
	    ec++;

	hkp = rpmhkpFree(hkp);

    }

    return ec;
}

static struct poptOption optionsTable[] = {
 { "print", 'p', POPT_ARG_VAL,  &_printing, 1,			NULL, NULL },
 { "noprint", 'n', POPT_ARG_VAL, &_printing, 0,			NULL, NULL },
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,			NULL, NULL },
 { "spew", '\0', POPT_ARG_VAL,	&_rpmhkp_spew, -1,		NULL, NULL },
 { "noneon", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noNeon, 1,
	N_("disable use of libneon for HTTP"), NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
        N_("Common options:"), NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);;
    uint64_t * keyids = _keyids;
    int ec = 0;

    if (ac == 1 && !strcmp(av[0], "-")) {
	ARGV_t kav = NULL;
	int xx = argvFgets(&kav, NULL);
	int kac = argvCount(kav);
	int i, j;
xx = xx;

	if (kac == 0)
	    goto exit;
	keyids = xcalloc(kac+1, sizeof(*keyids));
	for (i = 0, j = 0; i < kac; i++) {
	    if (sscanf(kav[i], "%llx", keyids+j))
		j++;
	}
	kav = argvFree(kav);
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    rpmbfParams(_rpmhkp_awol.n, _rpmhkp_awol.e, &_rpmhkp_awol.m, &_rpmhkp_awol.k);
    _rpmhkp_awol.bf = rpmbfNew(_rpmhkp_awol.m, _rpmhkp_awol.k, 0);
    rpmbfParams(_rpmhkp_crl.n, _rpmhkp_crl.e, &_rpmhkp_crl.m, &_rpmhkp_crl.k);
    _rpmhkp_crl.bf = rpmbfNew(_rpmhkp_crl.m, _rpmhkp_crl.k, 0);

    ec = rpmhkpReadKeys(keyids);

fprintf(stderr, "============\n");
fprintf(stderr, "    LOOKUPS:%10u\n", SUM.lookups);
fprintf(stderr, "    PUBKEYS:%10u\n", SUM.certs);
fprintf(stderr, " SIGNATURES:%10u\
\n  PUB bound:%10u\trevoked:%10u\texpired:%10u\
\n  SUB bound:%10u\trevoked:%10u\
\n    expired:%10u\
\n   filtered:%10u\
\n", SUM.sigs,
SUM.pubbound, SUM.pubrevoked, SUM.keyexpired,
SUM.subbound, SUM.subrevoked,
SUM.expired, SUM.filtered);
fprintf(stderr, " DSA:%10u:%-10u\n", SUM.DSA.good, (SUM.DSA.good+SUM.DSA.bad));
fprintf(stderr, " RSA:%10u:%-10u\n", SUM.RSA.good, (SUM.RSA.good+SUM.RSA.bad));
fprintf(stderr, "HASH:%10u:%-10u\n", SUM.HASH.good, (SUM.HASH.good+SUM.HASH.bad));
fprintf(stderr, "AWOL:%10u:%-10u\n", SUM.AWOL.good, (SUM.AWOL.good+SUM.AWOL.bad));
fprintf(stderr, "SKIP:%10u:%-10u\n", SUM.SKIP.good, (SUM.SKIP.good+SUM.SKIP.bad));

exit:
    if (keyids != _keyids)
	keyids = _free(keyids);

    _rpmhkp_awol.bf = rpmbfFree(_rpmhkp_awol.bf);
    _rpmhkp_crl.bf = rpmbfFree(_rpmhkp_crl.bf);

/*@i@*/ urlFreeCache();

    optCon = rpmioFini(optCon);

    return ec;
}
