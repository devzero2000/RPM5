#include "system.h"

#define	_RPMTD_INTERNAL
#define	_RPMTAG_INTERNAL	/* XXX rpmHeaderFormatFuncByValue() */
#include <rpmtd.h>
#include <rpmpgp.h>

#include "debug.h"

#ifdef __cplusplus
GENfree(rpmtd)
#endif	/* __cplusplus */

#define RPM_NULL_TYPE		0
#define	RPM_CHAR_TYPE		1
#define RPM_INT8_TYPE		RPM_UINT8_TYPE
#define RPM_INT16_TYPE		RPM_UINT16_TYPE
#define RPM_INT32_TYPE		RPM_UINT32_TYPE
#define RPM_INT64_TYPE		RPM_UINT64_TYPE

#define	rpmTagGetType(_tag)	tagType(_tag)

#define	rpmHeaderFormats headerCompoundFormats

rpmTagClass rpmTagTypeGetClass(rpmTagType type)
{
    rpmTagClass class;
    switch (type & RPM_MASK_TYPE) {
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
    case RPM_INT64_TYPE:
	class = RPM_NUMERIC_CLASS;
	break;
    case RPM_I18NSTRING_TYPE:
#if !defined(SUPPORT_I18NSTRING_TYPE)
assert(0);
#endif
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	class = RPM_STRING_CLASS;
	break;
    case RPM_BIN_TYPE:
	class = RPM_BINARY_CLASS;
	break;
    case RPM_NULL_TYPE:
    default:
	class = RPM_NULL_CLASS;
	break;
    }
    return class;
}

rpmTagClass rpmTagGetClass(rpmTag tag)
{
    return rpmTagTypeGetClass(rpmTagGetType(tag));
}

static void * rpmHeaderFormatFuncByName(const char *fmt)
{
    headerSprintfExtension ext;
    void * func = NULL;

    for (ext = rpmHeaderFormats; ext->name != NULL; ext++) {
	if (ext->name == NULL || ext->type != HEADER_EXT_FORMAT)
	    continue;
        if (!strcmp(ext->name, fmt)) {
	    func = (void *) ext->u.fmtFunction;
	    break;
        }
    }
    return func;
}

static struct rpmtdkey_s {
    const char * str;
    rpmtdFormats fmt;
} keyFormats[] = {
    { "armor",		RPMTD_FORMAT_ARMOR },
#ifdef	NOTYET
    { "arraysize",	RPMTD_FORMAT_ARRAYSIZE },
#endif
    { "base64",		RPMTD_FORMAT_BASE64 },
    { "date",		RPMTD_FORMAT_DATE },
    { "day",		RPMTD_FORMAT_DAY },
    { "depflags",	RPMTD_FORMAT_DEPFLAGS },
    { "deptype",	RPMTD_FORMAT_DEPTYPE },
    { "fflags",		RPMTD_FORMAT_FFLAGS },
    { "fstate",		RPMTD_FORMAT_FSTATE },
    { "hex",		RPMTD_FORMAT_HEX },
    { "octal",		RPMTD_FORMAT_OCTAL },
    { "permissions",	RPMTD_FORMAT_PERMS },	/* XXX alias */
    { "perms",		RPMTD_FORMAT_PERMS },
    { "pgpsig",		RPMTD_FORMAT_PGPSIG },
    { "shescape",	RPMTD_FORMAT_SHESCAPE },
#ifdef	NOTYET
    { "string",		RPMTD_FORMAT_STRING },
#endif
    { "triggertype",	RPMTD_FORMAT_TRIGGERTYPE },
    { "vflags",		RPMTD_FORMAT_VFLAGS },
    { "xml",		RPMTD_FORMAT_XML },
};
static size_t nKeyFormats = sizeof(keyFormats) / sizeof(keyFormats[0]);

static const char * fmt2name(rpmtdFormats fmt)
{
    const char * str = NULL;
    int i;

    for (i = 0; i < (int)nKeyFormats; i++) {
	if (fmt != keyFormats[i].fmt)
	    continue;
	str = keyFormats[i].str;
	break;
    }
    return str;
}
 
