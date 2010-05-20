#include "system.h"

#define	_RPMHKP_INTERNAL
#include <rpmhkp.h>

#include <poptIO.h>

#include "debug.h"

/* XXX renaming work-in-progress */
#define	SUM	_rpmhkp_stats

extern int _rpmhkp_spew;

static const char * _hkp_keyserver;
/* XXX the "0x" is sometimes in macro and sometimes in keyname. */
static const char * _hkp_keyserver_query =
	"hkp://%{_hkp_keyserver}/pks/lookup?op=get&search=";

static const char * _keyids[] = {
#if 0
    "Russell Coker <russell@coker.com.au>",	/* 0xc2b079fcf5c75256 */
    "Red Hat, Inc. automated build signing key (2003) <rawhide@redhat.com>", /* 0x94cd5742e418e3aa */
    "0xb44269d04f2a6fd2",	/* Fedora Project <fedora@redhat.com> */
    "Fedora Project (Test Software) <rawhide@redhat.com>", /* 0xda84cbd430c9ecf8 */
    "Fedora Linux (RPMS) <security@fedora.us>",	/* 0x29d5ba248df56d05 */
    "0x1CFC22F3363DEAE3",	/* XXX DSASHA224/DSA-SHA256 */
    "0xa520e8f1cba29bf9",	/* V3 RSA (V2 revocations) */
    "0x58e727c4c621be0f",	/* REVOKE */
    "0x5D385582001AE622",	/* REVOKE */
    "Enrico Scholz <enrico.scholz@sigma-chemnitz.de>", /* 0xBC916AF40D001429 */
    "Warren Togami (Linux) <warren@togami.com>",	/* 0x6bddfe8e54a2acf1 */
    "0x219180cddb42a60e",	/* Red Hat, Inc <security@redhat.com> */
    "0xfd372689897da07a",	/* Red Hat, Inc. (Beta Test Software) <rawhide@redhat.com> */
    "Fedora Project automated build signing key (2003) <rawhide@redhat.com>", /* 0xe1385d4e1cddbca9 */
    "0xD5CA9B04F2C423BC",	/* Stefano Zacchiroli <zack@pps.jussieu.fr> */
#endif
    "Jeff Johnson (ARS N3NPQ) <jbj@redhat.com>",	/* 0xb873641b2039b291 */
#if 0
/* --- RHEL6 */
    "0x37017186",	/* Red Hat, Inc. (release key) <security@redhat.com> */
    "Red Hat, Inc. (RHX key) <rhx-support@redhat.com>",		/* 0x42193e6b */
    "Red Hat, Inc. (Beta Test Software) <rawhide@redhat.com>",	/* 0x897da07a */
    "Red Hat, Inc <security@redhat.com>",			/* 0xdb42a60e */
    "Red Hat, Inc. (beta key 2) <security@redhat.com>",		/* 0xf21541eb */
    "Red Hat, Inc. (release key 2) <security@redhat.com>",	/* 0xfd431d51 */
/* --- Fedorable */
    "Fedora (11) <fedora@fedoraproject.org>",	/* 0xd22e77f2 */
    "0x57bbccba",	/* Fedora (12) <fedora@fedoraproject.org> */
    "0xe8e40fde",	/* Fedora (13) <fedora@fedoraproject.org> */
#endif
    "0x9AC53D4D",
    NULL,
};

/*==============================================================*/

static rpmRC rpmhkpReadKeys(const char ** keyids)
{
    const char ** kip;
    rpmRC rc;
    int ec = 0;

    for (kip = keyids; *kip; kip++) {
fprintf(stderr, "===============\n");
	rc = rpmhkpValidate(NULL, *kip);
	if (rc)
	    ec++;
    }

    return ec;
}

static struct poptOption optionsTable[] = {

 { "spew", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&_rpmhkp_spew, -1,
	NULL, NULL },
 { "host", 'H', POPT_ARG_STRING,			&_hkp_keyserver, 0,
	NULL, NULL },

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
    const char ** keyids = _keyids;
    int ec = 0;

    if (_hkp_keyserver == NULL)
	_hkp_keyserver = xstrdup("keys.rpm5.org");

    /* XXX no macros are loaded if using poptIO. */
    addMacro(NULL, "_hkp_keyserver",		NULL, _hkp_keyserver, -1);
    addMacro(NULL, "_hkp_keyserver_query",	NULL, _hkp_keyserver_query, -1);

    if (ac == 1 && !strcmp(av[0], "-")) {
	keyids = NULL;
	if (argvFgets(&keyids, NULL) || argvCount(keyids) == 0)
	    goto exit;
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
	keyids = argvFree(keyids);
    _hkp_keyserver = _free(_hkp_keyserver);

    _rpmhkp_awol.bf = rpmbfFree(_rpmhkp_awol.bf);
    _rpmhkp_crl.bf = rpmbfFree(_rpmhkp_crl.bf);


/*@i@*/ urlFreeCache();

    optCon = rpmioFini(optCon);

    return ec;
}
