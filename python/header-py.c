/** \ingroup py_c
 * \file python/header-py.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include "rpmcli.h"	/* XXX for rpmCheckSig */

#include "legacy.h"
#include "header_internal.h"	/* XXX HEADERFLAG_ALLOCATED */

#include "rpmts.h"	/* XXX rpmtsCreate/rpmtsFree */
#define	_RPMEVR_INTERNAL
#include "rpmevr.h"

#include "header-py.h"
#include "rpmds-py.h"
#include "rpmfi-py.h"

#include "debug.h"

/** \ingroup python
 * \class Rpm
 * \brief START HERE / RPM base module for the Python API
 *
 * The rpm base module provides the main starting point for
 * accessing RPM from Python. For most usage, call
 * the TransactionSet method to get a transaction set (rpmts).
 *
 * For example:
 * \code
 *	import rpm
 *
 *	ts = rpm.TransactionSet()
 * \endcode
 *
 * The transaction set will open the RPM database as needed, so
 * in most cases, you do not need to explicitly open the
 * database. The transaction set is the workhorse of RPM.
 *
 * You can open another RPM database, such as one that holds
 * all packages for a given Linux distribution, to provide
 * packages used to solve dependencies. To do this, use
 * the following code:
 *
 * \code
 * rpm.addMacro('_dbpath', '/path/to/alternate/database')
 * solvets = rpm.TransactionSet()
 * solvets.openDB()
 * rpm.delMacro('_dbpath')
 *
 * # Open default database
 * ts = rpm.TransactionSet()
 * \endcode
 *
 * This code gives you access to two RPM databases through
 * two transaction sets (rpmts): ts is a transaction set
 * associated with the default RPM database and solvets
 * is a transaction set tied to an alternate database, which
 * is very useful for resolving dependencies.
 *
 * The rpm methods used here are:
 *
 * - addMacro(macro, value)
 * @param macro   Name of macro to add
 * @param value   Value for the macro
 *
 * - delMacro(macro)
 * @param macro   Name of macro to delete
 *
 */

/** \ingroup python
 * \class Rpmhdr
 * \brief A python rpm.hdr object represents an RPM package header.
 *
 * All RPM packages have headers that provide metadata for the package.
 * Header objects can be returned by database queries or loaded from a
 * binary package on disk.
 *
 * The ts.hdrFromFdno() function returns the package header from a
 * package on disk, verifying package signatures and digests of the
 * package while reading.
 *
 * Note: The older method rpm.headerFromPackage() which has been replaced
 * by ts.hdrFromFdno() used to return a (hdr, isSource) tuple.
 *
 * If you need to distinguish source/binary headers, do:
 * \code
 * 	import os, rpm
 *
 *	ts = rpm.TranssactionSet()
 * 	fdno = os.open("/tmp/foo-1.0-1.i386.rpm", os.O_RDONLY)
 * 	hdr = ts.hdrFromFdno(fdno)
 *	os.close(fdno)
 *	if hdr[rpm.RPMTAG_SOURCERPM]:
 *	   print "header is from a binary package"
 *	else:
 *	   print "header is from a source package"
 * \endcode
 *
 * The Python interface to the header data is quite elegant.  It
 * presents the data in a dictionary form.  We'll take the header we
 * just loaded and access the data within it:
 * \code
 * 	print hdr[rpm.RPMTAG_NAME]
 * 	print hdr[rpm.RPMTAG_VERSION]
 * 	print hdr[rpm.RPMTAG_RELEASE]
 * \endcode
 * in the case of our "foo-1.0-1.i386.rpm" package, this code would
 * output:
\verbatim
  	foo
  	1.0
  	1
\endverbatim
 *
 * You make also access the header data by string name:
 * \code
 * 	print hdr['name']
 * 	print hdr['version']
 * 	print hdr['release']
 * \endcode
 *
 * This method of access is a teensy bit slower because the name must be
 * translated into the tag number dynamically. You also must make sure
 * the strings in header lookups don't get translated, or the lookups
 * will fail.
 */

/** \ingroup py_c
 */
struct hdrObject_s {
    PyObject_HEAD
    Header h;
    char ** md5list;
    char ** fileList;
    char ** linkList;
    int_32 * fileSizes;
    int_32 * mtimes;
    int_32 * uids, * gids;	/* XXX these tags are not used anymore */
    unsigned short * rdevs;
    unsigned short * modes;
} ;

/**
 */
