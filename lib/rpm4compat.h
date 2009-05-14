/*
 * Copyright © 2008 Per Øyvind Karlsen <peroyvind@mandriva.org>
 *
 * $Id$
 */

#ifndef	H_RPM4COMPAT
#define	H_RPM4COMPAT		1

#define	RPM_NULL_TYPE		0
#define	RPM_CHAR_TYPE		RPM_UINT8_TYPE
#define	RPM_INT8_TYPE 		RPM_UINT8_TYPE
#define	RPM_INT16_TYPE		RPM_UINT16_TYPE
#define	RPM_INT32_TYPE		RPM_UINT32_TYPE

#define	RPMMESS_DEBUG		RPMLOG_DEBUG
#define	RPMMESS_VERBOSE		RPMLOG_INFO
#define	RPMMESS_NORMAL		RPMLOG_NOTICE
#define	RPMMESS_WARNING		RPMLOG_WARNING
#define	RPMMESS_ERROR		RPMLOG_ERR
#define	RPMMESS_FATALERROR	RPMLOG_CRIT
#define	RPMMESS_QUIET		RPMMESS_WARNING

#define	RPMERR_BADSPEC		RPMLOG_ERR

#define	RPMTRANS_FLAG_NOMD5	RPMTRANS_FLAG_NOFDIGESTS
#define	RPMTRANS_FLAG_NOSUGGEST	RPMDEPS_FLAG_NOSUGGEST
#define	RPMTRANS_FLAG_ADDINDEPS	RPMDEPS_FLAG_ADDINDEPS

#define	RPMBUILD_ISSOURCE	RPMFILE_SOURCE
#define	RPMBUILD_ISPATCH	RPMFILE_PATCH
#define	RPMBUILD_ISICON		RPMFILE_ICON
#define	RPMBUILD_ISNO		RPMFILE_MISSINGOK

#define buildRestrictions       sourceHeader

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define _RPMTAG_INTERNAL
#define _RPMEVR_INTERNAL
#define _RPMPS_INTERNAL

#include <rpm/rpmio.h>
#include <rpm/rpmcb.h>
#include <rpm/rpmiotypes.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmpgp.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmevr.h>
#include <rpm/pkgio.h>

#include <rpm/rpmds.h>
#include <rpm/rpmts.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmps.h>

enum hMagic {
	HEADER_MAGIC_NO             = 0,
	HEADER_MAGIC_YES            = 1
};

typedef	rpmuint32_t *	hTAG_t;
typedef	rpmuint32_t *	hTYP_t;
typedef	const void *	hPTR_t;
typedef	rpmuint32_t *	hCNT_t;
typedef	rpmuint32_t	int_32;
typedef	rpmuint32_t	uint_32;
typedef	rpmuint16_t	uint_16;
typedef	rpmuint16_t	int_16;
typedef	rpmuint8_t	int_8;
typedef	rpmuint8_t	byte;

typedef	union hRET_s {
	const void * ptr;
	const char ** argv;
	const char * str;
	rpmuint32_t * ui32p;
	rpmuint16_t * ui16p;
	rpmuint32_t * i32p;
	rpmuint16_t * i16p;
	rpmuint8_t * i8p;
} * hRET_t;

typedef enum pgpVSFlags_e rpmVSFlags_e;

enum rpm_machtable_e {
    RPM_MACHTABLE_INSTARCH      = 0,    /*!< Install platform architecture. */
    RPM_MACHTABLE_INSTOS        = 1,    /*!< Install platform operating system. */
    RPM_MACHTABLE_BUILDARCH     = 2,    /*!< Build platform architecture. */
    RPM_MACHTABLE_BUILDOS       = 3     /*!< Build platform operating system. */
};
#define RPM_MACHTABLE_COUNT     4       /*!< No. of arch/os tables. */

typedef enum urltype_e {
    URL_IS_UNKNOWN      = 0,    /*!< unknown (aka a file) */
    URL_IS_DASH         = 1,    /*!< stdin/stdout */
    URL_IS_PATH         = 2,    /*!< file://... */
    URL_IS_FTP          = 3,    /*!< ftp://... */
    URL_IS_HTTP         = 4,    /*!< http://... */
    URL_IS_HTTPS        = 5,    /*!< https://... */
    URL_IS_HKP          = 6     /*!< hkp://... */
} urltype;

