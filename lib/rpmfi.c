/** \ingroup rpmfi
 * \file lib/rpmfi.c
 * Routines to handle file info tag sets.
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmlib.h>

#include "cpio.h"	/* XXX CPIO_FOO */
#include "fsm.h"	/* XXX newFSM() */
#include "legacy.h"	/* XXX dodigest */

#include "rpmds.h"

#define	_RPMFI_INTERNAL
#include "rpmfi.h"

#include "rpmsx.h"

#define	_RPMTE_INTERNAL	/* relocations */
#include "rpmte.h"
#include "rpmts.h"

#include "misc.h"	/* XXX stripTrailingChar */
#include "rpmmacro.h"	/* XXX rpmCleanPath */

#include "debug.h"

/*@access rpmte @*/
/*@access FSM_t @*/	/* XXX fsm->repackaged */

/*@unchecked@*/
int _rpmfi_debug = 0;

rpmfi XrpmfiUnlink(rpmfi fi, const char * msg, const char * fn, unsigned ln)
{
    if (fi == NULL) return NULL;
/*@-modfilesys@*/
if (_rpmfi_debug && msg != NULL)
fprintf(stderr, "--> fi %p -- %d %s at %s:%u\n", fi, fi->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    fi->nrefs--;
    return NULL;
}

rpmfi XrpmfiLink(rpmfi fi, const char * msg, const char * fn, unsigned ln)
{
    if (fi == NULL) return NULL;
    fi->nrefs++;
/*@-modfilesys@*/
if (_rpmfi_debug && msg != NULL)
fprintf(stderr, "--> fi %p ++ %d %s at %s:%u\n", fi, fi->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    /*@-refcounttrans@*/ return fi; /*@=refcounttrans@*/
}

int rpmfiFC(rpmfi fi)
{
    return (fi != NULL ? fi->fc : 0);
}

int rpmfiDC(rpmfi fi)
{
    return (fi != NULL ? fi->dc : 0);
}

#ifdef	NOTYET
int rpmfiDI(rpmfi fi)
{
}
#endif

int rpmfiFX(rpmfi fi)
{
    return (fi != NULL ? fi->i : -1);
}

int rpmfiSetFX(rpmfi fi, int fx)
{
    int i = -1;

    if (fi != NULL && fx >= 0 && fx < fi->fc) {
	i = fi->i;
	fi->i = fx;
/*@-boundsread@*/
	fi->j = fi->dil[fi->i];
/*@=boundsread@*/
    }
    return i;
}

int rpmfiDX(rpmfi fi)
{
    return (fi != NULL ? fi->j : -1);
}

int rpmfiSetDX(rpmfi fi, int dx)
{
    int j = -1;

    if (fi != NULL && dx >= 0 && dx < fi->dc) {
	j = fi->j;
	fi->j = dx;
    }
    return j;
}

const char * rpmfiBN(rpmfi fi)
{
    const char * BN = NULL;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->bnl != NULL)
	    BN = fi->bnl[fi->i];
/*@=boundsread@*/
    }
    return BN;
}

const char * rpmfiDN(rpmfi fi)
{
    const char * DN = NULL;

    if (fi != NULL && fi->j >= 0 && fi->j < fi->dc) {
/*@-boundsread@*/
	if (fi->dnl != NULL)
	    DN = fi->dnl[fi->j];
/*@=boundsread@*/
    }
    return DN;
}

const char * rpmfiFN(rpmfi fi)
{
    const char * FN = "";

    /*@-branchstate@*/
    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
	const char *dn;
	char * t;
	if (fi->fn == NULL)
	    fi->fn = xmalloc(fi->fnlen);
	FN = t = fi->fn;
	(void) urlPath(fi->dnl[fi->dil[fi->i]], &dn);
/*@-boundswrite@*/
	*t = '\0';
	t = stpcpy(t, dn);
	t = stpcpy(t, fi->bnl[fi->i]);
/*@=boundswrite@*/
    }
    /*@=branchstate@*/
    return FN;
}

uint_32 rpmfiFFlags(rpmfi fi)
{
    uint_32 FFlags = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fflags != NULL)
	    FFlags = fi->fflags[fi->i];
/*@=boundsread@*/
    }
    return FFlags;
}

uint_32 rpmfiSetFFlags(rpmfi fi, uint_32 FFlags)
{
    uint_32 oFFlags = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fflags != NULL && fi->h == NULL) {
	    oFFlags = fi->fflags[fi->i];
	    *((uint_32 *)(fi->fflags + fi->i)) = FFlags;
	}
/*@=boundsread@*/
    }
    return oFFlags;
}

uint_32 rpmfiVFlags(rpmfi fi)
{
    uint_32 VFlags = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->vflags != NULL)
	    VFlags = fi->vflags[fi->i];
/*@=boundsread@*/
    }
    return VFlags;
}

uint_32 rpmfiSetVFlags(rpmfi fi, uint_32 VFlags)
{
    uint_32 oVFlags = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->vflags != NULL && fi->h == NULL) {
	    oVFlags = fi->vflags[fi->i];
	    *((uint_32 *)(fi->vflags + fi->i)) = VFlags;
	}
/*@=boundsread@*/
    }
    return oVFlags;
}

int_16 rpmfiFMode(rpmfi fi)
{
    int_16 fmode = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fmodes != NULL)
	    fmode = fi->fmodes[fi->i];
/*@=boundsread@*/
    }
    return fmode;
}

rpmfileState rpmfiFState(rpmfi fi)
{
    rpmfileState fstate = RPMFILE_STATE_MISSING;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fstates != NULL)
	    fstate = fi->fstates[fi->i];
/*@=boundsread@*/
    }
    return fstate;
}

rpmfileState rpmfiSetFState(rpmfi fi, rpmfileState fstate)
{
    int_32 ofstate = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fstates != NULL) {
	    ofstate = fi->fstates[fi->i];
	    fi->fstates[fi->i] = fstate;
	}
/*@=boundsread@*/
    }
    return ofstate;
}

const unsigned char * rpmfiDigest(rpmfi fi, int * algop, size_t * lenp)
{
    unsigned char * digest = NULL;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->digests != NULL) {
	    digest = fi->digests + (fi->digestlen * fi->i);
	    if (algop != NULL)
		*algop = (fi->fdigestalgos
			? fi->fdigestalgos[fi->i] : fi->digestalgo);
	    if (lenp != NULL)
		*lenp = fi->digestlen;
	}
/*@=boundsread@*/
    }
    return digest;
}

const char * rpmfiFLink(rpmfi fi)
{
    const char * flink = NULL;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->flinks != NULL)
	    flink = fi->flinks[fi->i];
/*@=boundsread@*/
    }
    return flink;
}

int_32 rpmfiFSize(rpmfi fi)
{
    int_32 fsize = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fsizes != NULL)
	    fsize = fi->fsizes[fi->i];
/*@=boundsread@*/
    }
    return fsize;
}

int_16 rpmfiFRdev(rpmfi fi)
{
    int_16 frdev = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->frdevs != NULL)
	    frdev = fi->frdevs[fi->i];
/*@=boundsread@*/
    }
    return frdev;
}

int_32 rpmfiFInode(rpmfi fi)
{
    int_32 finode = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->finodes != NULL)
	    finode = fi->finodes[fi->i];
/*@=boundsread@*/
    }
    return finode;
}

uint_32 rpmfiColor(rpmfi fi)
{
    uint_32 color = 0;

    if (fi != NULL)
	/* XXX ignore all but lsnibble for now. */
	color = fi->color & 0xf;
    return color;
}

uint_32 rpmfiFColor(rpmfi fi)
{
    uint_32 fcolor = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fcolors != NULL)
	    /* XXX ignore all but lsnibble for now. */
	    fcolor = (fi->fcolors[fi->i] & 0x0f);