/*@unused@*/ static inline Header headerAllocated(Header h)
	/*@modifies h @*/
{
    h->flags |= HEADERFLAG_ALLOCATED;
    return 0;
}

#if defined(SUPPORT_RPMV3_BASENAMES_HACKS)
/*@-boundsread@*/
static int dncmp(const void * a, const void * b)
	/*@*/
{
    const char *const * first = a;
    const char *const * second = b;
    return strcmp(*first, *second);
}
/*@=boundsread@*/

/**
 * Convert (dirname,basename,dirindex) tags to absolute path tag.
 * @param h		header
 */
static void expandFilelist(Header h)
        /*@modifies h @*/
{
    HGE_t hge = headerGetExtension;
    HAE_t hae = headerAddExtension;
    HRE_t hre = headerRemoveExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    int xx;

    /*@-branchstate@*/
    if (!headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	he->tag = RPMTAG_FILEPATHS;
	xx = hge(h, he, 0);
	if (he->p.ptr == NULL || he->c <= 0)
	    return;
	he->tag = RPMTAG_OLDFILENAMES;
	xx = hae(h, he, 0);
	he->p.ptr = _free(he->p.ptr);
    }
    /*@=branchstate@*/

    he->tag = RPMTAG_DIRNAMES;
    xx = hre(h, he, 0);
    he->tag = RPMTAG_BASENAMES;
    xx = hre(h, he, 0);
    he->tag = RPMTAG_DIRINDEXES;
    xx = hre(h, he, 0);
}

/*@-bounds@*/
/**
 * Convert absolute path tag to (dirname,basename,dirindex) tags.
 * @param h             header
 */
static void compressFilelist(Header h)
	/*@modifies h @*/
{
    HGE_t hge = headerGetExtension;
    HAE_t hae = headerAddExtension;
    HRE_t hre = headerRemoveExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const char ** fileNames;
    const char ** dirNames;
    const char ** baseNames;
    uint32_t * dirIndexes;
    uint32_t count;
    int dirIndex = -1;
    int xx;
    int i;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	he->tag = RPMTAG_OLDFILENAMES;
	xx = hre(h, he, 0);
	return;		/* Already converted. */
    }

    he->tag = RPMTAG_OLDFILENAMES;
    xx = hge(h, he, 0);
    fileNames = he->p.argv;
    count = he->c;
    if (!xx || fileNames == NULL || count <= 0)
	return;		/* no file list */

    dirNames = alloca(sizeof(*dirNames) * count);	/* worst case */
    baseNames = alloca(sizeof(*dirNames) * count);
    dirIndexes = alloca(sizeof(*dirIndexes) * count);

    if (fileNames[0][0] != '/') {
	/* HACK. Source RPM, so just do things differently */
	dirIndex = 0;
	dirNames[dirIndex] = "";
	for (i = 0; i < count; i++) {
	    dirIndexes[i] = dirIndex;
	    baseNames[i] = fileNames[i];
	}
	goto exit;
    }

    /*@-branchstate@*/
    for (i = 0; i < count; i++) {
	const char ** needle;
	char savechar;
	char * baseName;
	int len;

	if (fileNames[i] == NULL)	/* XXX can't happen */
	    continue;
	baseName = strrchr(fileNames[i], '/') + 1;
	len = baseName - fileNames[i];
	needle = dirNames;
	savechar = *baseName;
	*baseName = '\0';
/*@-compdef@*/
	if (dirIndex < 0 ||
	    (needle = bsearch(&fileNames[i], dirNames, dirIndex + 1, sizeof(dirNames[0]), dncmp)) == NULL) {
	    char *s = alloca(len + 1);
	    memcpy(s, fileNames[i], len + 1);
	    s[len] = '\0';
	    dirIndexes[i] = ++dirIndex;
	    dirNames[dirIndex] = s;
	} else
	    dirIndexes[i] = needle - dirNames;
/*@=compdef@*/

	*baseName = savechar;
	baseNames[i] = baseName;
    }
    /*@=branchstate@*/

