/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 1;
extern int _pgp_debug;
extern int _pgp_print;

#include "system.h"
#include <rpmio_internal.h>	/* XXX rpmioSlurp */
#include <rpmmacro.h>

#define	_RPMPGP_INTERNAL
#define	_RPMBC_INTERNAL
#include <rpmbc.h>
#define	_RPMGC_INTERNAL
#include <rpmgc.h>
#if defined(WITH_NSS)
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#endif
#if defined(WITH_SSL)
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>
#endif

#include "genpgp.h"

#define	_RPMTS_INTERNAL		/* XXX ts->pkpkt */
#include <rpmcli.h>

#include <rpmcb.h>
#include <rpmdb.h>
#include <rpmps.h>
#include <rpmts.h>

#include "debug.h"

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

static
int rpmCheckPgpSignatureOnFile(rpmts ts, const char * fn, const char * sigfn,
		const char * pubfn, const char * pubid)
{
    pgpDig dig = rpmtsDig(ts);
    pgpDigParams sigp;
    pgpDigParams pubp;
    const unsigned char * sigpkt = NULL;
    size_t sigpktlen = 0;
    DIGEST_CTX ctx = NULL;
    int printing = 0;
    int rc = 0;
    int xx;

if (_debug)
fprintf(stderr, "==> check(%s, %s, %s, %s)\n", fn, sigfn, pubfn, pubid);

    /* Load the signature. Use sigfn if specified, otherwise clearsign. */
    if (sigfn != NULL) {
	const char * _sigfn = rpmExpand(sigfn, NULL);
	xx = pgpReadPkts(_sigfn, &sigpkt, &sigpktlen);
	if (xx != PGPARMOR_SIGNATURE) {
if (_debug)
fprintf(stderr, "==> pgpReadPkts(%s) SIG %p[%u] ret %d\n", _sigfn, sigpkt, sigpktlen, xx);
	    _sigfn = _free(_sigfn);
	    goto exit;
	}
	_sigfn = _free(_sigfn);
    } else {
	const char * _sigfn = rpmExpand(fn, NULL);
	xx = pgpReadPkts(_sigfn, &sigpkt, &sigpktlen);
	if (xx != PGPARMOR_SIGNATURE) {
if (_debug)
fprintf(stderr, "==> pgpReadPkts(%s) SIG %p[%u] ret %d\n", _sigfn, sigpkt, sigpktlen, xx);
	    _sigfn = _free(_sigfn);
	    goto exit;
	}
	_sigfn = _free(_sigfn);
    }
    xx = pgpPrtPkts((uint8_t *)sigpkt, sigpktlen, dig, printing);
    if (xx) {
if (_debug)
fprintf(stderr, "==> pgpPrtPkts SIG %p[%u] ret %d\n", sigpkt, sigpktlen, xx);
	goto exit;
    }

    sigp = pgpGetSignature(dig);

    if (sigp->version != 3 && sigp->version != 4) {
if (_debug)
fprintf(stderr, "==> unverifiable V%d\n", sigp->version);
	goto exit;
    }

    /* Load the pubkey. Use pubfn if specified, otherwise rpmdb keyring. */
    if (pubfn != NULL) {
	const char * _pubfn = rpmExpand(pubfn, NULL);
	xx = pgpReadPkts(_pubfn, &ts->pkpkt, &ts->pkpktlen);
	if (xx != PGPARMOR_PUBKEY) {
if (_debug)
fprintf(stderr, "==> pgpReadPkts(%s) PUB %p[%u] ret %d\n", _pubfn, ts->pkpkt, ts->pkpktlen, xx);
	    _pubfn = _free(_pubfn);
	    goto exit;
	}
	_pubfn = _free(_pubfn);
	xx = pgpPrtPkts((uint8_t *)ts->pkpkt, ts->pkpktlen, dig, printing);
	if (xx) {
if (_debug)
fprintf(stderr, "==> pgpPrtPkts PUB %p[%u] ret %d\n", ts->pkpkt, ts->pkpktlen, xx);
	    goto exit;
	}
    } else {
	rpmRC res = pgpFindPubkey(dig);
	if (res != RPMRC_OK) {
if (_debug)
fprintf(stderr, "==> pgpFindPubkey ret %d\n", res);
	    goto exit;
	}
    }

    pubp = pgpGetPubkey(dig);

    /* Is this the requested pubkey? */
    if (pubid != NULL) {
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
     && (pubp->pubkey_algo == PGPPUBKEYALGO_RSA || !memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid))) ) )
    {
if (_debug) {
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
if (_debug)
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
	xx = 1;
	break;
    case PGPPUBKEYALGO_DSA:
	xx = pgpImplSetDSA(ctx, dig, sigp);
	break;
    case PGPPUBKEYALGO_RSA:
	xx = pgpImplSetRSA(ctx, dig, sigp);
	break;
    }
    if (xx) {
if (_debug)
fprintf(stderr, "==> can't load pubkey_algo(%u)\n", sigp->pubkey_algo);
	goto exit;
    }

    /* Verify the signature. */
    switch(sigp->pubkey_algo) {
    default:
	rc = 0;
	break;
    case PGPPUBKEYALGO_DSA:
	rc = pgpImplVerifyDSA(dig);
	break;
    case PGPPUBKEYALGO_RSA:
	rc = pgpImplVerifyRSA(dig);
	break;
    }

exit:
    sigpkt = _free(sigpkt);
    ts->pkpkt = _free(ts->pkpkt);
    ts->pkpktlen = 0;
    rpmtsCleanDig(ts);

if (_debug)
fprintf(stderr, "============================ verify: rc %d\n", rc);

    return rc;
}