/*@=boundsread@*/
    }
    return fcolor;
}

const char * rpmfiFClass(rpmfi fi)
{
    const char * fclass = NULL;
    int cdictx;

    if (fi != NULL && fi->fcdictx != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	cdictx = fi->fcdictx[fi->i];
	if (fi->cdict != NULL && cdictx >= 0 && cdictx < fi->ncdict)
	    fclass = fi->cdict[cdictx];
/*@=boundsread@*/
    }
    return fclass;
}

const char * rpmfiFContext(rpmfi fi)
{
    const char * fcontext = NULL;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fcontexts != NULL)
	    fcontext = fi->fcontexts[fi->i];
/*@=boundsread@*/
    }
    return fcontext;
}

int_32 rpmfiFDepends(rpmfi fi, const uint_32 ** fddictp)
{
    int fddictx = -1;
    int fddictn = 0;
    const uint_32 * fddict = NULL;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fddictn != NULL)
	    fddictn = fi->fddictn[fi->i];
	if (fddictn > 0 && fi->fddictx != NULL)
	    fddictx = fi->fddictx[fi->i];
	if (fi->ddict != NULL && fddictx >= 0 && (fddictx+fddictn) <= fi->nddict)
	    fddict = fi->ddict + fddictx;
/*@=boundsread@*/
    }
/*@-boundswrite -dependenttrans -onlytrans @*/
    if (fddictp)
	*fddictp = fddict;
/*@=boundswrite =dependenttrans =onlytrans @*/
    return fddictn;
}

int_32 rpmfiFNlink(rpmfi fi)
{
    int_32 nlink = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
	/* XXX rpm-2.3.12 has not RPMTAG_FILEINODES */
/*@-boundsread@*/
	if (fi->finodes && fi->frdevs) {
	    int_32 finode = fi->finodes[fi->i];
	    int_16 frdev = fi->frdevs[fi->i];
	    int j;

	    for (j = 0; j < fi->fc; j++) {
		if (fi->frdevs[j] == frdev && fi->finodes[j] == finode)
		    nlink++;
	    }
	}
/*@=boundsread@*/
    }
    return nlink;
}

int_32 rpmfiFMtime(rpmfi fi)
{
    int_32 fmtime = 0;

    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fmtimes != NULL)
	    fmtime = fi->fmtimes[fi->i];
/*@=boundsread@*/
    }
    return fmtime;
}

const char * rpmfiFUser(rpmfi fi)
{
    const char * fuser = NULL;

    /* XXX add support for ancient RPMTAG_FILEUIDS? */
    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fuser != NULL)
	    fuser = fi->fuser[fi->i];
/*@=boundsread@*/
    }
    return fuser;
}

const char * rpmfiFGroup(rpmfi fi)
{
    const char * fgroup = NULL;

    /* XXX add support for ancient RPMTAG_FILEGIDS? */
    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
/*@-boundsread@*/
	if (fi->fgroup != NULL)
	    fgroup = fi->fgroup[fi->i];
/*@=boundsread@*/
    }
    return fgroup;
}

int rpmfiNext(rpmfi fi)
{
    int i = -1;

    if (fi != NULL && ++fi->i >= 0) {
	if (fi->i < fi->fc) {
	    i = fi->i;
/*@-boundsread@*/
	    if (fi->dil != NULL)
		fi->j = fi->dil[fi->i];
/*@=boundsread@*/
	} else
	    fi->i = -1;

/*@-modfilesys @*/
if (_rpmfi_debug  < 0 && i != -1)
fprintf(stderr, "*** fi %p\t%s[%d] %s%s\n", fi, (fi->Type ? fi->Type : "?Type?"), i, (i >= 0 ? fi->dnl[fi->j] : ""), (i >= 0 ? fi->bnl[fi->i] : ""));
/*@=modfilesys @*/

    }

    return i;
}

rpmfi rpmfiInit(rpmfi fi, int fx)
{
    if (fi != NULL) {
	if (fx >= 0 && fx < fi->fc) {
	    fi->i = fx - 1;
	    fi->j = -1;
	}
    }

    /*@-refcounttrans@*/
    return fi;
    /*@=refcounttrans@*/
}

int rpmfiNextD(rpmfi fi)
{
    int j = -1;

    if (fi != NULL && ++fi->j >= 0) {
	if (fi->j < fi->dc)
	    j = fi->j;
	else
	    fi->j = -1;

/*@-modfilesys @*/
if (_rpmfi_debug  < 0 && j != -1)
fprintf(stderr, "*** fi %p\t%s[%d]\n", fi, (fi->Type ? fi->Type : "?Type?"), j);
/*@=modfilesys @*/

    }

    return j;
}

rpmfi rpmfiInitD(rpmfi fi, int dx)
{
    if (fi != NULL) {
	if (dx >= 0 && dx < fi->fc)
	    fi->j = dx - 1;
	else
	    fi = NULL;
    }

    /*@-refcounttrans@*/
    return fi;
    /*@=refcounttrans@*/
}

/**
 * Identify a file type.
 * @param ft		file type
 * @return		string to identify a file type
 */
static /*@observer@*/
const char * ftstring (fileTypes ft)
	/*@*/
{
    switch (ft) {
    case XDIR:	return "directory";
    case CDEV:	return "char dev";
    case BDEV:	return "block dev";
    case LINK:	return "link";
    case SOCK:	return "sock";
    case PIPE:	return "fifo/pipe";
    case REG:	return "file";
    default:	return "unknown file type";
    }
    /*@notreached@*/
}

fileTypes whatis(uint_16 mode)
{
    if (S_ISDIR(mode))	return XDIR;
    if (S_ISCHR(mode))	return CDEV;
    if (S_ISBLK(mode))	return BDEV;
    if (S_ISLNK(mode))	return LINK;
/*@-unrecog@*/
    if (S_ISSOCK(mode))	return SOCK;
/*@=unrecog@*/
    if (S_ISFIFO(mode))	return PIPE;
    return REG;
}

/*@-boundsread@*/
int rpmfiCompare(const rpmfi afi, const rpmfi bfi)
	/*@*/
{
    fileTypes awhat = whatis(rpmfiFMode(afi));
    fileTypes bwhat = whatis(rpmfiFMode(bfi));

    if (awhat != bwhat) return 1;

    if (awhat == LINK) {
	const char * alink = rpmfiFLink(afi);
	const char * blink = rpmfiFLink(bfi);
	if (alink == blink) return 0;
	if (alink == NULL) return 1;
	if (blink == NULL) return -1;
	return strcmp(alink, blink);
    } else if (awhat == REG) {
	int aalgo = 0;
	size_t alen = 0;
	const unsigned char * adigest = rpmfiDigest(afi, &aalgo, &alen);
	int balgo = 0;
	size_t blen = 0;
	const unsigned char * bdigest = rpmfiDigest(bfi, &balgo, &blen);
	/* XXX W2DO? changing file digest algo may break rpmfiCompare. */
	if (!(aalgo == balgo && alen == blen))
	    return -1;
	if (adigest == bdigest) return 0;
	if (adigest == NULL) return 1;
	if (bdigest == NULL) return -1;
	return memcmp(adigest, bdigest, alen);
    }

    return 0;
}
/*@=boundsread@*/

