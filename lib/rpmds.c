/** \ingroup rpmds
 * \file lib/rpmds.c
 */
#include "system.h"

#if HAVE_GELF_H
#if LIBELF_H_LFS_CONFLICT
/* Some implementations of libelf.h/gelf.h are incompatible with
 * the Large File API.
 */
# undef _LARGEFILE64_SOURCE
# undef _LARGEFILE_SOURCE
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif

#include <gelf.h>
/*
 * On Solaris, gelf.h included libelf.h, which #undef'ed the gettext
 * convenience macro _().  Repair by repeating (from system.h) just
 * the bits that are needed for _() to function.
 */

#if defined(__sun)
#if ENABLE_NLS && !defined(__LCLINT__)
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#endif /* gettext _() fixup */
#endif
#endif /* HAVE_GELF_H */

#if !defined(DT_GNU_HASH)
#define	DT_GNU_HASH	0x6ffffef5
#endif

#include <rpmio_internal.h>	/* XXX fdGetFILE */
#include <rpmlib.h>
#include <rpmmacro.h>

#define	_RPMDS_INTERNAL
#define	_RPMEVR_INTERNAL
#define	_RPMPRCO_INTERNAL
#include <rpmds.h>

#include <argv.h>

#include "debug.h"

#define	_isspace(_c)	\
	((_c) == ' ' || (_c) == '\t' || (_c) == '\r' || (_c) == '\n')

/**
 * Enable noisy range comparison debugging message?
 */
/*@unchecked@*/
static int _noisy_range_comparison_debug_message = 0;

/*@unchecked@*/
int _rpmds_debug = 0;

/*@unchecked@*/
int _rpmds_nopromote = 1;

/*@unchecked@*/
/*@-exportheadervar@*/
int _rpmds_unspecified_epoch_noise = 0;
/*@=exportheadervar@*/

rpmds XrpmdsUnlink(rpmds ds, const char * msg, const char * fn, unsigned ln)
{
    if (ds == NULL) return NULL;
/*@-modfilesys@*/
if (_rpmds_debug && msg != NULL)
fprintf(stderr, "--> ds %p -- %d %s at %s:%u\n", ds, ds->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    ds->nrefs--;
    return NULL;
}

rpmds XrpmdsLink(rpmds ds, const char * msg, const char * fn, unsigned ln)
{
    if (ds == NULL) return NULL;
    ds->nrefs++;

/*@-modfilesys@*/
if (_rpmds_debug && msg != NULL)
fprintf(stderr, "--> ds %p ++ %d %s at %s:%u\n", ds, ds->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    /*@-refcounttrans@*/ return ds; /*@=refcounttrans@*/
}

rpmds rpmdsFree(rpmds ds)
{
    HFD_t hfd = headerFreeData;
    rpmTag tagEVR, tagF;

    if (ds == NULL)
	return NULL;

    if (ds->nrefs > 1)
	return rpmdsUnlink(ds, ds->Type);

/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "*** ds %p\t%s[%d]\n", ds, ds->Type, ds->Count);
/*@=modfilesys@*/

    if (ds->tagN == RPMTAG_PROVIDENAME) {
	tagEVR = RPMTAG_PROVIDEVERSION;
	tagF = RPMTAG_PROVIDEFLAGS;
    } else
    if (ds->tagN == RPMTAG_REQUIRENAME) {
	tagEVR = RPMTAG_REQUIREVERSION;
	tagF = RPMTAG_REQUIREFLAGS;
    } else
    if (ds->tagN == RPMTAG_CONFLICTNAME) {
	tagEVR = RPMTAG_CONFLICTVERSION;
	tagF = RPMTAG_CONFLICTFLAGS;
    } else
    if (ds->tagN == RPMTAG_OBSOLETENAME) {
	tagEVR = RPMTAG_OBSOLETEVERSION;
	tagF = RPMTAG_OBSOLETEFLAGS;
    } else
    if (ds->tagN == RPMTAG_TRIGGERNAME) {
	tagEVR = RPMTAG_TRIGGERVERSION;
	tagF = RPMTAG_TRIGGERFLAGS;
    } else
    if (ds->tagN == RPMTAG_DIRNAMES) {
	tagEVR = 0;
	tagF = 0;
    } else
    if (ds->tagN == RPMTAG_FILELINKTOS) {
	tagEVR = 0;
	tagF = 0;
    } else
	return NULL;

    /*@-branchstate@*/
    if (ds->Count > 0) {
	ds->N = hfd(ds->N, ds->Nt);
	ds->EVR = hfd(ds->EVR, ds->EVRt);
	/*@-evalorder@*/
	ds->Flags = (ds->h != NULL ? hfd(ds->Flags, ds->Ft) : _free(ds->Flags));
	/*@=evalorder@*/
	ds->h = headerFree(ds->h);
    }
    /*@=branchstate@*/

    ds->DNEVR = _free(ds->DNEVR);
    ds->ns.str = _free(ds->ns.str);
    memset(&ds->ns, 0, sizeof(ds->ns));
    ds->A = _free(ds->A);
    ds->Color = _free(ds->Color);
    ds->Refs = _free(ds->Refs);
    ds->Result = _free(ds->Result);

    (void) rpmdsUnlink(ds, ds->Type);
    /*@-refcounttrans -usereleased@*/
/*@-boundswrite@*/
    memset(ds, 0, sizeof(*ds));		/* XXX trash and burn */
/*@=boundswrite@*/
    ds = _free(ds);
    /*@=refcounttrans =usereleased@*/
    return NULL;
}

/*@-bounds@*/
static /*@null@*/
const char ** rpmdsDupArgv(/*@null@*/ const char ** argv, int argc)
	/*@*/
{
    const char ** av;
    size_t nb = 0;
    int ac = 0;
    char * t;

    if (argv == NULL)
	return NULL;
    for (ac = 0; ac < argc; ac++) {
assert(argv[ac] != NULL);
	nb += strlen(argv[ac]) + 1;
    }
    nb += (ac + 1) * sizeof(*av);

    av = xmalloc(nb);
    t = (char *) (av + ac + 1);
    for (ac = 0; ac < argc; ac++) {
	av[ac] = t;
	t = stpcpy(t, argv[ac]) + 1;
    }
    av[ac] = NULL;
/*@-nullret@*/
    return av;
/*@=nullret@*/
}
/*@=bounds@*/

rpmds rpmdsNew(Header h, rpmTag tagN, int flags)
{
    HFD_t hfd = headerFreeData;
    int scareMem = (flags & 0x1);
    HGE_t hge =
	(scareMem ? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry);
    rpmTag tagEVR, tagF;
    rpmds ds = NULL;
    const char * Type;
    const char ** N;
    rpmTagType Nt;
    int_32 Count;

assert(scareMem == 0);		/* XXX always allocate memory */
    if (tagN == RPMTAG_PROVIDENAME) {
	Type = "Provides";
	tagEVR = RPMTAG_PROVIDEVERSION;
	tagF = RPMTAG_PROVIDEFLAGS;
    } else
    if (tagN == RPMTAG_REQUIRENAME) {
	Type = "Requires";
	tagEVR = RPMTAG_REQUIREVERSION;
	tagF = RPMTAG_REQUIREFLAGS;
    } else
    if (tagN == RPMTAG_CONFLICTNAME) {
	Type = "Conflicts";
	tagEVR = RPMTAG_CONFLICTVERSION;
	tagF = RPMTAG_CONFLICTFLAGS;
    } else
    if (tagN == RPMTAG_OBSOLETENAME) {
	Type = "Obsoletes";
	tagEVR = RPMTAG_OBSOLETEVERSION;
	tagF = RPMTAG_OBSOLETEFLAGS;
    } else
    if (tagN == RPMTAG_TRIGGERNAME) {
	Type = "Triggers";
	tagEVR = RPMTAG_TRIGGERVERSION;
	tagF = RPMTAG_TRIGGERFLAGS;
    } else
    if (tagN == RPMTAG_DIRNAMES) {
	Type = "Dirnames";
	tagEVR = 0;
	tagF = 0;
    } else
    if (tagN == RPMTAG_FILELINKTOS) {
	Type = "Filelinktos";
	tagEVR = RPMTAG_DIRNAMES;
	tagF = RPMTAG_DIRINDEXES;
    } else
	goto exit;

    /*@-branchstate@*/
    if (hge(h, tagN, &Nt, (void **) &N, &Count)
     && N != NULL && Count > 0)
    {
	int xx;

	ds = xcalloc(1, sizeof(*ds));
	ds->Type = Type;
	ds->h = (scareMem ? headerLink(h) : NULL);
	ds->i = -1;
	ds->DNEVR = NULL;
	ds->tagN = tagN;
	ds->N = N;
	ds->Nt = Nt;
	ds->Count = Count;
	ds->nopromote = _rpmds_nopromote;

	if (tagEVR > 0)
	    xx = hge(h, tagEVR, &ds->EVRt, (void **) &ds->EVR, NULL);
	if (tagF > 0)
	    xx = hge(h, tagF, &ds->Ft, (void **) &ds->Flags, NULL);
/*@-boundsread@*/
	if (!scareMem && ds->Flags != NULL)
	    ds->Flags = memcpy(xmalloc(ds->Count * sizeof(*ds->Flags)),
                                ds->Flags, ds->Count * sizeof(*ds->Flags));
	{   rpmTag tagA = RPMTAG_ARCH;
	    rpmTagType At;
	    const char * A = NULL;
	    if (tagA > 0)
		xx = hge(h, tagA, &At, (void **) &A, NULL);
	    ds->A = (xx && A != NULL ? xstrdup(A) : NULL);
	}
	{   rpmTag tagBT = RPMTAG_BUILDTIME;
	    rpmTagType BTt;
	    int_32 * BTp = NULL;
	    if (tagBT > 0)
		xx = hge(h, tagBT, &BTt, (void **) &BTp, NULL);
	    ds->BT = (xx && BTp != NULL && BTt == RPM_INT32_TYPE ? *BTp : 0);
	}
/*@=boundsread@*/

	if (tagN == RPMTAG_DIRNAMES) {
	    char * t;
	    size_t len;
	    int i;
	    /* XXX Dirnames always have trailing '/', trim that here. */
	    for (i = 0; i < Count; i++) {
		(void) urlPath(N[i], (const char **)&t);
		if (t > N[i])
		    N[i] = t;
		t = (char *)N[i];
		len = strlen(t);
		/* XXX don't truncate if parent is / */
		if (len > 1 && t[len-1] == '/')
		    t[len-1] = '\0';
	    }
	} else
	if (tagN == RPMTAG_FILELINKTOS) {
	    /* XXX Construct the absolute path of the target symlink(s). */
	    const char ** av = xcalloc(Count+1, sizeof(*av));
	    int i;

	    for (i = 0; i < Count; i++) {
		if (N[i] == NULL || *N[i] == '\0')
		    av[i] = xstrdup("");
		else if (*N[i] == '/')
		    av[i] = xstrdup(N[i]);
		else if (ds->EVR && ds->Flags)
/*@-nullderef@*/	/* XXX ds->Flags != NULL */
		    av[i] = rpmGenPath(NULL, ds->EVR[ds->Flags[i]], N[i]);
/*@=nullderef@*/
		else
		    av[i] = NULL;
	    }
	    av[Count] = NULL;

	    N = ds->N = hfd(ds->N, ds->Nt);
	    ds->N = rpmdsDupArgv(av, Count);
	    av = argvFree(av);
	    ds->EVR = hfd(ds->EVR, ds->EVRt);
	    /*@-evalorder@*/
	    ds->Flags = (ds->h != NULL ? hfd(ds->Flags, ds->Ft) : _free(ds->Flags));
	    /*@=evalorder@*/
	}

/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "*** ds %p\t%s[%d]\n", ds, ds->Type, ds->Count);
/*@=modfilesys@*/

    }
    /*@=branchstate@*/

exit:
/*@-compdef -usereleased@*/	/* FIX: ds->Flags may be NULL */
    /*@-nullstate@*/ /* FIX: ds->Flags may be NULL */
    ds = rpmdsLink(ds, (ds ? ds->Type : NULL));
    /*@=nullstate@*/

    return ds;
/*@=compdef =usereleased@*/
}

/*@-mods@*/ /* FIX: correct annotations for ds->ns shadow */
const char * rpmdsNewN(rpmds ds)
{
    rpmns ns = &ds->ns;
    const char * Name = ds->N[ds->i];;
    int_32 Flags = rpmdsFlags(ds);
    int xx;

    memset(ns, 0, sizeof(*ns));
    if (Name[0] == '%' && (Flags & RPMSENSE_INTERP))
	ns->N = ns->str = rpmExpand(Name, NULL);
    else
	xx = rpmnsParse(Name, ns);
    Name = ns->N;
/*@-usereleased -compdef@*/ /* FIX: correct annotations for ds->ns shadow */
    return Name;
/*@-usereleased -compdef@*/
}
/*@=mods@*/

char * rpmdsNewDNEVR(const char * dspfx, rpmds ds)
{
    const char * N = rpmdsNewN(ds);
    const char * NS = ds->ns.NS;
    const char * A = ds->ns.A;
    char * tbuf, * t;
    size_t nb = 0;

    if (dspfx)	nb += strlen(dspfx) + 1;
/*@-boundsread@*/
    if (NS)	nb += strlen(NS) + sizeof("()") - 1;
    if (N)	nb += strlen(N);
    if (A) {
	if (_rpmns_N_at_A && _rpmns_N_at_A[0])
	    nb += sizeof(_rpmns_N_at_A[0]);
	nb += strlen(A);
    }
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->Flags != NULL && (ds->Flags[ds->i] & RPMSENSE_SENSEMASK)) {
	if (nb)	nb++;
	if (ds->Flags[ds->i] & RPMSENSE_LESS)	nb++;
	if (ds->Flags[ds->i] & RPMSENSE_GREATER) nb++;
	if (ds->Flags[ds->i] & RPMSENSE_EQUAL)	nb++;
    }
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->EVR != NULL && ds->EVR[ds->i] && *ds->EVR[ds->i]) {
	if (nb)	nb++;
	nb += strlen(ds->EVR[ds->i]);
    }
/*@=boundsread@*/

/*@-boundswrite@*/
    t = tbuf = xmalloc(nb + 1);
    if (dspfx) {
	t = stpcpy(t, dspfx);
	*t++ = ' ';
    }
    if (NS)
	t = stpcpy( stpcpy(t, NS), "(");
    if (N)
	t = stpcpy(t, N);
    if (NS)
	t = stpcpy(t, ")");
    if (A) {
	if (_rpmns_N_at_A && _rpmns_N_at_A[0])
	    *t++ = _rpmns_N_at_A[0];
	t = stpcpy(t, A);
    }

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->Flags != NULL && (ds->Flags[ds->i] & RPMSENSE_SENSEMASK)) {
	if (t != tbuf)	*t++ = ' ';
	if (ds->Flags[ds->i] & RPMSENSE_LESS)	*t++ = '<';
	if (ds->Flags[ds->i] & RPMSENSE_GREATER) *t++ = '>';
	if (ds->Flags[ds->i] & RPMSENSE_EQUAL)	*t++ = '=';
    }
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->EVR != NULL && ds->EVR[ds->i] && *ds->EVR[ds->i]) {
	if (t != tbuf)	*t++ = ' ';
	t = stpcpy(t, ds->EVR[ds->i]);
    }
    *t = '\0';
/*@=boundswrite@*/
    return tbuf;
}

rpmds rpmdsThis(Header h, rpmTag tagN, int_32 Flags)
{
    HGE_t hge = (HGE_t) headerGetEntryMinMemory;
    rpmds ds = NULL;
    const char * Type;
    const char * n, * v, * r;
    int_32 * ep;
    const char ** N, ** EVR;
    char * t;
    int xx;

    if (tagN == RPMTAG_PROVIDENAME) {
	Type = "Provides";
    } else
    if (tagN == RPMTAG_REQUIRENAME) {
	Type = "Requires";
    } else
    if (tagN == RPMTAG_CONFLICTNAME) {
	Type = "Conflicts";
    } else
    if (tagN == RPMTAG_OBSOLETENAME) {
	Type = "Obsoletes";
    } else
    if (tagN == RPMTAG_TRIGGERNAME) {
	Type = "Triggers";
    } else
    if (tagN == RPMTAG_DIRNAMES) {
	Type = "Dirnames";
    } else
    if (tagN == RPMTAG_FILELINKTOS) {
	Type = "Filelinktos";
    } else
	goto exit;

    xx = headerNVR(h, &n, &v, &r);
    ep = NULL;
    xx = hge(h, RPMTAG_EPOCH, NULL, (void **)&ep, NULL);

    t = xmalloc(sizeof(*N) + strlen(n) + 1);
/*@-boundswrite@*/
    N = (const char **) t;
    t += sizeof(*N);
    *t = '\0';
    N[0] = t;
    t = stpcpy(t, n);

    t = xmalloc(sizeof(*EVR) +
		(ep ? 20 : 0) + strlen(v) + strlen(r) + sizeof("-"));
    EVR = (const char **) t;
    t += sizeof(*EVR);
    *t = '\0';
    EVR[0] = t;
    if (ep) {
	sprintf(t, "%d:", *ep);
	t += strlen(t);
    }
    t = stpcpy( stpcpy( stpcpy( t, v), "-"), r);
/*@=boundswrite@*/

    ds = xcalloc(1, sizeof(*ds));
    ds->Type = Type;
    ds->tagN = tagN;
    ds->Count = 1;
    ds->N = N;
    ds->Nt = -1;	/* XXX to insure that hfd will free */
    ds->EVR = EVR;
    ds->EVRt = -1;	/* XXX to insure that hfd will free */
/*@-boundswrite@*/
    ds->Flags = xmalloc(sizeof(*ds->Flags));	ds->Flags[0] = Flags;
/*@=boundswrite@*/
	{   rpmTag tagA = RPMTAG_ARCH;
	    rpmTagType At;
	    const char * A = NULL;
	    if (tagA > 0)
		xx = hge(h, tagA, &At, (void **) &A, NULL);
	    ds->A = (xx && A != NULL ? xstrdup(A) : NULL);
	}
	{   rpmTag tagBT = RPMTAG_BUILDTIME;
	    rpmTagType BTt;
	    int_32 * BTp = NULL;
	    if (tagBT > 0)
		xx = hge(h, tagBT, &BTt, (void **) &BTp, NULL);
	    ds->BT = (xx && BTp != NULL && BTt == RPM_INT32_TYPE ? *BTp : 0);
	}
    {	char pre[2];
/*@-boundsread@*/
	pre[0] = ds->Type[0];
/*@=boundsread@*/
	pre[1] = '\0';
	/*@-nullstate@*/ /* LCL: ds->Type may be NULL ??? */
/*@i@*/	ds->DNEVR = rpmdsNewDNEVR(pre, ds);
	/*@=nullstate@*/
    }

exit:
    return rpmdsLink(ds, (ds ? ds->Type : NULL));
}

