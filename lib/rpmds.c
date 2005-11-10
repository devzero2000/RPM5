/** \ingroup rpmdep
 * \file lib/rpmds.c
 */
#include "system.h"

#include <rpmio_internal.h>	/* XXX fdGetFILE */
#include <rpmlib.h>

#define	_RPMDS_INTERNAL
#include "rpmds.h"

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

/*@unchecked@*/ /*@observer@*/
static const char * beehiveToken = "redhatbuilddependency";

/**
 * Return archScore filter boolean.
 * @param arch		beehive arch score token
 * @returns		0 == false, 1 == true
 */
static int archFilter(const char * arch)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    static int oneshot = 0;
    int negate = 0;	/* assume no negation. */
    int rc = 0;		/* assume arch does not apply */

    if (*arch == '!') {
	negate = 1;
	arch++;
    }
    if (*arch == '=') {
	const char * myarch = NULL;
	arch++;

	if (oneshot <= 0) {
	    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
	    rpmSetMachine(NULL, NULL);
	    oneshot++;
	}
	rpmGetMachine(&myarch, NULL);
	if (myarch != NULL) {
	    if (negate)
		rc = (!strcmp(arch, myarch) ? 0 : 1);
	    else
		rc = (!strcmp(arch, myarch) ? 1 : 0);
/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "=== strcmp(\"%s\", \"%s\") negate %d rc %d\n", arch, myarch, negate, rc);
/*@=modfilesys@*/
	}
    } else {
	int archScore = rpmMachineScore(RPM_MACHTABLE_INSTARCH, arch);
	if (negate)
	    rc = (archScore > 0 ? 0 : 1);
	else
	    rc = (archScore > 0 ? 1 : 0);
/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "=== archScore(\"%s\") %d negate %d rc %d\n", arch, archScore, negate, rc);
/*@=modfilesys@*/
    }
    return rc;
}

/**
 * Filter dependency set, removing "foo(bar,i386,=s390,!sparcv8)" wrapper.
 * @param ds		dependency set
 * @param token		namespace string
 * @returns		filtered dependency set
 */
/*@null@*/
static rpmds rpmdsFilter(/*@null@*/ /*@returned@*/ rpmds ds,
		/*@null@*/ const char * token)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ds, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    size_t toklen;
    rpmds fds;
    int i;

    if (ds == NULL || token == NULL || *token == '\0')
	goto exit;

    toklen = strlen(token);
    fds = rpmdsLink(ds, ds->Type);
    fds = rpmdsInit(ds);
    if (fds != NULL)
    while ((i = rpmdsNext(fds)) >= 0) {
	const char * N = rpmdsN(fds);
	const char * gN;
	const char * f, * fe;
	const char * g, * ge;
	size_t len;
	int ignore;
	int state;
	char buf[1024+1];
	int nb;

	if (N == NULL)
	    continue;
	len = strlen(N);
	if (len < (toklen + (sizeof("()")-1)))
	    continue;
	if (strncmp(N, token, toklen))
	    continue;
	if (*(f = N + toklen) != '(')
	    continue;
	if (*(fe = N + len - 1) != ')')
	    continue;
/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "*** f \"%s\"\n", f);
/*@=modfilesys@*/
	g = f + 1;
	state = 0;
	gN = NULL;
	ignore = 1;	/* assume depedency will be skipped. */
	for (ge = (char *) g; ge < fe; g = ++ge) {
	    while (ge < fe && *ge != ':')
		ge++;

	    nb = (ge - g);
	    if (nb < 0 || nb > (sizeof(buf)-1))
		nb = sizeof(buf) - 1;
	    (void) strncpy(buf, g, nb);
	    buf[nb] = '\0';
/*@-branchstate@*/
	    switch (state) {
	    case 0:		/* g is unwrapped N */
		gN = xstrdup(buf);
		/*@switchbreak@*/ break;
	    default:		/* g is next arch score token. */
		/* arch score tokens are compared assuming || */
		if (archFilter(buf))
		    ignore = 0;
		/*@switchbreak@*/ break;
	    }
/*@=branchstate@*/
	    state++;
	}
	if (ignore) {
	    int Count = rpmdsCount(fds);
/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "***   deleting N[%d:%d] = \"%s\"\n", i, Count, N);
/*@=modfilesys@*/
	    if (i < (Count - 1)) {
		memmove((fds->N + i), (fds->N + i + 1), (Count - (i+1)) * sizeof(*fds->N));
		if (fds->EVR != NULL)
		    memmove((fds->EVR + i), (fds->EVR + i + 1), (Count - (i+1)) * sizeof(*fds->EVR));
		if (fds->Flags != NULL)
		    memmove((fds->Flags + i), (fds->Flags + i + 1), (Count - (i+1)) * sizeof(*fds->Flags));
	 	fds->i--;
	    }
	    fds->Count--;
	} else if (gN != NULL) {
/*@-modobserver -observertrans@*/
	    char * t = (char *) N;
	    (void) strcpy(t, gN);
/*@=modobserver =observertrans@*/
/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "*** unwrapping N[%d] = \"%s\"\n", i, N);
/*@=modfilesys@*/
	}
	gN = _free(gN);
    }
    fds = rpmdsFree(fds);

