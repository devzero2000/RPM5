/**
 * \file rpmdb/tagname.c
 */

#include "system.h"

#include <rpmio_internal.h>	/* XXX DIGEST_CTX, xtolower, xstrcasecmp */
#define	_RPMTAG_INTERNAL
#include <rpmtag.h>
#include "debug.h"

/*@access headerTagTableEntry @*/
/*@access headerTagIndices @*/

/**
 * Compare tag table entries by name.
 * @param *avp		tag table entry a
 * @param *bvp		tag table entry b
 * @return		comparison
 */
static int tagCmpName(const void * avp, const void * bvp)
        /*@*/
{
    headerTagTableEntry a = *(headerTagTableEntry *) avp;
    headerTagTableEntry b = *(headerTagTableEntry *) bvp;
    return strcmp(a->name, b->name);
}

/**
 * Compare tag table entries by value.
 * @param *avp		tag table entry a
 * @param *bvp		tag table entry b
 * @return		comparison
 */
static int tagCmpValue(const void * avp, const void * bvp)
        /*@*/
{
    headerTagTableEntry a = *(headerTagTableEntry *) avp;
    headerTagTableEntry b = *(headerTagTableEntry *) bvp;
    int ret = ((int)a->val - (int)b->val);
    /* Make sure that sort is stable, longest name first. */
    if (ret == 0)
	ret = ((int)strlen(b->name) - (int)strlen(a->name));
    return ret;
}

/**
 * Load/sort a tag index.
 * @retval *ipp		tag index
 * @retval *np		no. of tags
 * @param cmp		sort compare routine
 * @return		0 always
 */
static int tagLoadIndex(headerTagTableEntry ** ipp, int * np,
		int (*cmp) (const void * avp, const void * bvp))
	/*@modifies *ipp, *np @*/
{
    headerTagTableEntry tte, *ip;
    int n = 0;

    ip = xcalloc(rpmTagTableSize, sizeof(*ip));
    n = 0;
/*@-dependenttrans@*/ /*@-observertrans@*/ /*@-castexpose@*/ /*@-mods@*/ /*@-modobserver@*/
    for (tte = rpmTagTable; tte->name != NULL; tte++) {
	ip[n] = tte;
	n++;
    }
assert(n == rpmTagTableSize);
/*@=dependenttrans@*/ /*@=observertrans@*/ /*@=castexpose@*/ /*@=mods@*/ /*@=modobserver@*/

    if (n > 1)
	qsort(ip, n, sizeof(*ip), cmp);
    *ipp = ip;
    *np = n;
    return 0;
}

static char * _tagCanonicalize(const char * s)
	/*@*/
{
    const char * se = s;
    size_t nb = 0;
    char * te;
    char * t;
    int c;

    while ((c = *se++) && xisalpha(c))
	nb++;

    te = t = xmalloc(nb+1);
    if (*s) {
	*te++ = xtoupper(*s++);
	nb--;
    }
    while (nb--)
	*te++ = xtolower(*s++);
    *te = '\0';

    return t;
}

static rpmTag _tagGenerate(const char *s)
	/*@*/
{
    DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
    const char * digest = NULL;
    size_t digestlen = 0;
    size_t nb = strlen(s);
    rpmTag tag = 0;

    rpmDigestUpdate(ctx, s, nb);
    rpmDigestFinal(ctx, &digest, &digestlen, 0);
    if (digest && digestlen > 4) {
	memcpy(&tag, digest + (digestlen - 4), 4);
	tag &= 0x3fffffff;
	tag |= 0x40000000;
    }
    digest = _free(digest);
    return tag;
}

/* forward refs */
static const char * _tagName(rpmTag tag)
	/*@*/;
static unsigned int _tagType(rpmTag tag)
	/*@*/;
static rpmTag _tagValue(const char * tagstr)
	/*@*/;

/*@unchecked@*/
static struct headerTagIndices_s _rpmTags = {
    tagLoadIndex,
    NULL, 0, tagCmpName, _tagValue,
    NULL, 0, tagCmpValue, _tagName, _tagType,
    NULL, _tagCanonicalize, _tagGenerate
};

/*@-compmempass@*/
/*@unchecked@*/
headerTagIndices rpmTags = &_rpmTags;
/*@=compmempass@*/

static const char * _tagName(rpmTag tag)
{
    static char nameBuf[128];	/* XXX yuk */
    headerTagTableEntry t;
    int comparison, i, l, u;
    int xx;
    char *s;

    if (_rpmTags.byValue == NULL)
	xx = tagLoadIndex(&_rpmTags.byValue, &_rpmTags.byValueSize, tagCmpValue);

    switch (tag) {
    case RPMDBI_PACKAGES:
	strcpy(nameBuf, "Packages");
	break;
    case RPMDBI_DEPENDS:
	strcpy(nameBuf, "Depends");
	break;
    case RPMDBI_ADDED:
	strcpy(nameBuf, "Added");
	break;
    case RPMDBI_REMOVED:
	strcpy(nameBuf, "Removed");
	break;
    case RPMDBI_AVAILABLE:
	strcpy(nameBuf, "Available");
	break;
    case RPMDBI_HDLIST:
	strcpy(nameBuf, "Hdlist");
	break;
    case RPMDBI_ARGLIST:
	strcpy(nameBuf, "Arglist");
	break;
    case RPMDBI_FTSWALK:
	strcpy(nameBuf, "Ftswalk");
	break;

    /* XXX make sure rpmdb indices are identically named. */
    case RPMTAG_CONFLICTS:
	strcpy(nameBuf, "Conflictname");
	break;
    case RPMTAG_HDRID:
	strcpy(nameBuf, "Sha1header");
	break;

    default:
	strcpy(nameBuf, "(unknown)");
	if (_rpmTags.byValue == NULL)
	    break;
	l = 0;
	u = _rpmTags.byValueSize;
	while (l < u) {
	    i = (l + u) / 2;
	    t = _rpmTags.byValue[i];
	
	    comparison = ((int)tag - (int)t->val);

	    if (comparison < 0)
		u = i;
	    else if (comparison > 0)
		l = i + 1;
	    else {
		nameBuf[0] = nameBuf[1] = '\0';
		/* Make sure that the bsearch retrieve is stable. */
		while (i > 0 && tag == _rpmTags.byValue[i-1]->val) {
		    i--;
		    t--;
		}
		t = _rpmTags.byValue[i];
		if (t->name != NULL)
		    strcpy(nameBuf, t->name + (sizeof("RPMTAG_")-1));
		for (s = nameBuf+1; *s != '\0'; s++)
		    *s = (char) xtolower((int)*s);
		/*@loopbreak@*/ break;
	    }
	}
	break;
    }
/*@-statictrans@*/
    return nameBuf;
/*@=statictrans@*/
}