static void * rpmHeaderFormatFuncByValue(rpmtdFormats fmt)
{
    const char * name = fmt2name(fmt);
    return (name != NULL ? rpmHeaderFormatFuncByName(name) : NULL);
}

rpmtd rpmtdNew(void)
{
    rpmtd td = xmalloc(sizeof(*td));
    td = rpmtdReset(td);
    return td;
}

rpmtd rpmtdFree(rpmtd td)
{
    td = _free(td);
    return NULL;
}

rpmtd rpmtdReset(rpmtd td)
{
    assert(td != NULL);

    memset(td, 0, sizeof(*td));
    td->ix = -1;
    return td;
}

rpmtd rpmtdFreeData(rpmtd td)
{
    assert(td != NULL);

    if (td->flags & RPMTD_ALLOCED) {
	if (td->flags & RPMTD_PTR_ALLOCED) {
	    assert(td->data != NULL);
	    char **data = (char **)td->data;
	    uint32_t i;
	    for (i = 0; i < td->count; i++)
		data[i] = _free(data[i]);
	}
	td->data = _free(td->data);
    }
    td = rpmtdReset(td);
    return td;
}

uint32_t rpmtdCount(rpmtd td)
{
    assert(td != NULL);
    /* fix up for binary type abusing count as data length */
    return (td->type == RPM_BIN_TYPE) ? 1 : td->count;
}

rpmTag rpmtdTag(rpmtd td)
{
    assert(td != NULL);
    return td->tag;
}

rpmTagType rpmtdType(rpmtd td)
{
    assert(td != NULL);
    return td->type;
}

rpmTagClass rpmtdClass(rpmtd td)
{
    assert(td != NULL);
    return rpmTagTypeGetClass(td->type);
}

int rpmtdGetIndex(rpmtd td)
{
    assert(td != NULL);
    return td->ix;
}

int rpmtdSetIndex(rpmtd td, int index)
{
    assert(td != NULL);

    if (index < 0 || index >= (int)rpmtdCount(td))
	return -1;
    td->ix = index;
    return td->ix;
}

int rpmtdInit(rpmtd td)
{
    assert(td != NULL);

    /* XXX check that this is an array type? */
    td->ix = -1;
    return 0;
}

int rpmtdNext(rpmtd td)
{
    int i = -1;

    assert(td != NULL);
    
    if (++td->ix >= 0) {
	if (td->ix < (int)rpmtdCount(td))
	    i = td->ix;
	else
	    td->ix = i;
    }
    return i;
}

uint32_t *rpmtdNextUint32(rpmtd td)
{
    uint32_t *res = NULL;
    assert(td != NULL);
    if (rpmtdNext(td) >= 0)
	res = rpmtdGetUint32(td);
    return res;
}

uint64_t *rpmtdNextUint64(rpmtd td)
{
    uint64_t *res = NULL;
    assert(td != NULL);
    if (rpmtdNext(td) >= 0)
	res = rpmtdGetUint64(td);
    return res;
}

const char *rpmtdNextString(rpmtd td)
{
    const char *res = NULL;
    assert(td != NULL);
    if (rpmtdNext(td) >= 0)
	res = rpmtdGetString(td);
    return res;
}

uint8_t * rpmtdGetUint8(rpmtd td)
{
    uint8_t *res = NULL;

    assert(td != NULL);

    if (td->type == RPM_UINT8_TYPE || td->type == RPM_CHAR_TYPE) {
	int ix = (td->ix >= 0 ? td->ix : 0);
	res = (uint8_t *) td->data + ix;
    } 
    return res;
}
uint16_t * rpmtdGetUint16(rpmtd td)
{
    uint16_t *res = NULL;

    assert(td != NULL);

    if (td->type == RPM_INT16_TYPE) {
	int ix = (td->ix >= 0 ? td->ix : 0);
	res = (uint16_t *) td->data + ix;
    } 
    return res;
}