exit:
    /*@-refcounttrans@*/
    return ds;
    /*@=refcounttrans@*/
}

rpmds rpmdsNew(Header h, rpmTag tagN, int flags)
{
    int scareMem = (flags & 0x1);
    int nofilter = (flags & 0x2);
    HGE_t hge =
	(scareMem ? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry);
    rpmTag tagBT = RPMTAG_BUILDTIME;
    rpmTagType BTt;
    int_32 * BTp;
    rpmTag tagEVR, tagF;
    rpmds ds = NULL;
    const char * Type;
    const char ** N;
    rpmTagType Nt;
    int_32 Count;

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
	Type = "Trigger";
	tagEVR = RPMTAG_TRIGGERVERSION;
	tagF = RPMTAG_TRIGGERFLAGS;
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

	xx = hge(h, tagEVR, &ds->EVRt, (void **) &ds->EVR, NULL);
	xx = hge(h, tagF, &ds->Ft, (void **) &ds->Flags, NULL);
/*@-boundsread@*/
	if (!scareMem && ds->Flags != NULL)
	    ds->Flags = memcpy(xmalloc(ds->Count * sizeof(*ds->Flags)),
                                ds->Flags, ds->Count * sizeof(*ds->Flags));
	xx = hge(h, tagBT, &BTt, (void **) &BTp, NULL);
	ds->BT = (xx && BTp != NULL && BTt == RPM_INT32_TYPE ? *BTp : 0);
/*@=boundsread@*/
	ds->Color = xcalloc(Count, sizeof(*ds->Color));

/*@-modfilesys@*/
if (_rpmds_debug < 0)
fprintf(stderr, "*** ds %p\t%s[%d]\n", ds, ds->Type, ds->Count);
/*@=modfilesys@*/

    }
    /*@=branchstate@*/

exit:
    /*@-nullstate@*/ /* FIX: ds->Flags may be NULL */
    ds = rpmdsLink(ds, (ds ? ds->Type : NULL));
    /*@=nullstate@*/

    if (!nofilter)
	ds = rpmdsFilter(ds, beehiveToken);

    return ds;
}