rpmds rpmdsSingle(rpmTag tagN, const char * N, const char * EVR, int_32 Flags)
{
    rpmds ds = NULL;
    const char * Type;

    if (tagN == RPMTAG_PROVIDENAME) {
	Type = "Provides";
    } else
    if (tagN == RPMTAG_REQUIRENAME) {
	Type = "Requires";
    } else
    if (tagN == RPMTAG_CONFLICTNAME) {
	Type = "Conflicts";
    } else
    if (tagN == RPMTAG_OBSOLETENAME) {
	Type = "Obsoletes";
    } else
    if (tagN == RPMTAG_TRIGGERNAME) {
	Type = "Triggers";
    } else
    if (tagN == RPMTAG_DIRNAMES) {
	Type = "Dirnames";
    } else
    if (tagN == RPMTAG_FILELINKTOS) {
	Type = "Filelinktos";
    } else
	goto exit;

    ds = xcalloc(1, sizeof(*ds));
    ds->Type = Type;
    ds->tagN = tagN;
    ds->A = NULL;
    {	time_t now = time(NULL);
	ds->BT = now;
    }
    ds->Count = 1;
    /*@-assignexpose@*/
/*@-boundswrite@*/
    ds->N = xmalloc(sizeof(*ds->N));		ds->N[0] = N;
    ds->Nt = -1;	/* XXX to insure that hfd will free */
    ds->EVR = xmalloc(sizeof(*ds->EVR));	ds->EVR[0] = EVR;
    ds->EVRt = -1;	/* XXX to insure that hfd will free */
    /*@=assignexpose@*/
    ds->Flags = xmalloc(sizeof(*ds->Flags));	ds->Flags[0] = Flags;
/*@=boundswrite@*/
    {	char t[2];
/*@-boundsread@*/
	t[0] = ds->Type[0];
/*@=boundsread@*/
	t[1] = '\0';
/*@i@*/	ds->DNEVR = rpmdsNewDNEVR(t, ds);
    }

exit:
    return rpmdsLink(ds, (ds ? ds->Type : NULL));
}

int rpmdsCount(const rpmds ds)
{
    return (ds != NULL ? ds->Count : 0);
}

int rpmdsIx(const rpmds ds)
{
    return (ds != NULL ? ds->i : -1);
}

int rpmdsSetIx(rpmds ds, int ix)
{
    int i = -1;

    if (ds != NULL) {
	i = ds->i;
	ds->i = ix;
    }
    return i;
}

const char * rpmdsDNEVR(const rpmds ds)
{
    const char * DNEVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
/*@-boundsread@*/
	if (ds->DNEVR != NULL)
	    DNEVR = ds->DNEVR;
/*@=boundsread@*/
    }
    return DNEVR;
}

const char * rpmdsN(const rpmds ds)
{
    const char * N = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
/*@-boundsread -globs -mods @*/	/* FIX: correct annotations for ds->ns shadow */
	N = (ds->ns.N ? ds->ns.N : rpmdsNewN(ds));
/*@=boundsread =globs =mods @*/
    }
    return N;
}

const char * rpmdsEVR(const rpmds ds)
{
    const char * EVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
/*@-boundsread@*/
	if (ds->EVR != NULL)
	    EVR = ds->EVR[ds->i];
/*@=boundsread@*/
    }
    return EVR;
}

int_32 rpmdsFlags(const rpmds ds)
{
    int_32 Flags = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
/*@-boundsread@*/
	if (ds->Flags != NULL)
	    Flags = ds->Flags[ds->i];
/*@=boundsread@*/
    }
    return Flags;
}

rpmTag rpmdsTagN(const rpmds ds)
{
    rpmTag tagN = 0;

    if (ds != NULL)
	tagN = ds->tagN;
    return tagN;
}

const char * rpmdsA(const rpmds ds)
{
    const char * A = NULL;

    if (ds != NULL)
	A = ds->A;
    return A;
}

time_t rpmdsBT(const rpmds ds)
{
    time_t BT = 0;
    if (ds != NULL && ds->BT > 0)
	BT = ds->BT;
    return BT;
}

time_t rpmdsSetBT(const rpmds ds, time_t BT)
{
    time_t oBT = 0;
    if (ds != NULL) {
	oBT = ds->BT;
	ds->BT = BT;
    }
    return oBT;
}

nsType rpmdsNSType(const rpmds ds)
{
    nsType NSType = RPMNS_TYPE_UNKNOWN;
    if (ds != NULL)
	NSType = ds->ns.Type;
    return NSType;
}

int rpmdsNoPromote(const rpmds ds)
{
    int nopromote = 0;

    if (ds != NULL)
	nopromote = ds->nopromote;
    return nopromote;
}

int rpmdsSetNoPromote(rpmds ds, int nopromote)
{
    int onopromote = 0;

    if (ds != NULL) {
	onopromote = ds->nopromote;
	ds->nopromote = nopromote;
    }
    return onopromote;
}

void * rpmdsSetEVRparse(rpmds ds,
	int (*EVRparse)(const char *evrstr, EVR_t evr))
{
    void * oEVRparse = NULL;

    if (ds != NULL) {
/*@i@*/	oEVRparse = ds->EVRparse;
/*@i@*/	ds->EVRparse = EVRparse;
    }
    return oEVRparse;
}

void * rpmdsSetEVRcmp(rpmds ds, int (*EVRcmp)(const char *a, const char *b))
{
    void * oEVRcmp = NULL;

    if (ds != NULL) {
/*@i@*/	oEVRcmp = ds->EVRcmp;
/*@i@*/	ds->EVRcmp = EVRcmp;
    }
    return oEVRcmp;
}

uint_32 rpmdsColor(const rpmds ds)
{
    uint_32 Color = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
/*@-boundsread@*/
	if (ds->Color != NULL)
	    Color = ds->Color[ds->i];
/*@=boundsread@*/
    }
    return Color;
}

uint_32 rpmdsSetColor(const rpmds ds, uint_32 color)
{
    uint_32 ocolor = 0;

    if (ds == NULL)
	return ocolor;

    if (ds->Color == NULL && ds->Count > 0)	/* XXX lazy malloc */
	ds->Color = xcalloc(ds->Count, sizeof(*ds->Color));

    if (ds->i >= 0 && ds->i < ds->Count) {
/*@-bounds@*/
	if (ds->Color != NULL) {
	    ocolor = ds->Color[ds->i];
	    ds->Color[ds->i] = color;
	}
/*@=bounds@*/
    }
    return ocolor;
}

int_32 rpmdsRefs(const rpmds ds)
{
    int_32 Refs = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
/*@-boundsread@*/
	if (ds->Refs != NULL)
	    Refs = ds->Refs[ds->i];
/*@=boundsread@*/
    }
    return Refs;
}

int_32 rpmdsSetRefs(const rpmds ds, int_32 refs)
{
    int_32 orefs = 0;

    if (ds == NULL)
	return orefs;

    if (ds->Refs == NULL && ds->Count > 0)	/* XXX lazy malloc */
	ds->Refs = xcalloc(ds->Count, sizeof(*ds->Refs));

    if (ds->i >= 0 && ds->i < ds->Count) {
/*@-bounds@*/
	if (ds->Refs != NULL) {
	    orefs = ds->Refs[ds->i];
	    ds->Refs[ds->i] = refs;
	}
/*@=bounds@*/
    }
    return orefs;
}

int_32 rpmdsResult(const rpmds ds)
{
    int_32 result = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
/*@-boundsread@*/
	if (ds->Result != NULL)
	    result = ds->Result[ds->i];
/*@=boundsread@*/
    }
    return result;
}

int_32 rpmdsSetResult(const rpmds ds, int_32 result)
{
    int_32 oresult = 0;

    if (ds == NULL)
	return oresult;

    if (ds->Result == NULL && ds->Count > 0)	/* XXX lazy malloc */
	ds->Result = xcalloc(ds->Count, sizeof(*ds->Result));

    if (ds->i >= 0 && ds->i < ds->Count) {
/*@-bounds@*/
	if (ds->Result != NULL) {
	    oresult = ds->Result[ds->i];
	    ds->Result[ds->i] = result;
	}
/*@=bounds@*/
    }
    return oresult;
}

void rpmdsNotify(rpmds ds, const char * where, int rc)
{
    if (!(ds != NULL && ds->i >= 0 && ds->i < ds->Count))
	return;
    if (!(ds->Type != NULL && ds->DNEVR != NULL))
	return;

    rpmMessage(RPMMESS_DEBUG, "%9s: %-45s %-s %s\n", ds->Type,
		(!strcmp(ds->DNEVR, "cached") ? ds->DNEVR : ds->DNEVR+2),
		(rc ? _("NO ") : _("YES")),
		(where != NULL ? where : ""));
}

int rpmdsNext(/*@null@*/ rpmds ds)
	/*@modifies ds @*/
{
    int i = -1;

    if (ds != NULL && ++ds->i >= 0) {
	if (ds->i < ds->Count) {
	    char t[2];
	    i = ds->i;
	    ds->DNEVR = _free(ds->DNEVR);
	    ds->ns.str = _free(ds->ns.str);
	    memset(&ds->ns, 0, sizeof(ds->ns));
	    t[0] = ((ds->Type != NULL) ? ds->Type[0] : '\0');
	    t[1] = '\0';
	    /*@-nullstate@*/
	   /*@i@*/ ds->DNEVR = rpmdsNewDNEVR(t, ds);
	    /*@=nullstate@*/

	} else
	    ds->i = -1;

/*@-modfilesys @*/
if (_rpmds_debug  < 0 && i != -1)
fprintf(stderr, "*** ds %p\t%s[%d]: %s\n", ds, (ds->Type ? ds->Type : "?Type?"), i, (ds->DNEVR ? ds->DNEVR : "?DNEVR?"));
/*@=modfilesys @*/

    }

    return i;
}

rpmds rpmdsInit(/*@null@*/ rpmds ds)
	/*@modifies ds @*/
{
    if (ds != NULL)
	ds->i = -1;
    /*@-refcounttrans@*/
    return ds;
    /*@=refcounttrans@*/
}

/*@null@*/
static rpmds rpmdsDup(const rpmds ods)
	/*@modifies ods @*/
{
    rpmds ds = xcalloc(1, sizeof(*ds));
    size_t nb;

    ds->h = (ods->h != NULL ? headerLink(ods->h) : NULL);
/*@-assignexpose@*/
    ds->Type = ods->Type;
/*@=assignexpose@*/
    ds->tagN = ods->tagN;
    ds->Count = ods->Count;
    ds->i = ods->i;
    ds->l = ods->l;
    ds->u = ods->u;

    nb = (ds->Count+1) * sizeof(*ds->N);
    ds->N = (ds->h != NULL
	? memcpy(xmalloc(nb), ods->N, nb)
	: rpmdsDupArgv(ods->N, ods->Count) );
    ds->Nt = ods->Nt;

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
assert(ods->EVR != NULL);
assert(ods->Flags != NULL);

    nb = (ds->Count+1) * sizeof(*ds->EVR);
    ds->EVR = (ds->h != NULL
	? memcpy(xmalloc(nb), ods->EVR, nb)
	: rpmdsDupArgv(ods->EVR, ods->Count) );
    ds->EVRt = ods->EVRt;

    nb = (ds->Count * sizeof(*ds->Flags));
    ds->Flags = (ds->h != NULL
	? ods->Flags
	: memcpy(xmalloc(nb), ods->Flags, nb) );
    ds->Ft = ods->Ft;
    ds->nopromote = ods->nopromote;
/*@-assignexpose@*/
    ds->EVRcmp = ods->EVRcmp;;
/*@=assignexpose@*/

/*@-compmempass@*/ /* FIX: ds->Flags is kept, not only */
    return rpmdsLink(ds, (ds ? ds->Type : NULL));
/*@=compmempass@*/

}

int rpmdsFind(rpmds ds, const rpmds ods)
{
    int comparison;

    if (ds == NULL || ods == NULL)
	return -1;

    ds->l = 0;
    ds->u = ds->Count;
    while (ds->l < ds->u) {
	ds->i = (ds->l + ds->u) / 2;

	comparison = strcmp(ods->N[ods->i], ds->N[ds->i]);

	/* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
/*@-nullderef@*/
	if (comparison == 0 && ods->EVR && ds->EVR)
	    comparison = strcmp(ods->EVR[ods->i], ds->EVR[ds->i]);
	if (comparison == 0 && ods->Flags && ds->Flags)
	    comparison = (ods->Flags[ods->i] - ds->Flags[ds->i]);
/*@=nullderef@*/

	if (comparison < 0)
	    ds->u = ds->i;
	else if (comparison > 0)
	    ds->l = ds->i + 1;
	else
	    return ds->i;
    }
    return -1;
}

int rpmdsMerge(rpmds * dsp, rpmds ods)
{
    rpmds ds;
    const char ** N;
    const char ** EVR;
    int_32 * Flags;
    int j;
int save;

    if (dsp == NULL || ods == NULL)
	return -1;

    /* If not initialized yet, dup the 1st entry. */
/*@-branchstate@*/
    if (*dsp == NULL) {
	save = ods->Count;
	ods->Count = 1;
	*dsp = rpmdsDup(ods);
	ods->Count = save;
    }
/*@=branchstate@*/
    ds = *dsp;
    if (ds == NULL)
	return -1;

    /*
     * Add new entries.
     */
save = ods->i;
    ods = rpmdsInit(ods);
    if (ods != NULL)
    while (rpmdsNext(ods) >= 0) {
	/*
	 * If this entry is already present, don't bother.
	 */
	if (rpmdsFind(ds, ods) >= 0)
	    continue;

	/*
	 * Insert new entry.
	 */
	for (j = ds->Count; j > ds->u; j--)
	    ds->N[j] = ds->N[j-1];
	ds->N[ds->u] = ods->N[ods->i];
	N = rpmdsDupArgv(ds->N, ds->Count+1);
	ds->N = _free(ds->N);
	ds->N = N;
	
	/* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
/*@-nullderef -nullpass -nullptrarith @*/
assert(ods->EVR != NULL);
assert(ods->Flags != NULL);

	for (j = ds->Count; j > ds->u; j--)
	    ds->EVR[j] = ds->EVR[j-1];
	ds->EVR[ds->u] = ods->EVR[ods->i];
	EVR = rpmdsDupArgv(ds->EVR, ds->Count+1);
	ds->EVR = _free(ds->EVR);
	ds->EVR = EVR;

	Flags = xmalloc((ds->Count+1) * sizeof(*Flags));
	if (ds->u > 0)
	    memcpy(Flags, ds->Flags, ds->u * sizeof(*Flags));
	if (ds->u < ds->Count)
	    memcpy(Flags + ds->u + 1, ds->Flags + ds->u, (ds->Count - ds->u) * sizeof(*Flags));
	Flags[ds->u] = ods->Flags[ods->i];
	ds->Flags = _free(ds->Flags);
	ds->Flags = Flags;
/*@=nullderef =nullpass =nullptrarith @*/

	ds->i = ds->Count;
	ds->Count++;

    }
/*@-nullderef@*/
ods->i = save;
/*@=nullderef@*/
    return 0;
}

int rpmdsSearch(rpmds ds, rpmds ods)
{
    int comparison;
    int i, l, u;

    if (ds == NULL || ods == NULL)
	return -1;

    /* Binary search to find the [l,u) subset that contains N */
    i = -1;
    l = 0;
    u = ds->Count;
    while (l < u) {
	i = (l + u) / 2;

	comparison = strcmp(ods->N[ods->i], ds->N[i]);

	if (comparison < 0)
	    u = i;
	else if (comparison > 0)
	    l = i + 1;
	else {
	    /* Set l to 1st member of set that contains N. */
	    if (strcmp(ods->N[ods->i], ds->N[l]))
		l = i;
	    while (l > 0 && !strcmp(ods->N[ods->i], ds->N[l-1]))
		l--;
	    /* Set u to 1st member of set that does not contain N. */
	    if (u >= ds->Count || strcmp(ods->N[ods->i], ds->N[u]))
		u = i;
	    while (++u < ds->Count) {
		if (strcmp(ods->N[ods->i], ds->N[u]))
		    /*@innerbreak@*/ break;
	    }
	    break;
	}
    }

    /* Check each member of [l,u) subset for ranges overlap. */
    i = -1;
    if (l < u) {
	int save = rpmdsSetIx(ds, l-1);
	while ((l = rpmdsNext(ds)) >= 0 && (l < u)) {
	    if ((i = rpmdsCompare(ods, ds)) != 0)
		break;
	}
	/* Return element index that overlaps, or -1. */
	if (i)
	    i = rpmdsIx(ds);
	else {
	    (void) rpmdsSetIx(ds, save);
	    i = -1;
	}
	/* Save the return value. */
	if (ods->Result != NULL)
	    (void) rpmdsSetResult(ods, (i != -1 ? 1 : 0));
    }
    return i;
}

struct cpuinfo_s {
/*@observer@*/ /*@null@*/
    const char *name;
    int done;
    int flags;
};