static
int doit(rpmts ts, const char * sigtype)
{
    int rc = 0;

    if (!strcmp("DSA", sigtype)) {
	rc = rpmCheckPgpSignatureOnFile(ts, DSApem, NULL, DSApub, DSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, DSAsig, DSApub, DSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, DSAsig, DSApubpem, DSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, DSAsigpem, DSApub, DSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, DSAsigpem, DSApubpem, DSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, DSAsig, NULL, DSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, DSAsigpem, NULL, DSApubid);
    }
    if (!strcmp("RSA", sigtype)) {
	rc = rpmCheckPgpSignatureOnFile(ts, RSApem, NULL, RSApub, RSApubid);
#ifdef	NOTYET	/* XXX RSA key id's are funky. */
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, RSAsig, RSApub, RSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, RSAsigpem, RSApubpem, RSApubid);
	rc = rpmCheckPgpSignatureOnFile(ts, plaintextfn, RSAsig, NULL, RSApubid);
#endif
    }
    
    return rc;
}

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options:"),
        NULL },

 /* XXX Note: these entries assume sizeof(int) == sizeof (void *). */
 { "bc", 0, POPT_ARG_VAL, &pgpImplVecs, (int)&rpmbcImplVecs,
        N_("use beecrypt crypto implementation"), NULL },
#ifdef	NOTYET
 { "gc", 0, POPT_ARG_VAL, &pgpImplVecs, (int)&rpmgcImplVecs,
        N_("use gcrypt crypto implementation"), NULL },
#endif
#if defined(WITH_NSS)
 { "nss", 0, POPT_ARG_VAL, &pgpImplVecs, (int)&rpmnssImplVecs,
        N_("use NSS crypto implementation"), NULL },
#endif
#if defined(WITH_SSL)
 { "ssl", 0, POPT_ARG_VAL, &pgpImplVecs, (int)&rpmsslImplVecs,
        N_("use OpenSSL crypto implementation"), NULL },
#endif

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};      

int
main(int argc, char *argv[])
{
    poptContext optCon;
    rpmts ts = NULL;
    int rc;

#if defined(WITH_NSS)
    pgpImplVecs = &rpmnssImplVecs;
#else
    pgpImplVecs = &rpmbcImplVecs;
#endif
_pgp_debug = 1;
_pgp_print = 1;

    optCon = rpmcliInit(argc, argv, optionsTable);
    ts = rpmtsCreate();
    (void) rpmtsOpenDB(ts, O_RDONLY);

    rc = doit(ts, "DSA");

    rc = doit(ts, "RSA");

    ts = rpmtsFree(ts);

#if defined(WITH_NSS)
    if (pgpImplVecs == &rpmnssImplVecs)
	NSS_Shutdown();
#endif

    optCon = rpmcliFini(optCon);

    return rc;
}