char * rpmdsNewDNEVR(const char * dspfx, const rpmds ds)
{
    char * tbuf, * t;
    size_t nb;

    nb = 0;
    if (dspfx)	nb += strlen(dspfx) + 1;
/*@-boundsread@*/
    if (ds->N[ds->i])	nb += strlen(ds->N[ds->i]);
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
    if (ds->N[ds->i])
	t = stpcpy(t, ds->N[ds->i]);
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
	Type = "Trigger";
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
    ds->h = NULL;
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
    ds->i = 0;
    {	char pre[2];
/*@-boundsread@*/
	pre[0] = ds->Type[0];
/*@=boundsread@*/
	pre[1] = '\0';
	/*@-nullstate@*/ /* LCL: ds->Type may be NULL ??? */
	ds->DNEVR = rpmdsNewDNEVR(pre, ds);
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
	Type = "Trigger";
    } else
	goto exit;

    ds = xcalloc(1, sizeof(*ds));
    ds->h = NULL;
    ds->Type = Type;
    ds->tagN = tagN;
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
    ds->i = 0;
    {	char t[2];
/*@-boundsread@*/
	t[0] = ds->Type[0];
/*@=boundsread@*/
	t[1] = '\0';
	ds->DNEVR = rpmdsNewDNEVR(t, ds);
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
/*@-boundsread@*/
	if (ds->N != NULL)
	    N = ds->N[ds->i];
/*@=boundsread@*/
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
	    t[0] = ((ds->Type != NULL) ? ds->Type[0] : '\0');
	    t[1] = '\0';
	    /*@-nullstate@*/
	    ds->DNEVR = rpmdsNewDNEVR(t, ds);
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
	return 0;

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
	while (rpmdsNext(ds) < u) {
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
/*@unchecked@*/ /*@observer@*/
static const char * cpuinfo = _PROC_CPUINFO;

int rpmdsCpuinfo(rpmds *dsp, const char * fn)
	/*@globals ctags @*/
	/*@modifies ctags @*/
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

    if (fn == NULL)
	fn = cpuinfo;

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
    { "rpmlib(BuiltinLuaScripts)",    "4.2.2-1",
	(                RPMSENSE_EQUAL),
    N_("internal support for lua scripts.") },
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

#define	_ETC_RPM_SYSINFO	"/etc/rpm/sysinfo"
/*@unchecked@*/ /*@observer@*/
static const char * etcrpmsysinfo = _ETC_RPM_SYSINFO;

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

int rpmdsSysinfo(rpmds *dsp, const char * fn)
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

    if (fn == NULL)
	fn = etcrpmsysinfo;

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
	ds = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR , Flags);
	xx = rpmdsMerge(dsp, ds);
	ds = rpmdsFree(ds);
    }

exit:
/*@-branchstate@*/
    if (fd != NULL) (void) Fclose(fd);
/*@=branchstate@*/
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

int
rpmdsGetconf(rpmds * dsp, const char *path)
{
    const struct conf *c;
    size_t clen;
    long int value;
    const char * NS = "getconf";
    const char *N;
    char * EVR;
    char * t;
    int_32 Flags;

/*@-branchstate@*/
    if (path == NULL)
	path = "/";

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
/*@-unrecog@*/
		if (c->call_name == _SC_UINT_MAX
		|| c->call_name == _SC_ULONG_MAX) {
		    EVR = xmalloc(32);
		    sprintf(EVR, "%lu", value);
		}
/*@=unrecog@*/
	    } else {
		EVR = xmalloc(32);
		sprintf(EVR, "%ld", value);
	    }
	    /*@switchbreak@*/ break;
	case CONFSTR:
	    clen = confstr(c->call_name, (char *) NULL, 0);
	    EVR = xmalloc(clen+1);
	    *EVR = '\0';
	    if (confstr (c->call_name, EVR, clen) != clen)
		error (3, errno, "confstr");
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

#define	_SBIN_LDCONFIG	"/sbin/ldconfig -p"
/*@unchecked@*/ /*@observer@*/
static const char * _sbin_ldconfig_p = _SBIN_LDCONFIG;

#define	_LD_SO_CACHE	"/etc/ld.so.cache"
/*@unchecked@*/ /*@observer@*/
static const char * _ld_so_cache = _LD_SO_CACHE;

int rpmdsLdconfig(rpmds * dsp, const char * fn)
{
    char buf[BUFSIZ];
    const char *N, *EVR;
    int_32 Flags = 0;
    rpmds ds;
    char * f, * fe;
    char * g, * ge;
    char * t;
    FILE * fp = NULL;
    int rc = -1;
    int xx;

    if (fn == NULL)
	fn = _ld_so_cache;

    fp = popen(_sbin_ldconfig_p, "r");
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
	xx = rpmdsMerge(dsp, ds);
	ds = rpmdsFree(ds);
    }
    rc = 0;

exit:
    if (fp != NULL) (void) pclose(fp);
    return rc;
}

int rpmdsUname(rpmds *dsp, const struct utsname * un)
{
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
#if defined(_GNU_SOURCE)
    if (un->domainname != NULL && strcmp(un->domainname, "(none)"))
	rpmdsNSAdd(dsp, NS, "domainname", un->domainname, RPMSENSE_EQUAL);
#endif
    rc = 0;

exit:
    return rc;
}

