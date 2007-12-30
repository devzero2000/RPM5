/** \ingroup rpmds
 * \file lib/rpmns.c
 */
#include "system.h"

#include <rpmio_internal.h>	/* XXX rpmioSlurp */
#include <rpmmacro.h>

#define	_RPMPGP_INTERNAL
#include <rpmpgp.h>

#include <rpmlib.h>		/* XXX RPMRC_OK */
#define	_RPMEVR_INTERNAL
#include <rpmevr.h>
#define	_RPMNS_INTERNAL
#include <rpmns.h>

#include <rpmcb.h>
#include <rpmdb.h>
#include <rpmps.h>
#define	_RPMTS_INTERNAL		/* XXX ts->pkpkt */
#include <rpmts.h>

#include "debug.h"

/*@unchecked@*/
int _rpmns_debug = 0;

/*@unchecked@*/ /*@observer@*/ /*@relnull@*/
const char *_rpmns_N_at_A = ".";

/*@-nullassign@*/
/*@unchecked@*/ /*@observer@*/
static const char *rpmnsArches[] = {
    "i386", "i486", "i586", "i686", "athlon", "pentium3", "pentium4",
    "x86_64", "amd64", "ia32e",
    "alpha", "alphaev5", "alphaev56", "alphapca56", "alphaev6", "alphaev67",
    "sparc", "sun4", "sun4m", "sun4c", "sun4d", "sparcv8",
    "sparcv9", "sparcv9v",
    "sparc64", "sun4u", "sparc64v",
    "mips", "mipsel", "IP",
    "ppc", "ppciseries", "ppcpseries",
    "ppc64", "ppc64iseries", "ppc64pseries",
    "m68k",
    "rs6000",
    "ia64",
    "armv3l", "armv4b", "armv4l",
    "armv5teb", "armv5tel", "armv5tejl",
    "armv6l",
    "s390", "i370", "s390x",
    "sh", "xtensa",
    "noarch", "fat",
    NULL,
};
/*@=nullassign@*/

nsType rpmnsArch(const char * str)
{
    const char ** av;
    for (av = rpmnsArches; *av != NULL; av++) {
	if (!strcmp(str, *av))
	    return RPMNS_TYPE_ARCH;
    }
    return RPMNS_TYPE_UNKNOWN;
}

/**
 * Dependency probe table.
 */
/*@unchecked@*/ /*@observer@*/
static struct _rpmnsProbes_s {
/*@observer@*/ /*@relnull@*/
    const char * NS;
    nsType Type;
} rpmnsProbes[] = {
    { "rpmlib",		RPMNS_TYPE_RPMLIB },
    { "cpuinfo",	RPMNS_TYPE_CPUINFO },
    { "getconf",	RPMNS_TYPE_GETCONF },
    { "uname",		RPMNS_TYPE_UNAME },
    { "soname",		RPMNS_TYPE_SONAME },
    { "user",		RPMNS_TYPE_USER },
    { "group",		RPMNS_TYPE_GROUP },
    { "mounted",	RPMNS_TYPE_MOUNTED },
    { "diskspace",	RPMNS_TYPE_DISKSPACE },
    { "digest",		RPMNS_TYPE_DIGEST },
    { "gnupg",		RPMNS_TYPE_GNUPG },
    { "macro",		RPMNS_TYPE_MACRO },
    { "envvar",		RPMNS_TYPE_ENVVAR },
    { "running",	RPMNS_TYPE_RUNNING },
    { "sanitycheck",	RPMNS_TYPE_SANITY },
    { "vcheck",		RPMNS_TYPE_VCHECK },
    { "signature",	RPMNS_TYPE_SIGNATURE },
    { "exists",		RPMNS_TYPE_ACCESS },
    { "executable",	RPMNS_TYPE_ACCESS },
    { "readable",	RPMNS_TYPE_ACCESS },
    { "writable",	RPMNS_TYPE_ACCESS },
    { "RWX",		RPMNS_TYPE_ACCESS },
    { "RWx",		RPMNS_TYPE_ACCESS },
    { "RW_",		RPMNS_TYPE_ACCESS },
    { "RwX",		RPMNS_TYPE_ACCESS },
    { "Rwx",		RPMNS_TYPE_ACCESS },
    { "Rw_",		RPMNS_TYPE_ACCESS },
    { "R_X",		RPMNS_TYPE_ACCESS },
    { "R_x",		RPMNS_TYPE_ACCESS },
    { "R__",		RPMNS_TYPE_ACCESS },
    { "rWX",		RPMNS_TYPE_ACCESS },
    { "rWx",		RPMNS_TYPE_ACCESS },
    { "rW_",		RPMNS_TYPE_ACCESS },
    { "rwX",		RPMNS_TYPE_ACCESS },
    { "rwx",		RPMNS_TYPE_ACCESS },
    { "rw_",		RPMNS_TYPE_ACCESS },
    { "r_X",		RPMNS_TYPE_ACCESS },
    { "r_x",		RPMNS_TYPE_ACCESS },
    { "r__",		RPMNS_TYPE_ACCESS },
    { "_WX",		RPMNS_TYPE_ACCESS },
    { "_Wx",		RPMNS_TYPE_ACCESS },
    { "_W_",		RPMNS_TYPE_ACCESS },
    { "_wX",		RPMNS_TYPE_ACCESS },
    { "_wx",		RPMNS_TYPE_ACCESS },
    { "_w_",		RPMNS_TYPE_ACCESS },
    { "__X",		RPMNS_TYPE_ACCESS },
    { "__x",		RPMNS_TYPE_ACCESS },
    { "___",		RPMNS_TYPE_ACCESS },
    { NULL, 0 }
};

