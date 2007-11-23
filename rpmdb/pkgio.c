
/**
 * \file rpmdb/pkgio.c
 */

#include "system.h"

#if defined(HAVE_MACHINE_TYPES_H)
# include <machine/types.h>
#endif

#include <netinet/in.h>

#if defined(HAVE_KEYUTILS_H)
#include <keyutils.h>
#endif

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmcb.h>		/* XXX fnpyKey */
#include <rpmtag.h>

#include <rpmdb.h>
#include <pkgio.h>

#define	_RPMTS_INTERNAL
#include "rpmts.h"

#include <rpmxar.h>

#include "header_internal.h"
#include "signature.h"
#include "debug.h"

/*@access rpmts @*/
/*@access pgpDig @*/
/*@access pgpDigParams @*/
/*@access Header @*/            /* XXX compared with NULL */
/*@access HV_t @*/
/*@access entryInfo @*/
/*@access indexEntry @*/
/*@access FD_t @*/              /* XXX stealing digests */

/*@unchecked@*/
int _use_xar = 0;

/*@unchecked@*/
static int _print_pkts = 0;

/*@-redecl@*/
/*@unchecked@*/
extern int _newmagic;
/*@=redecl@*/

/*@-type@*/
/*@observer@*/ /*@unchecked@*/
static unsigned char sigh_magic[8] = {
	0x8e, 0xad, 0xe8, 0x3e, 0x00, 0x00, 0x00, 0x00
};
/*@=type@*/

/**
 */
/*@-exportheader@*/
/*@unused@*/ ssize_t timedRead(FD_t fd, /*@out@*/ void * bufptr, size_t length)
	/*@globals fileSystem @*/
	/*@modifies fd, *bufptr, fileSystem @*/;
#define	timedRead	(ufdio->read)
/*@=exportheader@*/

/*===============================================*/
/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @return		RPMRC_OK on success
 */