exit:
    if (count > 0) {
	he->tag = RPMTAG_DIRINDEXES;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = dirIndexes;
	he->c = count;
	xx = hae(h, he, 0);

	he->tag = RPMTAG_DIRINDEXES;
	he->t = RPM_STRING_ARRAY_TYPE;
	he->p.argv = baseNames;
	he->c = count;
	xx = hae(h, he, 0);

	he->tag = RPMTAG_DIRNAMES;
	he->t = RPM_STRING_ARRAY_TYPE;
	he->p.argv = dirNames;
	he->c = dirIndex + 1;
	xx = hae(h, he, 0);
    }

    fileNames = _free(fileNames);

    he->tag = RPMTAG_OLDFILENAMES;
    xx = hre(h, he, 0);
}
/*@=bounds@*/

/* make a header with _all_ the tags we need */
/**
 */
static void mungeFilelist(Header h)
	/*@*/
{
    HGE_t hge = headerGetExtension;
    HAE_t hae = headerAddExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    int xx;

    if (!headerIsEntry (h, RPMTAG_BASENAMES)
	|| !headerIsEntry (h, RPMTAG_DIRNAMES)
	|| !headerIsEntry (h, RPMTAG_DIRINDEXES))
	compressFilelist(h);

    he->tag = RPMTAG_FILEPATHS;
    xx = hge(h, he, 0);

    if (he->p.ptr == NULL || he->c <= 0)
	return;

    /* XXX Legacy tag needs to go away. */
    he->tag = RPMTAG_OLDFILENAMES;
    xx = hae(h, he, 0);

    he->p.ptr = _free(he->p.ptr);
}
#endif	/* SUPPORT_RPMV3_BASENAMES_HACKS */

#if defined(SUPPORT_RPMV3_PROVIDE_SELF)
/**
 * Retrofit an explicit Provides: N = E:V-R dependency into package headers.
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * @param h             header
 */
static void providePackageNVR(Header h)
{
    HGE_t hge = headerGetExtension;
    HAE_t hae = headerAddExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const char *N, *V, *R;
    uint32_t E;
    int gotE;
    const char *pEVR;
    char *p;
    uint32_t pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    uint32_t * provideFlags = NULL;
    uint32_t providesCount;
    int i, xx;
    int bingo = 1;

    /* Generate provides for this package N-V-R. */
    xx = headerNEVRA(h, &N, NULL, &V, &R, NULL);
    if (!(N && V && R))
	return;
    pEVR = p = alloca(21 + strlen(V) + 1 + strlen(R) + 1);
    *p = '\0';
    he->tag = RPMTAG_EPOCH;
    gotE = hge(h, he, 0);
    E = (he->p.i32p ? *he->p.i32p : 0);
    he->p.ptr = _free(he->p.ptr);
    if (gotE) {
	sprintf(p, "%d:", E);
	p += strlen(p);
    }
    (void) stpcpy( stpcpy( stpcpy(p, V) , "-") , R);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    he->tag = RPMTAG_PROVIDENAME;
    xx = hge(h, he, 0);
    provides = he->p.argv;
    providesCount = he->c;
    if (!xx)
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    he->tag = RPMTAG_PROVIDEVERSION;
    xx = hge(h, he, 0);
    providesEVR = he->p.argv;
    if (!xx) {
	for (i = 0; i < providesCount; i++) {
	    static char vdummy[] = "";
	    static uint32_t fdummy = RPMSENSE_ANY;
	    he->tag = RPMTAG_PROVIDEVERSION;
	    he->t = RPM_STRING_ARRAY_TYPE;
	    he->p.argv = &vdummy;
	    he->c = 1;
	    xx = hae(h, he, 0);
	    he->tag = RPMTAG_PROVIDEFLAGS;
	    he->t = RPM_UINT32_TYPE;
	    he->p.ui32p = &fdummy;
	    he->c = 1;
	    xx = hae(h, he, 0);
	}
	goto exit;
    }

    he->tag = RPMTAG_PROVIDEFLAGS;
    xx = hge(h, he, 0);
    provideFlags = he->p.i32p;

    /*@-nullderef@*/	/* LCL: providesEVR is not NULL */
    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(N, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }
    /*@=nullderef@*/

exit:
    provides = _free(provides);
    providesEVR = _free(providesEVR);
    provideFlags = _free(provideFlags);

    if (bingo) {
	he->tag = RPMTAG_PROVIDENAME;
	he->t = RPM_STRING_ARRAY_TYPE;
	he->p.argv = N;
	he->c = 1;
	he->append = 1;
	xx = hae(h, he, 0);
	he->append = 0;

	he->tag = RPMTAG_PROVIDEVERSION;
	he->t = RPM_STRING_ARRAY_TYPE;
	he->p.argv = pEVR;
	he->c = 1;
	he->append = 1;
	xx = hae(h, he, 0);
	he->append = 0;

	he->tag = RPMTAG_PROVIDEFLAGS;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = pFlags;
	he->c = 1;
	he->append = 1;
	xx = hae(h, he, 0);
	he->append = 0;
    }
}
#endif	/* SUPPORT_RPMV3_PROVIDE_SELF */