nsType rpmnsProbe(const char * str)
{
    const struct _rpmnsProbes_s * av;
    size_t sn = strlen(str);
    size_t nb;

    if (sn >= 5 && str[sn-1] == ')')
    for (av = rpmnsProbes; av->NS != NULL; av++) {
	nb = strlen(av->NS);
	if (sn > nb && str[nb] == '(' && !strncmp(str, av->NS, nb))
	    return av->Type;
    }
    return RPMNS_TYPE_UNKNOWN;
}

nsType rpmnsClassify(const char * str)
{
    const char * s;
    nsType Type = RPMNS_TYPE_STRING;

    if (*str == '!')
	str++;
    if (*str == '/')
	return RPMNS_TYPE_PATH;
    s = str + strlen(str);
    if (str[0] == '%' && str[1] == '{' && s[-1] == '}')
	return RPMNS_TYPE_FUNCTION;
    if ((s - str) > 3 && s[-3] == '.' && s[-2] == 's' && s[-1] == 'o')
	return RPMNS_TYPE_DSO;
    Type = rpmnsProbe(str);
    if (Type != RPMNS_TYPE_UNKNOWN)
	return Type;
    for (s = str; *s; s++) {
	if (s[0] == '(' || s[strlen(s)-1] == ')')
	    return RPMNS_TYPE_NAMESPACE;
	if (s[0] == '.' && s[1] == 's' && s[2] == 'o')
	    return RPMNS_TYPE_DSO;
	if (s[0] == '.' && xisdigit(s[-1]) && xisdigit(s[1]))
	    return RPMNS_TYPE_VERSION;
	if (_rpmns_N_at_A && _rpmns_N_at_A[0]) {
	    if (s[0] == _rpmns_N_at_A[0] && rpmnsArch(s+1))
		return RPMNS_TYPE_ARCH;
	}
/*@-globstate@*/
	if (s[0] == '.')
	    return RPMNS_TYPE_COMPOUND;
    }
    return RPMNS_TYPE_STRING;
/*@=globstate@*/
}