static
rpmRC rpmWriteHeader(FD_t fd, /*@null@*/ Header h, /*@null@*/ const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, h, *msg, fileSystem @*/
{
    const void * uh = NULL;
    ssize_t nb;
    size_t length;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */

    if (h == NULL) {
	if (msg)
	    *msg = xstrdup(_("write of NULL header"));
	goto exit;
    }

    uh = headerUnload(h, &length);
    if (uh == NULL) {
	if (msg)
	    *msg = xstrdup(_("headerUnload failed"));
	goto exit;
    }

    {   unsigned char * hmagic = NULL;
	size_t nmagic = 0;

	(void) headerGetMagic(NULL, &hmagic, &nmagic);
	nb = Fwrite(hmagic, sizeof(hmagic[0]), nmagic, fd);
	if (nb != nmagic || Ferror(fd)) {
	    if (msg)
		*msg = (nb > 0
			? xstrdup(_("short write of header magic"))
			: xstrdup(Fstrerror(fd)) );
	    goto exit;
	}
    }

    /*@-sizeoftype@*/
    nb = Fwrite(uh, sizeof(char), length, fd);
    /*@=sizeoftype@*/
    if (nb != length || Ferror(fd)) {
	if (msg)
	    *msg = (nb > 0
		    ? xstrdup(_("short write of header"))
		    : xstrdup(Fstrerror(fd)) );
	    goto exit;
    }
    rc = RPMRC_OK;

exit:
    uh = _free(uh);
    return rc;
}

/*===============================================*/

rpmop rpmtsOp(rpmts ts, rpmtsOpX opx)
{
    rpmop op = NULL;

    if (ts != NULL && opx >= 0 && opx < RPMTS_OP_MAX)
	op = ts->ops + opx;
/*@-usereleased -compdef @*/
    return op;
/*@=usereleased =compdef @*/
}

pgpDigParams rpmtsPubkey(const rpmts ts)
{
/*@-onlytrans@*/
    return pgpGetPubkey(rpmtsDig(ts));
/*@=onlytrans@*/
}

rpmdb rpmtsGetRdb(rpmts ts)
{
    rpmdb rdb = NULL;
    if (ts != NULL) {
	rdb = ts->rdb;
    }
/*@-compdef -refcounttrans -usereleased @*/
    return rdb;
/*@=compdef =refcounttrans =usereleased @*/
}

rpmRC rpmtsFindPubkey(rpmts ts, void * _dig)
{
    HGE_t hge = headerGetExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    pgpDig dig = (_dig ? _dig : rpmtsDig(ts));
    const void * sig = pgpGetSig(dig);
    pgpDigParams sigp = pgpGetSignature(dig);
    pgpDigParams pubp = pgpGetPubkey(dig);
    rpmRC res = RPMRC_NOKEY;
    const char * pubkeysource = NULL;
#if defined(HAVE_KEYUTILS_H)
    int krcache = 1;	/* XXX assume pubkeys are cached in keyutils keyring. */
#endif
    int xx;

assert(sig != NULL);
assert(dig != NULL);
assert(sigp != NULL);
assert(pubp != NULL);
assert(rpmtsDig(ts) == dig);

#if 0
fprintf(stderr, "==> find sig id %08x %08x ts pubkey id %08x %08x\n",
pgpGrab(sigp->signid, 4), pgpGrab(sigp->signid+4, 4),
pgpGrab(ts->pksignid, 4), pgpGrab(ts->pksignid+4, 4));
#endif

    /* Lazy free of previous pubkey if pubkey does not match this signature. */
    if (memcmp(sigp->signid, ts->pksignid, sizeof(ts->pksignid))) {
#if 0
fprintf(stderr, "*** free pkt %p[%d] id %08x %08x\n", ts->pkpkt, ts->pkpktlen, pgpGrab(ts->pksignid, 4), pgpGrab(ts->pksignid+4, 4));
#endif
	ts->pkpkt = _free(ts->pkpkt);
	ts->pkpktlen = 0;
	memset(ts->pksignid, 0, sizeof(ts->pksignid));
    }

#if defined(HAVE_KEYUTILS_H)
	/* Try keyutils keyring lookup. */
    if (krcache && ts->pkpkt == NULL) {
	key_serial_t keyring = KEY_SPEC_PROCESS_KEYRING;
	const char * krprefix = "rpm:gpg:pubkey:";
	char krfp[32];
	char * krn = alloca(strlen(krprefix) + sizeof("12345678"));
	long key;

	(void) snprintf(krfp, sizeof(krfp), "%08X", pgpGrab(sigp->signid+4, 4));
	krfp[sizeof(krfp)-1] = '\0';
	*krn = '\0';
	(void) stpcpy( stpcpy(krn, krprefix), krfp);

/*@-moduncon@*/
	key = keyctl_search(keyring, "user", krn, 0);
	xx = keyctl_read(key, NULL, 0);
	if (xx > 0) {
	    ts->pkpktlen = xx;
	    ts->pkpkt = NULL;
	    xx = keyctl_read_alloc(key, (void **)&ts->pkpkt);
	    if (xx > 0) {
		pubkeysource = xstrdup(krn);
		krcache = 0;	/* XXX don't bother caching. */
	    } else {
		ts->pkpkt = _free(ts->pkpkt);
		ts->pkpktlen = 0;
	    }
	}
/*@=moduncon@*/
    }
#endif

    /* Try rpmdb keyring lookup. */
    if (ts->pkpkt == NULL) {
	unsigned hx = 0xffffffff;
	unsigned ix = 0xffffffff;
	rpmdbMatchIterator mi;
	Header h;

	/* Retrieve the pubkey that matches the signature. */
	he->tag = RPMTAG_PUBKEYS;
	mi = rpmdbInitIterator(rpmtsGetRdb(ts), RPMTAG_PUBKEYS, sigp->signid, sizeof(sigp->signid));
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (!hge(h, he, 0))
		continue;
	    hx = rpmdbGetIteratorOffset(mi);
	    ix = rpmdbGetIteratorFileNum(mi);
/*@-moduncon -nullstate @*/
	    if (ix >= (unsigned) he->c
	     || b64decode(he->p.argv[ix], (void **) &ts->pkpkt, &ts->pkpktlen))
		ix = 0xffffffff;
/*@=moduncon =nullstate @*/
	    he->p.ptr = _free(he->p.ptr);
	    break;
	}
	mi = rpmdbFreeIterator(mi);

	if (ix < 0xffffffff) {
	    char hnum[32];
	    sprintf(hnum, "h#%u", hx);
	    pubkeysource = xstrdup(hnum);
	} else {
	    ts->pkpkt = _free(ts->pkpkt);
	    ts->pkpktlen = 0;
	}
    }

    /* Try keyserver lookup. */
    if (ts->pkpkt == NULL) {
	const char * fn = rpmExpand("%{_hkp_keyserver_query}",
			pgpHexStr(sigp->signid, sizeof(sigp->signid)), NULL);

	xx = 0;
	if (fn && *fn != '%') {
	    xx = (pgpReadPkts(fn, (const byte **)&ts->pkpkt, &ts->pkpktlen) != PGPARMOR_PUBKEY);
	}
	fn = _free(fn);
	if (xx) {
	    ts->pkpkt = _free(ts->pkpkt);
	    ts->pkpktlen = 0;
	} else {
	    /* Save new pubkey in local ts keyring for delayed import. */
	    pubkeysource = xstrdup("keyserver");
	}
    }

#ifdef	NOTNOW
    /* Try filename from macro lookup. */
    if (ts->pkpkt == NULL) {
	const char * fn = rpmExpand("%{_gpg_pubkey}", NULL);

	xx = 0;
	if (fn && *fn != '%')
	    xx = (pgpReadPkts(fn,&ts->pkpkt,&ts->pkpktlen) != PGPARMOR_PUBKEY);
	fn = _free(fn);
	if (xx) {
	    ts->pkpkt = _free(ts->pkpkt);
	    ts->pkpktlen = 0;
	} else {
	    pubkeysource = xstrdup("macro");
	}
    }
#endif

    /* Was a matching pubkey found? */
    if (ts->pkpkt == NULL || ts->pkpktlen == 0)
	goto exit;

    /* Retrieve parameters from pubkey packet(s). */
    xx = pgpPrtPkts((byte *)ts->pkpkt, ts->pkpktlen, dig, 0);

    /* Do the parameters match the signature? */
    if (sigp->pubkey_algo == pubp->pubkey_algo
#ifdef	NOTYET
     && sigp->hash_algo == pubp->hash_algo
#endif
     &&	!memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid)) )
    {

	/* XXX Verify any pubkey signatures. */

#if defined(HAVE_KEYUTILS_H)
	/* Save the pubkey in the keyutils keyring. */
	if (krcache) {
	    key_serial_t keyring = KEY_SPEC_PROCESS_KEYRING;
	    const char * krprefix = "rpm:gpg:pubkey:";
	    char krfp[32];
	    char * krn = alloca(strlen(krprefix) + sizeof("12345678"));

	    (void) snprintf(krfp, sizeof(krfp), "%08X", pgpGrab(sigp->signid+4, 4));
	    krfp[sizeof(krfp)-1] = '\0';
	    *krn = '\0';
	    (void) stpcpy( stpcpy(krn, krprefix), krfp);
/*@-moduncon -noeffectuncon @*/
	    (void) add_key("user", krn, ts->pkpkt, ts->pkpktlen, keyring);
/*@=moduncon =noeffectuncon @*/
	}
#endif

	/* Pubkey packet looks good, save the signer id. */
	memcpy(ts->pksignid, pubp->signid, sizeof(ts->pksignid));

	if (pubkeysource)
	    rpmlog(RPMLOG_DEBUG, "========== %s pubkey id %08x %08x (%s)\n",
		(sigp->pubkey_algo == PGPPUBKEYALGO_DSA ? "DSA" :
		(sigp->pubkey_algo == PGPPUBKEYALGO_RSA ? "RSA" : "???")),
		pgpGrab(sigp->signid, 4), pgpGrab(sigp->signid+4, 4),
		pubkeysource);

	res = RPMRC_OK;
    }

