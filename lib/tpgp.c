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
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>

#include "genpgp.h"

#define	_RPMTS_INTERNAL		/* XXX ts->pkpkt */
#include <rpmcli.h>

#include <rpmcb.h>
#include <rpmdb.h>
#include <rpmps.h>
#include <rpmts.h>

#include "debug.h"

static
int rpmCheckPgpSignatureOnFile(rpmts ts, const char * fn, const char * sigfn,
		const char * pubfn, const char * pubfingerprint)
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
fprintf(stderr, "==> check(%s, %s, %s, %s)\n", fn, sigfn, pubfn, pubfingerprint);

    /* Load the signature. Use sigfn if specified, otherwise clearsign. */
    if (sigfn != NULL) {
	const char * _sigfn = rpmExpand(sigfn, NULL);
	xx = pgpReadPkts(_sigfn, &sigpkt, &sigpktlen);
	if (xx != PGPARMOR_SIGNATURE) {
fprintf(stderr, "==> pgpReadPkts(%s) SIG %p[%u] ret %d\n", _sigfn, sigpkt, sigpktlen, xx);
	    _sigfn = _free(_sigfn);
	    goto exit;
	}
	_sigfn = _free(_sigfn);
    } else {
	/* XXX FIXME: read clearsign'd file with appended signature.
    }
    xx = pgpPrtPkts((uint8_t *)sigpkt, sigpktlen, dig, printing);
    if (xx) {
fprintf(stderr, "==> pgpPrtPkts SIG %p[%u] ret %d\n", sigpkt, sigpktlen, xx);
	goto exit;
    }

    sigp = pgpGetSignature(dig);

    if (sigp->version != 3 && sigp->version != 4) {
fprintf(stderr, "==> unverifiable V%d\n", sigp->version);
	goto exit;
    }

    /* Load the pubkey. Use pubfn if specified, otherwise rpmdb keyring. */
    if (pubfn != NULL) {
	const char * _pubfn = rpmExpand(pubfn, NULL);
	xx = pgpReadPkts(_pubfn, &ts->pkpkt, &ts->pkpktlen);
	if (xx != PGPARMOR_PUBKEY) {
fprintf(stderr, "==> pgpReadPkts(%s) PUB %p[%u] ret %d\n", _pubfn, ts->pkpkt, ts->pkpktlen, xx);
	    _pubfn = _free(_pubfn);
	    goto exit;
	}
	_pubfn = _free(_pubfn);
	xx = pgpPrtPkts((uint8_t *)ts->pkpkt, ts->pkpktlen, dig, printing);
	if (xx) {
fprintf(stderr, "==> pgpPrtPkts PUB %p[%u] ret %d\n", ts->pkpkt, ts->pkpktlen, xx);
	    goto exit;
	}
    } else {
	rpmRC res = pgpFindPubkey(dig);
	if (res != RPMRC_OK) {
fprintf(stderr, "==> rpmtsFindPubkey ret %d\n", res);
	    goto exit;
	}
    }

    pubp = pgpGetPubkey(dig);

    /* Do the parameters match the signature? */
    if (!(sigp->pubkey_algo == pubp->pubkey_algo
#ifdef  NOTYET
     && sigp->hash_algo == pubp->hash_algo
#endif
    /* XXX V4 RSA key id's seem to be broken. */
     && (pubp->pubkey_algo == PGPPUBKEYALGO_RSA || !memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid))) ) )
    {
fprintf(stderr, "==> mismatch between signature and pubkey\n");
fprintf(stderr, "\tpubkey_algo: %u  %u\n", sigp->pubkey_algo, pubp->pubkey_algo);
fprintf(stderr, "\tsignid: %08X %08X    %08X %08X\n",
pgpGrab(sigp->signid, 4), pgpGrab(sigp->signid+4, 4), 
pgpGrab(pubp->signid, 4), pgpGrab(pubp->signid+4, 4));
	goto exit;
    }

    /* Compute the message digest. */
    ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);

    {	const char * _fn = rpmExpand(fn, NULL);
	uint8_t * b = NULL;
	ssize_t blen = 0;
	int _rc = rpmioSlurp(_fn, &b, &blen);

	if (!(_rc == 0 && b != NULL && blen > 0)) {
fprintf(stderr, "==> rpmioSlurp(%s) MSG %p[%u] ret %d\n", _fn, b, blen, _rc);
	    b = _free(b);
	    _fn = _free(_fn);
	    goto exit;
	}
	_fn = _free(_fn);
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
	rc = rpmCheckPgpSignatureOnFile(ts, "plaintext", DSAsig, DSApub, NULL);
	rc = rpmCheckPgpSignatureOnFile(ts, "plaintext", DSAsig, NULL, NULL);
    }
    if (!strcmp("RSA", sigtype)) {
	rc = rpmCheckPgpSignatureOnFile(ts, "plaintext", RSAsig, RSApub, NULL);
	rc = rpmCheckPgpSignatureOnFile(ts, "plaintext", RSAsig, NULL, NULL);
    }
    
    return rc;
}

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options:"),
        NULL },
   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};      

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    rpmts ts = NULL;
    int rc;

    pgpImplVecs = &rpmnssImplVecs;
_pgp_debug = 1;
_pgp_print = 1;

    ts = rpmtsCreate();
    (void) rpmtsOpenDB(ts, O_RDONLY);

    rc = doit(ts, "DSA");

    rc = doit(ts, "RSA");

    ts = rpmtsFree(ts);

    if (pgpImplVecs == &rpmnssImplVecs)
	NSS_Shutdown();

    optCon = rpmcliFini(optCon);

    return rc;
}
