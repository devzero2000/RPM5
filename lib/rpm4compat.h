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
#include <stdint.h>
#include <stdio.h>

#define	WITH_DB
#define _RPMPS_INTERNAL
#define _RPMEVR_INTERNAL
#define _RPMTAG_INTERNAL
#include <rpm/rpmio.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmevr.h>
#include <rpm/pkgio.h>
#include <rpm/rpmcb.h>
#include <rpm/rpmts.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmurl.h>

enum hMagic {
	HEADER_MAGIC_NO             = 0,
	HEADER_MAGIC_YES            = 1
};


typedef	uint32_t *	hTAG_t;
typedef	uint32_t *	hTYP_t;
typedef	const void *	hPTR_t;
typedef	uint32_t *	hCNT_t;
typedef	uint32_t	int_32;
typedef	uint32_t	uint_32;
typedef	uint16_t	uint_16;
typedef	uint16_t	int_16;
typedef	uint8_t		int_8;
typedef	uint8_t		byte;

typedef	union hRET_s {
	const void * ptr;
	const char ** argv;
	const char * str;
	uint32_t * ui32p;
	uint16_t * ui16p;
	uint32_t * i32p;
	uint16_t * i16p;
	uint8_t * i8p;
} * hRET_t;

typedef enum pgpVSFlags_e rpmVSFlags_e;

#ifdef __cplusplus
extern "C" {
#endif

static inline int headerGetEntry(Header h, int_32 tag, hTYP_t type, void ** p, hCNT_t c) {
	HE_t he = (HE_s*)memset(alloca(sizeof(*he)), 0, sizeof(*he));
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
	HE_t he = (HE_s*)memset(alloca(sizeof(*he)), 0, sizeof(*he));
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
	HE_t he = (HE_s*)memset(alloca(sizeof(*he)), 0, sizeof(*he));

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
		strcpy(t, dn);
		t += strlen(t);
		t = strcpy(t, baseNames.argv[i]);
		t += strlen(t);
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
	HE_t he = (HE_s*)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	he->tag = (rpmTag)tag;
	he->t = (rpmTagType)type;
	he->p.str = (const char*)p;
	he->c = (rpmTagCount)c;
	return headerPut(h, he, 0);
}

static inline int headerRemoveEntry(Header h, int_32 tag) {
	HE_t he = (HE_s*)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	he->tag = (rpmTag)tag;
	return headerDel(h, he, 0);
}

static inline int headerModifyEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c) {
	HE_t he = (HE_s*)memset(alloca(sizeof(*he)), 0, sizeof(*he));

	he->tag = (rpmTag)tag;
	he->t = (rpmTagType)type;
	he->p.str = (const char*)p;
	he->c = (rpmTagCount)c;
	return headerMod(h, he, 0);

}

static inline int headerNextIterator(HeaderIterator hi, hTAG_t tag, hTYP_t type, hPTR_t * p, hCNT_t c) {
	HE_t he = (HE_s*)memset(alloca(sizeof(*he)), 0, sizeof(*he));
	
	he->tag = *(rpmTag*)tag;
	return headerNext(hi, he, 0);
}

static inline HeaderIterator headerFreeIterator(HeaderIterator hi) {
	return headerFini(hi);
}

static inline HeaderIterator headerInitIterator(Header h){
	return headerInit(h);
}

static inline void * headerFreeData(const void * data, rpmTagType type) {
	if (data)
		free((void *)data);
	return NULL;
}

static inline int headerWrite(void * _fd, Header h, enum hMagic magicp) {
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

static inline Header headerRead(void * _fd, enum hMagic magicp) {
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

static inline int rpmMachineScore(int type, const char * name) {
	char * platform = rpmExpand(name, "-%{_real_vendor}-%{_target_os}%{?_gnu}", NULL);
	int score = rpmPlatformScore(platform, NULL, 0);

	_free(platform);
	return score;
}

rpmRC rpmcliImportPubkey(const rpmts ts, const unsigned char * pkt, ssize_t pktlen) {
	return rpmtsImportPubkey(ts, pkt, pktlen);
}

#ifdef __cplusplus
}

static inline rpmds rpmdsSingle(rpmTag tagN, const char * N, const char * EVR, int_32 Flags){
	return rpmdsSingle(tagN, N, EVR, (evrFlags)Flags);
}
#endif

#endif /* rpm4compat.h */