exit:
    pubkeysource = _free(pubkeysource);
    if (res != RPMRC_OK) {
	ts->pkpkt = _free(ts->pkpkt);
	ts->pkpktlen = 0;
    }
    return res;
}

pgpDig rpmtsDig(rpmts ts)
{
/*@-mods@*/ /* FIX: hide lazy malloc for now */
    if (ts->dig == NULL) {
	ts->dig = pgpDigNew(0);
/*@-refcounttrans@*/
	(void) pgpSetFindPubkey(ts->dig, (int (*)(void *, void *))rpmtsFindPubkey, ts);
/*@=refcounttrans@*/
    }
/*@=mods@*/
/*@-compdef -retexpose -usereleased@*/
    return ts->dig;
/*@=compdef =retexpose =usereleased@*/
}

void rpmtsCleanDig(rpmts ts)
{
    if (ts && ts->dig) {
	int opx;
	opx = RPMTS_OP_DIGEST;
	(void) rpmswAdd(rpmtsOp(ts, opx), pgpStatsAccumulator(ts->dig, opx));
	opx = RPMTS_OP_SIGNATURE;
	(void) rpmswAdd(rpmtsOp(ts, opx), pgpStatsAccumulator(ts->dig, opx));
/*@-onlytrans@*/
	ts->dig = pgpDigFree(ts->dig);
/*@=onlytrans@*/
    }
}

/*===============================================*/

/**
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead {
    unsigned char magic[4];
    unsigned char major;
    unsigned char minor;
    unsigned short type;
    unsigned short archnum;
    char name[66];
    unsigned short osnum;
    unsigned short signature_type; /*!< Signature type (RPMSIG_HEADERSIG) */
/*@unused@*/
    char reserved[16];		/*!< Pad to 96 bytes -- 8 byte aligned! */
} ;

/*@unchecked@*/
int _nolead = SUPPORT_RPMLEAD;

/*@-type@*/
/*@unchecked@*/ /*@observer@*/
static unsigned char lead_magic[] = {
    0xed, 0xab, 0xee, 0xdb, 0x00, 0x00, 0x00, 0x00
};
/*@=type@*/

/* The lead needs to be 8 byte aligned */

/**
 * Write lead to file handle.
 * @param fd		file handle
 * @param ptr		package lead
 * @param *msg		name to include in lead (or NULL)
 * @return		RPMRC_OK on success, RPMRC_FAIL on error
 */