/*@-boundsread@*/
fileAction rpmfiDecideFate(const rpmfi ofi, rpmfi nfi, int skipMissing)
{
    const char * fn = rpmfiFN(nfi);
    int newFlags = rpmfiFFlags(nfi);
    char buffer[1024+1];
    fileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int save = (newFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SAVE;

    if (lstat(fn, &sb)) {
	/*
	 * The file doesn't exist on the disk. Create it unless the new
	 * package has marked it as missingok, or allfiles is requested.
	 */
	if (skipMissing && (newFlags & RPMFILE_MISSINGOK)) {
	    rpmMessage(RPMMESS_DEBUG, _("%s skipped due to missingok flag\n"),
			fn);
	    return FA_SKIP;
	} else {
	    return FA_CREATE;
	}
    }

    diskWhat = whatis((int_16)sb.st_mode);
    dbWhat = whatis(rpmfiFMode(ofi));
    newWhat = whatis(rpmfiFMode(nfi));

    /*
     * RPM >= 2.3.10 shouldn't create config directories -- we'll ignore
     * them in older packages as well.
     */
    if (newWhat == XDIR)
	return FA_CREATE;

    if (diskWhat != newWhat && dbWhat != REG && dbWhat != LINK)
	return save;
    else if (newWhat != dbWhat && diskWhat != dbWhat)
	return save;
    else if (dbWhat != newWhat)
	return FA_CREATE;
    else if (dbWhat != LINK && dbWhat != REG)
	return FA_CREATE;

    /*
     * This order matters - we'd prefer to CREATE the file if at all
     * possible in case something else (like the timestamp) has changed.
     */
    memset(buffer, 0, sizeof(buffer));
    if (dbWhat == REG) {
	int oalgo = 0;
	size_t olen = 0;
	const unsigned char * odigest;
	int nalgo = 0;
	size_t nlen = 0;
	const unsigned char * ndigest;
	odigest = rpmfiDigest(ofi, &oalgo, &olen);
	if (diskWhat == REG) {
	    if (!(newFlags & RPMFILE_SPARSE))
	    if (dodigest(oalgo, fn, (unsigned char *)buffer, 0, NULL))
		return FA_CREATE;	/* assume file has been removed */
	    if (odigest && !memcmp(odigest, buffer, olen))
		return FA_CREATE;	/* unmodified config file, replace. */
	}
	ndigest = rpmfiDigest(nfi, &nalgo, &nlen);
/*@-nullpass@*/
	if (odigest && ndigest && oalgo == nalgo && olen == nlen
	 && !memcmp(odigest, ndigest, nlen))
	    return FA_SKIP;	/* identical file, don't bother. */
/*@=nullpass@*/
    } else /* dbWhat == LINK */ {
	const char * oFLink, * nFLink;
	oFLink = rpmfiFLink(ofi);
	if (diskWhat == LINK) {
	    if (readlink(fn, buffer, sizeof(buffer) - 1) == -1)
		return FA_CREATE;	/* assume file has been removed */
	    buffer[sizeof(buffer)-1] = '\0';
	    if (oFLink && !strcmp(oFLink, buffer))
		return FA_CREATE;	/* unmodified config file, replace. */
	}
	nFLink = rpmfiFLink(nfi);
/*@-nullpass@*/
	if (oFLink && nFLink && !strcmp(oFLink, nFLink))
	    return FA_SKIP;	/* identical file, don't bother. */
/*@=nullpass@*/
    }

    /*
     * The config file on the disk has been modified, but
     * the ones in the two packages are different. It would
     * be nice if RPM was smart enough to at least try and
     * merge the difference ala CVS, but...
     */
    return save;
}
/*@=boundsread@*/

/*@observer@*/
const char * rpmfiTypeString(rpmfi fi)
{
    switch(rpmteType(fi->te)) {
    case TR_ADDED:	return " install";
    case TR_REMOVED:	return "   erase";
    default:		return "???";
    }
    /*@noteached@*/
}

#define alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/**
 * Relocate files in header.
 * @todo multilib file dispositions need to be checked.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param origH		package header
 * @param actions	file dispositions
 * @return		header with relocated files
 */
/*@-bounds@*/
static
Header relocateFileList(const rpmts ts, rpmfi fi,
		Header origH, fileAction * actions)
	/*@globals h_errno, rpmGlobalMacroContext @*/
	/*@modifies ts, fi, origH, actions, rpmGlobalMacroContext @*/
{
    rpmte p = rpmtsRelocateElement(ts);
    HGE_t hge = fi->hge;
    HAE_t hae = fi->hae;
    HME_t hme = fi->hme;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    static int _printed = 0;
    int allowBadRelocate = (rpmtsFilterFlags(ts) & RPMPROB_FILTER_FORCERELOCATE);
    rpmRelocation relocations = NULL;
    int numRelocations;
    const char ** validRelocations;
    rpmTagType validType;
    int numValid;
    const char ** baseNames;
    const char ** dirNames;
    int_32 * dirIndexes;
    int_32 * newDirIndexes;
    int_32 fileCount;
    int_32 dirCount;
    uint_32 mydColor = rpmExpandNumeric("%{?_autorelocate_dcolor}");
    uint_32 * fFlags = NULL;
    uint_32 * fColors = NULL;
    uint_32 * dColors = NULL;
    uint_16 * fModes = NULL;
    Header h;
    int nrelocated = 0;
    int fileAlloced = 0;
    char * fn = NULL;
    int haveRelocatedFile = 0;
    int reldel = 0;
    int len;
    int i, j, xx;

    if (!hge(origH, RPMTAG_PREFIXES, &validType, &validRelocations, &numValid))
	numValid = 0;

assert(p != NULL);
    numRelocations = 0;
    if (p->relocs)
	while (p->relocs[numRelocations].newPath ||
	       p->relocs[numRelocations].oldPath)
	    numRelocations++;

    /*
     * If no relocations are specified (usually the case), then return the
     * original header. If there are prefixes, however, then INSTPREFIXES
     * should be added, but, since relocateFileList() can be called more
     * than once for the same header, don't bother if already present.
     */
    if (p->relocs == NULL || numRelocations == 0) {
	if (numValid) {
	    if (!headerIsEntry(origH, RPMTAG_INSTPREFIXES))
		xx = hae(origH, RPMTAG_INSTPREFIXES,
			validType, validRelocations, numValid);
	    validRelocations = hfd(validRelocations, validType);
	}
	/* XXX FIXME multilib file actions need to be checked. */
	return headerLink(origH);
    }

    h = headerLink(origH);

    relocations = alloca(sizeof(*relocations) * numRelocations);

    /* Build sorted relocation list from raw relocations. */
    for (i = 0; i < numRelocations; i++) {
	char * t;

	/*
	 * Default relocations (oldPath == NULL) are handled in the UI,
	 * not rpmlib.
	 */
	if (p->relocs[i].oldPath == NULL) continue; /* XXX can't happen */

	/* FIXME: Trailing /'s will confuse us greatly. Internal ones will 
	   too, but those are more trouble to fix up. :-( */
	t = alloca_strdup(p->relocs[i].oldPath);
	/*@-branchstate@*/
	relocations[i].oldPath = (t[0] == '/' && t[1] == '\0')
	    ? t
	    : stripTrailingChar(t, '/');
	/*@=branchstate@*/

	/* An old path w/o a new path is valid, and indicates exclusion */
	if (p->relocs[i].newPath) {
	    int del;

	    t = alloca_strdup(p->relocs[i].newPath);
	    /*@-branchstate@*/
	    relocations[i].newPath = (t[0] == '/' && t[1] == '\0')
		? t
		: stripTrailingChar(t, '/');
	    /*@=branchstate@*/

	    /*@-nullpass@*/	/* FIX:  relocations[i].oldPath == NULL */
	    /* Verify that the relocation's old path is in the header. */
	    for (j = 0; j < numValid; j++) {
		if (!strcmp(validRelocations[j], relocations[i].oldPath))
		    /*@innerbreak@*/ break;
	    }

	    /* XXX actions check prevents problem from being appended twice. */
	    if (j == numValid && !allowBadRelocate && actions) {
		rpmps ps = rpmtsProblems(ts);
		rpmpsAppend(ps, RPMPROB_BADRELOCATE,
			rpmteNEVR(p), rpmteKey(p),
			relocations[i].oldPath, NULL, NULL, 0);
		ps = rpmpsFree(ps);
	    }
	    del =
		strlen(relocations[i].newPath) - strlen(relocations[i].oldPath);
	    /*@=nullpass@*/

	    if (del > reldel)
		reldel = del;
	} else {
	    relocations[i].newPath = NULL;
	}
    }

    /* stupid bubble sort, but it's probably faster here */
    for (i = 0; i < numRelocations; i++) {
	int madeSwap;
	madeSwap = 0;
	for (j = 1; j < numRelocations; j++) {
	    struct rpmRelocation_s tmpReloc;
	    if (relocations[j - 1].oldPath == NULL || /* XXX can't happen */
		relocations[j    ].oldPath == NULL || /* XXX can't happen */
	strcmp(relocations[j - 1].oldPath, relocations[j].oldPath) <= 0)
		/*@innercontinue@*/ continue;
	    /*@-usereleased@*/ /* LCL: ??? */
	    tmpReloc = relocations[j - 1];
	    relocations[j - 1] = relocations[j];
	    relocations[j] = tmpReloc;
	    /*@=usereleased@*/
	    madeSwap = 1;
	}
	if (!madeSwap) break;
    }

    if (!_printed) {
	_printed = 1;
	rpmMessage(RPMMESS_DEBUG, _("========== relocations\n"));
	for (i = 0; i < numRelocations; i++) {
	    if (relocations[i].oldPath == NULL) continue; /* XXX can't happen */
	    if (relocations[i].newPath == NULL)
		rpmMessage(RPMMESS_DEBUG, _("%5d exclude  %s\n"),
			i, relocations[i].oldPath);
	    else
		rpmMessage(RPMMESS_DEBUG, _("%5d relocate %s -> %s\n"),
			i, relocations[i].oldPath, relocations[i].newPath);
	}
    }

    /* Add relocation values to the header */
    if (numValid) {
	const char ** actualRelocations;
	int numActual;

	actualRelocations = xmalloc(numValid * sizeof(*actualRelocations));
	numActual = 0;
	for (i = 0; i < numValid; i++) {
	    for (j = 0; j < numRelocations; j++) {
		if (relocations[j].oldPath == NULL || /* XXX can't happen */
		    strcmp(validRelocations[i], relocations[j].oldPath))
		    /*@innercontinue@*/ continue;
		/* On install, a relocate to NULL means skip the path. */
		if (relocations[j].newPath) {
		    actualRelocations[numActual] = relocations[j].newPath;
		    numActual++;
		}
		/*@innerbreak@*/ break;
	    }
	    if (j == numRelocations) {
		actualRelocations[numActual] = validRelocations[i];
		numActual++;
	    }
	}

	if (numActual)
	    xx = hae(h, RPMTAG_INSTPREFIXES, RPM_STRING_ARRAY_TYPE,
		       (void **) actualRelocations, numActual);

	actualRelocations = _free(actualRelocations);
	validRelocations = hfd(validRelocations, validType);
    }

    xx = hge(h, RPMTAG_BASENAMES, NULL, &baseNames, &fileCount);
    xx = hge(h, RPMTAG_DIRINDEXES, NULL, &dirIndexes, NULL);
    xx = hge(h, RPMTAG_DIRNAMES, NULL, &dirNames, &dirCount);
    xx = hge(h, RPMTAG_FILEFLAGS, NULL, &fFlags, NULL);
    xx = hge(h, RPMTAG_FILECOLORS, NULL, &fColors, NULL);
    xx = hge(h, RPMTAG_FILEMODES, NULL, &fModes, NULL);

    dColors = alloca(dirCount * sizeof(*dColors));
    memset(dColors, 0, dirCount * sizeof(*dColors));

    newDirIndexes = alloca(sizeof(*newDirIndexes) * fileCount);
    memcpy(newDirIndexes, dirIndexes, sizeof(*newDirIndexes) * fileCount);
    dirIndexes = newDirIndexes;

    /*
     * For all relocations, we go through sorted file/relocation lists 
     * backwards so that /usr/local relocations take precedence over /usr 
     * ones.
     */

    /* Relocate individual paths. */

    for (i = fileCount - 1; i >= 0; i--) {
	fileTypes ft;
	int fnlen;

	len = reldel +
		strlen(dirNames[dirIndexes[i]]) + strlen(baseNames[i]) + 1;
	/*@-branchstate@*/
	if (len >= fileAlloced) {
	    fileAlloced = len * 2;
	    fn = xrealloc(fn, fileAlloced);
	}
	/*@=branchstate@*/

assert(fn != NULL);		/* XXX can't happen */
	*fn = '\0';
	fnlen = stpcpy( stpcpy(fn, dirNames[dirIndexes[i]]), baseNames[i]) - fn;

if (fColors != NULL) {
/* XXX pkgs may not have unique dirNames, so color all dirNames that match. */
for (j = 0; j < dirCount; j++) {
if (strcmp(dirNames[dirIndexes[i]], dirNames[j])) /*@innercontinue@*/ continue;
dColors[j] |= fColors[i];
}
}

	/*
	 * See if this file path needs relocating.
	 */
	/*
	 * XXX FIXME: Would a bsearch of the (already sorted) 
	 * relocation list be a good idea?
	 */
	for (j = numRelocations - 1; j >= 0; j--) {
	    if (relocations[j].oldPath == NULL) /* XXX can't happen */
		/*@innercontinue@*/ continue;
	    len = strcmp(relocations[j].oldPath, "/")
		? strlen(relocations[j].oldPath)
		: 0;

	    if (fnlen < len)
		/*@innercontinue@*/ continue;
	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (!(fn[len] == '/' || fnlen == len))
		/*@innercontinue@*/ continue;

	    if (strncmp(relocations[j].oldPath, fn, len))
		/*@innercontinue@*/ continue;
	    /*@innerbreak@*/ break;
	}
	if (j < 0) continue;

/*@-nullderef@*/ /* FIX: fModes may be NULL */
	ft = whatis(fModes[i]);
/*@=nullderef@*/

	/* On install, a relocate to NULL means skip the path. */
	if (relocations[j].newPath == NULL) {
	    if (ft == XDIR) {
		/* Start with the parent, looking for directory to exclude. */
		for (j = dirIndexes[i]; j < dirCount; j++) {
		    len = strlen(dirNames[j]) - 1;
		    while (len > 0 && dirNames[j][len-1] == '/') len--;
		    if (fnlen != len)
			/*@innercontinue@*/ continue;
		    if (strncmp(fn, dirNames[j], fnlen))
			/*@innercontinue@*/ continue;
		    /*@innerbreak@*/ break;
		}
	    }
	    if (actions) {
		actions[i] = FA_SKIPNSTATE;
		rpmMessage(RPMMESS_DEBUG, _("excluding %s %s\n"),
			ftstring(ft), fn);
	    }
	    continue;
	}

	/* Relocation on full paths only, please. */
	if (fnlen != len) continue;

	if (actions)
	    rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
		    fn, relocations[j].newPath);
	nrelocated++;

	strcpy(fn, relocations[j].newPath);
	{   char * te = strrchr(fn, '/');
	    if (te) {
		if (te > fn) te++;	/* root is special */
		fnlen = te - fn;
	    } else
		te = fn + strlen(fn);
	    /*@-nullpass -nullderef@*/	/* LCL: te != NULL here. */
	    if (strcmp(baseNames[i], te)) /* basename changed too? */
		baseNames[i] = alloca_strdup(te);
	    *te = '\0';			/* terminate new directory name */
	    /*@=nullpass =nullderef@*/
	}

	/* Does this directory already exist in the directory list? */
	for (j = 0; j < dirCount; j++) {
	    if (fnlen != strlen(dirNames[j]))
		/*@innercontinue@*/ continue;
	    if (strncmp(fn, dirNames[j], fnlen))
		/*@innercontinue@*/ continue;
	    /*@innerbreak@*/ break;
	}
	
	if (j < dirCount) {
	    dirIndexes[i] = j;
	    continue;
	}

	/* Creating new paths is a pita */
	if (!haveRelocatedFile) {
	    const char ** newDirList;

	    haveRelocatedFile = 1;
	    newDirList = xmalloc((dirCount + 1) * sizeof(*newDirList));
	    for (j = 0; j < dirCount; j++)
		newDirList[j] = alloca_strdup(dirNames[j]);
	    dirNames = hfd(dirNames, RPM_STRING_ARRAY_TYPE);
	    dirNames = newDirList;
	} else {
	    dirNames = xrealloc(dirNames, 
			       sizeof(*dirNames) * (dirCount + 1));
	}

	dirNames[dirCount] = alloca_strdup(fn);
	dirIndexes[i] = dirCount;
	dirCount++;
    }

    /* Finish off by relocating directories. */
    for (i = dirCount - 1; i >= 0; i--) {
	for (j = numRelocations - 1; j >= 0; j--) {

           /* XXX Don't autorelocate uncolored directories. */
           if (j == p->autorelocatex
            && (dColors[i] == 0 || !(dColors[i] & mydColor)))
               /*@innercontinue@*/ continue;

	    if (relocations[j].oldPath == NULL) /* XXX can't happen */
		/*@innercontinue@*/ continue;
	    len = strcmp(relocations[j].oldPath, "/")
		? strlen(relocations[j].oldPath)
		: 0;

	    if (len && strncmp(relocations[j].oldPath, dirNames[i], len))
		/*@innercontinue@*/ continue;

	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (dirNames[i][len] != '/')
		/*@innercontinue@*/ continue;

	    if (relocations[j].newPath) { /* Relocate the path */
		const char * s = relocations[j].newPath;
		char * t = alloca(strlen(s) + strlen(dirNames[i]) - len + 1);
		size_t slen;

		(void) stpcpy( stpcpy(t, s) , dirNames[i] + len);

		/* Unfortunatly rpmCleanPath strips the trailing slash.. */
		(void) rpmCleanPath(t);
		slen = strlen(t);
		t[slen] = '/';
		t[slen+1] = '\0';

		if (actions)
		    rpmMessage(RPMMESS_DEBUG,
			_("relocating directory %s to %s\n"), dirNames[i], t);
		dirNames[i] = t;
		nrelocated++;
	    }
	}
    }

    /* Save original filenames in header and replace (relocated) filenames. */
    if (nrelocated) {
	int c;
	void * d;
	rpmTagType t;

	d = NULL;
	xx = hge(h, RPMTAG_BASENAMES, &t, &d, &c);
	xx = hae(h, RPMTAG_ORIGBASENAMES, t, d, c);
	d = hfd(d, t);

	d = NULL;
	xx = hge(h, RPMTAG_DIRNAMES, &t, &d, &c);
	xx = hae(h, RPMTAG_ORIGDIRNAMES, t, d, c);
	d = hfd(d, t);

	d = NULL;
	xx = hge(h, RPMTAG_DIRINDEXES, &t, &d, &c);
	xx = hae(h, RPMTAG_ORIGDIRINDEXES, t, d, c);
	d = hfd(d, t);

	xx = hme(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			  baseNames, fileCount);
	fi->bnl = hfd(fi->bnl, RPM_STRING_ARRAY_TYPE);
	xx = hge(h, RPMTAG_BASENAMES, NULL, &fi->bnl, &fi->fc);

	xx = hme(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			  dirNames, dirCount);
	fi->dnl = hfd(fi->dnl, RPM_STRING_ARRAY_TYPE);
	xx = hge(h, RPMTAG_DIRNAMES, NULL, &fi->dnl, &fi->dc);

	xx = hme(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE,
			  dirIndexes, fileCount);
	xx = hge(h, RPMTAG_DIRINDEXES, NULL, &fi->dil, NULL);
    }

    baseNames = hfd(baseNames, RPM_STRING_ARRAY_TYPE);
    dirNames = hfd(dirNames, RPM_STRING_ARRAY_TYPE);
/*@-dependenttrans@*/
    fn = _free(fn);
/*@=dependenttrans@*/

    return h;
}
/*@=bounds@*/