int rpmnsParse(const char * str, rpmns ns)
{
    char *t;
    ns->str = t = rpmExpand(str, NULL);
    ns->Type = rpmnsClassify(ns->str);
    switch (ns->Type) {
    case RPMNS_TYPE_ARCH:
	ns->NS = NULL;
	ns->N = ns->str;
	if (ns->N[0] == '!')
	    ns->N++;
	if ((t = strrchr(t, _rpmns_N_at_A[0])) != NULL)
	    *t++ = '\0';
	ns->A = t;
	break;
    case RPMNS_TYPE_RPMLIB:
    case RPMNS_TYPE_CPUINFO:
    case RPMNS_TYPE_GETCONF:
    case RPMNS_TYPE_UNAME:
    case RPMNS_TYPE_SONAME:
    case RPMNS_TYPE_ACCESS:
    case RPMNS_TYPE_USER:
    case RPMNS_TYPE_GROUP:
    case RPMNS_TYPE_MOUNTED:
    case RPMNS_TYPE_DISKSPACE:
    case RPMNS_TYPE_DIGEST:
    case RPMNS_TYPE_GNUPG:
    case RPMNS_TYPE_MACRO:
    case RPMNS_TYPE_ENVVAR:
    case RPMNS_TYPE_RUNNING:
    case RPMNS_TYPE_SANITY:
    case RPMNS_TYPE_VCHECK:
    case RPMNS_TYPE_SIGNATURE:
	ns->NS = ns->str;
	if (ns->NS[0] == '!')
	    ns->NS++;
	if ((t = strchr(t, '(')) != NULL) {
	    *t++ = '\0';
	    ns->N = t;
	    t[strlen(t)-1] = '\0';
	} else
	   ns->N = NULL;
	ns->A = NULL;
	break;
    case RPMNS_TYPE_UNKNOWN:
    case RPMNS_TYPE_STRING:
    case RPMNS_TYPE_PATH:
    case RPMNS_TYPE_DSO:
    case RPMNS_TYPE_FUNCTION:
    case RPMNS_TYPE_VERSION:
    case RPMNS_TYPE_COMPOUND:
    case RPMNS_TYPE_NAMESPACE:
    case RPMNS_TYPE_TAG:
    default:
	ns->NS = NULL;
	ns->N = ns->str;
	if (ns->N[0] == '!')
	    ns->N++;
	ns->A = NULL;
	break;
    }
    return 0;
}

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static inline unsigned char nibble(char c)
	/*@*/
{
    if (c >= '0' && c <= '9')
	return (c - '0');
    if (c >= 'A' && c <= 'F')
	return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
	return (c - 'a') + 10;
    return 0;
}