static rpmRC wrLead(FD_t fd, const void * ptr, const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, fileSystem @*/
{
    struct rpmlead l;

    memcpy(&l, ptr, sizeof(l));

    /* Set some sane defaults */
    if ((int)l.major == 0)
	l.major = (unsigned char) 3;
    if (l.signature_type == 0)
	l.signature_type = 5;		/* RPMSIGTYPE_HEADERSIG */
    if (msg && *msg)
	(void) strncpy(l.name, *msg, sizeof(l.name));
    
    memcpy(&l.magic, lead_magic, sizeof(l.magic));
    l.type = (unsigned short) htons(l.type);
    l.archnum = (unsigned short) htons(l.archnum);
    l.osnum = (unsigned short) htons(l.osnum);
    l.signature_type = (unsigned short) htons(l.signature_type);
	
    if (Fwrite(&l, 1, sizeof(l), fd) != sizeof(l))
	return RPMRC_FAIL;

    return RPMRC_OK;
}

/**
 * Read lead from file handle.
 * @param fd		file handle
 * @retval *ptr		package lead
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC rdLead(FD_t fd, /*@out@*/ /*@null@*/ void * ptr,
		const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, *ptr, *msg, fileSystem @*/
{
    rpmxar xar = fdGetXAR(fd);
    struct rpmlead ** leadp = ptr;
    struct rpmlead * l = xcalloc(1, sizeof(*l));
    char buf[BUFSIZ];
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;

    buf[0] = '\0';
    if (leadp != NULL) *leadp = NULL;

    /* Read the first 96 bytes of the file. */
    if ((xx = (int) timedRead(fd, (char *)l, sizeof(*l))) != (int) sizeof(*l)) {
	if (Ferror(fd)) {
	    (void) snprintf(buf, sizeof(buf),
		_("lead size(%u): BAD, read(%d), %s(%d)"),
		(unsigned)sizeof(*l), xx, Fstrerror(fd), errno);
	    rc = RPMRC_FAIL;
	} else {
	    (void) snprintf(buf, sizeof(buf),
		_("lead size(%u): BAD, read(%d), %s(%d)"),
		(unsigned)sizeof(*l), xx, strerror(errno), errno);
	    rc = RPMRC_NOTFOUND;
	}
	goto exit;
    }

    /* Attach rpmxar handler to fd if this is a xar archive. */
    if (xar == NULL) {
	unsigned char * bh = (unsigned char *)l;
	if (bh[0] == 'x' && bh[1] == 'a' && bh[2] == 'r' && bh[3] == '!') {
	    const char * fn = fdGetOPath(fd);
	    xar = rpmxarNew(fn, "r");
	    fdSetXAR(fd, xar);
	    (void) rpmxarFree(xar);
	}
    }

    /* With XAR, read lead from a xar archive file called "Lead". */
    xar = fdGetXAR(fd);
    if (xar != NULL) {
	void *b = NULL;
	size_t nb = 0;
	const char item[] = "Lead";
	if ((xx = rpmxarNext(xar)) != 0 || (xx = rpmxarPull(xar, item)) != 0) {
	    (void) snprintf(buf, sizeof(buf),
		_("XAR file not found (or no XAR support)"));
	    rc = RPMRC_NOTFOUND;
	    goto exit;
	}
	(void) rpmxarSwapBuf(xar, NULL, 0, (char **)&b, &nb);
	if (nb != sizeof(*l)) {
	    b = _free(b);
	    (void) snprintf(buf, sizeof(buf),
		_("lead size(%u): BAD, xar read(%u)"),
		(unsigned)sizeof(*l), (unsigned)nb);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	memcpy(l, b, nb);
	b = _free(b);
    }

    l->type = (unsigned short) ntohs(l->type);
    l->archnum = (unsigned short) ntohs(l->archnum);
    l->osnum = (unsigned short) ntohs(l->osnum);
    l->signature_type = (unsigned short) ntohs(l->signature_type);

    if (memcmp(l->magic, lead_magic, sizeof(l->magic))) {
	(void) snprintf(buf, sizeof(buf), _("lead magic: BAD, read %02x%02x%02x%02x"), l->magic[0], l->magic[1], l->magic[2], l->magic[3]);
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    switch (l->major) {
    default:
	(void) snprintf(buf, sizeof(buf),
		_("lead version(%u): UNSUPPORTED"), (unsigned) l->major);
	rc = RPMRC_NOTFOUND;
	goto exit;
	/*@notreached@*/ break;
    case 3:
    case 4:
	break;
    }

    if (l->signature_type != 5) {	/* RPMSIGTYPE_HEADERSIG */
	(void) snprintf(buf, sizeof(buf),
		_("sigh type(%u): UNSUPPORTED"), (unsigned) l->signature_type);
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    rc = RPMRC_OK;

exit:
    if (rc == RPMRC_OK && leadp != NULL)
	*leadp = l;
    else
	l = _free(l);
	
    if (msg != NULL && buf[0] != '\0') {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }
    return rc;
}

/*===============================================*/

/**
 * Write signature header.
 * @param fd		file handle
 * @param ptr		signature header
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC wrSignature(FD_t fd, void * ptr, /*@unused@*/ const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, ptr, *msg, fileSystem @*/
{
    Header sigh = ptr;
    static unsigned char zero[8]
	= { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
    size_t sigSize;
    size_t pad;
    rpmRC rc = RPMRC_OK;

    rc = rpmWriteHeader(fd, sigh, msg);
    if (rc != RPMRC_OK)
	return rc;

    sigSize = headerSizeof(sigh);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
	if (Fwrite(zero, sizeof(zero[0]), pad, fd) != pad)
	    rc = RPMRC_FAIL;
    }
    rpmlog(RPMLOG_DEBUG, D_("Signature: size(%u)+pad(%u)\n"), (unsigned)sigSize, (unsigned)pad);
    return rc;
}

/**
 * Print package size.
 * @todo rpmio: use fdSize rather than fstat(2) to get file size.
 * @param fd			package file handle
 * @param siglen		signature header size
 * @param pad			signature padding
 * @param datalen		length of header+payload
 * @return 			rpmRC return code
 */
static inline rpmRC printSize(FD_t fd, size_t siglen, size_t pad, size_t datalen)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int fdno = Fileno(fd);
    struct stat sb, * st = &sb;
    size_t expected;
    size_t nl = rpmpkgSizeof("Lead", NULL);

    /* HACK: workaround for davRead wiring. */
    if (fdno == 123456789) {
	st->st_size = 0;
	st->st_size -= nl + siglen + pad + datalen;
    } else
    if (fstat(fdno, st) < 0)
	return RPMRC_FAIL;

    expected = nl + siglen + pad + datalen;
    rpmlog(RPMLOG_DEBUG,
	D_("Expected size: %12lu = lead(%u)+sigs(%u)+pad(%u)+data(%lu)\n"),
		(unsigned long)expected,
		(unsigned)nl, (unsigned) siglen, (unsigned) pad,
		(unsigned long)datalen);
    rpmlog(RPMLOG_DEBUG,
	D_("  Actual size: %12lu\n"), (unsigned long)st->st_size);

    return RPMRC_OK;
}

/**
 * Read (and verify header+payload size) signature header.
 * @param fd		file handle
 * @retval *ptr		signature header (or NULL)
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC rdSignature(FD_t fd, /*@out@*/ /*@null@*/ void * ptr,
		const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies *ptr, *msg, fileSystem @*/
{
rpmxar xar = fdGetXAR(fd);
    HGE_t hge = headerGetExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    Header * sighp = ptr;
    char buf[BUFSIZ];
    uint32_t block[4];
    uint32_t il;
    uint32_t dl;
    uint32_t * ei = NULL;
    entryInfo pe;
    size_t nb;
    uint32_t ril = 0;
    indexEntry entry = memset(alloca(sizeof(*entry)), 0, sizeof(*entry));
    entryInfo info = memset(alloca(sizeof(*info)), 0, sizeof(*info));
    unsigned char * dataStart;
    unsigned char * dataEnd = NULL;
    Header sigh = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;
    uint32_t i;

    buf[0] = '\0';
    if (sighp)
	*sighp = NULL;

    memset(block, 0, sizeof(block));
    if (xar != NULL) {
	const char item[] = "Signature";
	if ((xx = rpmxarNext(xar)) != 0 || (xx = rpmxarPull(xar, item)) != 0) {
	    (void) snprintf(buf, sizeof(buf),
		_("XAR file not found (or no XAR support)"));
	    rc = RPMRC_NOTFOUND;
	    goto exit;
	}
    }
    if ((xx = (int) timedRead(fd, (void *)block, sizeof(block))) != (int) sizeof(block)) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh size(%d): BAD, read returned %d"), (int)sizeof(block), xx);
	goto exit;
    }

    {   unsigned char * hmagic = NULL;
	size_t nmagic = 0;

	(void) headerGetMagic(NULL, &hmagic, &nmagic);
	if (_newmagic)	/* XXX FIXME: sigh needs its own magic. */
	    hmagic = sigh_magic;

	if (memcmp(block, hmagic, nmagic)) {
	    unsigned char * x = (unsigned char *)block;
	    (void) snprintf(buf, sizeof(buf), _("sigh magic: BAD, read %02x%02x%02x%02x%02x%02x%02x%02x"), x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]);
	    goto exit;
	}
    }
    il = (uint32_t) ntohl(block[2]);
    if (il > 32) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh tags: BAD, no. of tags(%u) out of range"), (unsigned) il);
	goto exit;
    }
    dl = (uint32_t) ntohl(block[3]);
    if (dl > 8192) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh data: BAD, no. of bytes(%u) out of range"), (unsigned) dl);
	goto exit;
    }