rpmfi rpmfiFree(rpmfi fi)
{
    HFD_t hfd = headerFreeData;

    if (fi == NULL) return NULL;

    if (fi->nrefs > 1)
	return rpmfiUnlink(fi, fi->Type);

/*@-modfilesys@*/
if (_rpmfi_debug < 0)
fprintf(stderr, "*** fi %p\t%s[%d]\n", fi, fi->Type, fi->fc);
/*@=modfilesys@*/

    /* Free pre- and post-transaction script and interpreter strings. */
    fi->pretrans = _free(fi->pretrans);
    fi->pretransprog = _free(fi->pretransprog);
    fi->posttrans = _free(fi->posttrans);
    fi->posttransprog = _free(fi->posttransprog);

    /*@-branchstate@*/
    if (fi->fc > 0) {
	fi->bnl = hfd(fi->bnl, -1);
	fi->dnl = hfd(fi->dnl, -1);

	fi->flinks = hfd(fi->flinks, -1);
	fi->flangs = hfd(fi->flangs, -1);
	fi->fdigests = hfd(fi->fdigests, -1);
	fi->digests = _free(fi->digests);

	fi->cdict = hfd(fi->cdict, -1);

	fi->fuser = hfd(fi->fuser, -1);
	fi->fgroup = hfd(fi->fgroup, -1);

	fi->fstates = _free(fi->fstates);

	/*@-evalorder@*/
	if (!fi->keep_header && fi->h == NULL) {
	    fi->fmtimes = _free(fi->fmtimes);
	    fi->fmodes = _free(fi->fmodes);
	    fi->fflags = _free(fi->fflags);
	    fi->vflags = _free(fi->vflags);
	    fi->fsizes = _free(fi->fsizes);
	    fi->frdevs = _free(fi->frdevs);
	    fi->finodes = _free(fi->finodes);
	    fi->dil = _free(fi->dil);

	    fi->fcolors = _free(fi->fcolors);
	    fi->fcdictx = _free(fi->fcdictx);
	    fi->ddict = _free(fi->ddict);
	    fi->fddictx = _free(fi->fddictx);
	    fi->fddictn = _free(fi->fddictn);

	}
	/*@=evalorder@*/
    }
    /*@=branchstate@*/

    fi->fsm = freeFSM(fi->fsm);

    fi->fn = _free(fi->fn);
    fi->apath = _free(fi->apath);
    fi->fmapflags = _free(fi->fmapflags);

    fi->obnl = hfd(fi->obnl, -1);
    fi->odnl = hfd(fi->odnl, -1);

    fi->fcontexts = hfd(fi->fcontexts, -1);

    fi->actions = _free(fi->actions);
    fi->replacedSizes = _free(fi->replacedSizes);

    fi->h = headerFree(fi->h);

    /*@-nullstate -refcounttrans -usereleased@*/
    (void) rpmfiUnlink(fi, fi->Type);
    memset(fi, 0, sizeof(*fi));		/* XXX trash and burn */
    fi = _free(fi);
    /*@=nullstate =refcounttrans =usereleased@*/

    return NULL;
}