urltype urlPath(const char * url, /*@out@*/ const char ** pathp);

#ifdef __cplusplus
extern "C" {
#endif

static inline int headerGetEntry(Header h, int_32 tag, hTYP_t type, void ** p, hCNT_t c) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));
	int rc;
	
	/* Always ensure to initialize */
	*(void **)p = NULL;
	he->tag = (rpmTag)tag;
	rc = headerGet(h, he, 0);
	if (rc) {
		if (type) *type = he->t;
		if (p) *(void **) p = he->p.ptr;
		if (c) *c = he->c;
	}

	return rc;
}


static inline int headerGetRawEntry(Header h, int_32 tag, hTYP_t type, void * p, hCNT_t c) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));
	int rc;

	he->tag = (rpmTag)tag;
	he->t = *(rpmTagType*)type;
	he->p.str = (const char*)p;
	he->c = *(rpmTagCount*)c;

	rc = headerGet(h, he, tag);

	if (rc) {
		if (type) *type = he->t;
		if (p) *(void **) p = he->p.ptr;
		if (c) *c = he->c;
	}
	
	return rc;
}

static inline void rpmfiBuildFNames(Header h, rpmTag tagN, const char *** fnp, rpmTagCount * fcp) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	rpmTag dirNameTag = (rpmTag)0;
	rpmTag dirIndexesTag = (rpmTag)0;
	rpmTagData baseNames;
	rpmTagData dirNames;
	rpmTagData dirIndexes;
	rpmTagData fileNames;
	rpmTagCount count;
	size_t size;
	char * t;
	unsigned i;
	int xx;

	if (tagN == RPMTAG_BASENAMES) {
		dirNameTag = RPMTAG_DIRNAMES;
		dirIndexesTag = RPMTAG_DIRINDEXES;
	} else if (tagN == RPMTAG_ORIGBASENAMES) {
		dirNameTag = RPMTAG_ORIGDIRNAMES;
		dirIndexesTag = RPMTAG_ORIGDIRINDEXES;
	} else {
		if (fnp) *fnp = NULL;
		if (fcp) *fcp = 0;
		return;
	}
	
	he->tag = tagN;
	xx = headerGet(h, he, 0);
	baseNames.argv = he->p.argv;
	count = he->c;
	
	if (!xx) {
		if (fnp) *fnp = NULL;
		if (fcp) *fcp = 0;
		return;         /* no file list */
	}
	
	he->tag = dirNameTag;
	xx = headerGet(h, he, 0);
	dirNames.argv = he->p.argv;
	
	he->tag = dirIndexesTag;
	xx = headerGet(h, he, 0);
	dirIndexes.ui32p = he->p.ui32p;
	count = he->c;

	size = sizeof(*fileNames.argv) * count;
	for (i = 0; i < (unsigned)count; i++) {
		const char * dn = NULL;
		(void) urlPath(dirNames.argv[dirIndexes.ui32p[i]], &dn);
		size += strlen(baseNames.argv[i]) + strlen(dn) + 1;
	}

	fileNames.argv = (const char**)malloc(size);
	t = (char *)&fileNames.argv[count];
	for (i = 0; i < (unsigned)count; i++) {
		const char * dn = NULL;
		(void) urlPath(dirNames.argv[dirIndexes.ui32p[i]], &dn);
		fileNames.argv[i] = t;
		t = stpcpy( stpcpy(t, dn), baseNames.argv[i]);
		*t++ = '\0';
	}
	baseNames.ptr = _free(baseNames.ptr);
	dirNames.ptr = _free(dirNames.ptr);
	dirIndexes.ptr = _free(dirIndexes.ptr);

	if (fnp)
		*fnp = fileNames.argv;
	else
		fileNames.ptr = _free(fileNames.ptr);
	if (fcp) *fcp = count;
}

