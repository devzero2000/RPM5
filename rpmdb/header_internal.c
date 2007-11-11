/*@-sizeoftype@*/
/** \ingroup header
 * \file rpmdb/header_internal.c
 */

#include "system.h"

#include <header_internal.h>

#include "debug.h"

/**
 * Alignment needs (and sizeof scalars types) for internal rpm data types.
 */
/*@observer@*/ /*@unchecked@*/
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

int headerVerifyInfo(int il, int dl, const void * pev, void * iv, int negate)
{
/*@-castexpose@*/
    entryInfo pe = (entryInfo) pev;
/*@=castexpose@*/
    entryInfo info = iv;
    int i;

    for (i = 0; i < il; i++) {
	info->tag = ntohl(pe[i].tag);
	info->type = ntohl(pe[i].type);
	info->offset = ntohl(pe[i].offset);
	if (negate)
	    info->offset = -info->offset;
	info->count = ntohl(pe[i].count);

	if (hdrchkType(info->type))
	    return i;
	if (hdrchkAlign(info->type, info->offset))
	    return i;
	if (!negate && hdrchkRange(dl, info->offset))
	    return i;
	if (hdrchkData(info->count))
	    return i;

    }
    return -1;
}
/*@=sizeoftype@*/