/**
 * Convert hex to binary nibble.
 * @param c		hex character
 * @return		binary nibble
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

#define	_fdupe(_fi, _data)	\
    if ((_fi)->_data != NULL)	\
	(_fi)->_data = memcpy(xmalloc((_fi)->fc * sizeof(*(_fi)->_data)), \
			(_fi)->_data, (_fi)->fc * sizeof(*(_fi)->_data))

/* XXX Ick, not SEF. */
#define _fdupestring(_h, _tag, _data) \
    if (hge((_h), (_tag), NULL, &(_data), NULL)) \
	_data = xstrdup(_data)

rpmfi rpmfiNew(const rpmts ts, Header h, rpmTag tagN, int flags)
{
    HFD_t hfd = headerFreeData;
    int scareMem = (flags & 0x1);
    HGE_t hge =
	(scareMem ? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry);
    rpmte p;
    rpmfi fi = NULL;
    const char * Type;
    uint_32 * uip;
    int dnlmax, bnlmax;
    unsigned char * t;
    int len;
    int xx;
    int i;

#ifdef	NOTYET			/* XXX transaction.c still needs scareMem 1 */
assert(scareMem == 0);		/* XXX always allocate memory */
#endif
    if (tagN == RPMTAG_BASENAMES) {
	Type = "Files";
    } else {
	Type = "?Type?";
	goto exit;
    }

    fi = xcalloc(1, sizeof(*fi));
    if (fi == NULL)	/* XXX can't happen */
	goto exit;

    fi->magic = RPMFIMAGIC;
    fi->Type = Type;
    fi->i = -1;
    fi->tagN = tagN;

    fi->hge = hge;
    fi->hae = (HAE_t) headerAddEntry;
    fi->hme = (HME_t) headerModifyEntry;
    fi->hre = (HRE_t) headerRemoveEntry;
    fi->hfd = headerFreeData;

    fi->h = (h != NULL && scareMem ? headerLink(h) : NULL);

    if (fi->fsm == NULL)
	fi->fsm = newFSM();

    fi->fsm->repackaged = (headerIsEntry(h, RPMTAG_REMOVETID) ? 1 : 0);

    /* 0 means unknown */
    xx = hge(h, RPMTAG_ARCHIVESIZE, NULL, &uip, NULL);
    fi->archivePos = 0;
    fi->archiveSize = (xx ? *uip : 0);

    /* Extract pre- and post-transaction script and interpreter strings. */
    _fdupestring(h, RPMTAG_PRETRANS, fi->pretrans);
    _fdupestring(h, RPMTAG_PRETRANSPROG, fi->pretransprog);
    _fdupestring(h, RPMTAG_POSTTRANS, fi->posttrans);
    _fdupestring(h, RPMTAG_POSTTRANSPROG, fi->posttransprog);

    if (!hge(h, RPMTAG_BASENAMES, NULL, &fi->bnl, &fi->fc)) {
	fi->fc = 0;
	fi->dc = 0;
	goto exit;
    }
    xx = hge(h, RPMTAG_DIRNAMES, NULL, &fi->dnl, &fi->dc);
    xx = hge(h, RPMTAG_DIRINDEXES, NULL, &fi->dil, NULL);
    xx = hge(h, RPMTAG_FILEMODES, NULL, &fi->fmodes, NULL);
    xx = hge(h, RPMTAG_FILEFLAGS, NULL, &fi->fflags, NULL);
    xx = hge(h, RPMTAG_FILEVERIFYFLAGS, NULL, &fi->vflags, NULL);
    xx = hge(h, RPMTAG_FILESIZES, NULL, &fi->fsizes, NULL);

    xx = hge(h, RPMTAG_FILECOLORS, NULL, &fi->fcolors, NULL);
    fi->color = 0;
    if (fi->fcolors != NULL)
    for (i = 0; i < fi->fc; i++)
	fi->color |= fi->fcolors[i];
    xx = hge(h, RPMTAG_CLASSDICT, NULL, &fi->cdict, &fi->ncdict);
    xx = hge(h, RPMTAG_FILECLASS, NULL, &fi->fcdictx, NULL);

    xx = hge(h, RPMTAG_DEPENDSDICT, NULL, &fi->ddict, &fi->nddict);
    xx = hge(h, RPMTAG_FILEDEPENDSX, NULL, &fi->fddictx, NULL);
    xx = hge(h, RPMTAG_FILEDEPENDSN, NULL, &fi->fddictn, NULL);

    xx = hge(h, RPMTAG_FILESTATES, NULL, &fi->fstates, NULL);
    if (xx == 0 || fi->fstates == NULL)
	fi->fstates = xcalloc(fi->fc, sizeof(*fi->fstates));
    else
	_fdupe(fi, fstates);

    fi->action = FA_UNKNOWN;
    fi->flags = 0;

if (fi->actions == NULL)
	fi->actions = xcalloc(fi->fc, sizeof(*fi->actions));

    fi->keep_header = (scareMem ? 1 : 0);

    /* XXX TR_REMOVED needs CPIO_MAP_{ABSOLUTE,ADDDOT} CPIO_ALL_HARDLINKS */
    fi->mapflags =
		CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;

    xx = hge(h, RPMTAG_FILELINKTOS, NULL, &fi->flinks, NULL);
    xx = hge(h, RPMTAG_FILELANGS, NULL, &fi->flangs, NULL);

    fi->digestalgo = PGPHASHALGO_MD5;
    fi->digestlen = 16;
    fi->fdigestalgos = NULL;
    xx = hge(h, RPMTAG_FILEDIGESTALGOS, NULL, &fi->fdigestalgos, NULL);
    if (fi->fdigestalgos) {
	int dalgo = 0;
	/* XXX Insure that all algorithms are either 0 or constant. */
	for (i = 0; i < fi->fc; i++) {
	    if (fi->fdigestalgos[i] == 0)
		continue;
	    if (dalgo == 0)
		dalgo = fi->fdigestalgos[i];
	    else
assert(dalgo == fi->fdigestalgos[i]);
	}
	fi->digestalgo = dalgo;
	switch (dalgo) {
	case PGPHASHALGO_MD5:		fi->digestlen = 128/8;	break;
	case PGPHASHALGO_SHA1:		fi->digestlen = 160/8;	break;
	case PGPHASHALGO_RIPEMD128:	fi->digestlen = 128/8;	break;
	case PGPHASHALGO_RIPEMD160:	fi->digestlen = 160/8;	break;
	case PGPHASHALGO_SHA256:	fi->digestlen = 256/8;	break;
	case PGPHASHALGO_SHA384:	fi->digestlen = 384/8;	break;
	case PGPHASHALGO_SHA512:	fi->digestlen = 512/8;	break;
	case PGPHASHALGO_CRC32:		fi->digestlen = 32/8;	break;
	}
	fi->fdigestalgos = NULL;
    }

    fi->fdigests = NULL;
    xx = hge(h, RPMTAG_FILEDIGESTS, NULL, &fi->fdigests, NULL);

    fi->digests = NULL;
    if (fi->fdigests) {
	t = xmalloc(fi->fc * fi->digestlen);
	fi->digests = t;
	for (i = 0; i < fi->fc; i++) {
	    const char * fdigests;
	    int j;

	    fdigests = fi->fdigests[i];
	    if (!(fdigests && *fdigests != '\0')) {
		memset(t, 0, fi->digestlen);
		t += fi->digestlen;
		continue;
	    }
	    for (j = 0; j < fi->digestlen; j++, t++, fdigests += 2)
		*t = (nibble(fdigests[0]) << 4) | nibble(fdigests[1]);
	}
	fi->fdigests = hfd(fi->fdigests, -1);
    }

    /* XXX TR_REMOVED doesn;t need fmtimes, frdevs, finodes, or fcontexts */
    xx = hge(h, RPMTAG_FILEMTIMES, NULL, &fi->fmtimes, NULL);
    xx = hge(h, RPMTAG_FILERDEVS, NULL, &fi->frdevs, NULL);
    xx = hge(h, RPMTAG_FILEINODES, NULL, &fi->finodes, NULL);
    xx = hge(h, RPMTAG_FILECONTEXTS, NULL, &fi->fcontexts, NULL);

    fi->replacedSizes = xcalloc(fi->fc, sizeof(*fi->replacedSizes));

    xx = hge(h, RPMTAG_FILEUSERNAME, NULL, &fi->fuser, NULL);
    xx = hge(h, RPMTAG_FILEGROUPNAME, NULL, &fi->fgroup, NULL);

    if (ts != NULL)
    if (fi != NULL)
    if ((p = rpmtsRelocateElement(ts)) != NULL && rpmteType(p) == TR_ADDED
     && headerIsEntry(h, RPMTAG_SOURCERPM)
     && !headerIsEntry(h, RPMTAG_ORIGBASENAMES))
    {
	const char * fmt = rpmGetPath("%{?_autorelocate_path}", NULL);
	const char * errstr;
	char * newPath;
	Header foo;

	/* XXX error handling. */
	newPath = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	fmt = _free(fmt);

#if __ia64__
	/* XXX On ia64, change leading /emul/ix86 -> /emul/ia32, ick. */
 	if (newPath != NULL && *newPath != '\0'
	 && strlen(newPath) >= (sizeof("/emul/i386")-1)
	 && newPath[0] == '/' && newPath[1] == 'e' && newPath[2] == 'm'
	 && newPath[3] == 'u' && newPath[4] == 'l' && newPath[5] == '/'
	 && newPath[6] == 'i' && newPath[8] == '8' && newPath[9] == '6')
 	{
	    newPath[7] = 'a';
	    newPath[8] = '3';
	    newPath[9] = '2';
	}
#endif
 
	/* XXX Make sure autoreloc is not already specified. */
	i = p->nrelocs;
	if (newPath != NULL && *newPath != '\0' && p->relocs != NULL)
	for (i = 0; i < p->nrelocs; i++) {
/*@-nullpass@*/ /* XXX {old,new}Path might be NULL */
	   if (strcmp(p->relocs[i].oldPath, "/"))
		continue;
	   if (strcmp(p->relocs[i].newPath, newPath))
		continue;
/*@=nullpass@*/
	   break;
	}

	/* XXX test for incompatible arch triggering autorelocation is dumb. */
	/* XXX DIEDIEDIE: used to test '... && p->archScore == 0' */
	if (newPath != NULL && *newPath != '\0' && i == p->nrelocs) {

	    p->relocs =
		xrealloc(p->relocs, (p->nrelocs + 2) * sizeof(*p->relocs));
	    p->relocs[p->nrelocs].oldPath = xstrdup("/");
	    p->relocs[p->nrelocs].newPath = xstrdup(newPath);
	    p->autorelocatex = p->nrelocs;
	    p->nrelocs++;
	    p->relocs[p->nrelocs].oldPath = NULL;
	    p->relocs[p->nrelocs].newPath = NULL;
	}
	newPath = _free(newPath);

/* XXX DYING */
if (fi->actions == NULL)
	fi->actions = xcalloc(fi->fc, sizeof(*fi->actions));
	/*@-compdef@*/ /* FIX: fi->digests undefined */
	foo = relocateFileList(ts, fi, h, fi->actions);
	/*@=compdef@*/
	fi->h = headerFree(fi->h);
	fi->h = headerLink(foo);
	foo = headerFree(foo);
    }

    if (!scareMem) {
	_fdupe(fi, fmtimes);
	_fdupe(fi, frdevs);
	_fdupe(fi, finodes);
	_fdupe(fi, fsizes);
	_fdupe(fi, fflags);
	_fdupe(fi, vflags);
	_fdupe(fi, fmodes);
	_fdupe(fi, dil);

	_fdupe(fi, fcolors);
	_fdupe(fi, fcdictx);

	if (fi->ddict != NULL)
	    fi->ddict = memcpy(xmalloc(fi->nddict * sizeof(*fi->ddict)),
			fi->ddict, fi->nddict * sizeof(*fi->ddict));

	_fdupe(fi, fddictx);
	_fdupe(fi, fddictn);

	fi->h = headerFree(fi->h);
    }

    dnlmax = -1;
    for (i = 0; i < fi->dc; i++) {
	if ((len = strlen(fi->dnl[i])) > dnlmax)
	    dnlmax = len;
    }
    bnlmax = -1;
    for (i = 0; i < fi->fc; i++) {
	if ((len = strlen(fi->bnl[i])) > bnlmax)
	    bnlmax = len;
    }
    fi->fnlen = dnlmax + bnlmax + 1;
    fi->fn = NULL;

    fi->dperms = 0755;
    fi->fperms = 0644;

exit:
/*@-modfilesys@*/
if (_rpmfi_debug < 0)
fprintf(stderr, "*** fi %p\t%s[%d]\n", fi, Type, (fi ? fi->fc : 0));
/*@=modfilesys@*/

    /*@-compdef -nullstate@*/ /* FIX: rpmfi null annotations */
    return rpmfiLink(fi, (fi ? fi->Type : NULL));
    /*@=compdef =nullstate@*/
}