rpmRC rpmnsProbeSignature(void * _ts, const char * fn, const char * sigfn,
		const char * pubfn, const char * pubid, int flags)
{
    rpmts ts = _ts;
    pgpDig dig = rpmtsDig(ts);
    pgpDigParams sigp;
    pgpDigParams pubp;
    const unsigned char * sigpkt = NULL;
    size_t sigpktlen = 0;
    DIGEST_CTX ctx = NULL;
    int printing = 0;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int xx;

if (_rpmns_debug)
fprintf(stderr, "==> check(%s, %s, %s, %s)\n", fn, sigfn, pubfn, pubid);

    /* Load the signature. Use sigfn if specified, otherwise clearsign. */
    if (sigfn && *sigfn) {
	const char * _sigfn = rpmExpand(sigfn, NULL);
	xx = pgpReadPkts(_sigfn, &sigpkt, &sigpktlen);
	if (xx != PGPARMOR_SIGNATURE) {
if (_rpmns_debug)
fprintf(stderr, "==> pgpReadPkts(%s) SIG %p[%u] ret %d\n", _sigfn, sigpkt, sigpktlen, xx);
	    _sigfn = _free(_sigfn);
	    goto exit;
	}
	_sigfn = _free(_sigfn);
    } else {
	const char * _sigfn = rpmExpand(fn, NULL);
	xx = pgpReadPkts(_sigfn, &sigpkt, &sigpktlen);
	if (xx != PGPARMOR_SIGNATURE) {
if (_rpmns_debug)
fprintf(stderr, "==> pgpReadPkts(%s) SIG %p[%u] ret %d\n", _sigfn, sigpkt, sigpktlen, xx);
	    _sigfn = _free(_sigfn);
	    goto exit;
	}
	_sigfn = _free(_sigfn);
    }
    xx = pgpPrtPkts((uint8_t *)sigpkt, sigpktlen, dig, printing);
    if (xx) {
if (_rpmns_debug)
fprintf(stderr, "==> pgpPrtPkts SIG %p[%u] ret %d\n", sigpkt, sigpktlen, xx);
	goto exit;
    }

    sigp = pgpGetSignature(dig);

    if (sigp->version != 3 && sigp->version != 4) {
if (_rpmns_debug)
fprintf(stderr, "==> unverifiable V%d\n", sigp->version);
	goto exit;
    }

    /* Load the pubkey. Use pubfn if specified, otherwise rpmdb keyring. */
    if (pubfn && *pubfn) {
	const char * _pubfn = rpmExpand(pubfn, NULL);
	xx = pgpReadPkts(_pubfn, &ts->pkpkt, &ts->pkpktlen);
	if (xx != PGPARMOR_PUBKEY) {
if (_rpmns_debug)
fprintf(stderr, "==> pgpReadPkts(%s) PUB %p[%u] ret %d\n", _pubfn, ts->pkpkt, ts->pkpktlen, xx);
	    _pubfn = _free(_pubfn);
	    goto exit;
	}
	_pubfn = _free(_pubfn);
	xx = pgpPrtPkts((uint8_t *)ts->pkpkt, ts->pkpktlen, dig, printing);
	if (xx) {
if (_rpmns_debug)
fprintf(stderr, "==> pgpPrtPkts PUB %p[%u] ret %d\n", ts->pkpkt, ts->pkpktlen, xx);
	    goto exit;
	}
    } else {
	if ((rc = pgpFindPubkey(dig)) != RPMRC_OK) {
if (_rpmns_debug)
fprintf(stderr, "==> pgpFindPubkey ret %d\n", xx);
	    goto exit;
	}
    }

    pubp = pgpGetPubkey(dig);

    /* Is this the requested pubkey? */
    if (pubid && *pubid) {
	size_t ns = strlen(pubid);
	const char * s;
	char * t;
	int i;

	/* At least 8 hex digits please. */
	for (i = 0, s = pubid; *s && isxdigit(*s); s++, i++)
	    ;
	if (!(*s == '\0' && i > 8 && (i%2) == 0))
	    goto exit;

	/* Truncate to key id size. */
	s = pubid;
	if (ns > 16) {
	    s += (ns - 16);
	    ns = 16;
	}
	ns >>= 1;
	t = memset(alloca(ns), 0, ns);
	for (i = 0; i < ns; i++)
	    t[i] = (nibble(s[2*i]) << 4) | nibble(s[2*i+1]);

	/* Compare the pubkey id. */
	s = (const char *)pubp->signid;
	xx = memcmp(t, s + (8 - ns), ns);

	/* XXX HACK: V4 RSA key id's are wonky atm. */
	if (pubp->pubkey_algo == PGPPUBKEYALGO_RSA)
	    xx = 0;

	if (xx) {
if (_rpmns_debug)
fprintf(stderr, "==> mismatched: pubkey id (%08x %08x) != %s\n",
pgpGrab(pubp->signid, 4), pgpGrab(pubp->signid+4, 4), pubid);
	    goto exit;
	}
    }

    /* Do the parameters match the signature? */
    if (!(sigp->pubkey_algo == pubp->pubkey_algo
#ifdef  NOTYET
     && sigp->hash_algo == pubp->hash_algo
#endif
    /* XXX HACK: V4 RSA key id's are wonky atm. */
     && (pubp->pubkey_algo == PGPPUBKEYALGO_RSA || !memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid))) ) ) {
if (_rpmns_debug) {
fprintf(stderr, "==> mismatch between signature and pubkey\n");
fprintf(stderr, "\tpubkey_algo: %u  %u\n", sigp->pubkey_algo, pubp->pubkey_algo);
fprintf(stderr, "\tsignid: %08X %08X    %08X %08X\n",
pgpGrab(sigp->signid, 4), pgpGrab(sigp->signid+4, 4), 
pgpGrab(pubp->signid, 4), pgpGrab(pubp->signid+4, 4));
}
	goto exit;
    }

    /* Compute the message digest. */
    ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);

    {	
	static const char clrtxt[] = "-----BEGIN PGP SIGNED MESSAGE-----";
	static const char sigtxt[] = "-----BEGIN PGP SIGNATURE-----";
	const char * _fn = rpmExpand(fn, NULL);
	uint8_t * b = NULL;
	ssize_t blen = 0;
	int _rc = rpmioSlurp(_fn, &b, &blen);

	if (!(_rc == 0 && b != NULL && blen > 0)) {
if (_rpmns_debug)
fprintf(stderr, "==> rpmioSlurp(%s) MSG %p[%u] ret %d\n", _fn, b, blen, _rc);
	    b = _free(b);
	    _fn = _free(_fn);
	    goto exit;
	}
	_fn = _free(_fn);

	/* XXX clearsign sig is PGPSIGTYPE_TEXT not PGPSIGTYPE_BINARY. */
	if (!strncmp((char *)b, clrtxt, strlen(clrtxt))) {
	    const char * be = (char *) (b + blen);
	    const char * t;
	    const char * te;

	    /* Skip to '\n\n' start-of-plaintext */
	    t = (char *) b;
	    while (t && t < be && *t != '\n')
		t = strchr(t, '\n') + 1;
	    if (!(t && t < be))
		goto exit;
	    t++;

	    /* Skip to start-of-signature */
	    te = t;
	    while (te && te < be && strncmp(te, sigtxt, strlen(sigtxt)))
		te = strchr(te, '\n') + 1;
	    if (!(te && te < be))
		goto exit;
	    te--;	/* hmmm, one too far? does clearsign snip last \n? */

	    xx = rpmDigestUpdate(ctx, t, (te - t));
	} else
	    xx = rpmDigestUpdate(ctx, b, blen);

	b = _free(b);
    }

    if (sigp->hash != NULL)
	xx = rpmDigestUpdate(ctx, sigp->hash, sigp->hashlen);
    if (sigp->version == 4) {
	uint32_t nb = sigp->hashlen;
	uint8_t trailer[6];
	nb = htonl(nb);
	trailer[0] = sigp->version;
	trailer[1] = 0xff;
	memcpy(trailer+2, &nb, sizeof(nb));
	xx = rpmDigestUpdate(ctx, trailer, sizeof(trailer));
    }

    /* Load the message digest. */
    switch(sigp->pubkey_algo) {
    default:
	rc = RPMRC_FAIL;
	break;
    case PGPPUBKEYALGO_DSA:
	rc = (pgpImplSetDSA(ctx, dig, sigp) ? RPMRC_FAIL : RPMRC_OK);
	break;
    case PGPPUBKEYALGO_RSA:
	rc = (pgpImplSetRSA(ctx, dig, sigp) ? RPMRC_FAIL : RPMRC_OK);
	break;
    }
    if (rc != RPMRC_OK) {
if (_rpmns_debug)
fprintf(stderr, "==> can't load pubkey_algo(%u)\n", sigp->pubkey_algo);
	goto exit;
    }

    /* Verify the signature. */
    switch(sigp->pubkey_algo) {
    default:
	rc = RPMRC_FAIL;
	break;
    case PGPPUBKEYALGO_DSA:
	rc = (pgpImplVerifyDSA(dig) ? RPMRC_OK : RPMRC_FAIL);
	break;
    case PGPPUBKEYALGO_RSA:
	rc = (pgpImplVerifyRSA(dig) ? RPMRC_OK : RPMRC_FAIL);
	break;
    }

exit:
    sigpkt = _free(sigpkt);
    ts->pkpkt = _free(ts->pkpkt);
    ts->pkpktlen = 0;
    rpmtsCleanDig(ts);

if (_rpmns_debug)
fprintf(stderr, "============================ verify: %s\n",
	(rc == RPMRC_OK ? "OK" : "FAIL"));

    return rc;
}