/** \ingroup python
 * \name Class: Rpmhdr
 */
/*@{*/

/**
 */
static PyObject * hdrKeyList(hdrObject * s)
	/*@*/
{
    PyObject * list, *o;
    HeaderIterator hi;
    rpmTag tag;
    rpmTagType type;

    list = PyList_New(0);

    hi = headerInitIterator(s->h);
    while (headerNextIterator(hi, &tag, &type, NULL, NULL)) {
        if (tag == HEADER_I18NTABLE) continue;

	switch (type) {
	case RPM_NULL_TYPE:
	case RPM_I18NSTRING_TYPE:
	case RPM_MASK_TYPE:
	    continue;
	    /*@notreached@*/ break;
	case RPM_OPENPGP_TYPE:
	case RPM_ASN1_TYPE:
	case RPM_BIN_TYPE:
	case RPM_UINT64_TYPE:
	case RPM_UINT32_TYPE:
	case RPM_UINT16_TYPE:
	case RPM_UINT8_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_STRING_ARRAY_TYPE:
	case RPM_STRING_TYPE:
	    PyList_Append(list, o=PyInt_FromLong(tag));
	    Py_DECREF(o);
	    break;
	}
    }
    headerFreeIterator(hi);

    return list;
}

/**
 */
static PyObject * hdrUnload(hdrObject * s, PyObject * args, PyObject *keywords)
	/*@*/
{
    char * buf;
    PyObject * rc;
    int legacy = 0;
    int nb;
    Header h;
    static char *kwlist[] = { "legacyHeader", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|i", kwlist, &legacy))
	return NULL;

    h = headerLink(s->h);
    /* XXX this legacy switch is a hack, needs to be removed. */
    if (legacy) {
	h = headerCopy(s->h);	/* XXX strip region tags, etc */
	headerFree(s->h);
    }
    {	size_t len;
	buf = headerUnload(h, &len);
	nb = len;
	nb -= 8;	/* XXX HEADER_MAGIC_NO */
    }
    h = headerFree(h);

    if (buf == NULL || nb == 0) {
	PyErr_SetString(pyrpmError, "can't unload bad header\n");
	return NULL;
    }

    rc = PyString_FromStringAndSize(buf, nb);
    buf = _free(buf);

    return rc;
}

#if defined(SUPPORT_RPMV3_BASENAMES_HACKS)
/**
 */
static PyObject * hdrExpandFilelist(hdrObject * s)
	/*@*/
{
    expandFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * hdrCompressFilelist(hdrObject * s)
	/*@*/
{
    compressFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * hdrFullFilelist(hdrObject * s)
	/*@*/
{
    mungeFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}
#endif	/* SUPPORT_RPMV3_BASENAMES_HACKS */

/**
 */
static PyObject * hdrGetOrigin(hdrObject * s)
	/*@*/
{
    const char * origin = NULL;
    if (s->h != NULL)

	origin = headerGetOrigin(s->h);
    if (origin != NULL)
	return Py_BuildValue("s", origin);
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * hdrSetOrigin(hdrObject * s, PyObject * args, PyObject * kwds)
	/*@*/
{
    char * kwlist[] = {"origin", NULL};
    const char * origin = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:SetOrigin", kwlist, &origin))
        return NULL;

    if (s->h != NULL && origin != NULL)
	headerSetOrigin(s->h, origin);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * hdrSprintf(hdrObject * s, PyObject * args, PyObject * kwds)
	/*@*/
{
    char * fmt;
    char * r;
    errmsg_t err;
    PyObject * result;
    char * kwlist[] = {"format", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &fmt))
	return NULL;

    r = headerSprintf(s->h, fmt, rpmTagTable, rpmHeaderFormats, &err);
    if (!r) {
	PyErr_SetString(pyrpmError, err);
	return NULL;
    }

    result = Py_BuildValue("s", r);
    r = _free(r);

    return result;
}

/*@}*/

/** \ingroup py_c
 */
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef hdr_methods[] = {
    {"keys",		(PyCFunction) hdrKeyList,	METH_NOARGS,
	NULL },
    {"unload",		(PyCFunction) hdrUnload,	METH_VARARGS|METH_KEYWORDS,
	NULL },
#if defined(SUPPORT_RPMV3_BASENAMES_HACKS)
    {"expandFilelist",	(PyCFunction) hdrExpandFilelist,METH_NOARGS,
	NULL },
    {"compressFilelist",(PyCFunction) hdrCompressFilelist,METH_NOARGS,
	NULL },
    {"fullFilelist",	(PyCFunction) hdrFullFilelist,	METH_NOARGS,
	NULL },
#endif	/* SUPPORT_RPMV3_BASENAMES_HACKS */
    {"getorigin",	(PyCFunction) hdrGetOrigin,	METH_NOARGS,
	NULL },
    {"setorigin",	(PyCFunction) hdrSetOrigin,	METH_NOARGS,
	NULL },
    {"sprintf",		(PyCFunction) hdrSprintf,	METH_VARARGS|METH_KEYWORDS,
	NULL },

    {"dsOfHeader",	(PyCFunction)hdr_dsOfHeader,	METH_NOARGS,
	NULL},
    {"dsFromHeader",	(PyCFunction)hdr_dsFromHeader,	METH_VARARGS|METH_KEYWORDS,
	NULL},
    {"fiFromHeader",	(PyCFunction)hdr_fiFromHeader,	METH_VARARGS|METH_KEYWORDS,
	NULL},

    {NULL,		NULL}		/* sentinel */
};

/**
 */
static int hdr_compare(hdrObject * a, hdrObject * b)
	/*@*/
{
    return rpmVersionCompare(a->h, b->h);
}

static long hdr_hash(PyObject * h)
{
    return (long) h;
}

/** \ingroup py_c
 */
static void hdr_dealloc(hdrObject * s)
	/*@*/
{
    if (s->h) headerFree(s->h);
    s->md5list = _free(s->md5list);
    s->fileList = _free(s->fileList);
    s->linkList = _free(s->linkList);
    PyObject_Del(s);
}

/** \ingroup py_c
 */
long tagNumFromPyObject (PyObject *item)
{
    char * str;

    if (PyInt_Check(item)) {
	return PyInt_AsLong(item);
    } else if (PyString_Check(item) || PyUnicode_Check(item)) {
	str = PyString_AsString(item);
	return tagValue(str);
    }
    return -1;
}

/** \ingroup py_c
 */
static PyObject * hdr_subscript(hdrObject * s, PyObject * item)
	/*@*/
{
    HGE_t hge = headerGetExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    int_32 tag = -1;
    int i;
    PyObject * o, * metao;
    int forceArray = 0;
    const struct headerSprintfExtension_s * ext = NULL;
    int xx;

    if (PyCObject_Check (item))
        ext = PyCObject_AsVoidPtr(item);
    else
	tag = tagNumFromPyObject (item);

    if (tag == -1 && (PyString_Check(item) || PyUnicode_Check(item))) {
	const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;
	char * str;
	/* if we still don't have the tag, go looking for the header
	   extensions */
	str = PyString_AsString(item);
	while (extensions->name) {
	    if (extensions->type == HEADER_EXT_TAG
	     && !xstrcasecmp(extensions->name + 7, str)) {
		ext = extensions;
	    }
	    extensions++;
	    if (extensions->type == HEADER_EXT_MORE)
		extensions = extensions->u.more;
	}
    }

    /* Retrieve data from extension or header. */
    if (ext) {
        ext->u.tagFunction(s->h, he);
    } else {
        if (tag == -1) {
            PyErr_SetString(PyExc_KeyError, "unknown header tag");
            return NULL;
        }
        
	he->tag = tag;
	xx = hge(s->h, he, 0);
	if (!xx) {
	    he->p.ptr = _free(he->p.ptr);
	    switch (tag) {
	    case RPMTAG_EPOCH:
	    case RPMTAG_NAME:
	    case RPMTAG_VERSION:
	    case RPMTAG_RELEASE:
	    case RPMTAG_ARCH:
	    case RPMTAG_OS:
		Py_INCREF(Py_None);
		return Py_None;
		break;
	    default:
		return PyList_New(0);
		break;
	    }
	}
    }

    switch (tag) {
    case RPMTAG_FILEPATHS:
    case RPMTAG_ORIGPATHS:
    case RPMTAG_OLDFILENAMES:
    case RPMTAG_FILESIZES:
    case RPMTAG_FILESTATES:
    case RPMTAG_FILEMODES:
    case RPMTAG_FILEUIDS:
    case RPMTAG_FILEGIDS:
    case RPMTAG_FILERDEVS:
    case RPMTAG_FILEMTIMES:
    case RPMTAG_FILEMD5S:
    case RPMTAG_FILELINKTOS:
    case RPMTAG_FILEFLAGS:
    case RPMTAG_ROOT:
    case RPMTAG_FILEUSERNAME:
    case RPMTAG_FILEGROUPNAME:
    case RPMTAG_REQUIRENAME:
    case RPMTAG_REQUIREFLAGS:
    case RPMTAG_REQUIREVERSION:
    case RPMTAG_PROVIDENAME:
    case RPMTAG_PROVIDEFLAGS:
    case RPMTAG_PROVIDEVERSION:
    case RPMTAG_OBSOLETENAME:
    case RPMTAG_OBSOLETEFLAGS:
    case RPMTAG_OBSOLETEVERSION:
    case RPMTAG_CONFLICTNAME:
    case RPMTAG_CONFLICTFLAGS:
    case RPMTAG_CONFLICTVERSION:
    case RPMTAG_CHANGELOGTIME:
    case RPMTAG_FILEVERIFYFLAGS:
	forceArray = 1;
	break;
    default:
        break;
    }

    switch (he->t) {
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:
    case RPM_BIN_TYPE:
	o = PyString_FromStringAndSize(he->p.str, he->c);
	break;

    case RPM_CHAR_TYPE:
    case RPM_UINT8_TYPE:
	if (he->c != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < he->c; i++) {
		o = PyInt_FromLong(he->p.ui8p[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(he->p.ui8p[0]);
	}
	break;

    case RPM_UINT16_TYPE:
	if (he->c != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < he->c; i++) {
		o = PyInt_FromLong(he->p.ui16p[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(he->p.ui16p[0]);
	}
	break;

    case RPM_UINT32_TYPE:
	if (he->c != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < he->c; i++) {
		o = PyInt_FromLong(he->p.ui32p[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(he->p.ui32p[0]);
	}
	break;

    case RPM_UINT64_TYPE:
	if (he->c != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < he->c; i++) {
		o = PyInt_FromLong(he->p.ui64p[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(he->p.ui64p[0]);
	}
	break;

    case RPM_STRING_ARRAY_TYPE:
	metao = PyList_New(0);
	for (i = 0; i < he->c; i++) {
	    o = PyString_FromString(he->p.argv[i]);
	    PyList_Append(metao, o);
	    Py_DECREF(o);
	}
	o = metao;
	break;

    case RPM_STRING_TYPE:
	o = PyString_FromString(he->p.str);
	break;

    default:
	PyErr_SetString(PyExc_TypeError, "unsupported type in header");
	return NULL;
    }
    if (he->freeData)
	he->p.ptr = _free(he->p.ptr);

    return o;
}

/** \ingroup py_c
 */
/*@unchecked@*/ /*@observer@*/
static PyMappingMethods hdr_as_mapping = {
	(inquiry) 0,			/* mp_length */
	(binaryfunc) hdr_subscript,	/* mp_subscript */
	(objobjargproc)0,		/* mp_ass_subscript */
};

static PyObject * hdr_getattro(hdrObject * o, PyObject * n)
	/*@*/
{
    PyObject * res;
    res = PyObject_GenericGetAttr((PyObject *)o, n);
    if (res == NULL)
	res = hdr_subscript(o, n);
    return res;
}

static int hdr_setattro(hdrObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr((PyObject *)o, n, v);
}

/**
 */
static char hdr_doc[] =
"";

/** \ingroup py_c
 */
/*@unchecked@*/ /*@observer@*/
PyTypeObject hdr_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.hdr",			/* tp_name */
	sizeof(hdrObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) hdr_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) 0, 		/* tp_getattr */
	0,				/* tp_setattr */
	(cmpfunc) hdr_compare,		/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,	 			/* tp_as_sequence */
	&hdr_as_mapping,		/* tp_as_mapping */
	hdr_hash,			/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	(getattrofunc) hdr_getattro,	/* tp_getattro */
	(setattrofunc) hdr_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	hdr_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	hdr_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
#endif
};

hdrObject * hdr_Wrap(Header h)
{
    hdrObject * hdr = PyObject_New(hdrObject, &hdr_Type);
    hdr->h = headerLink(h);
    hdr->fileList = hdr->linkList = hdr->md5list = NULL;
    hdr->uids = hdr->gids = hdr->mtimes = hdr->fileSizes = NULL;
    hdr->modes = hdr->rdevs = NULL;
    return hdr;
}

Header hdrGetHeader(hdrObject * s)
{
    return s->h;
}

/**
 */
PyObject * hdrLoad(PyObject * self, PyObject * args, PyObject * kwds)
{
    hdrObject * hdr;
    char * copy = NULL;
    char * obj;
    Header h;
    int len;
    char * kwlist[] = {"headers", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, &obj, &len))
	return NULL;

    /* malloc is needed to avoid surprises from data swab in headerLoad(). */
    copy = malloc(len);
    if (copy == NULL) {
	PyErr_SetString(pyrpmError, "out of memory");
	return NULL;
    }
    memcpy (copy, obj, len);

    h = headerLoad(copy);
    if (!h) {
	PyErr_SetString(pyrpmError, "bad header");
	return NULL;
    }
    headerAllocated(h);
#if defined(SUPPORT_RPMV3_BASENAMES_HACKS)
    compressFilelist (h);
#endif	/* SUPPORT_RPMV3_BASENAMES_HACKS */
#if defined(SUPPORT_RPMV3_PROVIDE_SELF)
    providePackageNVR (h);
#endif	/* SUPPORT_RPMV3_PROVIDE_SELF */

    hdr = hdr_Wrap(h);
    h = headerFree(h);	/* XXX ref held by hdr */

    return (PyObject *) hdr;
}

/**
 */
PyObject * rpmReadHeaders (FD_t fd)
{
    PyObject * list;
    Header h;
    hdrObject * hdr;

    if (!fd) {
	PyErr_SetFromErrno(pyrpmError);
	return NULL;
    }

    list = PyList_New(0);
    Py_BEGIN_ALLOW_THREADS
    h = headerRead(fd);
    Py_END_ALLOW_THREADS

    while (h) {
#if defined(SUPPORT_RPMV3_BASENAMES_HACKS)
	compressFilelist(h);
#endif	/* SUPPORT_RPMV3_BASENAMES_HACKS */
#if defined(SUPPORT_RPMV3_PROVIDE_SELF)
	providePackageNVR(h);
#endif	/* SUPPORT_RPMV3_PROVIDE_SELF */
	hdr = hdr_Wrap(h);
	if (PyList_Append(list, (PyObject *) hdr)) {
	    Py_DECREF(list);
	    Py_DECREF(hdr);
	    return NULL;
	}
	Py_DECREF(hdr);

	h = headerFree(h);	/* XXX ref held by hdr */

	Py_BEGIN_ALLOW_THREADS
	h = headerRead(fd);
	Py_END_ALLOW_THREADS
    }

    return list;
}

/**
 */
PyObject * rpmHeaderFromFD(PyObject * self, PyObject * args, PyObject * kwds)
{
    FD_t fd;
    int fileno;
    PyObject * list;
    char * kwlist[] = {"fd", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &fileno))
	return NULL;

    fd = fdDup(fileno);

    list = rpmReadHeaders (fd);
    Fclose(fd);

    return list;
}

/**
 */
PyObject * rpmHeaderFromFile(PyObject * self, PyObject * args, PyObject *kwds)
{
    char * filespec;
    FD_t fd;
    PyObject * list;
    char * kwlist[] = {"file", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &filespec))
	return NULL;

    fd = Fopen(filespec, "r.fdio");

    if (!fd) {
	PyErr_SetFromErrno(pyrpmError);
	return NULL;
    }

    list = rpmReadHeaders (fd);
    Fclose(fd);

    return list;
}

/**
 * This assumes the order of list matches the order of the new headers, and
 * throws an exception if that isn't true.
 */
int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag)
{
    HGE_t hge = headerGetExtension;
    HAE_t hae = headerAddExtension;
    HRE_t hre = headerRemoveExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    Header h;
    HeaderIterator hi;
    rpmTagData newMatch;
    rpmTagData oldMatch;
    hdrObject * hdr;
    uint32_t count = 0;
    int xx;

    Py_BEGIN_ALLOW_THREADS
    h = headerRead(fd);
    Py_END_ALLOW_THREADS

    while (h) {
	he->tag = matchTag;
	xx = hge(hdr->h, he, 0);
	newMatch.ptr = he->p.ptr;
	if (!xx) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    return 1;
	}

	hdr = (hdrObject *) PyList_GetItem(list, count++);
	if (!hdr) {
	    PyErr_SetString(pyrpmError, "match list item missing");
	    return 1;
	}

	he->tag = matchTag;
	xx = hge(hdr->h, he, 0);
	oldMatch.ptr = he->p.ptr;
	if (!xx) {
	    PyErr_SetString(pyrpmError, "match tag missing in old header");
	    return 1;
	}

	if (*newMatch.ui32p != *oldMatch.ui32p) {
	    PyErr_SetString(pyrpmError, "match tag mismatch");
	    return 1;
	}

	hdr->md5list = _free(hdr->md5list);
	hdr->fileList = _free(hdr->fileList);
	hdr->linkList = _free(hdr->linkList);

	for (hi = headerInitIterator(h);
	    headerNextIterator(hi, &he->tag, &he->t, &he->p, &he->c);
	    he->p.ptr = headerFreeData(he->p.ptr, he->t))
	{
	    /* could be dupes */
	    xx = hre(hdr->h, he, 0);
	    xx = hae(hdr->h, he, 0);
	}

	headerFreeIterator(hi);
	h = headerFree(h);

	Py_BEGIN_ALLOW_THREADS
	h = headerRead(fd);
	Py_END_ALLOW_THREADS
    }

    return 0;
}

PyObject *
rpmMergeHeadersFromFD(PyObject * self, PyObject * args, PyObject * kwds)
{
    FD_t fd;
    int fileno;
    PyObject * list;
    int rc;
    int matchTag;
    char * kwlist[] = {"list", "fd", "matchTag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oii", kwlist, &list,
	    &fileno, &matchTag))
	return NULL;

    if (!PyList_Check(list)) {
	PyErr_SetString(PyExc_TypeError, "first parameter must be a list");
	return NULL;
    }

    fd = fdDup(fileno);

    rc = rpmMergeHeaders (list, fd, matchTag);
    Fclose(fd);

    if (rc) {
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
PyObject *
rpmSingleHeaderFromFD(PyObject * self, PyObject * args, PyObject * kwds)
{
    FD_t fd;
    int fileno;
    off_t offset;
    PyObject * tuple;
    Header h;
    char * kwlist[] = {"fd", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &fileno))
	return NULL;

    offset = lseek(fileno, 0, SEEK_CUR);

    fd = fdDup(fileno);

    if (!fd) {
	PyErr_SetFromErrno(pyrpmError);
	return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    h = headerRead(fd);
    Py_END_ALLOW_THREADS

    Fclose(fd);

    tuple = PyTuple_New(2);

    if (h && tuple) {
	PyTuple_SET_ITEM(tuple, 0, (PyObject *) hdr_Wrap(h));
	PyTuple_SET_ITEM(tuple, 1, PyLong_FromLong(offset));
	h = headerFree(h);
    } else {
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	PyTuple_SET_ITEM(tuple, 0, Py_None);
	PyTuple_SET_ITEM(tuple, 1, Py_None);
    }

    return tuple;
}

/**
 */
PyObject * versionCompare (PyObject * self, PyObject * args, PyObject * kwds)
{
    hdrObject * h1, * h2;
    char * kwlist[] = {"version0", "version1", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O!", kwlist, &hdr_Type,
	    &h1, &hdr_Type, &h2))
	return NULL;

    return Py_BuildValue("i", hdr_compare(h1, h2));
}

PyObject * labelCompare (PyObject * self, PyObject * args)
{
    EVR_t A = memset(alloca(sizeof(*A)), 0, sizeof(*A));
    EVR_t B = memset(alloca(sizeof(*B)), 0, sizeof(*B));
    int rc;

    if (!PyArg_ParseTuple(args, "(zzz)(zzz)",
			&A->E, &A->V, &A->R, &B->E, &B->V, &B->R))
	return NULL;

    if (A->E == NULL)	A->E = "0";
    if (B->E == NULL)	B->E = "0";
    if (A->V == NULL)	A->E = "";
    if (B->V == NULL)	B->E = "";
    if (A->R == NULL)	A->E = "";
    if (B->R == NULL)	B->E = "";

    rc = rpmEVRcompare(A, B);

    return Py_BuildValue("i", rc);
}