uint32_t * rpmtdGetUint32(rpmtd td)
{
    uint32_t *res = NULL;

    assert(td != NULL);

    if (td->type == RPM_INT32_TYPE) {
	int ix = (td->ix >= 0 ? td->ix : 0);
	res = (uint32_t *) td->data + ix;
    } 
    return res;
}

uint64_t * rpmtdGetUint64(rpmtd td)
{
    uint64_t *res = NULL;

    assert(td != NULL);

    if (td->type == RPM_INT64_TYPE) {
	int ix = (td->ix >= 0 ? td->ix : 0);
	res = (uint64_t *) td->data + ix;
    } 
    return res;
}

const char * rpmtdGetString(rpmtd td)
{
    const char *str = NULL;

    assert(td != NULL);

    if (td->type == RPM_STRING_TYPE)
	str = (const char *) td->data;
    else if (td->type == RPM_STRING_ARRAY_TYPE ||
	       td->type == RPM_I18NSTRING_TYPE)
    {
	/* XXX TODO: check for array bounds */
	int ix = (td->ix >= 0 ? td->ix : 0);
	str = *((const char**) td->data + ix);
    } 
    return str;
}

uint64_t rpmtdGetNumber(rpmtd td)
{
    uint64_t val = 0;
    int ix = (td->ix >= 0 ? td->ix : 0);
    assert(td != NULL);

    switch (td->type) {
    case RPM_INT64_TYPE:
	val = *((uint64_t *) td->data + ix);
	break;
    case RPM_INT32_TYPE:
	val = *((uint32_t *) td->data + ix);
	break;
    case RPM_INT16_TYPE:
	val = *((uint16_t *) td->data + ix);
	break;
    case RPM_INT8_TYPE:
    case RPM_CHAR_TYPE:
	val = *((uint8_t *) td->data + ix);
	break;
    default:
	break;
    }
    return val;
}

char *rpmtdFormat(rpmtd td, rpmtdFormats fmt, const char *errmsg)
{
    const char *err = NULL;
    char *str = NULL;

    headerTagFormatFunction func = rpmHeaderFormatFuncByValue(fmt);
    if (func) {
	HE_t he = (HE_t) memset(alloca(sizeof(*he)), 0, sizeof(*he));
	he->tag = td->tag;
	he->t = td->type;
	he->p.ptr = td->data;
	he->c = td->count;
	str = func(he, NULL);
    } else
	err = _("Unknown format");
    
    if (err && errmsg)
	errmsg = err;

    return str;
}

int rpmtdSetTag(rpmtd td, rpmTag tag)
{
    assert(td != NULL);
    rpmTagType newtype = rpmTagGetType(tag);
    int rc = 0;

    /* 
     * Sanity checks: 
     * - is the new tag valid at all
     * - if changing tag of non-empty container, require matching type 
     */
    if (newtype == RPM_NULL_TYPE)
	goto exit;

    if (td->data || td->count > 0) {
	if (rpmTagGetType(td->tag) != rpmTagGetType(tag))
	    goto exit;
    } 

    td->tag = tag;
    td->type = newtype & RPM_MASK_TYPE;
    rc = 1;
    
exit:
    return rc;
}

int rpmtdSet(rpmtd td, rpmTag tag, rpmTagType type, 
			    const void * data, uint32_t count)
{
    td = rpmtdReset(td);
    td->tag = tag;
    td->type = type;
    td->count = count;
    /* 
     * Discards const, but we won't touch the data (even rpmtdFreeData()
     * wont free it as allocation flags aren't set) so it's "ok". 
     * XXX: Should there be a separate RPMTD_FOO flag for "user data"?
     */
    td->data = (void *) data;
    return 1;
}

int rpmtdFromUint8(rpmtd td, rpmTag tag, uint8_t *data, uint32_t count)
{
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    rpmTagReturnType retype = rpmTagGetType(tag) & RPM_MASK_RETURN_TYPE;
    
    if (count < 1)
	return 0;

    /*
     * BIN type is really just an uint8_t array internally, it's just
     * treated specially otherwise.
     */
    switch (type) {
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	if (retype != RPM_ARRAY_RETURN_TYPE && count > 1) 
	    return 0;
	/*@fallthrough@*/
    case RPM_BIN_TYPE:
	break;
    default:
	return 0;
    }
    
    return rpmtdSet(td, tag, type, data, count);
}