void rpmfiBuildFClasses(Header h,
	/*@out@*/ const char *** fclassp, /*@out@*/ int * fcp)
{
    int scareMem = 0;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, scareMem);
    const char * FClass;
    const char ** av;
    int ac;
    size_t nb;
    char * t;

    if ((ac = rpmfiFC(fi)) <= 0) {
	av = NULL;
	ac = 0;
	goto exit;
    }

    /* Compute size of file class argv array blob. */
    nb = (ac + 1) * sizeof(*av);
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	FClass = rpmfiFClass(fi);
	if (FClass && *FClass != '\0')
	    nb += strlen(FClass);
	nb += 1;
    }

    /* Create and load file class argv array. */
    av = xmalloc(nb);
    t = ((char *) av) + ((ac + 1) * sizeof(*av));
    ac = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	FClass = rpmfiFClass(fi);
	av[ac++] = t;
	if (FClass && *FClass != '\0')
	    t = stpcpy(t, FClass);
	*t++ = '\0';
    }
    av[ac] = NULL;	/* XXX tag arrays are not NULL terminated. */
    /*@=branchstate@*/

exit:
    fi = rpmfiFree(fi);
    /*@-branchstate@*/
    if (fclassp)
	*fclassp = av;
    else
	av = _free(av);
    /*@=branchstate@*/
    if (fcp) *fcp = ac;
}