/*@unchecked@*/
static struct cpuinfo_s ctags[] = {
    { "processor",	0,  0 },
    { "vendor_id",	0,  0 },
    { "cpu_family",	0,  1 },
    { "model",		0,  1 },
    { "model_name",	0,  0 },
    { "stepping",	0,  1 },
    { "cpu_MHz",	0,  1 },
    { "cache_size",	0,  1 },
    { "physical_id",	0,  0 },
    { "siblings",	0,  0 },
    { "core_id",	0,  0 },
    { "cpu_cores",	0,  0 },
    { "fdiv_bug",	0,  3 },
    { "hlt_bug",	0,  3 },
    { "f00f_bug",	0,  3 },
    { "coma_bug",	0,  3 },
    { "fpu",		0,  0 },	/* XXX use flags attribute instead. */
    { "fpu_exception",	0,  3 },
    { "cpuid_level",	0,  0 },
    { "wp",		0,  3 },
    { "flags",		0,  4 },
    { "bogomips",	0,  1 },
    { NULL,		0, -1 }
};

/**
 * Return dependency format to use for a cpuinfo line.
 * @param name		field name
 * @return		type of format (0 == ignore, -1 == not found)
 */
static int rpmdsCpuinfoCtagFlags(const char * name)
	/*@globals ctags @*/
	/*@modifies ctags @*/
{
    struct cpuinfo_s * ct;
    int flags = -1;

    for (ct = ctags; ct->name != NULL; ct++) {
	if (strcmp(ct->name, name))
	    continue;
	if (ct->done)
	    continue;
	ct->done = 1;		/* XXX insure single occurrence */
	flags = ct->flags;
	break;
    }
    return flags;
}

/**
 * Merge a single provides, wrapping N as "NS(N)".
 * @retval *dsp		(loaded) dependency set
 * @param NS		dependency name space
 * @param N		name
 * @param EVR		epoch:version-release
 * @param Flags		comparison/context flags
 */
static void rpmdsNSAdd(/*@out@*/ rpmds *dsp, const char * NS,
		const char *N, const char *EVR, int_32 Flags)
	/*@modifies *dsp @*/
{
    char *t;
    rpmds ds;
    int xx;

    t = alloca(strlen(NS)+sizeof("()")+strlen(N));
    *t = '\0';
    (void) stpcpy( stpcpy( stpcpy( stpcpy(t, NS), "("), N), ")");

    ds = rpmdsSingle(RPMTAG_PROVIDENAME, t, EVR, Flags);
    xx = rpmdsMerge(dsp, ds);
    ds = rpmdsFree(ds);
}

#define	_PROC_CPUINFO	"/proc/cpuinfo"
/**
 */
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char * _cpuinfo_path = NULL;

int rpmdsCpuinfo(rpmds *dsp, const char * fn)
	/*@globals _cpuinfo_path, ctags @*/
	/*@modifies _cpuinfo_path, ctags @*/
{
    struct cpuinfo_s * ct;
    const char * NS = "cpuinfo";
    char buf[BUFSIZ];
    char * f, * fe;
    char * g, * ge;
    char * t;
    FD_t fd = NULL;
    FILE * fp;
    int rc = -1;

/*@-modobserver@*/
    if (_cpuinfo_path == NULL) {
	_cpuinfo_path = rpmExpand("%{?_rpmds_cpuinfo_path}", NULL);
	/* XXX may need to validate path existence somewhen. */
	if (!(_cpuinfo_path != NULL && *_cpuinfo_path == '/')) {
/*@-observertrans @*/
	    _cpuinfo_path = _free(_cpuinfo_path);
/*@=observertrans @*/
	    _cpuinfo_path = xstrdup(_PROC_CPUINFO);
	}
    }
/*@=modobserver@*/

/*@-branchstate@*/
    if (fn == NULL)
	fn = _cpuinfo_path;
/*@=branchstate@*/

    /* Reset done variables. */
    for (ct = ctags; ct->name != NULL; ct++)
	ct->done = 0;

    fd = Fopen(fn, "r.fpio");
    if (fd == NULL || Ferror(fd))
	goto exit;
    fp = fdGetFILE(fd);

    if (fp != NULL)
    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	/* rtrim on line. */
	ge = f + strlen(f);
	while (--ge > f && _isspace(*ge))
	    *ge = '\0';

	/* ltrim on line. */
	while (*f && _isspace(*f))
	    f++;

	/* split on ':' */
	fe = f;
	while (*fe && *fe != ':')
            fe++;
	if (*fe == '\0')
	    continue;
	g = fe + 1;

	/* rtrim on field 1. */
	*fe = '\0';
	while (--fe > f && _isspace(*fe))
	    *fe = '\0';
	if (*f == '\0')
	    continue;

	/* ltrim on field 2. */
	while (*g && _isspace(*g))
            g++;
	if (*g == '\0')
	    continue;

	for (t = f; *t != '\0'; t++) {
	    if (_isspace(*t))
		*t = '_';
	}

	switch (rpmdsCpuinfoCtagFlags(f)) {
	case -1:	/* not found */
	case 0:		/* ignore */
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case 1:		/* Provides: cpuinfo(f) = g */
	    for (t = g; *t != '\0'; t++) {
		if (_isspace(*t) || *t == '(' || *t == ')')
		    *t = '_';
	    }
	    rpmdsNSAdd(dsp, NS, f, g, RPMSENSE_PROBE|RPMSENSE_EQUAL);
	    /*@switchbreak@*/ break;
	case 2:		/* Provides: cpuinfo(g) */
	    for (t = g; *t != '\0'; t++) {
		if (_isspace(*t) || *t == '(' || *t == ')')
		    *t = '_';
	    }
	    rpmdsNSAdd(dsp, NS, g, "", RPMSENSE_PROBE);
	    /*@switchbreak@*/ break;
	case 3:		/* if ("yes") Provides: cpuinfo(f) */
	   if (!strcmp(g, "yes"))
		rpmdsNSAdd(dsp, NS, f, "", RPMSENSE_PROBE);
	    /*@switchbreak@*/ break;
	case 4:		/* Provides: cpuinfo(g[i]) */
	{   char ** av = NULL;
	    int i = 0;
	    rc = poptParseArgvString(g, NULL, (const char ***)&av);
	    if (!rc && av != NULL)
	    while ((t = av[i++]) != NULL)
		rpmdsNSAdd(dsp, NS, t, "", RPMSENSE_PROBE);
	    t = NULL;
	    if (av != NULL)
		free(av);
	}   /*@switchbreak@*/ break;
	}
    }

exit:
/*@-branchstate@*/
    if (fd != NULL) (void) Fclose(fd);
/*@=branchstate@*/
    return rc;
}

struct rpmlibProvides_s {
/*@observer@*/ /*@relnull@*/
    const char * featureName;
/*@observer@*/ /*@relnull@*/
    const char * featureEVR;
    int featureFlags;
/*@observer@*/ /*@relnull@*/
    const char * featureDescription;
};

/*@unchecked@*/ /*@observer@*/
static struct rpmlibProvides_s rpmlibProvides[] = {
    { "rpmlib(VersionedDependencies)",	"3.0.3-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("PreReq:, Provides:, and Obsoletes: dependencies support versions.") },
    { "rpmlib(CompressedFileNames)",	"3.0.4-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("file name(s) stored as (dirName,baseName,dirIndex) tuple, not as path.")},
    { "rpmlib(PayloadIsBzip2)",		"3.0.5-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload can be compressed using bzip2.") },
    { "rpmlib(PayloadFilesHavePrefix)",	"4.0-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload file(s) have \"./\" prefix.") },
    { "rpmlib(ExplicitPackageProvide)",	"4.0-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package name-version-release is not implicitly provided.") },
    { "rpmlib(HeaderLoadSortsTags)",    "4.0.1-1",
	(                RPMSENSE_EQUAL),
    N_("header tags are always sorted after being loaded.") },
    { "rpmlib(ScriptletInterpreterArgs)",    "4.0.3-1",
	(                RPMSENSE_EQUAL),
    N_("the scriptlet interpreter can use arguments from header.") },
    { "rpmlib(PartialHardlinkSets)",    "4.0.4-1",
	(                RPMSENSE_EQUAL),
    N_("a hardlink file set may be installed without being complete.") },
    { "rpmlib(ConcurrentAccess)",    "4.1-1",
	(                RPMSENSE_EQUAL),
    N_("package scriptlets may access the rpm database while installing.") },
#if defined(WITH_LUA)
    { "rpmlib(BuiltinLuaScripts)",    "4.2.2-1",
	(                RPMSENSE_EQUAL),
    N_("internal support for lua scripts.") },
#endif
    { "rpmlib(HeaderTagTypeInt64)",    "4.4.3-1",
	(                RPMSENSE_EQUAL),
    N_("header tags can be type int_64.") },
    { "rpmlib(PayloadIsUstar)",		"4.4.4-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload can be in ustar tar archive format.") },
    { "rpmlib(PayloadIsLzma)",		"4.4.6-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload can be compressed using lzma.") },
    { "rpmlib(FileDigestParameterized)",    "4.4.6-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("file digests can be other than MD5.") },
    { NULL,				NULL, 0,	NULL }
};

/**
 * Load rpmlib provides into a dependency set.
 * @retval *dsp		(loaded) depedency set
 * @param tblp		rpmlib provides table (NULL uses internal table)
 * @return		0 on success
 */
int rpmdsRpmlib(rpmds * dsp, void * tblp)
{
    const struct rpmlibProvides_s * rltblp = tblp;
    const struct rpmlibProvides_s * rlp;
    int xx;

    if (rltblp == NULL)
	rltblp = rpmlibProvides;

    for (rlp = rltblp; rlp->featureName != NULL; rlp++) {
	rpmds ds = rpmdsSingle(RPMTAG_PROVIDENAME, rlp->featureName,
			rlp->featureEVR, rlp->featureFlags);
	xx = rpmdsMerge(dsp, ds);
	ds = rpmdsFree(ds);
    }
    return 0;
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static struct cmpop {
/*@observer@*/ /*@null@*/
    const char * operator;
    rpmsenseFlags sense;
} cops[] = {
    { "<=", RPMSENSE_LESS | RPMSENSE_EQUAL},
    { "=<", RPMSENSE_LESS | RPMSENSE_EQUAL},

    { "==", RPMSENSE_EQUAL},
    
    { ">=", RPMSENSE_GREATER | RPMSENSE_EQUAL},
    { "=>", RPMSENSE_GREATER | RPMSENSE_EQUAL},

    { "<", RPMSENSE_LESS},
    { "=", RPMSENSE_EQUAL},
    { ">", RPMSENSE_GREATER},

    { NULL, 0 },
};

/**
 * Merge contents of a sysinfo tag file into sysinfo dependencies.
 * @retval *PRCO	provides/requires/conflicts/obsoletes depedency set(s)
 * @param fn		path to file
 * @param tagN		dependency set tag
 * @return		0 on success
 */
static int rpmdsSysinfoFile(rpmPRCO PRCO, const char * fn, int tagN)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies PRCO, fileSystem, internalState @*/
{
    char buf[BUFSIZ];
    const char *N, *EVR;
    int_32 Flags;
    rpmds ds;
    char * f, * fe;
    char * g, * ge;
    FD_t fd = NULL;
    FILE * fp;
    int rc = -1;
    int ln;
    int xx;

    /* XXX for now, collect Dirnames/Filelinktos in Providename */
    if (tagN == RPMTAG_DIRNAMES || tagN == RPMTAG_FILELINKTOS)
	tagN = RPMTAG_PROVIDENAME;

assert(fn != NULL);
    fd = Fopen(fn, "r.fpio");
    if (fd == NULL || Ferror(fd))
	goto exit;
    fp = fdGetFILE(fd);

    ln = 0;
    if (fp != NULL)
    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	ln++;

	/* insure a terminator. */
	buf[sizeof(buf)-1] = '\0';

	/* ltrim on line. */
	while (*f && _isspace(*f))
	    f++;

	/* XXX skip YAML "- " markup */
	if (f[0] == '-' && _isspace(f[1])) {
	    f += sizeof("- ")-1;
	    while (*f && _isspace(*f))
		f++;
	}

	/* skip empty lines and comments */
	if (*f == '\0' || *f == '#')
	    continue;

	/* rtrim on line. */
	fe = f + strlen(f);
	while (--fe > f && _isspace(*fe))
	    *fe = '\0';

	if (!(xisalnum(f[0]) || f[0] == '_' || f[0] == '/' || f[0] == '%')) {
	    /* XXX N must begin with alphanumeric, _, / or %. */
	    fprintf(stderr,
		_("%s:%d \"%s\" must begin with alphanumeric, '_', '/' or '%%'.\n"),
		    fn, ln, f);
	    continue;
        }

	/* split on ' '  or comparison operator. */
	fe = f;
	while (*fe && !_isspace(*fe) && strchr("<=>", *fe) == NULL)
            fe++;
	while (*fe && _isspace(*fe))
	    *fe++ = '\0';

	N = f;
	EVR = NULL;
	Flags = 0;

	/* parse for non-path, versioned dependency. */
/*@-branchstate@*/
	if (*f != '/' && *fe != '\0') {
	    struct cmpop *cop;

	    /* parse comparison operator */
	    g = fe;
	    for (cop = cops; cop->operator != NULL; cop++) {
		if (strncmp(g, cop->operator, strlen(cop->operator)))
		    /*@innercontinue@*/ continue;
		*g = '\0';
		g += strlen(cop->operator);
		Flags = cop->sense;
		/*@innerbreak@*/ break;
	    }

	    if (Flags == 0) {
		/* XXX No comparison operator found. */
		fprintf(stderr, _("%s:%d No comparison operator found.\n"),
			fn, ln);
		continue;
	    }

	    /* ltrim on field 2. */
	    while (*g && _isspace(*g))
		g++;
	    if (*g == '\0') {
		/* XXX No EVR comparison value found. */
		fprintf(stderr, _("%s:%d No EVR comparison value found.\n"),
			fn, ln);
		continue;
	    }

	    ge = g + 1;
	    while (*ge && !_isspace(*ge))
		ge++;

	    if (*ge != '\0')
		*ge = '\0';	/* XXX can't happen, line rtrim'ed already. */

	    EVR = g;
	}

	if (EVR == NULL)
	    EVR = "";
	Flags |= RPMSENSE_PROBE;
/*@=branchstate@*/
	ds = rpmdsSingle(tagN, N, EVR , Flags);
	if (ds) {	/* XXX can't happen */
	    xx = rpmdsMergePRCO(PRCO, ds);
	    ds = rpmdsFree(ds);
	}
    }
    rc = 0;

exit:
/*@-branchstate@*/
    if (fd != NULL) (void) Fclose(fd);
/*@=branchstate@*/
    return rc;
}

#define	_ETC_RPM_SYSINFO	"/etc/rpm/sysinfo"
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char *_sysinfo_path = NULL;

/*@unchecked@*/ /*@observer@*/ /*@relnull@*/
static const char *_sysinfo_tags[] = {
    "Providename",
    "Requirename",
    "Conflictname",
    "Obsoletename",
    "Dirnames",
    "Filelinktos",
    NULL
};

int rpmdsSysinfo(rpmPRCO PRCO, const char * fn)
	/*@globals _sysinfo_path @*/
	/*@modifies _sysinfo_path @*/
{
    struct stat * st = memset(alloca(sizeof(*st)), 0, sizeof(*st));
    int rc = -1;
    int xx;

/*@-modobserver@*/
    if (_sysinfo_path == NULL) {
	_sysinfo_path = rpmExpand("%{?_rpmds_sysinfo_path}", NULL);
	/* XXX may need to validate path existence somewhen. */
	if (!(_sysinfo_path != NULL && *_sysinfo_path == '/')) {
/*@-observertrans @*/
	    _sysinfo_path = _free(_sysinfo_path);
/*@=observertrans @*/
	    _sysinfo_path = xstrdup(_ETC_RPM_SYSINFO);
	}
    }
/*@=modobserver@*/

/*@-branchstate@*/
    if (fn == NULL)
	fn = _sysinfo_path;
/*@=branchstate@*/

    if (fn == NULL)
	goto exit;

    xx = Stat(fn, st);
    if (xx < 0)
	goto exit;

    if (S_ISDIR(st->st_mode)) {
	const char *dn = fn;
	const char **av;
	int tagN;
	rc = 0;		/* assume success */
/*@-branchstate@*/
	for (av = _sysinfo_tags; av && *av; av++) {
	    tagN = tagValue(*av);
	    if (tagN < 0)
		continue;
	    fn = rpmGetPath(dn, "/", *av, NULL);
	    st = memset(st, 0, sizeof(*st));
	    xx = Stat(fn, st);
	    if (xx == 0 && S_ISREG(st->st_mode))
		rc = rpmdsSysinfoFile(PRCO, fn, tagN);
	    fn = _free(fn);
	    if (rc)
		break;
	}
/*@=branchstate@*/
    } else
    /* XXX for now, collect Dirnames/Filelinktos in Providename */
    if (S_ISREG(st->st_mode))
	rc = rpmdsSysinfoFile(PRCO, fn, RPMTAG_PROVIDENAME);

exit:
    return rc;
}

struct conf {
/*@observer@*/ /*@relnull@*/
    const char *name;
    const int call_name;
    const enum { SYSCONF, CONFSTR, PATHCONF } call;
};