static unsigned int _tagType(rpmTag tag)
{
    headerTagTableEntry t;
    int comparison, i, l, u;
    int xx;

    if (_rpmTags.byValue == NULL)
	xx = tagLoadIndex(&_rpmTags.byValue, &_rpmTags.byValueSize, tagCmpValue);

    switch (tag) {
    case RPMDBI_PACKAGES:
    case RPMDBI_DEPENDS:
    case RPMDBI_ADDED:
    case RPMDBI_REMOVED:
    case RPMDBI_AVAILABLE:
    case RPMDBI_HDLIST:
    case RPMDBI_ARGLIST:
    case RPMDBI_FTSWALK:
	break;
    default:
	if (_rpmTags.byValue == NULL)
	    break;
	l = 0;
	u = _rpmTags.byValueSize;
	while (l < u) {
	    i = (l + u) / 2;
	    t = _rpmTags.byValue[i];
	
	    comparison = ((int)tag - (int)t->val);

	    if (comparison < 0)
		u = i;
	    else if (comparison > 0)
		l = i + 1;
	    else {
		/* Make sure that the bsearch retrieve is stable. */
		while (i > 0 && t->val == _rpmTags.byValue[i-1]->val) {
		    i--;
		    t--;
		}
		return t->type;
	    }
	}
	break;
    }
    return 0;
}

static rpmTag _tagValue(const char * tagstr)
{
    headerTagTableEntry t;
    int comparison;
    size_t i, l, u;
    const char * s;
    rpmTag tag;
    int xx;

    if (!xstrcasecmp(tagstr, "Packages"))
	return RPMDBI_PACKAGES;
    if (!xstrcasecmp(tagstr, "Depends"))
	return RPMDBI_DEPENDS;
    if (!xstrcasecmp(tagstr, "Added"))
	return RPMDBI_ADDED;
    if (!xstrcasecmp(tagstr, "Removed"))
	return RPMDBI_REMOVED;
    if (!xstrcasecmp(tagstr, "Available"))
	return RPMDBI_AVAILABLE;
    if (!xstrcasecmp(tagstr, "Hdlist"))
	return RPMDBI_HDLIST;
    if (!xstrcasecmp(tagstr, "Arglist"))
	return RPMDBI_ARGLIST;
    if (!xstrcasecmp(tagstr, "Ftswalk"))
	return RPMDBI_FTSWALK;

    if (_rpmTags.byName == NULL)
	xx = tagLoadIndex(&_rpmTags.byName, &_rpmTags.byNameSize, tagCmpName);
    if (_rpmTags.byName == NULL)
	goto exit;

    l = 0;
    u = _rpmTags.byNameSize;
    while (l < u) {
	i = (l + u) / 2;
	t = _rpmTags.byName[i];
	
	comparison = xstrcasecmp(tagstr, t->name + (sizeof("RPMTAG_")-1));

	if (comparison < 0)
	    u = i;
	else if (comparison > 0)
	    l = i + 1;
	else
	    return t->val;
    }

exit:
    /* Generate an arbitrary tag string. */
    s = _tagCanonicalize(tagstr);
    tag = _tagGenerate(s);
    s = _free(s);
    return tag;
}

const char * tagName(rpmTag tag)
{
    return ((*rpmTags->tagName)(tag));
}

unsigned int tagType(rpmTag tag)
{
    return ((*rpmTags->tagType)(tag));
}

rpmTag tagValue(const char * tagstr)
{
    return ((*rpmTags->tagValue)(tagstr));
}

char * tagCanonicalize(const char * s)
{
    return ((*rpmTags->tagCanonicalize)(s));
}

rpmTag tagGenerate(const char * s)
{
    return ((*rpmTags->tagGenerate)(s));
}

void tagClean(headerTagIndices _rpmTags)
{
    if (_rpmTags == NULL)
	_rpmTags = rpmTags;
   if (_rpmTags) {
	_rpmTags->nameBuf = _free(_rpmTags->nameBuf);
	_rpmTags->byName = _free(_rpmTags->byName);
	_rpmTags->byValue = _free(_rpmTags->byValue);
    }
}

#if defined(SUPPORT_IMPLICIT_TAG_DATA_TYPES)
/**
 * Validate that implicit and explicit types are identical.
 * @param he		tag container
 */
void tagTypeValidate(HE_t he)
{
/* XXX hack around known borkage for now. */
if (!he->signature)
if (!(he->tag == 261 || he->tag == 269))
if ((tagType(he->tag) & 0xffff) != he->t)
fprintf(stderr, "==> warning: tag %u type(0x%x) != implicit type(0x%x)\n", (unsigned) he->tag, he->t, tagType(he->tag));
}
#endif
