#include "system.h"

#include <rpmio.h>
#include <rpmdir.h>
#include <rpmdav.h>
#include <rpmmacro.h>
#include <rpmcb.h>
#include <popt.h>

#define	_RPMPGP_INTERNAL
#include <rpmpgp.h>

#include "debug.h"

static int _debug = 0;
static int _printing = 0;

int noNeon;

#define	HKPPATH		"hkp://keys.rpm5.org"
static char * hkppath = HKPPATH;

static unsigned int keyids[] = {
	0xc2b079fc, 0xf5c75256,
	0x94cd5742, 0xe418e3aa,
	0xb44269d0, 0x4f2a6fd2,
	0xda84cbd4, 0x30c9ecf8,
	0x29d5ba24, 0x8df56d05,
	0xa520e8f1, 0xcba29bf9,
	0x219180cd, 0xdb42a60e,
	0xfd372689, 0x897da07a,
	0xe1385d4e, 0x1cddbca9,
	0xb873641b, 0x2039b291,
	0x58e727c4, 0xc621be0f,
	0x6bddfe8e, 0x54a2acf1,
	0xb873641b, 0x2039b291,
	0
};

struct pgpPkt_s {
    pgpTag tag;
    unsigned int pktlen;
    const rpmuint8_t * h;
    unsigned int hlen;
};

static const rpmuint8_t * pgpGrabSubTagVal(const rpmuint8_t * h, size_t hlen,
		rpmuint8_t subtag, /*@null@*/ size_t * tlenp)
{
    const rpmuint8_t * p = h;
    const rpmuint8_t * pend = h + hlen;
    unsigned plen = 0;
    unsigned len;
rpmuint8_t stag;

fprintf(stderr, "--> %s(%p[%u], 0x%x, %p)\n\t%s\n", __FUNCTION__, h, (unsigned)hlen, subtag, tlenp, pgpHexStr(h, hlen));
    if (tlenp)
	*tlenp = 0;

    while (hlen > 0) {
	len = pgpLen(p, &plen);
fprintf(stderr, "\tlen %u = pgpLen(%p,%p) plen %u\n\t\t%s\n", len, p, &plen, plen, pgpHexStr(p+len, plen));
	p += len;
	hlen -= len;

	stag = (*p & ~PGPSUBTYPE_CRITICAL);
fprintf(stderr, "\t%02X %p[%u] %02X %s", stag, p+1, plen-1, p[plen], pgpValStr(pgpSubTypeTbl, stag));
if (plen)
fprintf(stderr, "\n\t\t%s", pgpHexStr(p+1, plen-1));
fprintf(stderr, "\n");

	if (stag == subtag) {
fprintf(stderr, "\tBINGO %p[%2u]\t%s\n", p+1, plen-1, pgpHexStr(p+1, plen-1));
	    if (tlenp)
		*tlenp = plen-1;
	    return p+1;
	}

	p += plen;
if (p >= pend) break;
	hlen -= plen;
    }
    return NULL;
}