/*@unchecked@*/ /*@observer@*/
static const struct conf vars[] = {
#ifdef _PC_LINK_MAX
    { "LINK_MAX", _PC_LINK_MAX, PATHCONF },
#endif
#ifdef _PC_LINK_MAX
    { "_POSIX_LINK_MAX", _PC_LINK_MAX, PATHCONF },
#endif
#ifdef _PC_MAX_CANON
    { "MAX_CANON", _PC_MAX_CANON, PATHCONF },
#endif
#ifdef _PC_MAX_CANON
    { "_POSIX_MAX_CANON", _PC_MAX_CANON, PATHCONF },
#endif
#ifdef _PC_MAX_INPUT
    { "MAX_INPUT", _PC_MAX_INPUT, PATHCONF },
#endif
#ifdef _PC_MAX_INPUT
    { "_POSIX_MAX_INPUT", _PC_MAX_INPUT, PATHCONF },
#endif
#ifdef _PC_NAME_MAX
    { "NAME_MAX", _PC_NAME_MAX, PATHCONF },
#endif
#ifdef _PC_NAME_MAX
    { "_POSIX_NAME_MAX", _PC_NAME_MAX, PATHCONF },
#endif
#ifdef _PC_PATH_MAX
    { "PATH_MAX", _PC_PATH_MAX, PATHCONF },
#endif
#ifdef _PC_PATH_MAX
    { "_POSIX_PATH_MAX", _PC_PATH_MAX, PATHCONF },
#endif
#ifdef _PC_PIPE_BUF
    { "PIPE_BUF", _PC_PIPE_BUF, PATHCONF },
#endif
#ifdef _PC_PIPE_BUF
    { "_POSIX_PIPE_BUF", _PC_PIPE_BUF, PATHCONF },
#endif
#ifdef _PC_SOCK_MAXBUF
    { "SOCK_MAXBUF", _PC_SOCK_MAXBUF, PATHCONF },
#endif
#ifdef _PC_ASYNC_IO
    { "_POSIX_ASYNC_IO", _PC_ASYNC_IO, PATHCONF },
#endif
#ifdef _PC_CHOWN_RESTRICTED
    { "_POSIX_CHOWN_RESTRICTED", _PC_CHOWN_RESTRICTED, PATHCONF },
#endif
#ifdef _PC_NO_TRUNC
    { "_POSIX_NO_TRUNC", _PC_NO_TRUNC, PATHCONF },
#endif
#ifdef _PC_PRIO_IO
    { "_POSIX_PRIO_IO", _PC_PRIO_IO, PATHCONF },
#endif
#ifdef _PC_SYNC_IO
    { "_POSIX_SYNC_IO", _PC_SYNC_IO, PATHCONF },
#endif
#ifdef _PC_VDISABLE
    { "_POSIX_VDISABLE", _PC_VDISABLE, PATHCONF },
#endif

#ifdef _SC_ARG_MAX
    { "ARG_MAX", _SC_ARG_MAX, SYSCONF },
#endif
#ifdef _SC_ATEXIT_MAX
    { "ATEXIT_MAX", _SC_ATEXIT_MAX, SYSCONF },
#endif
#ifdef _SC_CHAR_BIT
    { "CHAR_BIT", _SC_CHAR_BIT, SYSCONF },
#endif
#ifdef _SC_CHAR_MAX
    { "CHAR_MAX", _SC_CHAR_MAX, SYSCONF },
#endif
#ifdef _SC_CHAR_MIN
    { "CHAR_MIN", _SC_CHAR_MIN, SYSCONF },
#endif
#ifdef _SC_CHILD_MAX
    { "CHILD_MAX", _SC_CHILD_MAX, SYSCONF },
#endif
#ifdef _SC_CLK_TCK
    { "CLK_TCK", _SC_CLK_TCK, SYSCONF },
#endif
#ifdef _SC_INT_MAX
    { "INT_MAX", _SC_INT_MAX, SYSCONF },
#endif
#ifdef _SC_INT_MIN
    { "INT_MIN", _SC_INT_MIN, SYSCONF },
#endif
#ifdef _SC_UIO_MAXIOV
    { "IOV_MAX", _SC_UIO_MAXIOV, SYSCONF },
#endif
#ifdef _SC_LOGIN_NAME_MAX
    { "LOGNAME_MAX", _SC_LOGIN_NAME_MAX, SYSCONF },
#endif
#ifdef _SC_LONG_BIT
    { "LONG_BIT", _SC_LONG_BIT, SYSCONF },
#endif
#ifdef _SC_MB_LEN_MAX
    { "MB_LEN_MAX", _SC_MB_LEN_MAX, SYSCONF },
#endif
#ifdef _SC_NGROUPS_MAX
    { "NGROUPS_MAX", _SC_NGROUPS_MAX, SYSCONF },
#endif
#ifdef _SC_NL_ARGMAX
    { "NL_ARGMAX", _SC_NL_ARGMAX, SYSCONF },
#endif
#ifdef _SC_NL_LANGMAX
    { "NL_LANGMAX", _SC_NL_LANGMAX, SYSCONF },
#endif
#ifdef _SC_NL_MSGMAX
    { "NL_MSGMAX", _SC_NL_MSGMAX, SYSCONF },
#endif
#ifdef _SC_NL_NMAX
    { "NL_NMAX", _SC_NL_NMAX, SYSCONF },
#endif
#ifdef _SC_NL_SETMAX
    { "NL_SETMAX", _SC_NL_SETMAX, SYSCONF },
#endif
#ifdef _SC_NL_TEXTMAX
    { "NL_TEXTMAX", _SC_NL_TEXTMAX, SYSCONF },
#endif
#ifdef _SC_GETGR_R_SIZE_MAX
    { "NSS_BUFLEN_GROUP", _SC_GETGR_R_SIZE_MAX, SYSCONF },
#endif
#ifdef _SC_GETPW_R_SIZE_MAX
    { "NSS_BUFLEN_PASSWD", _SC_GETPW_R_SIZE_MAX, SYSCONF },
#endif
#ifdef _SC_NZERO
    { "NZERO", _SC_NZERO, SYSCONF },
#endif
#ifdef _SC_OPEN_MAX
    { "OPEN_MAX", _SC_OPEN_MAX, SYSCONF },
#endif
#ifdef _SC_PAGESIZE
    { "PAGESIZE", _SC_PAGESIZE, SYSCONF },
#endif
#ifdef _SC_PAGESIZE
    { "PAGE_SIZE", _SC_PAGESIZE, SYSCONF },
#endif
#ifdef _SC_PASS_MAX
    { "PASS_MAX", _SC_PASS_MAX, SYSCONF },
#endif
#ifdef _SC_THREAD_DESTRUCTOR_ITERATIONS
    { "PTHREAD_DESTRUCTOR_ITERATIONS", _SC_THREAD_DESTRUCTOR_ITERATIONS, SYSCONF },
#endif
#ifdef _SC_THREAD_KEYS_MAX
    { "PTHREAD_KEYS_MAX", _SC_THREAD_KEYS_MAX, SYSCONF },
#endif
#ifdef _SC_THREAD_STACK_MIN
    { "PTHREAD_STACK_MIN", _SC_THREAD_STACK_MIN, SYSCONF },
#endif
#ifdef _SC_THREAD_THREADS_MAX
    { "PTHREAD_THREADS_MAX", _SC_THREAD_THREADS_MAX, SYSCONF },
#endif
#ifdef _SC_SCHAR_MAX
    { "SCHAR_MAX", _SC_SCHAR_MAX, SYSCONF },
#endif
#ifdef _SC_SCHAR_MIN
    { "SCHAR_MIN", _SC_SCHAR_MIN, SYSCONF },
#endif
#ifdef _SC_SHRT_MAX
    { "SHRT_MAX", _SC_SHRT_MAX, SYSCONF },
#endif
#ifdef _SC_SHRT_MIN
    { "SHRT_MIN", _SC_SHRT_MIN, SYSCONF },
#endif
#ifdef _SC_SSIZE_MAX
    { "SSIZE_MAX", _SC_SSIZE_MAX, SYSCONF },
#endif
#ifdef _SC_TTY_NAME_MAX
    { "TTY_NAME_MAX", _SC_TTY_NAME_MAX, SYSCONF },
#endif
#ifdef _SC_TZNAME_MAX
    { "TZNAME_MAX", _SC_TZNAME_MAX, SYSCONF },
#endif
#ifdef _SC_UCHAR_MAX
    { "UCHAR_MAX", _SC_UCHAR_MAX, SYSCONF },
#endif
#ifdef _SC_UINT_MAX
    { "UINT_MAX", _SC_UINT_MAX, SYSCONF },
#endif
#ifdef _SC_UIO_MAXIOV
    { "UIO_MAXIOV", _SC_UIO_MAXIOV, SYSCONF },
#endif
#ifdef _SC_ULONG_MAX
    { "ULONG_MAX", _SC_ULONG_MAX, SYSCONF },
#endif
#ifdef _SC_USHRT_MAX
    { "USHRT_MAX", _SC_USHRT_MAX, SYSCONF },
#endif
#ifdef _SC_WORD_BIT
    { "WORD_BIT", _SC_WORD_BIT, SYSCONF },
#endif
#ifdef _SC_AVPHYS_PAGES
    { "_AVPHYS_PAGES", _SC_AVPHYS_PAGES, SYSCONF },
#endif
#ifdef _SC_NPROCESSORS_CONF
    { "_NPROCESSORS_CONF", _SC_NPROCESSORS_CONF, SYSCONF },
#endif
#ifdef _SC_NPROCESSORS_ONLN
    { "_NPROCESSORS_ONLN", _SC_NPROCESSORS_ONLN, SYSCONF },
#endif
#ifdef _SC_PHYS_PAGES
    { "_PHYS_PAGES", _SC_PHYS_PAGES, SYSCONF },
#endif
#ifdef _SC_ARG_MAX
    { "_POSIX_ARG_MAX", _SC_ARG_MAX, SYSCONF },
#endif
#ifdef _SC_ASYNCHRONOUS_IO
    { "_POSIX_ASYNCHRONOUS_IO", _SC_ASYNCHRONOUS_IO, SYSCONF },
#endif
#ifdef _SC_CHILD_MAX
    { "_POSIX_CHILD_MAX", _SC_CHILD_MAX, SYSCONF },
#endif
#ifdef _SC_FSYNC
    { "_POSIX_FSYNC", _SC_FSYNC, SYSCONF },
#endif
#ifdef _SC_JOB_CONTROL
    { "_POSIX_JOB_CONTROL", _SC_JOB_CONTROL, SYSCONF },
#endif
#ifdef _SC_MAPPED_FILES
    { "_POSIX_MAPPED_FILES", _SC_MAPPED_FILES, SYSCONF },
#endif
#ifdef _SC_MEMLOCK
    { "_POSIX_MEMLOCK", _SC_MEMLOCK, SYSCONF },
#endif
#ifdef _SC_MEMLOCK_RANGE
    { "_POSIX_MEMLOCK_RANGE", _SC_MEMLOCK_RANGE, SYSCONF },
#endif
#ifdef _SC_MEMORY_PROTECTION
    { "_POSIX_MEMORY_PROTECTION", _SC_MEMORY_PROTECTION, SYSCONF },
#endif
#ifdef _SC_MESSAGE_PASSING
    { "_POSIX_MESSAGE_PASSING", _SC_MESSAGE_PASSING, SYSCONF },
#endif
#ifdef _SC_NGROUPS_MAX
    { "_POSIX_NGROUPS_MAX", _SC_NGROUPS_MAX, SYSCONF },
#endif
#ifdef _SC_OPEN_MAX
    { "_POSIX_OPEN_MAX", _SC_OPEN_MAX, SYSCONF },
#endif
#ifdef _SC_PII
    { "_POSIX_PII", _SC_PII, SYSCONF },
#endif
#ifdef _SC_PII_INTERNET
    { "_POSIX_PII_INTERNET", _SC_PII_INTERNET, SYSCONF },
#endif
#ifdef _SC_PII_INTERNET_DGRAM
    { "_POSIX_PII_INTERNET_DGRAM", _SC_PII_INTERNET_DGRAM, SYSCONF },
#endif
#ifdef _SC_PII_INTERNET_STREAM
    { "_POSIX_PII_INTERNET_STREAM", _SC_PII_INTERNET_STREAM, SYSCONF },
#endif
#ifdef _SC_PII_OSI
    { "_POSIX_PII_OSI", _SC_PII_OSI, SYSCONF },
#endif
#ifdef _SC_PII_OSI_CLTS
    { "_POSIX_PII_OSI_CLTS", _SC_PII_OSI_CLTS, SYSCONF },
#endif
#ifdef _SC_PII_OSI_COTS
    { "_POSIX_PII_OSI_COTS", _SC_PII_OSI_COTS, SYSCONF },
#endif
#ifdef _SC_PII_OSI_M
    { "_POSIX_PII_OSI_M", _SC_PII_OSI_M, SYSCONF },
#endif
#ifdef _SC_PII_SOCKET
    { "_POSIX_PII_SOCKET", _SC_PII_SOCKET, SYSCONF },
#endif
#ifdef _SC_PII_XTI
    { "_POSIX_PII_XTI", _SC_PII_XTI, SYSCONF },
#endif
#ifdef _SC_POLL
    { "_POSIX_POLL", _SC_POLL, SYSCONF },
#endif
#ifdef _SC_PRIORITIZED_IO
    { "_POSIX_PRIORITIZED_IO", _SC_PRIORITIZED_IO, SYSCONF },
#endif
#ifdef _SC_PRIORITY_SCHEDULING
    { "_POSIX_PRIORITY_SCHEDULING", _SC_PRIORITY_SCHEDULING, SYSCONF },
#endif
#ifdef _SC_REALTIME_SIGNALS
    { "_POSIX_REALTIME_SIGNALS", _SC_REALTIME_SIGNALS, SYSCONF },
#endif
#ifdef _SC_SAVED_IDS
    { "_POSIX_SAVED_IDS", _SC_SAVED_IDS, SYSCONF },
#endif
#ifdef _SC_SELECT
    { "_POSIX_SELECT", _SC_SELECT, SYSCONF },
#endif
#ifdef _SC_SEMAPHORES
    { "_POSIX_SEMAPHORES", _SC_SEMAPHORES, SYSCONF },
#endif
#ifdef _SC_SHARED_MEMORY_OBJECTS
    { "_POSIX_SHARED_MEMORY_OBJECTS", _SC_SHARED_MEMORY_OBJECTS, SYSCONF },
#endif
#ifdef _SC_SSIZE_MAX
    { "_POSIX_SSIZE_MAX", _SC_SSIZE_MAX, SYSCONF },
#endif
#ifdef _SC_STREAM_MAX
    { "_POSIX_STREAM_MAX", _SC_STREAM_MAX, SYSCONF },
#endif
#ifdef _SC_SYNCHRONIZED_IO
    { "_POSIX_SYNCHRONIZED_IO", _SC_SYNCHRONIZED_IO, SYSCONF },
#endif
#ifdef _SC_THREADS
    { "_POSIX_THREADS", _SC_THREADS, SYSCONF },
#endif
#ifdef _SC_THREAD_ATTR_STACKADDR
    { "_POSIX_THREAD_ATTR_STACKADDR", _SC_THREAD_ATTR_STACKADDR, SYSCONF },
#endif
#ifdef _SC_THREAD_ATTR_STACKSIZE
    { "_POSIX_THREAD_ATTR_STACKSIZE", _SC_THREAD_ATTR_STACKSIZE, SYSCONF },
#endif
#ifdef _SC_THREAD_PRIORITY_SCHEDULING
    { "_POSIX_THREAD_PRIORITY_SCHEDULING", _SC_THREAD_PRIORITY_SCHEDULING, SYSCONF },
#endif
#ifdef _SC_THREAD_PRIO_INHERIT
    { "_POSIX_THREAD_PRIO_INHERIT", _SC_THREAD_PRIO_INHERIT, SYSCONF },
#endif
#ifdef _SC_THREAD_PRIO_PROTECT
    { "_POSIX_THREAD_PRIO_PROTECT", _SC_THREAD_PRIO_PROTECT, SYSCONF },
#endif
#ifdef _SC_THREAD_PROCESS_SHARED
    { "_POSIX_THREAD_PROCESS_SHARED", _SC_THREAD_PROCESS_SHARED, SYSCONF },
#endif
#ifdef _SC_THREAD_SAFE_FUNCTIONS
    { "_POSIX_THREAD_SAFE_FUNCTIONS", _SC_THREAD_SAFE_FUNCTIONS, SYSCONF },
#endif
#ifdef _SC_TIMERS
    { "_POSIX_TIMERS", _SC_TIMERS, SYSCONF },
#endif
#ifdef _SC_TIMER_MAX
    { "TIMER_MAX", _SC_TIMER_MAX, SYSCONF },
#endif
#ifdef _SC_TZNAME_MAX
    { "_POSIX_TZNAME_MAX", _SC_TZNAME_MAX, SYSCONF },
#endif
#ifdef _SC_VERSION
    { "_POSIX_VERSION", _SC_VERSION, SYSCONF },
#endif
#ifdef _SC_T_IOV_MAX
    { "_T_IOV_MAX", _SC_T_IOV_MAX, SYSCONF },
#endif
#ifdef _SC_XOPEN_CRYPT
    { "_XOPEN_CRYPT", _SC_XOPEN_CRYPT, SYSCONF },
#endif
#ifdef _SC_XOPEN_ENH_I18N
    { "_XOPEN_ENH_I18N", _SC_XOPEN_ENH_I18N, SYSCONF },
#endif
#ifdef _SC_XOPEN_LEGACY
    { "_XOPEN_LEGACY", _SC_XOPEN_LEGACY, SYSCONF },
#endif
#ifdef _SC_XOPEN_REALTIME
    { "_XOPEN_REALTIME", _SC_XOPEN_REALTIME, SYSCONF },
#endif
#ifdef _SC_XOPEN_REALTIME_THREADS
    { "_XOPEN_REALTIME_THREADS", _SC_XOPEN_REALTIME_THREADS, SYSCONF },
#endif
#ifdef _SC_XOPEN_SHM
    { "_XOPEN_SHM", _SC_XOPEN_SHM, SYSCONF },
#endif
#ifdef _SC_XOPEN_UNIX
    { "_XOPEN_UNIX", _SC_XOPEN_UNIX, SYSCONF },
#endif
#ifdef _SC_XOPEN_VERSION
    { "_XOPEN_VERSION", _SC_XOPEN_VERSION, SYSCONF },
#endif
#ifdef _SC_XOPEN_XCU_VERSION
    { "_XOPEN_XCU_VERSION", _SC_XOPEN_XCU_VERSION, SYSCONF },
#endif
#ifdef _SC_XOPEN_XPG2
    { "_XOPEN_XPG2", _SC_XOPEN_XPG2, SYSCONF },
#endif
#ifdef _SC_XOPEN_XPG3
    { "_XOPEN_XPG3", _SC_XOPEN_XPG3, SYSCONF },
#endif
#ifdef _SC_XOPEN_XPG4
    { "_XOPEN_XPG4", _SC_XOPEN_XPG4, SYSCONF },
#endif
    /* POSIX.2  */
#ifdef _SC_BC_BASE_MAX
    { "BC_BASE_MAX", _SC_BC_BASE_MAX, SYSCONF },
#endif
#ifdef _SC_BC_DIM_MAX
    { "BC_DIM_MAX", _SC_BC_DIM_MAX, SYSCONF },
#endif
#ifdef _SC_BC_SCALE_MAX
    { "BC_SCALE_MAX", _SC_BC_SCALE_MAX, SYSCONF },
#endif
#ifdef _SC_BC_STRING_MAX
    { "BC_STRING_MAX", _SC_BC_STRING_MAX, SYSCONF },
#endif
#ifdef _SC_CHARCLASS_NAME_MAX
    { "CHARCLASS_NAME_MAX", _SC_CHARCLASS_NAME_MAX, SYSCONF },
#endif
#ifdef _SC_COLL_WEIGHTS_MAX
    { "COLL_WEIGHTS_MAX", _SC_COLL_WEIGHTS_MAX, SYSCONF },
#endif
#ifdef _SC_EQUIV_CLASS_MAX
    { "EQUIV_CLASS_MAX", _SC_EQUIV_CLASS_MAX, SYSCONF },
#endif
#ifdef _SC_EXPR_NEST_MAX
    { "EXPR_NEST_MAX", _SC_EXPR_NEST_MAX, SYSCONF },
#endif
#ifdef _SC_LINE_MAX
    { "LINE_MAX", _SC_LINE_MAX, SYSCONF },
#endif
#ifdef _SC_BC_BASE_MAX
    { "POSIX2_BC_BASE_MAX", _SC_BC_BASE_MAX, SYSCONF },
#endif
#ifdef _SC_BC_DIM_MAX
    { "POSIX2_BC_DIM_MAX", _SC_BC_DIM_MAX, SYSCONF },
#endif
#ifdef _SC_BC_SCALE_MAX
    { "POSIX2_BC_SCALE_MAX", _SC_BC_SCALE_MAX, SYSCONF },
#endif
#ifdef _SC_BC_STRING_MAX
    { "POSIX2_BC_STRING_MAX", _SC_BC_STRING_MAX, SYSCONF },
#endif
#ifdef _SC_2_CHAR_TERM
    { "POSIX2_CHAR_TERM", _SC_2_CHAR_TERM, SYSCONF },
#endif
#ifdef _SC_COLL_WEIGHTS_MAX
    { "POSIX2_COLL_WEIGHTS_MAX", _SC_COLL_WEIGHTS_MAX, SYSCONF },
#endif
#ifdef _SC_2_C_BIND
    { "POSIX2_C_BIND", _SC_2_C_BIND, SYSCONF },
#endif
#ifdef _SC_2_C_DEV
    { "POSIX2_C_DEV", _SC_2_C_DEV, SYSCONF },
#endif
#ifdef _SC_2_C_VERSION
    { "POSIX2_C_VERSION", _SC_2_C_VERSION, SYSCONF },
#endif
#ifdef _SC_EXPR_NEST_MAX
    { "POSIX2_EXPR_NEST_MAX", _SC_EXPR_NEST_MAX, SYSCONF },
#endif
#ifdef _SC_2_FORT_DEV
    { "POSIX2_FORT_DEV", _SC_2_FORT_DEV, SYSCONF },
#endif
#ifdef _SC_2_FORT_RUN
    { "POSIX2_FORT_RUN", _SC_2_FORT_RUN, SYSCONF },
#endif
#ifdef _SC_LINE_MAX
    { "_POSIX2_LINE_MAX", _SC_LINE_MAX, SYSCONF },
#endif
#ifdef _SC_2_LOCALEDEF
    { "POSIX2_LOCALEDEF", _SC_2_LOCALEDEF, SYSCONF },
#endif
#ifdef _SC_RE_DUP_MAX
    { "POSIX2_RE_DUP_MAX", _SC_RE_DUP_MAX, SYSCONF },
#endif
#ifdef _SC_2_SW_DEV
    { "POSIX2_SW_DEV", _SC_2_SW_DEV, SYSCONF },
#endif
#ifdef _SC_2_UPE
    { "POSIX2_UPE", _SC_2_UPE, SYSCONF },
#endif
#ifdef _SC_2_VERSION
    { "POSIX2_VERSION", _SC_2_VERSION, SYSCONF },
#endif
#ifdef _SC_RE_DUP_MAX
    { "RE_DUP_MAX", _SC_RE_DUP_MAX, SYSCONF },
#endif

#ifdef _CS_PATH
    { "PATH", _CS_PATH, CONFSTR },
    { "CS_PATH", _CS_PATH, CONFSTR },
#endif

    /* LFS */
#ifdef _CS_LFS_CFLAGS
    { "LFS_CFLAGS", _CS_LFS_CFLAGS, CONFSTR },
#endif
#ifdef _CS_LFS_LDFLAGS
    { "LFS_LDFLAGS", _CS_LFS_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_LFS_LIBS
    { "LFS_LIBS", _CS_LFS_LIBS, CONFSTR },
#endif
#ifdef _CS_LFS_LINTFLAGS
    { "LFS_LINTFLAGS", _CS_LFS_LINTFLAGS, CONFSTR },
#endif
#ifdef _CS_LFS64_CFLAGS
    { "LFS64_CFLAGS", _CS_LFS64_CFLAGS, CONFSTR },
#endif
#ifdef _CS_LFS64_LDFLAGS
    { "LFS64_LDFLAGS", _CS_LFS64_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_LFS64_LIBS
    { "LFS64_LIBS", _CS_LFS64_LIBS, CONFSTR },
#endif
#ifdef _CS_LFS64_LINTFLAGS
    { "LFS64_LINTFLAGS", _CS_LFS64_LINTFLAGS, CONFSTR },
#endif

    /* Programming environments.  */
#ifdef _SC_XBS5_ILP32_OFF32
    { "_XBS5_ILP32_OFF32", _SC_XBS5_ILP32_OFF32, SYSCONF },
#endif
#ifdef _CS_XBS5_ILP32_OFF32_CFLAGS
    { "XBS5_ILP32_OFF32_CFLAGS", _CS_XBS5_ILP32_OFF32_CFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_ILP32_OFF32_LDFLAGS
    { "XBS5_ILP32_OFF32_LDFLAGS", _CS_XBS5_ILP32_OFF32_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_ILP32_OFF32_LIBS
    { "XBS5_ILP32_OFF32_LIBS", _CS_XBS5_ILP32_OFF32_LIBS, CONFSTR },
#endif
#ifdef _CS_XBS5_ILP32_OFF32_LINTFLAGS
    { "XBS5_ILP32_OFF32_LINTFLAGS", _CS_XBS5_ILP32_OFF32_LINTFLAGS, CONFSTR },
#endif

#ifdef _SC_XBS5_ILP32_OFFBIG
    { "_XBS5_ILP32_OFFBIG", _SC_XBS5_ILP32_OFFBIG, SYSCONF },
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_CFLAGS
    { "XBS5_ILP32_OFFBIG_CFLAGS", _CS_XBS5_ILP32_OFFBIG_CFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_LDFLAGS
    { "XBS5_ILP32_OFFBIG_LDFLAGS", _CS_XBS5_ILP32_OFFBIG_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_LIBS
    { "XBS5_ILP32_OFFBIG_LIBS", _CS_XBS5_ILP32_OFFBIG_LIBS, CONFSTR },
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_LINTFLAGS
    { "XBS5_ILP32_OFFBIG_LINTFLAGS", _CS_XBS5_ILP32_OFFBIG_LINTFLAGS, CONFSTR },
#endif

#ifdef _SC_XBS5_LP64_OFF64
    { "_XBS5_LP64_OFF64", _SC_XBS5_LP64_OFF64, SYSCONF },
#endif
#ifdef _CS_XBS5_LP64_OFF64_CFLAGS
    { "XBS5_LP64_OFF64_CFLAGS", _CS_XBS5_LP64_OFF64_CFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_LP64_OFF64_LDFLAGS
    { "XBS5_LP64_OFF64_LDFLAGS", _CS_XBS5_LP64_OFF64_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_LP64_OFF64_LIBS
    { "XBS5_LP64_OFF64_LIBS", _CS_XBS5_LP64_OFF64_LIBS, CONFSTR },
#endif
#ifdef _CS_XBS5_LP64_OFF64_LINTFLAGS
    { "XBS5_LP64_OFF64_LINTFLAGS", _CS_XBS5_LP64_OFF64_LINTFLAGS, CONFSTR },
#endif

#ifdef _SC_XBS5_LPBIG_OFFBIG
    { "_XBS5_LPBIG_OFFBIG", _SC_XBS5_LPBIG_OFFBIG, SYSCONF },
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_CFLAGS
    { "XBS5_LPBIG_OFFBIG_CFLAGS", _CS_XBS5_LPBIG_OFFBIG_CFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_LDFLAGS
    { "XBS5_LPBIG_OFFBIG_LDFLAGS", _CS_XBS5_LPBIG_OFFBIG_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_LIBS
    { "XBS5_LPBIG_OFFBIG_LIBS", _CS_XBS5_LPBIG_OFFBIG_LIBS, CONFSTR },
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS
    { "XBS5_LPBIG_OFFBIG_LINTFLAGS", _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS, CONFSTR },
#endif

#ifdef _SC_V6_ILP32_OFF32
    { "_POSIX_V6_ILP32_OFF32", _SC_V6_ILP32_OFF32, SYSCONF },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFF32_CFLAGS
    { "POSIX_V6_ILP32_OFF32_CFLAGS", _CS_POSIX_V6_ILP32_OFF32_CFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFF32_LDFLAGS
    { "POSIX_V6_ILP32_OFF32_LDFLAGS", _CS_POSIX_V6_ILP32_OFF32_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFF32_LIBS
    { "POSIX_V6_ILP32_OFF32_LIBS", _CS_POSIX_V6_ILP32_OFF32_LIBS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFF32_LINTFLAGS
    { "POSIX_V6_ILP32_OFF32_LINTFLAGS", _CS_POSIX_V6_ILP32_OFF32_LINTFLAGS, CONFSTR },
#endif

#ifdef _CS_V6_WIDTH_RESTRICTED_ENVS
    { "_POSIX_V6_WIDTH_RESTRICTED_ENVS", _CS_V6_WIDTH_RESTRICTED_ENVS, CONFSTR },
#endif

#ifdef _SC_V6_ILP32_OFFBIG
    { "_POSIX_V6_ILP32_OFFBIG", _SC_V6_ILP32_OFFBIG, SYSCONF },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS
    { "POSIX_V6_ILP32_OFFBIG_CFLAGS", _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS
    { "POSIX_V6_ILP32_OFFBIG_LDFLAGS", _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFFBIG_LIBS
    { "POSIX_V6_ILP32_OFFBIG_LIBS", _CS_POSIX_V6_ILP32_OFFBIG_LIBS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_ILP32_OFFBIG_LINTFLAGS
    { "POSIX_V6_ILP32_OFFBIG_LINTFLAGS", _CS_POSIX_V6_ILP32_OFFBIG_LINTFLAGS, CONFSTR },
#endif

#ifdef _SC_V6_LP64_OFF64
    { "_POSIX_V6_LP64_OFF64", _SC_V6_LP64_OFF64, SYSCONF },
#endif
#ifdef _CS_POSIX_V6_LP64_OFF64_CFLAGS
    { "POSIX_V6_LP64_OFF64_CFLAGS", _CS_POSIX_V6_LP64_OFF64_CFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_LP64_OFF64_LDFLAGS
    { "POSIX_V6_LP64_OFF64_LDFLAGS", _CS_POSIX_V6_LP64_OFF64_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_LP64_OFF64_LIBS
    { "POSIX_V6_LP64_OFF64_LIBS", _CS_POSIX_V6_LP64_OFF64_LIBS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_LP64_OFF64_LINTFLAGS
    { "POSIX_V6_LP64_OFF64_LINTFLAGS", _CS_POSIX_V6_LP64_OFF64_LINTFLAGS, CONFSTR },
#endif

#ifdef _SC_V6_LPBIG_OFFBIG
    { "_POSIX_V6_LPBIG_OFFBIG", _SC_V6_LPBIG_OFFBIG, SYSCONF },
#endif
#ifdef _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS
    { "POSIX_V6_LPBIG_OFFBIG_CFLAGS", _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS
    { "POSIX_V6_LPBIG_OFFBIG_LDFLAGS", _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_LPBIG_OFFBIG_LIBS
    { "POSIX_V6_LPBIG_OFFBIG_LIBS", _CS_POSIX_V6_LPBIG_OFFBIG_LIBS, CONFSTR },
#endif
#ifdef _CS_POSIX_V6_LPBIG_OFFBIG_LINTFLAGS
    { "POSIX_V6_LPBIG_OFFBIG_LINTFLAGS", _CS_POSIX_V6_LPBIG_OFFBIG_LINTFLAGS, CONFSTR },
#endif

#ifdef _SC_ADVISORY_INFO
    { "_POSIX_ADVISORY_INFO", _SC_ADVISORY_INFO, SYSCONF },
#endif
#ifdef _SC_BARRIERS
    { "_POSIX_BARRIERS", _SC_BARRIERS, SYSCONF },
#endif
#ifdef _SC_BASE
    { "_POSIX_BASE", _SC_BASE, SYSCONF },
#endif
#ifdef _SC_C_LANG_SUPPORT
    { "_POSIX_C_LANG_SUPPORT", _SC_C_LANG_SUPPORT, SYSCONF },
#endif
#ifdef _SC_C_LANG_SUPPORT_R
    { "_POSIX_C_LANG_SUPPORT_R", _SC_C_LANG_SUPPORT_R, SYSCONF },
#endif
#ifdef _SC_CLOCK_SELECTION
    { "_POSIX_CLOCK_SELECTION", _SC_CLOCK_SELECTION, SYSCONF },
#endif
#ifdef _SC_CPUTIME
    { "_POSIX_CPUTIME", _SC_CPUTIME, SYSCONF },
#endif
#ifdef _SC_THREAD_CPUTIME
    { "_POSIX_THREAD_CPUTIME", _SC_THREAD_CPUTIME, SYSCONF },
#endif
#ifdef _SC_DEVICE_SPECIFIC
    { "_POSIX_DEVICE_SPECIFIC", _SC_DEVICE_SPECIFIC, SYSCONF },
#endif
#ifdef _SC_DEVICE_SPECIFIC_R
    { "_POSIX_DEVICE_SPECIFIC_R", _SC_DEVICE_SPECIFIC_R, SYSCONF },
#endif
#ifdef _SC_FD_MGMT
    { "_POSIX_FD_MGMT", _SC_FD_MGMT, SYSCONF },
#endif
#ifdef _SC_FIFO
    { "_POSIX_FIFO", _SC_FIFO, SYSCONF },
#endif
#ifdef _SC_PIPE
    { "_POSIX_PIPE", _SC_PIPE, SYSCONF },
#endif
#ifdef _SC_FILE_ATTRIBUTES
    { "_POSIX_FILE_ATTRIBUTES", _SC_FILE_ATTRIBUTES, SYSCONF },
#endif
#ifdef _SC_FILE_LOCKING
    { "_POSIX_FILE_LOCKING", _SC_FILE_LOCKING, SYSCONF },
#endif
#ifdef _SC_FILE_SYSTEM
    { "_POSIX_FILE_SYSTEM", _SC_FILE_SYSTEM, SYSCONF },
#endif
#ifdef _SC_MONOTONIC_CLOCK
    { "_POSIX_MONOTONIC_CLOCK", _SC_MONOTONIC_CLOCK, SYSCONF },
#endif
#ifdef _SC_MULTI_PROCESS
    { "_POSIX_MULTI_PROCESS", _SC_MULTI_PROCESS, SYSCONF },
#endif
#ifdef _SC_SINGLE_PROCESS
    { "_POSIX_SINGLE_PROCESS", _SC_SINGLE_PROCESS, SYSCONF },
#endif
#ifdef _SC_NETWORKING
    { "_POSIX_NETWORKING", _SC_NETWORKING, SYSCONF },
#endif
#ifdef _SC_READER_WRITER_LOCKS
    { "_POSIX_READER_WRITER_LOCKS", _SC_READER_WRITER_LOCKS, SYSCONF },
#endif
#ifdef _SC_SPIN_LOCKS
    { "_POSIX_SPIN_LOCKS", _SC_SPIN_LOCKS, SYSCONF },
#endif
#ifdef _SC_REGEXP
    { "_POSIX_REGEXP", _SC_REGEXP, SYSCONF },
#endif
#ifdef _SC_REGEX_VERSION
    { "_REGEX_VERSION", _SC_REGEX_VERSION, SYSCONF },
#endif
#ifdef _SC_SHELL
    { "_POSIX_SHELL", _SC_SHELL, SYSCONF },
#endif
#ifdef _SC_SIGNALS
    { "_POSIX_SIGNALS", _SC_SIGNALS, SYSCONF },
#endif
#ifdef _SC_SPAWN
    { "_POSIX_SPAWN", _SC_SPAWN, SYSCONF },
#endif
#ifdef _SC_SPORADIC_SERVER
    { "_POSIX_SPORADIC_SERVER", _SC_SPORADIC_SERVER, SYSCONF },
#endif
#ifdef _SC_THREAD_SPORADIC_SERVER
    { "_POSIX_THREAD_SPORADIC_SERVER", _SC_THREAD_SPORADIC_SERVER, SYSCONF },
#endif
#ifdef _SC_SYSTEM_DATABASE
    { "_POSIX_SYSTEM_DATABASE", _SC_SYSTEM_DATABASE, SYSCONF },
#endif
#ifdef _SC_SYSTEM_DATABASE_R
    { "_POSIX_SYSTEM_DATABASE_R", _SC_SYSTEM_DATABASE_R, SYSCONF },
#endif
#ifdef _SC_TIMEOUTS
    { "_POSIX_TIMEOUTS", _SC_TIMEOUTS, SYSCONF },
#endif
#ifdef _SC_TYPED_MEMORY_OBJECTS
    { "_POSIX_TYPED_MEMORY_OBJECTS", _SC_TYPED_MEMORY_OBJECTS, SYSCONF },
#endif
#ifdef _SC_USER_GROUPS
    { "_POSIX_USER_GROUPS", _SC_USER_GROUPS, SYSCONF },
#endif
#ifdef _SC_USER_GROUPS_R
    { "_POSIX_USER_GROUPS_R", _SC_USER_GROUPS_R, SYSCONF },
#endif
#ifdef _SC_2_PBS
    { "POSIX2_PBS", _SC_2_PBS, SYSCONF },
#endif
#ifdef _SC_2_PBS_ACCOUNTING
    { "POSIX2_PBS_ACCOUNTING", _SC_2_PBS_ACCOUNTING, SYSCONF },
#endif
#ifdef _SC_2_PBS_LOCATE
    { "POSIX2_PBS_LOCATE", _SC_2_PBS_LOCATE, SYSCONF },
#endif
#ifdef _SC_2_PBS_TRACK
    { "POSIX2_PBS_TRACK", _SC_2_PBS_TRACK, SYSCONF },
#endif
#ifdef _SC_2_PBS_MESSAGE
    { "POSIX2_PBS_MESSAGE", _SC_2_PBS_MESSAGE, SYSCONF },
#endif
#ifdef _SC_SYMLOOP_MAX
    { "SYMLOOP_MAX", _SC_SYMLOOP_MAX, SYSCONF },
#endif
#ifdef _SC_STREAM_MAX
    { "STREAM_MAX", _SC_STREAM_MAX, SYSCONF },
#endif
#ifdef _SC_AIO_LISTIO_MAX
    { "AIO_LISTIO_MAX", _SC_AIO_LISTIO_MAX, SYSCONF },
#endif
#ifdef _SC_AIO_MAX
    { "AIO_MAX", _SC_AIO_MAX, SYSCONF },
#endif
#ifdef _SC_AIO_PRIO_DELTA_MAX
    { "AIO_PRIO_DELTA_MAX", _SC_AIO_PRIO_DELTA_MAX, SYSCONF },
#endif
#ifdef _SC_DELAYTIMER_MAX
    { "DELAYTIMER_MAX", _SC_DELAYTIMER_MAX, SYSCONF },
#endif
#ifdef _SC_HOST_NAME_MAX
    { "HOST_NAME_MAX", _SC_HOST_NAME_MAX, SYSCONF },
#endif
#ifdef _SC_LOGIN_NAME_MAX
    { "LOGIN_NAME_MAX", _SC_LOGIN_NAME_MAX, SYSCONF },
#endif
#ifdef _SC_MQ_OPEN_MAX
    { "MQ_OPEN_MAX", _SC_MQ_OPEN_MAX, SYSCONF },
#endif
#ifdef _SC_MQ_PRIO_MAX
    { "MQ_PRIO_MAX", _SC_MQ_PRIO_MAX, SYSCONF },
#endif
#ifdef _SC_DEVICE_IO
    { "_POSIX_DEVICE_IO", _SC_DEVICE_IO, SYSCONF },
#endif
#ifdef _SC_TRACE
    { "_POSIX_TRACE", _SC_TRACE, SYSCONF },
#endif
#ifdef _SC_TRACE_EVENT_FILTER
    { "_POSIX_TRACE_EVENT_FILTER", _SC_TRACE_EVENT_FILTER, SYSCONF },
#endif
#ifdef _SC_TRACE_INHERIT
    { "_POSIX_TRACE_INHERIT", _SC_TRACE_INHERIT, SYSCONF },
#endif
#ifdef _SC_TRACE_LOG
    { "_POSIX_TRACE_LOG", _SC_TRACE_LOG, SYSCONF },
#endif
#ifdef _SC_RTSIG_MAX
    { "RTSIG_MAX", _SC_RTSIG_MAX, SYSCONF },
#endif
#ifdef _SC_SEM_NSEMS_MAX
    { "SEM_NSEMS_MAX", _SC_SEM_NSEMS_MAX, SYSCONF },
#endif
#ifdef _SC_SEM_VALUE_MAX
    { "SEM_VALUE_MAX", _SC_SEM_VALUE_MAX, SYSCONF },
#endif
#ifdef _SC_SIGQUEUE_MAX
    { "SIGQUEUE_MAX", _SC_SIGQUEUE_MAX, SYSCONF },
#endif
#ifdef _PC_FILESIZEBITS
    { "FILESIZEBITS", _PC_FILESIZEBITS, PATHCONF },
#endif
#ifdef _PC_ALLOC_SIZE_MIN
    { "POSIX_ALLOC_SIZE_MIN", _PC_ALLOC_SIZE_MIN, PATHCONF },
#endif
#ifdef _PC_REC_INCR_XFER_SIZE
    { "POSIX_REC_INCR_XFER_SIZE", _PC_REC_INCR_XFER_SIZE, PATHCONF },
#endif
#ifdef _PC_REC_MAX_XFER_SIZE
    { "POSIX_REC_MAX_XFER_SIZE", _PC_REC_MAX_XFER_SIZE, PATHCONF },
#endif
#ifdef _PC_REC_MIN_XFER_SIZE
    { "POSIX_REC_MIN_XFER_SIZE", _PC_REC_MIN_XFER_SIZE, PATHCONF },
#endif
#ifdef _PC_REC_XFER_ALIGN
    { "POSIX_REC_XFER_ALIGN", _PC_REC_XFER_ALIGN, PATHCONF },
#endif
#ifdef _PC_SYMLINK_MAX
    { "SYMLINK_MAX", _PC_SYMLINK_MAX, PATHCONF },
#endif
#ifdef _CS_GNU_LIBC_VERSION
    { "GNU_LIBC_VERSION", _CS_GNU_LIBC_VERSION, CONFSTR },
#endif
#ifdef _CS_GNU_LIBPTHREAD_VERSION
    { "GNU_LIBPTHREAD_VERSION", _CS_GNU_LIBPTHREAD_VERSION, CONFSTR },
#endif
#ifdef _PC_2_SYMLINKS
    { "POSIX2_SYMLINKS", _PC_2_SYMLINKS, PATHCONF },
#endif

#ifdef _SC_LEVEL1_ICACHE_SIZE
    { "LEVEL1_ICACHE_SIZE", _SC_LEVEL1_ICACHE_SIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL1_ICACHE_ASSOC
    { "LEVEL1_ICACHE_ASSOC", _SC_LEVEL1_ICACHE_ASSOC, SYSCONF },
#endif
#ifdef _SC_LEVEL1_ICACHE_LINESIZE
    { "LEVEL1_ICACHE_LINESIZE", _SC_LEVEL1_ICACHE_LINESIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL1_DCACHE_SIZE
    { "LEVEL1_DCACHE_SIZE", _SC_LEVEL1_DCACHE_SIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL1_DCACHE_ASSOC
    { "LEVEL1_DCACHE_ASSOC", _SC_LEVEL1_DCACHE_ASSOC, SYSCONF },
#endif
#ifdef _SC_LEVEL1_DCACHE_LINESIZE
    { "LEVEL1_DCACHE_LINESIZE", _SC_LEVEL1_DCACHE_LINESIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL2_CACHE_SIZE
    { "LEVEL2_CACHE_SIZE", _SC_LEVEL2_CACHE_SIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL2_CACHE_ASSOC
    { "LEVEL2_CACHE_ASSOC", _SC_LEVEL2_CACHE_ASSOC, SYSCONF },
#endif
#ifdef _SC_LEVEL2_CACHE_LINESIZE
    { "LEVEL2_CACHE_LINESIZE", _SC_LEVEL2_CACHE_LINESIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL3_CACHE_SIZE
    { "LEVEL3_CACHE_SIZE", _SC_LEVEL3_CACHE_SIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL3_CACHE_ASSOC
    { "LEVEL3_CACHE_ASSOC", _SC_LEVEL3_CACHE_ASSOC, SYSCONF },
#endif
#ifdef _SC_LEVEL3_CACHE_LINESIZE
    { "LEVEL3_CACHE_LINESIZE", _SC_LEVEL3_CACHE_LINESIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL4_CACHE_SIZE
    { "LEVEL4_CACHE_SIZE", _SC_LEVEL4_CACHE_SIZE, SYSCONF },
#endif
#ifdef _SC_LEVEL4_CACHE_ASSOC
    { "LEVEL4_CACHE_ASSOC", _SC_LEVEL4_CACHE_ASSOC, SYSCONF },
#endif

#ifdef _SC_IPV6
    { "IPV6", _SC_IPV6, SYSCONF },
#endif
#ifdef _SC_RAW_SOCKETS
    { "RAW_SOCKETS", _SC_RAW_SOCKETS, SYSCONF },
#endif

    { NULL, 0, SYSCONF }
};

#define	_GETCONF_PATH	"/"
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char *_getconf_path = NULL;

int
rpmdsGetconf(rpmds * dsp, const char *path)
	/*@globals _getconf_path @*/
	/*@modifies _getconf_path @*/
{
    const struct conf *c;
    size_t clen;
    long int value;
    const char * NS = "getconf";
    const char *N;
    char * EVR;
    char * t;
    int_32 Flags;

/*@-modobserver@*/
    if (_getconf_path == NULL) {
	_getconf_path = rpmExpand("%{?_rpmds__getconf_path}", NULL);
	/* XXX may need to validate path existence somewhen. */
	if (!(_getconf_path != NULL && *_getconf_path == '/')) {
/*@-observertrans @*/
	    _getconf_path = _free(_getconf_path);
/*@=observertrans @*/
	    _getconf_path = xstrdup(_GETCONF_PATH);
	}
    }
/*@=modobserver@*/

/*@-branchstate@*/
    if (path == NULL)
	path = _getconf_path;

    for (c = vars; c->name != NULL; ++c) {
	N = c->name;
	EVR = NULL;
	switch (c->call) {
	case PATHCONF:
	    value = pathconf(path, c->call_name);
	    if (value != -1) {
		EVR = xmalloc(32);
		sprintf(EVR, "%ld", value);
	    }
	    /*@switchbreak@*/ break;
	case SYSCONF:
	    value = sysconf(c->call_name);
	    if (value == -1l) {
#if defined(_SC_UINT_MAX) && defined(_SC_ULONG_MAX)
/*@-unrecog@*/
		if (c->call_name == _SC_UINT_MAX
		|| c->call_name == _SC_ULONG_MAX) {
		    EVR = xmalloc(32);
		    sprintf(EVR, "%lu", value);
		}
/*@=unrecog@*/
#endif
	    } else {
		EVR = xmalloc(32);
		sprintf(EVR, "%ld", value);
	    }
	    /*@switchbreak@*/ break;
	case CONFSTR:
	    clen = confstr(c->call_name, (char *) NULL, 0);
	    EVR = xmalloc(clen+1);
	    *EVR = '\0';
	    if (confstr (c->call_name, EVR, clen) != clen) {
		fprintf(stderr, "confstr: %s\n", strerror(errno));
		exit (EXIT_FAILURE);
	    }
	    EVR[clen] = '\0';
	    /*@switchbreak@*/ break;
	}
	if (EVR == NULL)
	    continue;

	for (t = EVR; *t; t++) {
	    if (*t == '\n') *t = ' ';
	}
	if (!strcmp(N, "GNU_LIBC_VERSION")
	 || !strcmp(N, "GNU_LIBPTHREAD_VERSION"))
	{
	    for (t = EVR; *t; t++) {
		if (*t == ' ') *t = '-';
	    }
	}

	if (*EVR == '\0' || strchr(EVR, ' ') != NULL
	 || (EVR[0] == '-' && strchr("0123456789", EVR[1]) == NULL))
	{
	    EVR = _free(EVR);
	    continue;
	}

	Flags = RPMSENSE_PROBE|RPMSENSE_EQUAL;
	rpmdsNSAdd(dsp, NS, N, EVR, Flags);
	EVR = _free(EVR);
    }
/*@=branchstate@*/
    return 0;
}

int rpmdsMergePRCO(void * context, rpmds ds)
{
    rpmPRCO PRCO = context;
    int rc = -1;

/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "*** %s(%p, %p) %s\n", __FUNCTION__, context, ds, tagName(rpmdsTagN(ds)));
/*@=modfilesys@*/
    switch(rpmdsTagN(ds)) {
    default:
	break;
    case RPMTAG_PROVIDENAME:
	rc = rpmdsMerge(PRCO->Pdsp, ds);
	break;
    case RPMTAG_REQUIRENAME:
	rc = rpmdsMerge(PRCO->Rdsp, ds);
	break;
    case RPMTAG_CONFLICTNAME:
	rc = rpmdsMerge(PRCO->Cdsp, ds);
	break;
    case RPMTAG_OBSOLETENAME:
	rc = rpmdsMerge(PRCO->Odsp, ds);
	break;
    case RPMTAG_TRIGGERNAME:
	rc = rpmdsMerge(PRCO->Tdsp, ds);
	break;
    case RPMTAG_DIRNAMES:
	rc = rpmdsMerge(PRCO->Ddsp, ds);
	break;
    case RPMTAG_FILELINKTOS:
	rc = rpmdsMerge(PRCO->Ldsp, ds);
	break;
    }
    return rc;
}

rpmPRCO rpmdsFreePRCO(rpmPRCO PRCO)
{
    if (PRCO) {
	PRCO->this = rpmdsFree(PRCO->this);
	PRCO->P = rpmdsFree(PRCO->P);
	PRCO->R = rpmdsFree(PRCO->R);
	PRCO->C = rpmdsFree(PRCO->C);
	PRCO->O = rpmdsFree(PRCO->O);
	PRCO->T = rpmdsFree(PRCO->T);
	PRCO->D = rpmdsFree(PRCO->D);
	PRCO->L = rpmdsFree(PRCO->L);
	PRCO->Pdsp = NULL;
	PRCO->Rdsp = NULL;
	PRCO->Cdsp = NULL;
	PRCO->Odsp = NULL;
	PRCO->Tdsp = NULL;
	PRCO->Ddsp = NULL;
	PRCO->Ldsp = NULL;
	PRCO = _free(PRCO);
    }
    return NULL;
}

rpmPRCO rpmdsNewPRCO(Header h)
{
    rpmPRCO PRCO = xcalloc(1, sizeof(*PRCO));

    if (h != NULL) {
	int scareMem = 0;
	PRCO->this = rpmdsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
	PRCO->P = rpmdsNew(h, RPMTAG_PROVIDENAME, scareMem);
	PRCO->R = rpmdsNew(h, RPMTAG_REQUIRENAME, scareMem);
	PRCO->C = rpmdsNew(h, RPMTAG_CONFLICTNAME, scareMem);
	PRCO->O = rpmdsNew(h, RPMTAG_OBSOLETENAME, scareMem);
	PRCO->T = rpmdsNew(h, RPMTAG_TRIGGERNAME, scareMem);
	PRCO->D = rpmdsNew(h, RPMTAG_DIRNAMES, scareMem);
	PRCO->L = rpmdsNew(h, RPMTAG_FILELINKTOS, scareMem);
    }
    PRCO->Pdsp =  &PRCO->P;
    PRCO->Rdsp =  &PRCO->R;
    PRCO->Cdsp =  &PRCO->C;
    PRCO->Odsp =  &PRCO->O;
    PRCO->Tdsp =  &PRCO->T;
    PRCO->Ddsp =  &PRCO->D;
    PRCO->Ldsp =  &PRCO->L;
    return PRCO;
}

rpmds rpmdsFromPRCO(rpmPRCO PRCO, rpmTag tagN)
{
    if (PRCO == NULL)
	return NULL;
    /*@-compdef -refcounttrans -retalias -retexpose -usereleased @*/
    if (tagN == RPMTAG_NAME)
	return PRCO->this;
    if (tagN == RPMTAG_PROVIDENAME)
	return *PRCO->Pdsp;
    if (tagN == RPMTAG_REQUIRENAME)
	return *PRCO->Rdsp;
    if (tagN == RPMTAG_CONFLICTNAME)
	return *PRCO->Cdsp;
    if (tagN == RPMTAG_OBSOLETENAME)
	return *PRCO->Odsp;
    if (tagN == RPMTAG_TRIGGERNAME)
	return *PRCO->Tdsp;
    if (tagN == RPMTAG_DIRNAMES)
	return *PRCO->Ddsp;
    if (tagN == RPMTAG_FILELINKTOS)
	return *PRCO->Ldsp;
    return NULL;
    /*@=compdef =refcounttrans =retalias =retexpose =usereleased @*/
}

/**
 * Return a soname dependency constructed from an elf string.
 * @retval t		soname dependency
 * @param s		elf string (NULL uses "")
 * @param isElf64	is this an ELF64 symbol?
 */
static char * sonameDep(/*@returned@*/ char * t, const char * s, int isElf64)
	/*@modifies t @*/
{
    *t = '\0';
#if !defined(__alpha__) && ( !defined(__sun) && !defined(__unix) )
    if (isElf64) {
	if (s[strlen(s)-1] != ')')
	(void) stpcpy( stpcpy(t, s), "()(64bit)");
    else
	    (void) stpcpy( stpcpy(t, s), "(64bit)");
    }else
#endif
	(void) stpcpy(t, s);
    return t;
}

int rpmdsELF(const char * fn, int flags,
		int (*add) (void * context, rpmds ds), void * context)
{
#if HAVE_GELF_H && HAVE_LIBELF
    Elf * elf;
    Elf_Scn * scn;
    Elf_Data * data;
    GElf_Ehdr ehdr_mem, * ehdr;
    GElf_Shdr shdr_mem, * shdr;
    GElf_Verdef def_mem, * def;
    GElf_Verneed need_mem, * need;
    GElf_Dyn dyn_mem, * dyn;
    unsigned int auxoffset;
    unsigned int offset;
    int fdno;
    int cnt2;
    int cnt;
    char buf[BUFSIZ];
    const char * s;
    int is_executable;
    const char * soname = NULL;
    rpmds ds;
    char * t;
    int xx;
    int isElf64;
    int isDSO;
    int gotSONAME = 0;
    int gotDEBUG = 0;
    int gotHASH = 0;
    int gotGNUHASH = 0;
    int skipP = (flags & RPMELF_FLAG_SKIPPROVIDES);
    int skipR = (flags & RPMELF_FLAG_SKIPREQUIRES);
    static int filter_GLIBC_PRIVATE = 0;
    static int oneshot = 0;

/*@-castfcnptr@*/
if (_rpmds_debug < 0)
fprintf(stderr, "*** %s(%s, %d, %p, %p)\n", __FUNCTION__, fn, flags, (void *)add, context);
/*@=castfcnptr@*/
    if (oneshot == 0) {
	oneshot = 1;
	filter_GLIBC_PRIVATE = rpmExpandNumeric("%{?_filter_GLIBC_PRIVATE}");
    }

    /* Extract dependencies only from files with executable bit set. */
    {	struct stat sb, * st = &sb;
	if (stat(fn, st) != 0)
	    return -1;
	is_executable = (st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));
    }

    fdno = open(fn, O_RDONLY);
    if (fdno < 0)
	return fdno;

    (void) elf_version(EV_CURRENT);

/*@-evalorder@*/
    elf = NULL;
    if ((elf = elf_begin (fdno, ELF_C_READ, NULL)) == NULL
     || elf_kind(elf) != ELF_K_ELF
     || (ehdr = gelf_getehdr(elf, &ehdr_mem)) == NULL
     || !(ehdr->e_type == ET_DYN || ehdr->e_type == ET_EXEC))
	goto exit;
/*@=evalorder@*/

    isElf64 = ehdr->e_ident[EI_CLASS] == ELFCLASS64;
    isDSO = ehdr->e_type == ET_DYN;

    /*@-branchstate -uniondef @*/
    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
	shdr = gelf_getshdr(scn, &shdr_mem);
	if (shdr == NULL)
	    break;

	soname = _free(soname);
	switch (shdr->sh_type) {
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case SHT_GNU_verdef:
	    data = NULL;
	    if (!skipP)
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		
		    def = gelf_getverdef (data, offset, &def_mem);
		    if (def == NULL)
			/*@innerbreak@*/ break;
		    auxoffset = offset + def->vd_aux;
		    for (cnt2 = def->vd_cnt; --cnt2 >= 0; ) {
			GElf_Verdaux aux_mem, * aux;

			aux = gelf_getverdaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    /*@innerbreak@*/ break;

			s = elf_strptr(elf, shdr->sh_link, aux->vda_name);
			if (s == NULL)
			    /*@innerbreak@*/ break;

			if (def->vd_flags & VER_FLG_BASE) {
			    soname = _free(soname);
			    soname = xstrdup(s);
			} else
			if (soname != NULL
			 && !(filter_GLIBC_PRIVATE != 0
				&& !strcmp(s, "GLIBC_PRIVATE")))
			{
			    buf[0] = '\0';
			    t = buf;
			    t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

			    t++;	/* XXX "foo(bar)" already in buf. */

			    /* Add next provide dependency. */
			    ds = rpmdsSingle(RPMTAG_PROVIDES,
					sonameDep(t, buf, isElf64),
					"", RPMSENSE_FIND_PROVIDES);
			    xx = add(context, ds);
			    ds = rpmdsFree(ds);
			}
			auxoffset += aux->vda_next;
		    }
		    offset += def->vd_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_GNU_verneed:
	    data = NULL;
	    /* Only from files with executable bit set. */
	    if (!skipR && is_executable)
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		    need = gelf_getverneed (data, offset, &need_mem);
		    if (need == NULL)
			/*@innerbreak@*/ break;

		    s = elf_strptr(elf, shdr->sh_link, need->vn_file);
		    if (s == NULL)
			/*@innerbreak@*/ break;
		    soname = _free(soname);
		    soname = xstrdup(s);
		    auxoffset = offset + need->vn_aux;
		    for (cnt2 = need->vn_cnt; --cnt2 >= 0; ) {
			GElf_Vernaux aux_mem, * aux;

			aux = gelf_getvernaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    /*@innerbreak@*/ break;

			s = elf_strptr(elf, shdr->sh_link, aux->vna_name);
			if (s == NULL)
			    /*@innerbreak@*/ break;

			/* Filter dependencies that contain GLIBC_PRIVATE */
			if (soname != NULL
			 && !(filter_GLIBC_PRIVATE != 0
				&& !strcmp(s, "GLIBC_PRIVATE")))
			{
			    buf[0] = '\0';
			    t = buf;
			    t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

			    t++;	/* XXX "foo(bar)" already in buf. */

			    /* Add next require dependency. */
			    ds = rpmdsSingle(RPMTAG_REQUIRENAME,
					sonameDep(t, buf, isElf64),
					"", RPMSENSE_FIND_REQUIRES);
			    xx = add(context, ds);
			    ds = rpmdsFree(ds);
			}
			auxoffset += aux->vna_next;
		    }
		    offset += need->vn_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_DYNAMIC:
	    data = NULL;
	    while ((data = elf_getdata (scn, data)) != NULL) {
/*@-boundswrite@*/
		for (cnt = 0; cnt < (shdr->sh_size / shdr->sh_entsize); ++cnt) {
		    dyn = gelf_getdyn (data, cnt, &dyn_mem);
		    if (dyn == NULL)
			/*@innerbreak@*/ break;
		    s = NULL;
		    switch (dyn->d_tag) {
		    default:
			/*@innercontinue@*/ continue;
			/*@notreached@*/ /*@switchbreak@*/ break;
		    case DT_HASH:
			gotHASH= 1;
			/*@innercontinue@*/ continue;
		    case DT_GNU_HASH:
			gotGNUHASH= 1;
			/*@innercontinue@*/ continue;
                    case DT_DEBUG:    
			gotDEBUG = 1;
			/*@innercontinue@*/ continue;
		    case DT_NEEDED:
			/* Only from files with executable bit set. */
			if (skipR || !is_executable)
			    /*@innercontinue@*/ continue;
			/* Add next require dependency. */
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
assert(s != NULL);
			buf[0] = '\0';
			ds = rpmdsSingle(RPMTAG_REQUIRENAME,
				sonameDep(buf, s, isElf64),
				"", RPMSENSE_FIND_REQUIRES);
			xx = add(context, ds);
			ds = rpmdsFree(ds);
			/*@switchbreak@*/ break;
		    case DT_SONAME:
			gotSONAME = 1;
			if (skipP)
			    /*@innercontinue@*/ continue;
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
assert(s != NULL);
			/* Add next provide dependency. */
			buf[0] = '\0';
			ds = rpmdsSingle(RPMTAG_PROVIDENAME,
				sonameDep(buf, s, isElf64),
				"", RPMSENSE_FIND_PROVIDES);
			xx = add(context, ds);
			ds = rpmdsFree(ds);
			/*@switchbreak@*/ break;
		    }
		}
/*@=boundswrite@*/
	    }
	    /*@switchbreak@*/ break;
	}
    }
    /*@=branchstate =uniondef @*/

    /* For DSOs which use the .gnu_hash section and don't have a .hash
     * section, we need to ensure that we have a new enough glibc. */
    if (gotGNUHASH && !gotHASH) {
	ds = rpmdsSingle(RPMTAG_REQUIRENAME, "rtld(GNU_HASH)", "",
			RPMSENSE_FIND_REQUIRES);
	xx = add(context, ds);
	ds = rpmdsFree(ds);
    }

    /* For DSO's, provide the basename of the file if DT_SONAME not found. */
    if (!skipP && isDSO && !gotDEBUG && !gotSONAME) {
	s = strrchr(fn, '/');
	if (s != NULL)
	    s++;
	else
	    s = fn;
assert(s != NULL);

	/* Add next provide dependency. */
	buf[0] = '\0';
	ds = rpmdsSingle(RPMTAG_PROVIDENAME,
		sonameDep(buf, s, isElf64), "", RPMSENSE_FIND_PROVIDES);
	xx = add(context, ds);
	ds = rpmdsFree(ds);
    }

exit:
    soname = _free(soname);
    if (elf) (void) elf_end(elf);
    if (fdno > 0)
	xx = close(fdno);
    return 0;
#else
    return -1;
#endif
}

#define	_SBIN_LDCONFIG_P	"/sbin/ldconfig -p"
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char * _ldconfig_cmd = _SBIN_LDCONFIG_P;

#define	_LD_SO_CACHE	"/etc/ld.so.cache"
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char * _ldconfig_cache = NULL;

int rpmdsLdconfig(rpmPRCO PRCO, const char * fn)
	/*@globals _ldconfig_cmd, _ldconfig_cache @*/
	/*@modifies _ldconfig_cmd, _ldconfig_cache @*/
{
    char buf[BUFSIZ];
    const char *DSOfn;
    const char *N, *EVR;
    int_32 Flags = 0;
    rpmds ds;
    char * f, * fe;
    char * g, * ge;
    char * t;
    FILE * fp = NULL;
    int rc = -1;
    int xx;

    if (PRCO == NULL)
	return -1;

/*@-modobserver@*/
    if (_ldconfig_cmd == NULL) {
	_ldconfig_cmd = rpmExpand("%{?_rpmds_ldconfig_cmd}", NULL);
	if (!(_ldconfig_cmd != NULL && *_ldconfig_cmd == '/')) {
/*@-observertrans @*/
	    _ldconfig_cmd = _free(_ldconfig_cmd);
/*@=observertrans @*/
	    _ldconfig_cmd = xstrdup(_SBIN_LDCONFIG_P);
	}
    }

    if (_ldconfig_cache == NULL) {
	_ldconfig_cache = rpmExpand("%{?_rpmds_ldconfig_cache}", NULL);
	/* XXX may need to validate path existence somewhen. */
	if (!(_ldconfig_cache != NULL && *_ldconfig_cache == '/')) {
/*@-observertrans @*/
	    _ldconfig_cache = _free(_ldconfig_cache);
/*@=observertrans @*/
	    _ldconfig_cache = xstrdup(_LD_SO_CACHE);
	}
    }
/*@=modobserver@*/

/*@-branchstate@*/
    if (fn == NULL)
	fn = _ldconfig_cache;
/*@=branchstate@*/

if (_rpmds_debug < 0)
fprintf(stderr, "*** %s(%p, %s) P %p R %p C %p O %p T %p D %p L %p\n", __FUNCTION__, PRCO, fn, PRCO->Pdsp, PRCO->Rdsp, PRCO->Cdsp, PRCO->Odsp, PRCO->Tdsp, PRCO->Ddsp, PRCO->Ldsp);

    fp = popen(_ldconfig_cmd, "r");
    if (fp == NULL)
	goto exit;

    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	EVR = NULL;
	/* rtrim on line. */
	ge = f + strlen(f);
	while (--ge > f && _isspace(*ge))
	    *ge = '\0';

	/* ltrim on line. */
	while (*f && _isspace(*f))
	    f++;

	/* split on '=>' */
	fe = f;
	while (*fe && !(fe[0] == '=' && fe[1] == '>'))
            fe++;
	if (*fe == '\0')
	    continue;

	/* find the DSO file name. */
	DSOfn = fe + 2;

	/* ltrim on DSO file name. */
	while (*DSOfn && _isspace(*DSOfn))
	    DSOfn++;
	if (*DSOfn == '\0')
	    continue;

	/* rtrim from "=>" */
	if (fe > f && fe[-1] == ' ') fe[-1] = '\0';
	*fe++ = '\0';
	*fe++ = '\0';
	g = fe;

	/* ltrim on field 2. */
	while (*g && _isspace(*g))
            g++;
	if (*g == '\0')
	    continue;

	/* split out flags */
	for (t = f; *t != '\0'; t++) {
	    if (!_isspace(*t))
		/*@innercontinue@*/ continue;
	    *t++ = '\0';
	    /*@innerbreak@*/ break;
	}
	/* XXX "libc4" "ELF" "libc5" "libc6" _("unknown") */
	/* XXX use flags to generate soname color */
	/* ",64bit" ",IA-64" ",x86-64", ",64bit" are color = 2 */
	/* ",N32" for mips64/libn32 */

	/* XXX use flags and LDASSUME_KERNEL to skip sonames? */
	/* "Linux" "Hurd" "Solaris" "FreeBSD" "kNetBSD" N_("Unknown OS") */
	/* ", OS ABI: %s %d.%d.%d" */

	N = f;
/*@-branchstate@*/
	if (EVR == NULL)
	    EVR = "";
/*@=branchstate@*/
	Flags |= RPMSENSE_PROBE;
	ds = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR, Flags);
	xx = rpmdsMerge(PRCO->Pdsp, ds);
	ds = rpmdsFree(ds);

	xx = rpmdsELF(DSOfn, 0, rpmdsMergePRCO, PRCO);
    }
    rc = 0;

exit:
    if (fp != NULL) (void) pclose(fp);
    return rc;
}

#if HAVE_LIBELF && !HAVE_GELF_GETVERNAUX
/* We have gelf.h and libelf, but we don't have some of the
 * helper functions gelf_getvernaux(), gelf_getverneed(), etc.
 * Provide our own simple versions here.
 */

static GElf_Verdef *gelf_getverdef(Elf_Data *data, int offset,
                   GElf_Verdef *dst)
{
	return (GElf_Verdef *) ((char *) data->d_buf + offset);
}

static GElf_Verdaux *gelf_getverdaux(Elf_Data *data, int offset,
                    GElf_Verdaux *dst)
{
	return (GElf_Verdaux *) ((char *) data->d_buf + offset);
}

static GElf_Verneed *gelf_getverneed(Elf_Data *data, int offset,
                    GElf_Verneed *dst)
{
	return (GElf_Verneed *) ((char *) data->d_buf + offset);
}

static GElf_Vernaux *gelf_getvernaux(Elf_Data *data, int offset,
                    GElf_Vernaux *dst)
{
	return (GElf_Vernaux *) ((char *) data->d_buf + offset);
}

/* Most non-Linux systems won't have SHT_GNU_verdef or SHT_GNU_verneed,
 * but they might have something mostly-equivalent.  Solaris has
 * SHT_SUNW_{verdef,verneed}
 */
#if !defined(SHT_GNU_verdef) && defined(__sun) && defined(SHT_SUNW_verdef)
# define SHT_GNU_verdef SHT_SUNW_verdef
# define SHT_GNU_verneed SHT_SUNW_verneed
#endif

#endif /* HAVE_LIBELF && !HAVE_GELF_GETVERNAUX */


#if defined(__sun)
#define	_RLD_SEARCH_PATH	"/lib:/usr/lib"
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char * _rld_search_path = NULL;

/* search a colon-separated list of directories for shared objects */
int rpmdsRldpath(rpmPRCO PRCO, const char * rldp)
	/*@globals _rld_search_path @*/
	/*@modifies _rld_search_path @*/
{
    char buf[BUFSIZ];
    const char *N, *EVR;
    int_32 Flags = 0;
    rpmds ds;
    const char * f;
    const char * g;
    int rc = -1;
    int xx;
    glob_t  gl;
    char ** gp;

    if (PRCO == NULL)
	return -1;

/*@-modobserver@*/
    if (_rld_search_path == NULL) {
	_rld_search_path = rpmExpand("%{?_rpmds_rld_search_path}", NULL);
	/* XXX may need to validate path existence somewhen. */
	if (!(_rld_search_path != NULL && *_rld_search_path == '/')) {
/*@-observertrans @*/
	    _rld_search_path = _free(_rld_search_path);
/*@=observertrans @*/
	    _rld_search_path = xstrdup(_RLD_SEARCH_PATH);
	}
    }
/*@=modobserver@*/

/*@-branchstate@*/
    if (rldp == NULL)
	rldp = _rld_search_path;
/*@=branchstate@*/

if (_rpmds_debug > 0)
fprintf(stderr, "*** %s(%p, %s) P %p R %p C %p O %p\n", __FUNCTION__, PRCO, rldp, PRCO->Pdsp, PRCO->Rdsp, PRCO->Cdsp, PRCO->Odsp);

    f = rldp;
    /* move through the path, splitting on : */
    while (f) {
	EVR = NULL;
	g = strchr(f, ':');
	if (g == NULL) {
	    strcpy(buf, f);
	    /* this is the last element, no more :'s */
	    f = NULL;
	} else {
	    /* copy this chunk to buf */
	    strncpy(buf, f, g - f + 1);
	    buf[g-f] = '\0';

	    /* get ready for next time through */
	    f = g + 1;
	}

	if ( !(strlen(buf) > 0 && buf[0] == '/') )
	    continue;

	/* XXX: danger, buffer len */
	/* XXX: *.so.* should be configurable via a macro */
	strcat(buf, "/*.so.*");

if (_rpmds_debug > 0)
fprintf(stderr, "*** %s(%p, %s) globbing %s\n", __FUNCTION__, PRCO, rldp, buf);

	xx = glob(buf, 0, NULL, &gl);
	if (xx)		/* glob error, probably GLOB_NOMATCH */
	    continue;

if (_rpmds_debug > 0)
fprintf(stderr, "*** %s(%p, %s) glob matched %d files\n", __FUNCTION__, PRCO, rldp, gl.gl_pathc);

	gp = gl.gl_pathv;
	/* examine each match */
/*@-branchstate@*/
	while (gp && *gp) {
	    const char *DSOfn;
	    /* XXX: should probably verify that we matched a file */
	    DSOfn = *gp;
	    gp++;
	    if (EVR == NULL)
		EVR = "";

	    /* N needs to be basename of DSOfn */
	    N = DSOfn + strlen(DSOfn);
	    while (N > DSOfn && *N != '/')
		--N;

	    Flags |= RPMSENSE_PROBE;
	    ds = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR, Flags);
	    xx = rpmdsMerge(PRCO->Pdsp, ds);
	    ds = rpmdsFree(ds);

	    xx = rpmdsELF(DSOfn, 0, rpmdsMergePRCO, PRCO);
	}
/*@=branchstate@*/
/*@-immediatetrans@*/
	globfree(&gl);
/*@=immediatetrans@*/
    }
    rc = 0;

    return rc;
}

#define	_SOLARIS_CRLE	"/usr/sbin/crle"
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char * _crle_cmd = NULL;

int rpmdsCrle(rpmPRCO PRCO, /*@unused@*/ const char * fn)
	/*@globals _crle_cmd @*/
	/*@modifies _crle_cmd @*/
{
    char buf[BUFSIZ];
    char * f;
    char * g, * ge;
    FILE * fp = NULL;
    int rc = -1;	/* assume failure */
    int xx;
    int found_dlp = 0;

    if (PRCO == NULL)
	return -1;

/*@-modobserver@*/
    if (_crle_cmd == NULL) {
	_crle_cmd = rpmExpand("%{?_rpmds_crle_cmd}", NULL);
	if (!(_crle_cmd != NULL && *_crle_cmd == '/')) {
/*@-observertrans @*/
	    _crle_cmd = _free(_crle_cmd);
/*@=observertrans @*/
	    _crle_cmd = xstrdup(_SOLARIS_CRLE);
	}
    }

    /* XXX: we rely on _crle_cmd including the -64 arg, if ELF64 */
    fp = popen(_crle_cmd, "r");
    if (fp == NULL)
	return rc;

    /* 
     * we want the first line that contains "(ELF):"
     * we cannot search for "Default Library Path (ELF):" because that
     * changes in non-C locales.
     */
    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	if (found_dlp)	/* XXX read all data? */
	    continue;

	g = strstr(f, "(ELF):");
	if (g == NULL)
	    continue;

	found_dlp = 1;
	f = g + (sizeof("(ELF):")-1);
	while (_isspace(*f))
	    f++;

	/* rtrim path */
	ge = f + strlen(f);
	while (--ge > f && _isspace(*ge))
	    *ge = '\0';
    }
    xx = pclose(fp);

    /* we have the loader path, let rpmdsRldpath() do the work */
    if (found_dlp)
	rc = rpmdsRldpath(PRCO, f);

    return rc;
}
#endif

int rpmdsUname(rpmds *dsp, const struct utsname * un)
{
/*@observer@*/
    static const char * NS = "uname";
    struct utsname myun;
    int rc = -1;
    int xx;

    if (un == NULL) {
	xx = uname(&myun);
	if (xx != 0)
	    goto exit;
	un = &myun;
    }

/*@-type@*/
    /* XXX values need to be checked for EVR (i.e. no '-' character.) */
    if (un->sysname != NULL)
	rpmdsNSAdd(dsp, NS, "sysname", un->sysname, RPMSENSE_EQUAL);
    if (un->nodename != NULL)
	rpmdsNSAdd(dsp, NS, "nodename", un->nodename, RPMSENSE_EQUAL);
    if (un->release != NULL)
	rpmdsNSAdd(dsp, NS, "release", un->release, RPMSENSE_EQUAL);
#if 0	/* XXX has embedded spaces */
    if (un->version != NULL)
	rpmdsNSAdd(dsp, NS, "version", un->version, RPMSENSE_EQUAL);
#endif
    if (un->machine != NULL)
	rpmdsNSAdd(dsp, NS, "machine", un->machine, RPMSENSE_EQUAL);
#if defined(__linux__)
    if (un->domainname != NULL && strcmp(un->domainname, "(none)"))
	rpmdsNSAdd(dsp, NS, "domainname", un->domainname, RPMSENSE_EQUAL);
#endif
/*@=type@*/
    rc = 0;

exit:
    return rc;
}

#define	_PERL_PROVIDES	"/usr/bin/find /usr/lib/perl5 | /usr/lib/rpm/perl.prov"
/*@unchecked@*/ /*@observer@*/ /*@owned@*/ /*@relnull@*/
static const char * _perldeps_cmd = NULL;

int rpmdsPipe(rpmds * dsp, int_32 tagN, const char * cmd)
	/*@globals _perldeps_cmd @*/
	/*@modifies _perldeps_cmd @*/
{
    char buf[BUFSIZ];
    const char *N, *EVR;
    int_32 Flags = 0;
    rpmds ds;
    char * f, * fe;
    char * g, * ge;
    FILE * fp = NULL;
    int rc = -1;
    int cmdprinted;
    int ln;
    int xx;

/*@-modobserver@*/
    if (_perldeps_cmd == NULL) {
	_perldeps_cmd = rpmExpand("%{?_rpmds_perldeps_cmd}", NULL);
	/* XXX may need to validate path existence somewhen. */
	if (!(_perldeps_cmd != NULL && *_perldeps_cmd == '/')) {
/*@-observertrans @*/
	    _perldeps_cmd = _free(_perldeps_cmd);
/*@=observertrans @*/
	    _perldeps_cmd = xstrdup(_PERL_PROVIDES);
	}
    }
/*@=modobserver@*/

    if (tagN <= 0)
	tagN = RPMTAG_PROVIDENAME;
/*@-branchstate@*/
    if (cmd == NULL)
	cmd = _perldeps_cmd;
/*@=branchstate@*/

    fp = popen(cmd, "r");
    if (fp == NULL)
	goto exit;

    ln = 0;
    cmdprinted = 0;
    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	ln++;

	/* insure a terminator. */
	buf[sizeof(buf)-1] = '\0';

	/* ltrim on line. */
	while (*f && _isspace(*f))
	    f++;

	/* skip empty lines and comments */
	if (*f == '\0' || *f == '#')
	    continue;

	/* rtrim on line. */
	fe = f + strlen(f);
	while (--fe > f && _isspace(*fe))
	    *fe = '\0';

	/* split on ' '  or comparison operator. */
	fe = f;
	while (*fe && !_isspace(*fe) && strchr("<=>", *fe) == NULL)
	    fe++;
	while (*fe && _isspace(*fe))
	    *fe++ = '\0';

	if (!(xisalnum(f[0]) || f[0] == '_' || f[0] == '/' || f[0] == '%')) {
	    if (!cmdprinted++)
		fprintf(stderr, _("running \"%s\" pipe command\n"), cmd);
	    /* XXX N must begin with alphanumeric, _, / or %. */
	    fprintf(stderr,
		_("\tline %d: \"%s\" must begin with alphanumeric, '_', '/' or '%%'. Skipping ...\n"),
		    ln, f);
            continue;
	}

	N = f;
	EVR = NULL;
	Flags = 0;

	/* parse for non-path, versioned dependency. */
/*@-branchstate@*/
	if (*f != '/' && *fe != '\0') {
	    struct cmpop *cop;

	    /* parse comparison operator */
	    g = fe;
	    for (cop = cops; cop->operator != NULL; cop++) {
		if (strncmp(g, cop->operator, strlen(cop->operator)))
		    /*@innercontinue@*/ continue;
		*g = '\0';
		g += strlen(cop->operator);
		Flags = cop->sense;
		/*@innerbreak@*/ break;
	    }

	    if (Flags == 0) {
		if (!cmdprinted++)
		    fprintf(stderr, _("running \"%s\" pipe command\n"), cmd),
		/* XXX No comparison operator found. */
		fprintf(stderr, _("\tline %d: \"%s\" has not a comparison operator found. Skipping ...\n"),
			ln, g);
		continue;
	    }

	    /* ltrim on field 2. */
	    while (*g && _isspace(*g))
		g++;
	    if (*g == '\0') {
		if (!cmdprinted++)
		    fprintf(stderr, _("running \"%s\" pipe command\n"), cmd),
		/* XXX No EVR comparison value found. */
		fprintf(stderr, _("\tline %d: No EVR comparison value found.\n Skipping ..."),
			ln);
		continue;
	    }

	    ge = g + 1;
	    while (*ge && !_isspace(*ge))
		ge++;

	    if (*ge != '\0')
		*ge = '\0';	/* XXX can't happen, line rtrim'ed already. */

	    EVR = g;
	}

	if (EVR == NULL)
	    EVR = "";
	Flags |= RPMSENSE_PROBE;
/*@=branchstate@*/
	ds = rpmdsSingle(tagN, N, EVR, Flags);
	xx = rpmdsMerge(dsp, ds);
	ds = rpmdsFree(ds);
    }
    rc = 0;

exit:
    if (fp != NULL) (void) pclose(fp);
    return rc;
}

static int rpmdsNAcmp(rpmds A, rpmds B)
	/*@*/
{
    const char * AN = A->ns.N;
    const char * AA = A->ns.A;
    const char * BN = B->ns.N;
    const char * BA = B->ns.A;
    int rc;

    if (!AA && !BA) {
	rc = strcmp(AN, BN);
    } else if (AA && !BA) {
	rc = strncmp(AN, BN, (AA - AN)) || BN[AA - AN];
	if (!rc)
	    rc = strcmp(AA, B->A);
    } else if (!AA && BA) {
	rc = strncmp(AN, BN, (BA - BN)) || AN[BA - BN];
	if (!rc)
	    rc = strcmp(BA, A->A);
    } else {
	rc = strcmp(AN, BN);
    }
    return rc;
}

int rpmdsCompare(const rpmds A, const rpmds B)
{
    const char *aDepend = (A->DNEVR != NULL ? xstrdup(A->DNEVR+2) : "");
    const char *bDepend = (B->DNEVR != NULL ? xstrdup(B->DNEVR+2) : "");
    EVR_t a = memset(alloca(sizeof(*a)), 0, sizeof(*a));
    EVR_t b = memset(alloca(sizeof(*a)), 0, sizeof(*a));
    int (*EVRcmp) (const char *a, const char *b);
    int result;
    int sense;
    int xx;

/*@-boundsread@*/
    /* Different names (and/or name.arch's) don't overlap. */
    if (rpmdsNAcmp(A, B)) {
	result = 0;
	goto exit;
    }

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
/*@-nullderef@*/
    if (!(A->EVR && A->Flags && B->EVR && B->Flags)) {
	result = 1;
	goto exit;
    }

    /* Same name. If either A or B is an existence test, always overlap. */
    if (!((A->Flags[A->i] & RPMSENSE_SENSEMASK) && (B->Flags[B->i] & RPMSENSE_SENSEMASK))) {
	result = 1;
	goto exit;
    }

    /* If either EVR is non-existent or empty, always overlap. */
    if (!(A->EVR[A->i] && *A->EVR[A->i] && B->EVR[B->i] && *B->EVR[B->i])) {
	result = 1;
	goto exit;
    }

    /* Both AEVR and BEVR exist. */
/*@-boundswrite@*/
    xx = (A->EVRparse ? A->EVRparse : rpmEVRparse) (A->EVR[A->i], a);
    xx = (B->EVRparse ? B->EVRparse : rpmEVRparse) (B->EVR[B->i], b);
/*@=boundswrite@*/

    /* If EVRcmp is identical, use that, otherwise use default. */
    EVRcmp = (A->EVRcmp && B->EVRcmp && A->EVRcmp == B->EVRcmp)
	? A->EVRcmp : rpmvercmp;

    /* Compare {A,B} [epoch:]version[-release] */
    sense = 0;
    if (a->E && *a->E && b->E && *b->E)
/*@i@*/	sense = EVRcmp(a->E, b->E);
    else if (a->E && *a->E && atol(a->E) > 0) {
	if (!B->nopromote) {
	    int lvl = (_rpmds_unspecified_epoch_noise  ? RPMMESS_WARNING : RPMMESS_DEBUG);
	    rpmMessage(lvl, _("The \"B\" dependency needs an epoch (assuming same epoch as \"A\")\n\tA = \"%s\"\tB = \"%s\"\n"),
		aDepend, bDepend);
	    sense = 0;
	} else
	    sense = 1;
    } else if (b->E && *b->E && atol(b->E) > 0)
	sense = -1;

    if (sense == 0) {
/*@i@*/	sense = EVRcmp(a->V, b->V);
	if (sense == 0 && a->R && *a->R && b->R && *b->R)
/*@i@*/	    sense = EVRcmp(a->R, b->R);
    }
/*@=boundsread@*/
    a->str = _free(a->str);
    b->str = _free(b->str);

    /* Detect overlap of {A,B} range. */
    result = 0;
    if (sense < 0 && ((A->Flags[A->i] & RPMSENSE_GREATER) || (B->Flags[B->i] & RPMSENSE_LESS))) {
	result = 1;
    } else if (sense > 0 && ((A->Flags[A->i] & RPMSENSE_LESS) || (B->Flags[B->i] & RPMSENSE_GREATER))) {
	result = 1;
    } else if (sense == 0 &&
	(((A->Flags[A->i] & RPMSENSE_EQUAL) && (B->Flags[B->i] & RPMSENSE_EQUAL)) ||
	 ((A->Flags[A->i] & RPMSENSE_LESS) && (B->Flags[B->i] & RPMSENSE_LESS)) ||
	 ((A->Flags[A->i] & RPMSENSE_GREATER) && (B->Flags[B->i] & RPMSENSE_GREATER)))) {
	result = 1;
    }
/*@=nullderef@*/

exit:
    if (_noisy_range_comparison_debug_message)
    rpmMessage(RPMMESS_DEBUG, _("  %s    A %s\tB %s\n"),
	(result ? _("YES") : _("NO ")), aDepend, bDepend);
    aDepend = _free(aDepend);
    bDepend = _free(bDepend);
    return result;
}

void rpmdsProblem(rpmps ps, const char * pkgNEVR, const rpmds ds,
	const fnpyKey * suggestedKeys, int adding)
{
    const char * Name =  rpmdsN(ds);
    const char * DNEVR = rpmdsDNEVR(ds);
    const char * EVR = rpmdsEVR(ds);
    rpmProblemType type;
    fnpyKey key;

    if (ps == NULL) return;

    /*@-branchstate@*/
    if (Name == NULL) Name = "?N?";
    if (EVR == NULL) EVR = "?EVR?";
    if (DNEVR == NULL) DNEVR = "? ?N? ?OP? ?EVR?";
    /*@=branchstate@*/

    rpmMessage(RPMMESS_DEBUG, _("package %s has unsatisfied %s: %s\n"),
	    pkgNEVR, ds->Type, DNEVR+2);

    switch ((unsigned)DNEVR[0]) {
    case 'C':	type = RPMPROB_CONFLICT;	break;
    default:
    case 'R':	type = RPMPROB_REQUIRES;	break;
    }

    key = (suggestedKeys ? suggestedKeys[0] : NULL);
    rpmpsAppend(ps, type, pkgNEVR, key, NULL, NULL, DNEVR, adding);
}

int rpmdsAnyMatchesDep (const Header h, const rpmds req, int nopromote)
{
    int scareMem = 0;
    rpmds provides = NULL;
    int result = 0;

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (req->EVR == NULL || req->Flags == NULL)
	return 1;

    switch(req->ns.Type) {
    default:
/*@-boundsread@*/
	/* Primary key retrieve satisfes an existence compare. */
	if (!(req->Flags[req->i] & RPMSENSE_SENSEMASK) || !req->EVR[req->i] || *req->EVR[req->i] == '\0')
	    return 1;
/*@=boundsread@*/
	/*@fallthrough@*/
    case RPMNS_TYPE_ARCH:
	break;
    }

    /* Get provides information from header */
    provides = rpmdsInit(rpmdsNew(h, RPMTAG_PROVIDENAME, scareMem));
    if (provides == NULL)
	goto exit;	/* XXX should never happen */
    if (nopromote)
	(void) rpmdsSetNoPromote(provides, nopromote);

    /*
     * Rpm prior to 3.0.3 did not have versioned provides.
     * If no provides version info is available, match any/all requires
     * with same name.
     */
    if (provides->EVR == NULL) {
	result = 1;
	goto exit;
    }

    /* If any provide matches the require, we're done. */
    result = 0;
    if (provides != NULL)
    while (rpmdsNext(provides) >= 0)
	if ((result = rpmdsCompare(provides, req)))
	    break;

exit:
    provides = rpmdsFree(provides);

    return result;
}

int rpmdsNVRMatchesDep(const Header h, const rpmds req, int nopromote)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char * pkgN, * v, * r;
    int_32 * epoch;
    const char * pkgEVR;
    char * t;
    int_32 pkgFlags = RPMSENSE_EQUAL;
    rpmds pkg;
    int rc = 1;	/* XXX assume match, names already match here */

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (req->EVR == NULL || req->Flags == NULL)
	return rc;

/*@-boundsread@*/
    if (!((req->Flags[req->i] & RPMSENSE_SENSEMASK) && req->EVR[req->i] && *req->EVR[req->i]))
	return rc;
/*@=boundsread@*/

    /* Get package information from header */
    (void) headerNVR(h, &pkgN, &v, &r);

/*@-boundswrite@*/
    t = alloca(21 + strlen(v) + 1 + strlen(r) + 1);
    pkgEVR = t;
    *t = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(t, "%d:", *epoch);
	while (*t != '\0')
	    t++;
    }
    (void) stpcpy( stpcpy( stpcpy(t, v) , "-") , r);
/*@=boundswrite@*/

    if ((pkg = rpmdsSingle(RPMTAG_PROVIDENAME, pkgN, pkgEVR, pkgFlags)) != NULL) {
	if (nopromote)
	    (void) rpmdsSetNoPromote(pkg, nopromote);
	rc = rpmdsCompare(pkg, req);
	pkg = rpmdsFree(pkg);
    }

    return rc;
}
