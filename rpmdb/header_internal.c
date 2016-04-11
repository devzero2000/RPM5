/** \ingroup header
 * \file rpmdb/header_internal.c
 */

#include "system.h"

#define	_RPMTAG_INTERNAL
#include <header_internal.h>

#include "debug.h"

/**
 * Alignment needs (and sizeof scalars types) for internal rpm data types.
 */
int rpm_typeAlign[16] =  {
    1,	/*!< RPM_NULL_TYPE */
    1,	/*!< RPM_CHAR_TYPE */
    1,	/*!< RPM_UINT8_TYPE */
    2,	/*!< RPM_UINT16_TYPE */
    4,	/*!< RPM_UINT32_TYPE */
    8,	/*!< RPM_UINT64_TYPE */
    1,	/*!< RPM_STRING_TYPE */
    1,	/*!< RPM_BIN_TYPE */
    1,	/*!< RPM_STRING_ARRAY_TYPE */
    1,	/*!< RPM_I18NSTRING_TYPE */
    0,
    0,
    0,
    0,
    0,
    0
};

int headerVerifyInfo(rpmuint32_t il, rpmuint32_t dl, const void * pev, void * iv, int negate)
{
    entryInfo pe = (entryInfo) pev;
    entryInfo info = (entryInfo) iv;
    rpmuint32_t i;
    rpmTag ptag = 0;
    rpmint32_t poff = 0;

    for (i = 0; i < il; i++) {
	info->tag = (rpmTag) ntohl(pe[i].tag);
	info->type = (rpmTagType) ntohl(pe[i].type);
	info->offset = (rpmint32_t) ntohl(pe[i].offset);
	info->count = (rpmuint32_t) ntohl(pe[i].count);

	/* XXX Convert RPMTAG_FILESTATE to RPM_UINT8_TYPE. */
	if (info->tag == 1029 && info->type == 1) {
	    info->type = RPM_UINT8_TYPE;
	}

#ifdef	NOTYET	/* XXX more todo here */
#if !defined(SUPPORT_I18NSTRING_TYPE)
	/* XXX Re-map RPM_I18NSTRING_TYPE -> RPM_STRING_TYPE */
	if (info->type == RPM_I18NSTRING_TYPE)
	    info->type = RPM_STRING_TYPE;
#endif
#endif

	if (!(negate || info->offset >= 0))
	    return (int)i;
	if (negate)
	    info->offset = -info->offset;

	if (i > 0 && ptag > info->tag) {
	    /* Heuristic to determine whether this or previous tag was fubar. */
	    if (ptag > RPMTAG_FIRSTFREE_TAG) {
		i--;
		info->tag = (rpmTag) ntohl(pe[i].tag);
		info->type = (rpmTagType) ntohl(pe[i].type);
		info->offset = (rpmint32_t) ntohl(pe[i].offset);
		info->count = (rpmuint32_t) ntohl(pe[i].count);
	    }
	    return (int)i;
	}
	ptag = info->tag;

	if (hdrchkType(info->type))
	    return (int)i;

	if (hdrchkAlign(info->type, info->offset))
	    return (int)i;
	if (hdrchkRange((rpmint32_t)dl, info->offset))
	    return (int)i;

	if (i > 0 && info->offset >= 0 && poff > info->offset)
	    return (int)i;
	poff = info->offset;

	if (hdrchkData(info->count))
	    return (int)i;

	if (info->count < 1 || info->count > dl)
	    return (int)i;

	switch (info->type) {
	default:
	    break;
	case RPM_STRING_TYPE:
	    if (info->count != 1)	/* XXX reset count to 1? */
		return (int)i;
	    break;
	}
    }
    return -1;
}