static int readKeys(const char * uri)
{
pgpPkt pp = alloca(sizeof(*pp));
size_t pleft;
DIGEST_CTX pubctx = NULL;
DIGEST_CTX uctx = NULL;
DIGEST_CTX ctx = NULL;
rpmuint8_t goop[6];
pgpHashAlgo dalgo = PGPHASHALGO_SHA1;
    unsigned int * kip;
    rpmuint8_t * pkt;
    size_t pktlen;
    rpmuint8_t ** pkts;
    int npkts;
    rpmuint8_t keyid[8];
    char fn[BUFSIZ];
    pgpDig dig;
    int rc;
    int ec = 0;
    int xx;
    int i;

    dig = pgpDigNew(0);
    for (kip = keyids; *kip; kip += 2) {
	pgpArmor pa;

	sprintf(fn, "%s/pks/lookup?op=get&search=0x%08x%08x", uri, kip[0], kip[1]);
fprintf(stderr, "======================= %s\n", fn);
	pkt = NULL;
	pktlen = 0;
	pa = pgpReadPkts(fn, &pkt, &pktlen);
	if (pa == PGPARMOR_ERROR || pa == PGPARMOR_NONE
         || pkt == NULL || pktlen == 0)
        {
            ec++;
            continue;
        }

	rc = pgpPrtPkts(pkt, pktlen, dig, _printing);
	if (rc)
	    ec++;
#if 0
fprintf(stderr, "%s\n", pgpHexStr(pkt, pktlen));
#endif

	pkts = NULL;
	npkts = 0;
	xx = pgpGrabPkts(pkt, pktlen, &pkts, &npkts);

	if (!pgpPubkeyFingerprint(pkt, pktlen, keyid))
fprintf(stderr, "KEYID: %08x %08x\n", pgpGrab(keyid, 4), pgpGrab(keyid+4, 4));

	pleft = pktlen;
	if (pkts)
	for (i = 0; i < npkts; i++) {
	    xx = pgpPktLen(pkts[i], pleft, pp);
	    pleft -= pp->pktlen;
fprintf(stderr, "%6d %p[%3u] 0x%02x %s\n\t%s\n", i, pkts[i], (unsigned)pp->pktlen, *pkts[i], pgpValStr(pgpTagTbl, (rpmuint8_t)pp->tag), pgpHexStr(pkts[i], pp->pktlen));

	    switch (pp->tag) {
	    default:
		break;
	    case PGPTAG_PUBLIC_KEY:
assert(pubctx == NULL);
		pubctx = rpmDigestInit(dalgo, RPMDIGEST_NONE);
		goop[0] = 0x99;
		goop[1] = (pp->hlen >>  8) & 0xff;
		goop[2] = (pp->hlen      ) & 0xff;
		rpmDigestUpdate(pubctx, goop, 3);
fprintf(stderr, "*** Update(%3d): %s\n", 3, pgpHexStr(goop, 3));
		rpmDigestUpdate(pubctx, pp->h, pp->hlen);
fprintf(stderr, "*** Update(%3d): %s\n", pp->hlen, pgpHexStr(pp->h, pp->hlen));
		break;
	    case PGPTAG_USER_ID:
assert(pubctx != NULL);
if (uctx != NULL) xx = rpmDigestFinal(uctx, NULL, NULL, 0);
		uctx = rpmDigestDup(pubctx);
		goop[0] = *pkts[i];
		goop[1] = (pp->hlen >> 24) & 0xff;
		goop[2] = (pp->hlen >> 16) & 0xff;
		goop[3] = (pp->hlen >>  8) & 0xff;
		goop[4] = (pp->hlen      ) & 0xff;
		rpmDigestUpdate(uctx, goop, 5);
fprintf(stderr, "*** Update(%3d): %s\n", 5, pgpHexStr(goop, 5));
		rpmDigestUpdate(uctx, pp->h, pp->hlen);
fprintf(stderr, "*** Update(%3d): %s\n", pp->hlen, pgpHexStr(pp->h, pp->hlen));
		break;
	    case PGPTAG_PUBLIC_SUBKEY:
assert(pubctx != NULL);
if (ctx != NULL) xx = rpmDigestFinal(ctx, NULL, NULL, 0);
		ctx = rpmDigestDup(pubctx);
		goop[0] = 0x99;
		goop[1] = (pp->hlen >>  8) & 0xff;
		goop[2] = (pp->hlen      ) & 0xff;
		rpmDigestUpdate(ctx, goop, 3);
fprintf(stderr, "*** Update(%3d): %s\n", 3, pgpHexStr(goop, 3));
		rpmDigestUpdate(ctx, pp->h, pp->hlen);
fprintf(stderr, "*** Update(%3d): %s\n", pp->hlen, pgpHexStr(pp->h, pp->hlen));
		break;
	    case PGPTAG_SIGNATURE:
	      {
		rpmuint8_t version;
		rpmuint8_t sigtype;
		rpmuint32_t created;
		rpmuint32_t hashlen;
		const rpmuint8_t * signid;
		const rpmuint8_t * psignhash;
		int selfsigned;

		version = *pp->h;
		if (!(version == 3 || version == 4)) {
fprintf(stderr, "SIGNATURE: V%u\n", version);
fprintf(stderr, "\tSKIP(V%u != V3 | V4)\n", version);
		    break;
		}

		if (version == 3) {
		    pgpPktSigV3 v = (pgpPktSigV3)pp->h;

fprintf(stderr, "SIGNATURE: V%u type(0x%02X) algo(0x%02X) hash(0x%02X) len(%u) time(0x%08X) signid(%s) signhash(0x%04x)\n",
	v->version,
	v->sigtype,
	v->pubkey_algo,
	v->hash_algo,
	v->hashlen,
	pgpGrab(v->time, sizeof(v->time)),
	pgpHexStr(v->signid, sizeof(v->signid)),
	pgpGrab(v->signhash16, sizeof(v->signhash16))
);
if (v->hash_algo != dalgo) {
fprintf(stderr, "\tSKIP(%u != %u)\n", v->hash_algo, dalgo);
break;
}
if (v->sigtype == PGPSIGTYPE_KEY_REVOKE
 || v->sigtype == PGPSIGTYPE_SUBKEY_REVOKE
 || v->sigtype == PGPSIGTYPE_CERT_REVOKE)
{
fprintf(stderr, "\tSKIP(0x%02X REVOKE)\n", v->sigtype);
break;
}

		    sigtype = v->sigtype;
		    created = pgpGrab(v->time, 4);
		    hashlen = v->hashlen;
		    signid = v->signid;
		    psignhash = v->signhash16;

		}

		if (version == 4) {
		    pgpPktSigV4 v = (pgpPktSigV4)pp->h;
		    const rpmuint8_t * phash;
		    size_t nhash;
		    const rpmuint8_t * punhash;
		    size_t nunhash;
		    const rpmuint8_t * p;
		    size_t tlen;

fprintf(stderr, "SIGNATURE: V%u type(0x%02X) algo(0x%02X) hash(0x%02X) len(%u)\n",
	v->version,
	v->sigtype,
	v->pubkey_algo,
	v->hash_algo,
	pgpGrab(v->hashlen, sizeof(v->hashlen))
);
if (v->hash_algo != dalgo) {
fprintf(stderr, "\tSKIP(%u != %u)\n", v->hash_algo, dalgo);
break;
}
if (v->sigtype == PGPSIGTYPE_KEY_REVOKE
 || v->sigtype == PGPSIGTYPE_SUBKEY_REVOKE
 || v->sigtype == PGPSIGTYPE_CERT_REVOKE)
{
fprintf(stderr, "\tSKIP(0x%02X REVOKE)\n", v->sigtype);
break;
}

		    sigtype = v->sigtype;
		    hashlen = sizeof(*v) + pgpGrab(v->hashlen, sizeof(v->hashlen));
		    phash = pp->h + sizeof(*v);
		    nhash = pgpGrab(v->hashlen, sizeof(v->hashlen));
fprintf(stderr, "***     hash %p[%2u]\t%s\n", phash, (unsigned)nhash, pgpHexStr(phash, nhash));
		    nunhash = pgpGrab(phash+nhash, 2);
		    punhash = phash + nhash + 2;
fprintf(stderr, "***   unhash %p[%2u]\t%s\n", punhash, (unsigned)nunhash, pgpHexStr(punhash, nunhash));
		    psignhash = punhash + nunhash;
fprintf(stderr, "*** signhash %p[%2u]\t%s\n", psignhash, (unsigned)2, pgpHexStr(psignhash, 2));

		    tlen = 0;
		    p = pgpGrabSubTagVal(phash, nhash, PGPSUBTYPE_SIG_CREATE_TIME, &tlen);
assert(p != NULL);
assert(tlen == sizeof(created));
		    created = pgpGrab(p, tlen);

		    tlen = 0;
		    signid = pgpGrabSubTagVal(punhash, nunhash, PGPSUBTYPE_ISSUER_KEYID, &tlen);
assert(tlen == sizeof(keyid));
		}

		if (ctx == NULL) {
		    if (uctx == NULL) {
fprintf(stderr, "\tSKIP(uctx %p)\n", uctx);
			break;
		    }
		    ctx = rpmDigestDup(uctx);
		}

		selfsigned = !memcmp(keyid, signid, sizeof(keyid));
fprintf(stderr, "\t%s\t%s\n", (selfsigned ? " SELF" : "OTHER"), pgpHexStr(signid, 8));

		if (selfsigned) {
		    rpmuint8_t * digest = NULL;
		    size_t digestlen = 0;

		    if (version == 3) {
			goop[0] = sigtype;
			goop[1] = (created >> 24) & 0xff;
			goop[2] = (created >> 16) & 0xff;
			goop[3] = (created >>  8) & 0xff;
			goop[4] = (created      ) & 0xff;
fprintf(stderr, "*** Update(%3d): %s\n", 5, pgpHexStr(goop, 5));
		    } else
		    if (version == 4) {
			rpmDigestUpdate(ctx, pp->h, hashlen);
fprintf(stderr, "*** Update(%3d): %s\n", hashlen, pgpHexStr(pp->h, hashlen));

			goop[0] = version;
			goop[1] = (rpmuint8_t)0xff;
			goop[2] = (hashlen >> 24) & 0xff;
			goop[3] = (hashlen >> 16) & 0xff;
			goop[4] = (hashlen >>  8) & 0xff;
			goop[5] = (hashlen      ) & 0xff;
			rpmDigestUpdate(ctx, goop, 6);
fprintf(stderr, "*** Update(%3d): %s\n", 6, pgpHexStr(goop, 6));
		    }

		    xx = rpmDigestFinal(ctx, &digest, &digestlen, 0);
fprintf(stderr, "\t\t%s\n", pgpHexStr(digest, digestlen));
fprintf(stderr, "\t%s\n", (!memcmp(digest, psignhash, 2) ? " GOOD" : "  BAD"));
		    digest = _free(digest);
		    ctx = NULL;
		}
	      }	break;
	    }
	}

	if (uctx)
	    xx = rpmDigestFinal(uctx, NULL, NULL, 0);
	uctx = NULL;
	if (pubctx)
	    xx = rpmDigestFinal(pubctx, NULL, NULL, 0);
	pubctx = NULL;

	pkts = _free(pkts);
	npkts = 0;

	pgpDigClean(dig);

	free((void *)pkt);
	pkt = NULL;
    }
    dig = pgpDigFree(dig);

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
main(int argc, char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, (const char **)argv, optionsTable, 0);
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