void rpmfiBuildFContexts(Header h,
	/*@out@*/ const char *** fcontextp, /*@out@*/ int * fcp)
{
    int scareMem = 0;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, scareMem);
    const char * fcontext;
    const char ** av;
    int ac;
    size_t nb;
    char * t;

    if ((ac = rpmfiFC(fi)) <= 0) {
	av = NULL;
	ac = 0;
	goto exit;
    }

    /* Compute size of argv array blob. */
    nb = (ac + 1) * sizeof(*av);
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	fcontext = rpmfiFContext(fi);
	if (fcontext && *fcontext != '\0')
	    nb += strlen(fcontext);
	nb += 1;
    }

    /* Create and load argv array. */
    av = xmalloc(nb);
    t = ((char *) av) + ((ac + 1) * sizeof(*av));
    ac = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	fcontext = rpmfiFContext(fi);
	av[ac++] = t;
	if (fcontext && *fcontext != '\0')
	    t = stpcpy(t, fcontext);
	*t++ = '\0';
    }
    av[ac] = NULL;	/* XXX tag arrays are not NULL terminated. */
    /*@=branchstate@*/

exit:
    fi = rpmfiFree(fi);
    /*@-branchstate@*/
    if (fcontextp)
	*fcontextp = av;
    else
	av = _free(av);
    /*@=branchstate@*/
    if (fcp) *fcp = ac;
}