static inline int headerAddEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	he->tag = (rpmTag)tag;
	he->t = (rpmTagType)type;
	he->p.str = (const char*)p;
	he->c = (rpmTagCount)c;
	return headerPut(h, he, 0);
}

static inline int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	he->tag = (rpmTag)tag;
	he->t = (rpmTagType)type;
	he->p.str = (const char*)p;
	he->c = (rpmTagCount)c;
	he->append = 1;
	return headerPut(h, he, 0);
}

static inline int headerAppendEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c) {

    return headerAddOrAppendEntry(h, tag, type, p, c);
}

static inline int headerRemoveEntry(Header h, int_32 tag) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	he->tag = (rpmTag)tag;
	return headerDel(h, he, 0);
}

static inline int headerModifyEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	he->tag = (rpmTag)tag;
	he->t = (rpmTagType)type;
	he->p.str = (const char*)p;
	he->c = (rpmTagCount)c;
	return headerMod(h, he, 0);

}

static inline int headerNextIterator(HeaderIterator hi, hTAG_t tag, hTYP_t type, hPTR_t * p, hCNT_t c) {
	HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));
	int rc = headerNext(hi, he, 0);

	if (rc) {
		if (tag) *tag = he->tag;
		if (type) *type = he->t;
		if (p) *(void **) p = he->p.ptr;
		if (c) *c = he->c;
	}
	
	return rc;
}

static inline HeaderIterator headerFreeIterator(HeaderIterator hi) {
	return headerFini(hi);
}

static inline HeaderIterator headerInitIterator(Header h){
	return headerInit(h);
}

static inline void * headerFreeData(const void * data, __attribute__((unused)) rpmTagType type) {
	if (data)
		free((void *)data);
	return NULL;
}

static inline int headerWrite(void * _fd, Header h, __attribute__((unused)) enum hMagic magicp) {
	const char item[] = "Header";
	const char * msg = NULL;
	rpmRC rc = rpmpkgWrite(item, (FD_t)_fd, h, &msg);
	if (rc != RPMRC_OK) {
		rpmlog(RPMLOG_ERR, "%s: %s: %s\n", "headerWrite", item, msg);
		rc = RPMRC_FAIL;
	}
	msg = (const char*)_free(msg);
	return rc;
}

static inline Header headerRead(void * _fd, __attribute__((unused)) enum hMagic magicp) {
	const char item[] = "Header";
	Header h = NULL;
	const char * msg = NULL;
	rpmRC rc = rpmpkgRead(item, (FD_t)_fd, &h, &msg);
	switch (rc) {
		default:
			rpmlog(RPMLOG_ERR, "%s: %s: %s\n", "headerRead", item, msg);
		case RPMRC_NOTFOUND:
			h = NULL;
		case RPMRC_OK:
			break;
	}
	msg = (const char*)_free(msg);
	return h;
}

static inline char * headerFormat(Header h, const char * fmt, errmsg_t * errmsg) {
    return headerSprintf(h, fmt, NULL, NULL, errmsg);
}

static inline int rpmMachineScore(__attribute__((unused)) int type, const char * name) {
	char * platform = rpmExpand(name, "-%{_target_vendor}-%{_target_os}%{?_gnu}", NULL);
	int score = rpmPlatformScore(platform, NULL, 0);

	_free(platform);
	return score;
}

static inline rpmRC rpmtsImportPubkey(const rpmts ts, const unsigned char * pkt, ssize_t pktlen) {
	return rpmcliImportPubkey(ts, pkt, pktlen);
}

static inline rpmuint64_t rpmProblemGetLong(rpmProblem prob){
	return rpmProblemGetDiskNeed(prob);
}

static inline off_t fdSize(FD_t fd){
	struct stat sb;
	Fstat(fd, &sb);
	return sb.st_size;
}

#ifdef __cplusplus
}

static inline rpmds rpmdsSingle(rpmTag tagN, const char * N, const char * EVR, int_32 Flags){
	return rpmdsSingle(tagN, N, EVR, (evrFlags)Flags);
}
#endif

#endif /* rpm4compat.h */