int rpmtdFromUint16(rpmtd td, rpmTag tag, uint16_t *data, uint32_t count)
{
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    rpmTagReturnType retype = rpmTagGetType(tag) & RPM_MASK_RETURN_TYPE;
    if (type != RPM_INT16_TYPE || count < 1) 
	return 0;
    if (retype != RPM_ARRAY_RETURN_TYPE && count > 1) 
	return 0;
    
    return rpmtdSet(td, tag, type, data, count);
}

int rpmtdFromUint32(rpmtd td, rpmTag tag, uint32_t *data, uint32_t count)
{
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    rpmTagReturnType retype = rpmTagGetType(tag) & RPM_MASK_RETURN_TYPE;
    if (type != RPM_INT32_TYPE || count < 1) 
	return 0;
    if (retype != RPM_ARRAY_RETURN_TYPE && count > 1) 
	return 0;
    
    return rpmtdSet(td, tag, type, data, count);
}

int rpmtdFromUint64(rpmtd td, rpmTag tag, uint64_t *data, uint32_t count)
{
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    rpmTagReturnType retype = rpmTagGetType(tag) & RPM_MASK_RETURN_TYPE;
    if (type != RPM_INT64_TYPE || count < 1) 
	return 0;
    if (retype != RPM_ARRAY_RETURN_TYPE && count > 1) 
	return 0;
    
    return rpmtdSet(td, tag, type, data, count);
}

int rpmtdFromString(rpmtd td, rpmTag tag, const char *data)
{
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    int rc = 0;

    if (type == RPM_STRING_TYPE)
	rc = rpmtdSet(td, tag, type, data, 1);
    else if (type == RPM_STRING_ARRAY_TYPE)
	rc = rpmtdSet(td, tag, type, &data, 1);

    return rc;
}

int rpmtdFromStringArray(rpmtd td, rpmTag tag, const char **data, uint32_t count)
{
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    if (type != RPM_STRING_ARRAY_TYPE || count < 1)
	return 0;
    if (type == RPM_STRING_TYPE && count != 1)
	return 0;

    return rpmtdSet(td, tag, type, data, count);
}

int rpmtdFromArgv(rpmtd td, rpmTag tag, ARGV_t argv)
{
    int count = argvCount(argv);
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;

    if (type != RPM_STRING_ARRAY_TYPE || count < 1)
	return 0;

    return rpmtdSet(td, tag, type, argv, count);
}

int rpmtdFromArgi(rpmtd td, rpmTag tag, ARGI_t argi)
{
    int count = argiCount(argi);
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    rpmTagReturnType retype = rpmTagGetType(tag) & RPM_MASK_RETURN_TYPE;

    if (type != RPM_INT32_TYPE || retype != RPM_ARRAY_RETURN_TYPE || count < 1)
	return 0;

    return rpmtdSet(td, tag, type, argiData(argi), count);
}

rpmtd rpmtdDup(rpmtd td)
{
    rpmtd newtd = NULL;
    char **data = NULL;
    int i;
    
    assert(td != NULL);
    /* TODO: permit other types too */
    if (td->type != RPM_STRING_ARRAY_TYPE && td->type != RPM_I18NSTRING_TYPE)
	return NULL;

    /* deep-copy container and data, drop immutable flag */
    newtd = rpmtdNew();
    memcpy(newtd, td, sizeof(*td));
    newtd->flags &= ~(RPMTD_IMMUTABLE);

    newtd->flags |= (RPMTD_ALLOCED | RPMTD_PTR_ALLOCED);
    newtd->data = data = xmalloc(td->count * sizeof(*data));
    while ((i = rpmtdNext(td)) >= 0)
	data[i] = xstrdup(rpmtdGetString(td));

    return newtd;
}