void rpmfiBuildFSContexts(Header h,
	/*@out@*/ const char *** fcontextp, /*@out@*/ int * fcp)
{
    int scareMem = 0;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, scareMem);
    const char ** av;
    int ac;
    size_t nb;
    char * t;
    char * fctxt = NULL;
    size_t fctxtlen = 0;
    int * fcnb;

    if ((ac = rpmfiFC(fi)) <= 0) {
	av = NULL;
	ac = 0;
	goto exit;
    }

    /* Compute size of argv array blob, concatenating file contexts. */
    nb = ac * sizeof(*fcnb);
    fcnb = memset(alloca(nb), 0, nb);
    ac = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	const char * fn = rpmfiFN(fi);
	security_context_t scon;

	fcnb[ac] = lgetfilecon(fn, &scon);
/*@-branchstate@*/
	if (fcnb[ac] > 0) {
	    fctxt = xrealloc(fctxt, fctxtlen + fcnb[ac]);
	    memcpy(fctxt+fctxtlen, scon, fcnb[ac]);
	    fctxtlen += fcnb[ac];
	    freecon(scon);
	}
/*@=branchstate@*/
	ac++;
    }

    /* Create and load argv array from concatenated file contexts. */
    nb = (ac + 1) * sizeof(*av) + fctxtlen;
    av = xmalloc(nb);
    t = ((char *) av) + ((ac + 1) * sizeof(*av));
    if (fctxt != NULL && fctxtlen > 0)
	(void) memcpy(t, fctxt, fctxtlen);
    ac = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	av[ac] = "";
	if (fcnb[ac] > 0) {
	    av[ac] = t;
	    t += fcnb[ac];
	}
	ac++;
    }
    av[ac] = NULL;	/* XXX tag arrays are not NULL terminated. */

exit:
    fi = rpmfiFree(fi);
    /*@-branchstate@*/
    if (fcontextp)
	*fcontextp = av;
    else
	av = _free(av);
    /*@=branchstate@*/
    if (fcp) *fcp = ac;
}

void rpmfiBuildREContexts(Header h,
	/*@out@*/ const char *** fcontextp, /*@out@*/ int * fcp)
{
    int scareMem = 0;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, scareMem);
    rpmsx sx = NULL;
    const char ** av = NULL;
    int ac;
    size_t nb;
    char * t;
    char * fctxt = NULL;
    size_t fctxtlen = 0;
    int * fcnb;

    if ((ac = rpmfiFC(fi)) <= 0) {
	ac = 0;
	goto exit;
    }

    /* Read security context patterns. */
    sx = rpmsxNew(NULL);

    /* Compute size of argv array blob, concatenating file contexts. */
    nb = ac * sizeof(*fcnb);
    fcnb = memset(alloca(nb), 0, nb);
    ac = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	const char * fn = rpmfiFN(fi);
	mode_t fmode = rpmfiFMode(fi);
	const char * scon;

	scon = rpmsxFContext(sx, fn, fmode);
	if (scon != NULL) {
	    fcnb[ac] = strlen(scon) + 1;
/*@-branchstate@*/
	    if (fcnb[ac] > 0) {
		fctxt = xrealloc(fctxt, fctxtlen + fcnb[ac]);
		memcpy(fctxt+fctxtlen, scon, fcnb[ac]);
		fctxtlen += fcnb[ac];
	    }
/*@=branchstate@*/
	}
	ac++;
    }

    /* Create and load argv array from concatenated file contexts. */
    nb = (ac + 1) * sizeof(*av) + fctxtlen;
    av = xmalloc(nb);
    t = ((char *) av) + ((ac + 1) * sizeof(*av));
    (void) memcpy(t, fctxt, fctxtlen);
    ac = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	av[ac] = "";
	if (fcnb[ac] > 0) {
	    av[ac] = t;
	    t += fcnb[ac];
	}
	ac++;
    }
    av[ac] = NULL;	/* XXX tag arrays are not NULL terminated. */

exit:
    fi = rpmfiFree(fi);
    sx = rpmsxFree(sx);
    /*@-branchstate@*/
    if (fcontextp)
	*fcontextp = av;
    else
	av = _free(av);
    /*@=branchstate@*/
    if (fcp) *fcp = ac;
}

void rpmfiBuildFDeps(Header h, rpmTag tagN,
	/*@out@*/ const char *** fdepsp, /*@out@*/ int * fcp)
{
    int scareMem = 0;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, scareMem);
    rpmds ds = NULL;
    const char ** av;
    int ac;
    size_t nb;
    char * t;
    char deptype = 'R';
    char mydt;
    const char * DNEVR;
    const uint_32 * ddict;
    unsigned ix;
    int ndx;

    if ((ac = rpmfiFC(fi)) <= 0) {
	av = NULL;
	ac = 0;
	goto exit;
    }

    if (tagN == RPMTAG_PROVIDENAME)
	deptype = 'P';
    else if (tagN == RPMTAG_REQUIRENAME)
	deptype = 'R';

    ds = rpmdsNew(h, tagN, scareMem);

    /* Compute size of file depends argv array blob. */
    nb = (ac + 1) * sizeof(*av);
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	ddict = NULL;
	ndx = rpmfiFDepends(fi, &ddict);
	if (ddict != NULL)
	while (ndx-- > 0) {
	    ix = *ddict++;
	    mydt = ((ix >> 24) & 0xff);
	    if (mydt != deptype)
		/*@innercontinue@*/ continue;
	    ix &= 0x00ffffff;
	    (void) rpmdsSetIx(ds, ix-1);
	    if (rpmdsNext(ds) < 0)
		/*@innercontinue@*/ continue;
	    DNEVR = rpmdsDNEVR(ds);
	    if (DNEVR != NULL)
		nb += strlen(DNEVR+2) + 1;
	}
	nb += 1;
    }

    /* Create and load file depends argv array. */
    av = xmalloc(nb);
    t = ((char *) av) + ((ac + 1) * sizeof(*av));
    ac = 0;
    /*@-branchstate@*/
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	av[ac++] = t;
	ddict = NULL;
	ndx = rpmfiFDepends(fi, &ddict);
	if (ddict != NULL)
	while (ndx-- > 0) {
	    ix = *ddict++;
	    mydt = ((ix >> 24) & 0xff);
	    if (mydt != deptype)
		/*@innercontinue@*/ continue;
	    ix &= 0x00ffffff;
	    (void) rpmdsSetIx(ds, ix-1);
	    if (rpmdsNext(ds) < 0)
		/*@innercontinue@*/ continue;
	    DNEVR = rpmdsDNEVR(ds);
	    if (DNEVR != NULL) {
		t = stpcpy(t, DNEVR+2);
		*t++ = ' ';
		*t = '\0';
	    }
	}
	*t++ = '\0';
    }
    /*@=branchstate@*/
    av[ac] = NULL;	/* XXX tag arrays are not NULL terminated. */

exit:
    fi = rpmfiFree(fi);
    ds = rpmdsFree(ds);
    /*@-branchstate@*/
    if (fdepsp)
	*fdepsp = av;
    else
	av = _free(av);
    /*@=branchstate@*/
    if (fcp) *fcp = ac;
}