#define	_PERL_PROVIDES	"/usr/bin/find /usr/lib/perl5 | /usr/lib/rpm/perl.prov"
/*@unchecked@*/ /*@observer@*/
static const char * _perl_provides = _PERL_PROVIDES;

int rpmdsPipe(rpmds * dsp, int_32 tagN, const char * cmd)
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

    if (tagN <= 0)
	tagN = RPMTAG_PROVIDENAME;
    if (cmd == NULL)
	cmd = _perl_provides;

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

/**
 * Split EVR into epoch, version, and release components.
 * @param evr		[epoch:]version[-release] string
 * @retval *ep		pointer to epoch
 * @retval *vp		pointer to version
 * @retval *rp		pointer to release
 */
static
void parseEVR(char * evr,
		/*@exposed@*/ /*@out@*/ const char ** ep,
		/*@exposed@*/ /*@out@*/ const char ** vp,
		/*@exposed@*/ /*@out@*/ const char ** rp)
	/*@modifies *ep, *vp, *rp @*/
	/*@requires maxSet(ep) >= 0 /\ maxSet(vp) >= 0 /\ maxSet(rp) >= 0 @*/
{
    const char *epoch;
    const char *version;		/* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;
    while (*s && xisdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':') {
	epoch = evr;
	*s++ = '\0';
	version = s;
	/*@-branchstate@*/
	if (*epoch == '\0') epoch = "0";
	/*@=branchstate@*/
    } else {
	epoch = NULL;	/* XXX disable epoch compare if missing */
	version = evr;
    }
    if (se) {
/*@-boundswrite@*/
	*se++ = '\0';
/*@=boundswrite@*/
	release = se;
    } else {
	release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}

int rpmdsCompare(const rpmds A, const rpmds B)
{
    const char *aDepend = (A->DNEVR != NULL ? xstrdup(A->DNEVR+2) : "");
    const char *bDepend = (B->DNEVR != NULL ? xstrdup(B->DNEVR+2) : "");
    char *aEVR, *bEVR;
    const char *aE, *aV, *aR, *bE, *bV, *bR;
    int result;
    int sense;

/*@-boundsread@*/
    /* Different names don't overlap. */
    if (strcmp(A->N[A->i], B->N[B->i])) {
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
    aEVR = xstrdup(A->EVR[A->i]);
    parseEVR(aEVR, &aE, &aV, &aR);
    bEVR = xstrdup(B->EVR[B->i]);
    parseEVR(bEVR, &bE, &bV, &bR);
/*@=boundswrite@*/

    /* Compare {A,B} [epoch:]version[-release] */
    sense = 0;
    if (aE && *aE && bE && *bE)
	sense = rpmvercmp(aE, bE);
    else if (aE && *aE && atol(aE) > 0) {
	if (!B->nopromote) {
	    int lvl = (_rpmds_unspecified_epoch_noise  ? RPMMESS_WARNING : RPMMESS_DEBUG);
	    rpmMessage(lvl, _("The \"B\" dependency needs an epoch (assuming same epoch as \"A\")\n\tA = \"%s\"\tB = \"%s\"\n"),
		aDepend, bDepend);
	    sense = 0;
	} else
	    sense = 1;
    } else if (bE && *bE && atol(bE) > 0)
	sense = -1;

    if (sense == 0) {
	sense = rpmvercmp(aV, bV);
	if (sense == 0 && aR && *aR && bR && *bR)
	    sense = rpmvercmp(aR, bR);
    }
/*@=boundsread@*/
    aEVR = _free(aEVR);
    bEVR = _free(bEVR);

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

/*@-boundsread@*/
    if (!(req->Flags[req->i] & RPMSENSE_SENSEMASK) || !req->EVR[req->i] || *req->EVR[req->i] == '\0')
	return 1;
/*@=boundsread@*/

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

    result = 0;
    if (provides != NULL)
    while (rpmdsNext(provides) >= 0) {

	/* Filter out provides that came along for the ride. */
/*@-boundsread@*/
	if (strcmp(provides->N[provides->i], req->N[req->i]))
	    continue;
/*@=boundsread@*/

	result = rpmdsCompare(provides, req);

	/* If this provide matches the require, we're done. */
	if (result)
	    break;
    }

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