/*@-sizeoftype@*/
    nb = (il * sizeof(struct entryInfo_s)) + dl;
/*@=sizeoftype@*/
    ei = xmalloc(sizeof(il) + sizeof(dl) + nb);
    if ((xx = (int) timedRead(fd, (void *)&ei[2], nb)) != (int) nb) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh blob(%u): BAD, read returned %d"), (unsigned) nb, xx);
	goto exit;
    }
    ei[0] = block[2];
    ei[1] = block[3];
    pe = (entryInfo) &ei[2];
    dataStart = (unsigned char *) (pe + il);
    
    /* Check (and convert) the 1st tag element. */
    xx = headerVerifyInfo(1, dl, pe, &entry->info, 0);
    if (xx != -1) {
	(void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %u type %u offset %d count %u"),
		0, (unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag? */
/*@-sizeoftype@*/
    if (entry->info.tag == RPMTAG_HEADERSIGNATURES
       && entry->info.type == RPM_BIN_TYPE
       && entry->info.count == REGION_TAG_COUNT)
    {
/*@=sizeoftype@*/

assert(entry->info.offset > 0);	/* XXX insurance */
	if (entry->info.offset >= (int32_t)dl) {
	    (void) snprintf(buf, sizeof(buf),
		_("region offset: BAD, tag %u type %u offset %d count %u"),
		(unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	    goto exit;
	}

	/* Is there an immutable header region tag trailer? */
	dataEnd = dataStart + entry->info.offset;
/*@-sizeoftype@*/
	(void) memcpy(info, dataEnd, REGION_TAG_COUNT);
	/* XXX Really old packages have HEADER_IMAGE, not HEADER_SIGNATURES. */
	if (info->tag == (uint32_t) htonl(RPMTAG_HEADERIMAGE)) {
	    uint32_t stag = (uint32_t) htonl(RPMTAG_HEADERSIGNATURES);
	    info->tag = stag;
	    memcpy(dataEnd, &stag, sizeof(stag));
	}
	dataEnd += REGION_TAG_COUNT;

	xx = headerVerifyInfo(1, dl, info, &entry->info, 1);
	if (xx != -1 ||
	    !(entry->info.tag == RPMTAG_HEADERSIGNATURES
	   && entry->info.type == RPM_BIN_TYPE
	   && entry->info.count == REGION_TAG_COUNT))
	{
	    (void) snprintf(buf, sizeof(buf),
		_("region trailer: BAD, tag %u type %u offset %d count %u"),
		(unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	    goto exit;
	}
/*@=sizeoftype@*/
	memset(info, 0, sizeof(*info));

	/* Is the no. of tags in the region less than the total no. of tags? */
	ril = (uint32_t) entry->info.offset/sizeof(*pe);
	if ((entry->info.offset % sizeof(*pe)) || ril > il) {
	    (void) snprintf(buf, sizeof(buf),
		_("region size: BAD, ril(%u) > il(%u)"), (unsigned) ril, (unsigned) il);
	    goto exit;
	}
    }

    /* Sanity check signature tags */
    memset(info, 0, sizeof(*info));
    for (i = 1; i < (unsigned) il; i++) {
	xx = headerVerifyInfo(1, dl, pe+i, &entry->info, 0);
	if (xx != -1) {
	    (void) snprintf(buf, sizeof(buf),
		_("sigh tag[%u]: BAD, tag %u type %u offset %d count %u"),
		(unsigned) i, (unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	    goto exit;
	}
    }

    /* OK, blob looks sane, load the header. */
    sigh = headerLoad(ei);
    if (sigh == NULL) {
	(void) snprintf(buf, sizeof(buf), _("sigh load: BAD"));
	goto exit;
    }
    sigh->flags |= HEADERFLAG_ALLOCATED;
    sigh->flags |= HEADERFLAG_SIGNATURE;
    if (_newmagic)	/* XXX FIXME: sigh needs its own magic. */
	(void) headerSetMagic(sigh, sigh_magic, sizeof(sigh_magic));

    {	size_t sigSize = headerSizeof(sigh);
	size_t pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */

	/* Position at beginning of header. */
	if (pad && (xx = (int) timedRead(fd, (void *)block, pad)) != (int) pad)
	{
	    (void) snprintf(buf, sizeof(buf),
		_("sigh pad(%u): BAD, read %d bytes"), (unsigned) pad, xx);
	    goto exit;
	}

	/* Print package component sizes. */

	he->tag = (rpmTag) RPMSIGTAG_SIZE;
	xx = hge(sigh, he, 0);
	if (xx) {
	    size_t datasize = he->p.ui32p[0];
	    rc = printSize(fd, sigSize, pad, datasize);
	    if (rc != RPMRC_OK)
		(void) snprintf(buf, sizeof(buf),
			_("sigh sigSize(%u): BAD, fstat(2) failed"), (unsigned) sigSize);
	}
	he->p.ptr = _free(he->p.ptr);
    }

exit:
    if (sighp && sigh && rc == RPMRC_OK)
	*sighp = headerLink(sigh);
    sigh = headerFree(sigh);

    if (msg != NULL) {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }

    return rc;
}

/*===============================================*/

/**
 * Check header consistency, performing headerGetEntry() the hard way.
 *
 * Sanity checks on the header are performed while looking for a
 * header-only digest or signature to verify the blob. If found,
 * the digest or signature is verified.
 *
 * @param ts		transaction set
 * @param uh		unloaded header blob
 * @param uc		no. of bytes in blob (or 0 to disable)
 * @retval *msg		signature verification msg
 * @return		RPMRC_OK/RPMRC_NOTFOUND/RPMRC_FAIL
 */
rpmRC headerCheck(pgpDig dig, const void * uh, size_t uc, const char ** msg)
{
    char buf[8*BUFSIZ];
    uint32_t * ei = (uint32_t *) uh;
    uint32_t il = (uint32_t) ntohl(ei[0]);
    uint32_t dl = (uint32_t) ntohl(ei[1]);
/*@-castexpose@*/
    entryInfo pe = (entryInfo) &ei[2];
/*@=castexpose@*/
    uint32_t ildl[2];
    size_t pvlen = sizeof(ildl) + (il * sizeof(*pe)) + dl;
    unsigned char * dataStart = (unsigned char *) (pe + il);
    indexEntry entry = memset(alloca(sizeof(*entry)), 0, sizeof(*entry));
    entryInfo info = memset(alloca(sizeof(*info)), 0, sizeof(*info));
    const void * sig = NULL;
    unsigned char * b;
    rpmVSFlags vsflags = pgpGetVSFlags(dig);
    rpmop op;
    size_t siglen = 0;
    int blen;
    size_t nb;
    uint32_t ril = 0;
    unsigned char * regionEnd = NULL;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int xx;
    uint32_t i;

    buf[0] = '\0';

    /* Is the blob the right size? */
    if (uc > 0 && pvlen != uc) {
	(void) snprintf(buf, sizeof(buf),
		_("blob size(%d): BAD, 8 + 16 * il(%u) + dl(%u)"),
		(int)uc, (unsigned)il, (unsigned)dl);
	goto exit;
    }

    /* Check (and convert) the 1st tag element. */
    xx = headerVerifyInfo(1, dl, pe, &entry->info, 0);
    if (xx != -1) {
	(void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %u type %u offset %d count %u"),
		0, (unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag? */
/*@-sizeoftype@*/
    if (!(entry->info.tag == RPMTAG_HEADERIMMUTABLE
       && entry->info.type == RPM_BIN_TYPE
       && entry->info.count == REGION_TAG_COUNT))
    {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }
/*@=sizeoftype@*/

    /* Is the offset within the data area? */
    if (entry->info.offset >= (unsigned) dl) {
	(void) snprintf(buf, sizeof(buf),
		_("region offset: BAD, tag %u type %u offset %d count %u"),
		(unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag trailer? */
    regionEnd = dataStart + entry->info.offset;
/*@-sizeoftype@*/
    (void) memcpy(info, regionEnd, REGION_TAG_COUNT);
    regionEnd += REGION_TAG_COUNT;

    xx = headerVerifyInfo(1, dl, info, &entry->info, 1);
    if (xx != -1 ||
	!(entry->info.tag == RPMTAG_HEADERIMMUTABLE
       && entry->info.type == RPM_BIN_TYPE
       && entry->info.count == REGION_TAG_COUNT))
    {
	(void) snprintf(buf, sizeof(buf),
		_("region trailer: BAD, tag %u type %u offset %d count %u"),
		(unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	goto exit;
    }
/*@=sizeoftype@*/
    memset(info, 0, sizeof(*info));

    /* Is the no. of tags in the region less than the total no. of tags? */
    ril = (uint32_t) entry->info.offset/sizeof(*pe);
    if ((entry->info.offset % sizeof(*pe)) || ril > il) {
	(void) snprintf(buf, sizeof(buf),
		_("region size: BAD, ril(%u) > il(%u)"), (unsigned) ril, (unsigned)il);
	goto exit;
    }

    /* Find a header-only digest/signature tag. */
    for (i = ril; i < (unsigned) il; i++) {
	xx = headerVerifyInfo(1, dl, pe+i, &entry->info, 0);
	if (xx != -1) {
	    (void) snprintf(buf, sizeof(buf),
		_("tag[%u]: BAD, tag %u type %u offset %d count %u"),
		(unsigned) i, (unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	    goto exit;
	}

	switch (entry->info.tag) {
	case RPMTAG_SHA1HEADER:
	    if (vsflags & RPMVSF_NOSHA1HEADER)
		/*@switchbreak@*/ break;
	    blen = 0;
	    for (b = dataStart + entry->info.offset; *b != '\0'; b++) {
		if (strchr("0123456789abcdefABCDEF", *b) == NULL)
		    /*@innerbreak@*/ break;
		blen++;
	    }
	    if (entry->info.type != RPM_STRING_TYPE || *b != '\0' || blen != 40)
	    {
		(void) snprintf(buf, sizeof(buf), _("hdr SHA1: BAD, not hex"));
		goto exit;
	    }
	    if (info->tag == 0) {
		*info = entry->info;	/* structure assignment */
		siglen = blen + 1;
	    }
	    /*@switchbreak@*/ break;
	case RPMTAG_RSAHEADER:
	    if (vsflags & RPMVSF_NORSAHEADER)
		/*@switchbreak@*/ break;
	    if (entry->info.type != RPM_BIN_TYPE) {
		(void) snprintf(buf, sizeof(buf), _("hdr RSA: BAD, not binary"));
		goto exit;
	    }
	    *info = entry->info;	/* structure assignment */
	    siglen = info->count;
	    /*@switchbreak@*/ break;
	case RPMTAG_DSAHEADER:
	    if (vsflags & RPMVSF_NODSAHEADER)
		/*@switchbreak@*/ break;
	    if (entry->info.type != RPM_BIN_TYPE) {
		(void) snprintf(buf, sizeof(buf), _("hdr DSA: BAD, not binary"));
		goto exit;
	    }
	    *info = entry->info;	/* structure assignment */
	    siglen = info->count;
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
    }
    rc = RPMRC_NOTFOUND;

exit:
    /* Return determined RPMRC_OK/RPMRC_FAIL conditions. */
    if (rc != RPMRC_NOTFOUND) {
	buf[sizeof(buf)-1] = '\0';
	if (msg) *msg = xstrdup(buf);
	return rc;
    }

    /* If no header-only digest/signature, then do simple sanity check. */
    if (info->tag == 0) {
	xx = (ril > 0 ? headerVerifyInfo(ril-1, dl, pe+1, &entry->info, 0) : -1);
	if (xx != -1) {
	    (void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %u type %u offset %d count %u"),
		xx+1, (unsigned) entry->info.tag, (unsigned) entry->info.type,
		(int)entry->info.offset, (unsigned) entry->info.count);
	    rc = RPMRC_FAIL;
	} else {
	    (void) snprintf(buf, sizeof(buf), "Header sanity check: OK");
	    rc = RPMRC_OK;
	}
	buf[sizeof(buf)-1] = '\0';
	if (msg) *msg = xstrdup(buf);
	return rc;
    }

    /* Verify header-only digest/signature. */
assert(dig != NULL);
    dig->nbytes = 0;

    sig = memcpy(xmalloc(siglen), dataStart + info->offset, siglen);
    {
	const void * osig = pgpGetSig(dig);
/*@-modobserver -observertrans -dependenttrans @*/	/* FIX: pgpSetSig() lazy free. */
	osig = _free(osig);
/*@=modobserver =observertrans =dependenttrans @*/
	(void) pgpSetSig(dig, info->tag, info->type, sig, info->count);
    }

    switch (info->tag) {
    case RPMTAG_RSAHEADER:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, info->count, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping header with unverifiable V%u signature\n"),
		(unsigned) dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	ildl[0] = (uint32_t) htonl(ril);
	ildl[1] = (uint32_t) (regionEnd - dataStart);
	ildl[1] = (uint32_t) htonl(ildl[1]);

	op = pgpStatsAccumulator(dig, 10);	/* RPMTS_OP_DIGEST */
	(void) rpmswEnter(op, 0);
	dig->hdrmd5ctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);

	b = NULL; nb = 0;
	(void) headerGetMagic(NULL, &b, &nb);
	if (b && nb > 0) {
	    (void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
	    dig->nbytes += nb;
	}

	b = (unsigned char *) ildl;
	nb = sizeof(ildl);
	(void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
	dig->nbytes += nb;

	b = (unsigned char *) pe;
	nb = (size_t) (htonl(ildl[0]) * sizeof(*pe));
	(void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
	dig->nbytes += nb;

	b = (unsigned char *) dataStart;
	nb = (size_t) htonl(ildl[1]);
	(void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
	dig->nbytes += nb;
	(void) rpmswExit(op, dig->nbytes);

	break;
    case RPMTAG_DSAHEADER:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, info->count, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping header with unverifiable V%u signature\n"),
		(unsigned) dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	/*@fallthrough@*/
    case RPMTAG_SHA1HEADER:
	ildl[0] = (uint32_t) htonl(ril);
	ildl[1] = (uint32_t) (regionEnd - dataStart);
	ildl[1] = (uint32_t) htonl(ildl[1]);

	op = pgpStatsAccumulator(dig, 10);	/* RPMTS_OP_DIGEST */
	(void) rpmswEnter(op, 0);
	dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);

	b = NULL; nb = 0;
	(void) headerGetMagic(NULL, &b, &nb);
	if (b && nb > 0) {
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
	    dig->nbytes += nb;
	}

	b = (unsigned char *) ildl;
	nb = sizeof(ildl);
	(void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
	dig->nbytes += nb;

	b = (unsigned char *) pe;
	nb = (size_t) (htonl(ildl[0]) * sizeof(*pe));
	(void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
	dig->nbytes += nb;

	b = (unsigned char *) dataStart;
	nb = (size_t) htonl(ildl[1]);
	(void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
	dig->nbytes += nb;
	(void) rpmswExit(op, dig->nbytes);

	break;
    default:
	sig = _free(sig);
	break;
    }

    buf[0] = '\0';
    rc = rpmVerifySignature(dig, buf);

    buf[sizeof(buf)-1] = '\0';
    if (msg) *msg = xstrdup(buf);

    return rc;
}

/**
 * Return size of Header.
 * @param ptr		metadata header (at least 32 bytes)
 * @return		size of header
 */
static size_t szHeader(/*@null@*/ const void * ptr)
	/*@*/
{
    uint32_t p[4];
assert(ptr != NULL);
    memcpy(p, ptr, sizeof(p));
    return (8 + 8 + 16 * ntohl(p[2]) + ntohl(p[3]));
}

/*@-globuse@*/
/**
 * Check metadata header.
 * @param fd		file handle
 * @param ptr		metadata header
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC ckHeader(/*@unused@*/ FD_t fd, const void * ptr,
		/*@unused@*/ const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies ptr, fileSystem @*/
{
    rpmRC rc = RPMRC_OK;
    Header h;

    h = headerLoad((void *)ptr);
    if (h == NULL)
	rc = RPMRC_FAIL;
    h = headerFree(h);

    return rc;
}

/** 
 * Return checked and loaded header.
 * @param dig		signature parameters container
 * @param fd		file handle
 * @retval hdrp		address of header (or NULL)
 * @retval *msg		verification error message (or NULL)
 * @return		RPMRC_OK on success
 */
static rpmRC rpmReadHeader(FD_t fd, /*@null@*/ Header * hdrp,
		/*@null@*/ const char ** msg)
        /*@globals fileSystem, internalState @*/
        /*@modifies fd, *hdrp, *msg, fileSystem, internalState @*/
{
rpmxar xar = fdGetXAR(fd);
    pgpDig dig = fdGetDig(fd);
    char buf[BUFSIZ];
    uint32_t block[4];
    uint32_t il;
    uint32_t dl;
    uint32_t * ei = NULL;
    size_t uc;
    unsigned char * b;
    size_t nb;
    Header h = NULL;
    const char * origin = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;

    /* Create (if not already) a signature parameters container. */
    if (dig == NULL) {
	dig = pgpDigNew(0);
	(void) fdSetDig(fd, dig);
    }

    buf[0] = '\0';

    if (hdrp)
	*hdrp = NULL;

    memset(block, 0, sizeof(block));
    if (xar != NULL) {
	const char item[] = "Header";
	if ((xx = rpmxarNext(xar)) != 0 || (xx = rpmxarPull(xar, item)) != 0) {
	    (void) snprintf(buf, sizeof(buf),
		_("XAR file not found (or no XAR support)"));
	    rc = RPMRC_NOTFOUND;
	    goto exit;
	}
    }
    if ((xx = (int) timedRead(fd, (char *)block, sizeof(block))) != (int)sizeof(block)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr size(%u): BAD, read returned %d"), (unsigned)sizeof(block), xx);
	goto exit;
    }

    b = NULL;
    nb = 0;
    (void) headerGetMagic(NULL, &b, &nb);
    if (memcmp(block, b, nb)) {
	unsigned char * x = (unsigned char *) block;
	(void) snprintf(buf, sizeof(buf), _("hdr magic: BAD, read %02x%02x%02x%02x%02x%02x%02x%02x"), x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]);
	goto exit;
    }

    il = ntohl(block[2]);
    if (hdrchkTags(il)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr tags: BAD, no. of tags(%u) out of range"), (unsigned) il);

	goto exit;
    }
    dl = ntohl(block[3]);
    if (hdrchkData(dl)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr data: BAD, no. of bytes(%u) out of range\n"), (unsigned) dl);
	goto exit;
    }

/*@-sizeoftype@*/
    nb = (il * sizeof(struct entryInfo_s)) + dl;
/*@=sizeoftype@*/
    uc = sizeof(il) + sizeof(dl) + nb;
    ei = (uint32_t *) xmalloc(uc);
    if ((xx = (int) timedRead(fd, (char *)&ei[2], nb)) != (int) nb) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr blob(%u): BAD, read returned %d"), (unsigned)nb, xx);
	goto exit;
    }
    ei[0] = block[2];
    ei[1] = block[3];

    /* Sanity check header tags */
    rc = headerCheck(dig, ei, uc, msg);
    if (rc != RPMRC_OK)
	goto exit;

    /* OK, blob looks sane, load the header. */
    h = headerLoad(ei);
    if (h == NULL) {
	(void) snprintf(buf, sizeof(buf), _("hdr load: BAD\n"));
        goto exit;
    }
    h->flags |= HEADERFLAG_ALLOCATED;
    ei = NULL;	/* XXX will be freed with header */

    /* Save the opened path as the header origin. */
    origin = fdGetOPath(fd);
    if (origin != NULL)
	(void) headerSetOrigin(h, origin);
    
exit:
    if (hdrp && h && rc == RPMRC_OK)
	*hdrp = headerLink(h);
    ei = _free(ei);
    h = headerFree(h);

    if (msg != NULL && *msg == NULL && buf[0] != '\0') {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }

    return rc;
}

/**
 * Read metadata header.
 * @param fd		file handle
 * @retval *ptr		metadata header (or NULL)
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC rdHeader(FD_t fd, /*@out@*/ /*@null@*/ void * ptr,
		const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, *ptr, *msg, fileSystem @*/
{
    Header * hdrp = ptr;
/*@-compdef@*/
    return rpmReadHeader(fd, hdrp, msg);
/*@=compdef@*/
}

/**
 * Write metadata header.
 * @param fd		file handle
 * @param ptr		metadata header
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC wrHeader(FD_t fd, void * ptr, /*@unused@*/ const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, ptr, *msg, fileSystem @*/
{
    Header h = ptr;
    return rpmWriteHeader(fd, h, msg);
}
/*@=globuse@*/

/*===============================================*/

size_t rpmpkgSizeof(const char * fn, const void * ptr)
{
    size_t len = 0;

    if (!strcmp(fn, "Lead"))
	len = 96;	/* RPMLEAD_SIZE */
    else
    if (!strcmp(fn, "Signature")) {
	len = szHeader(ptr);
	len += ((8 - (len % 8)) % 8);   /* padding */
    } else
    if (!strcmp(fn, "Header"))
	len = szHeader(ptr);
    return len;
}

rpmRC rpmpkgCheck(const char * fn, FD_t fd, const void * ptr, const char ** msg)
{
    rpmRC rc = RPMRC_FAIL;

    if (msg)
	*msg = NULL;

    if (!strcmp(fn, "Header"))
	rc = ckHeader(fd, ptr, msg);
    return rc;
}

rpmRC rpmpkgRead(const char * fn, FD_t fd, void * ptr, const char ** msg)
{
    rpmRC rc = RPMRC_FAIL;

    if (msg)
	*msg = NULL;

    if (!strcmp(fn, "Lead"))
	rc = rdLead(fd, ptr, msg);
    else
    if (!strcmp(fn, "Signature"))
	rc = rdSignature(fd, ptr, msg);
    else
    if (!strcmp(fn, "Header"))
	rc = rdHeader(fd, ptr, msg);
    return rc;
}

rpmRC rpmpkgWrite(const char * fn, FD_t fd, void * ptr, const char ** msg)
{
    rpmRC rc = RPMRC_FAIL;

    if (msg)
	*msg = NULL;

    if (!strcmp(fn, "Lead"))
	rc = wrLead(fd, ptr, msg);
    else
    if (!strcmp(fn, "Signature"))
	rc = wrSignature(fd, ptr, msg);
    else
    if (!strcmp(fn, "Header"))
	rc = wrHeader(fd, ptr, msg);
    return rc;
}
