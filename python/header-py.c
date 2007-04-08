/** \ingroup py_c
 * \file python/header-py.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include "rpmcli.h"	/* XXX for rpmCheckSig */

#include "legacy.h"
#include "misc.h"
#include "header_internal.h"

#include "rpmts.h"	/* XXX rpmtsCreate/rpmtsFree */

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
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    const char ** fileNames = NULL;
    int count = 0;
    int xx;

    /*@-branchstate@*/
    if (!headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	rpmfiBuildFNames(h, RPMTAG_BASENAMES, &fileNames, &count);
	if (fileNames == NULL || count <= 0)
	    return;
	xx = hae(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);
	fileNames = _free(fileNames);
    }
    /*@=branchstate@*/

    xx = hre(h, RPMTAG_DIRNAMES);
    xx = hre(h, RPMTAG_BASENAMES);
    xx = hre(h, RPMTAG_DIRINDEXES);
}

/*@-bounds@*/
/**
 * Convert absolute path tag to (dirname,basename,dirindex) tags.
 * @param h             header
 */
static void compressFilelist(Header h)
	/*@modifies h @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    HFD_t hfd = headerFreeData;
    char ** fileNames;
    const char ** dirNames;
    const char ** baseNames;
    int_32 * dirIndexes;
    rpmTagType fnt;
    int count;
    int i, xx;
    int dirIndex = -1;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	xx = hre(h, RPMTAG_OLDFILENAMES);
	return;		/* Already converted. */
    }

    if (!hge(h, RPMTAG_OLDFILENAMES, &fnt, (void **) &fileNames, &count))
	return;		/* no file list */
    if (fileNames == NULL || count <= 0)
	return;

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
	xx = hae(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, dirIndexes, count);
	xx = hae(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			baseNames, count);
	xx = hae(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			dirNames, dirIndex + 1);
    }

    fileNames = hfd(fileNames, fnt);

    xx = hre(h, RPMTAG_OLDFILENAMES);
}
/*@=bounds@*/
/* make a header with _all_ the tags we need */
/**
 */
static void mungeFilelist(Header h)
	/*@*/
{
    const char ** fileNames = NULL;
    int count = 0;

    if (!headerIsEntry (h, RPMTAG_BASENAMES)
	|| !headerIsEntry (h, RPMTAG_DIRNAMES)
	|| !headerIsEntry (h, RPMTAG_DIRINDEXES))
	compressFilelist(h);

    rpmfiBuildFNames(h, RPMTAG_BASENAMES, &fileNames, &count);

    if (fileNames == NULL || count <= 0)
	return;

    /* XXX Legacy tag needs to go away. */
    headerAddEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);

    fileNames = _free(fileNames);
}

/**
 * Retrofit an explicit Provides: N = E:V-R dependency into package headers.
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * @param h             header
 */
static void providePackageNVR(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pEVR;
    char *p;
    int_32 pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    rpmTagType pnt, pvt;
    int_32 * provideFlags = NULL;
    int providesCount;
    int i, xx;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    xx = headerNVR(h, &name, &version, &release);
    if (!(name && version && release))
	return;
    pEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, (void **) &provides, &providesCount))
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt, (void **) &providesEVR, NULL)) {
	for (i = 0; i < providesCount; i++) {
	    char * vdummy = "";
	    int_32 fdummy = RPMSENSE_ANY;
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			&vdummy, 1);
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			&fdummy, 1);
	}
	goto exit;
    }

    xx = hge(h, RPMTAG_PROVIDEFLAGS, NULL, (void **) &provideFlags, NULL);

    /*@-nullderef@*/	/* LCL: providesEVR is not NULL */
    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }
    /*@=nullderef@*/

exit:
    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    if (bingo) {
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
    }
}

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
    int tag, type;

    list = PyList_New(0);

    hi = headerInitIterator(s->h);
    while (headerNextIterator(hi, &tag, &type, NULL, NULL)) {
        if (tag == HEADER_I18NTABLE) continue;

	switch (type) {
	case RPM_OPENPGP_TYPE:
	case RPM_ASN1_TYPE:
	case RPM_BIN_TYPE:
	case RPM_INT64_TYPE:
	case RPM_INT32_TYPE:
	case RPM_INT16_TYPE:
	case RPM_INT8_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_STRING_ARRAY_TYPE:
	case RPM_STRING_TYPE:
	    PyList_Append(list, o=PyInt_FromLong(tag));
	    Py_DECREF(o);
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
    int len, legacy = 0;
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
    len = headerSizeof(h, 0);
    buf = headerUnload(h);
    h = headerFree(h);

    if (buf == NULL || len == 0) {
	PyErr_SetString(pyrpmError, "can't unload bad header\n");
	return NULL;
    }

    rc = PyString_FromStringAndSize(buf, len);
    buf = _free(buf);

    return rc;
}

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
static PyObject * hdrFullFilelist(hdrObject * s)
	/*@*/
{
    mungeFilelist (s->h);

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
    {"expandFilelist",	(PyCFunction) hdrExpandFilelist,METH_NOARGS,
	NULL },
    {"compressFilelist",(PyCFunction) hdrCompressFilelist,METH_NOARGS,
	NULL },
    {"fullFilelist",	(PyCFunction) hdrFullFilelist,	METH_NOARGS,
	NULL },
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

static PyObject * hdr_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int hdr_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
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
    int i;

    if (PyInt_Check(item)) {
	return PyInt_AsLong(item);
    } else if (PyString_Check(item) || PyUnicode_Check(item)) {
	str = PyString_AsString(item);
	for (i = 0; i < rpmTagTableSize; i++)
	    if (!xstrcasecmp(rpmTagTable[i].name + 7, str)) break;
	if (i < rpmTagTableSize) return rpmTagTable[i].val;
    }
    return -1;
}

/** \ingroup py_c
 * Retrieve tag info from header.
 * This is a "dressed" entry to headerGetEntry to do:
 *	1) DIRNAME/BASENAME/DIRINDICES -> FILENAMES tag conversions.
 *	2) i18n lookaside (if enabled).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		0 on success, 1 on bad magic, 2 on error
 */
static int rpmHeaderGetEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
		/*@out@*/ void **p, /*@out@*/ int_32 *c)
	/*@modifies *type, *p, *c @*/
{
    switch (tag) {
    case RPMTAG_OLDFILENAMES:
    {	const char ** fl = NULL;
	int count;
	rpmfiBuildFNames(h, RPMTAG_BASENAMES, &fl, &count);
	if (count > 0) {
	    *p = fl;
	    if (c)	*c = count;
	    if (type)	*type = RPM_STRING_ARRAY_TYPE;
	    return 1;
	}
	if (c)	*c = 0;
	return 0;
    }	/*@notreached@*/ break;

    case RPMTAG_GROUP:
    case RPMTAG_DESCRIPTION:
    case RPMTAG_SUMMARY:
    {	char fmt[128];
	const char * msgstr;
	const char * errstr;

	fmt[0] = '\0';
	(void) stpcpy( stpcpy( stpcpy( fmt, "%{"), tagName(tag)), "}\n");

	/* XXX FIXME: memory leak. */
        msgstr = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	if (msgstr) {
	    *p = (void *) msgstr;
	    if (type)	*type = RPM_STRING_TYPE;
	    if (c)	*c = 1;
	    return 1;
	} else {
	    if (c)	*c = 0;
	    return 0;
	}
    }	/*@notreached@*/ break;

    default:
	return headerGetEntry(h, tag, type, p, c);
	/*@notreached@*/ break;
    }
    /*@notreached@*/
}

/** \ingroup py_c
 */
static PyObject * hdr_subscript(hdrObject * s, PyObject * item)
	/*@*/
{
    int type, count, i, tag = -1;
    void * data;
    PyObject * o, * metao;
    char ** stringArray;
    int forceArray = 0;
    int freeData = 0;
    char * str;
    const struct headerSprintfExtension_s * ext = NULL;
    const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;

    if (PyCObject_Check (item))
        ext = PyCObject_AsVoidPtr(item);
    else
	tag = tagNumFromPyObject (item);
    if (tag == -1 && (PyString_Check(item) || PyUnicode_Check(item))) {
	/* if we still don't have the tag, go looking for the header
	   extensions */
	str = PyString_AsString(item);
	while (extensions->name) {
	    if (extensions->type == HEADER_EXT_TAG
	     && !xstrcasecmp(extensions->name + 7, str)) {
		ext = extensions;
	    }
	    extensions++;
	}
    }

    /* Retrieve data from extension or header. */
    if (ext) {
        ext->u.tagFunction(s->h, &type, (const void **) &data, &count, &freeData);
    } else {
        if (tag == -1) {
            PyErr_SetString(PyExc_KeyError, "unknown header tag");
            return NULL;
        }
        
	if (!rpmHeaderGetEntry(s->h, tag, &type, &data, &count)) {
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
	forceArray = 1;
	break;
    case RPMTAG_SUMMARY:
    case RPMTAG_GROUP:
    case RPMTAG_DESCRIPTION:
	freeData = 1;
	break;
    default:
        break;
    }

    switch (type) {
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:
    case RPM_BIN_TYPE:
	o = PyString_FromStringAndSize(data, count);
	break;

    case RPM_INT64_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((long long *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((long long *) data));
	}
	break;
    case RPM_INT32_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((int *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((int *) data));
	}
	break;

    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((char *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((char *) data));
	}
	break;

    case RPM_INT16_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((short *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((short *) data));
	}
	break;

    case RPM_STRING_ARRAY_TYPE:
	stringArray = data;

	metao = PyList_New(0);
	for (i = 0; i < count; i++) {
	    o = PyString_FromString(stringArray[i]);
	    PyList_Append(metao, o);
	    Py_DECREF(o);
	}
	free (stringArray);
	o = metao;
	break;

    case RPM_STRING_TYPE:
	if (count != 1 || forceArray) {
	    stringArray = data;

	    metao = PyList_New(0);
	    for (i=0; i < count; i++) {
		o = PyString_FromString(stringArray[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyString_FromString(data);
	    if (freeData)
		free (data);
	}
	break;

    default:
	PyErr_SetString(PyExc_TypeError, "unsupported type in header");
	return NULL;
    }

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
    compressFilelist (h);
    providePackageNVR (h);

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
    h = headerRead(fd, HEADER_MAGIC_YES);
    Py_END_ALLOW_THREADS

    while (h) {
	compressFilelist(h);
	providePackageNVR(h);
	hdr = hdr_Wrap(h);
	if (PyList_Append(list, (PyObject *) hdr)) {
	    Py_DECREF(list);
	    Py_DECREF(hdr);
	    return NULL;
	}
	Py_DECREF(hdr);

	h = headerFree(h);	/* XXX ref held by hdr */

	Py_BEGIN_ALLOW_THREADS
	h = headerRead(fd, HEADER_MAGIC_YES);
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
    Header h;
    HeaderIterator hi;
    int_32 * newMatch;
    int_32 * oldMatch;
    hdrObject * hdr;
    int count = 0;
    int type, c, tag;
    void * p;

    Py_BEGIN_ALLOW_THREADS
    h = headerRead(fd, HEADER_MAGIC_YES);
    Py_END_ALLOW_THREADS

    while (h) {
	if (!headerGetEntry(h, matchTag, NULL, (void **) &newMatch, NULL)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    return 1;
	}

	hdr = (hdrObject *) PyList_GetItem(list, count++);
	if (!hdr) return 1;

	if (!headerGetEntry(hdr->h, matchTag, NULL, (void **) &oldMatch, NULL)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    return 1;
	}

	if (*newMatch != *oldMatch) {
	    PyErr_SetString(pyrpmError, "match tag mismatch");
	    return 1;
	}

	hdr->md5list = _free(hdr->md5list);
	hdr->fileList = _free(hdr->fileList);
	hdr->linkList = _free(hdr->linkList);

	for (hi = headerInitIterator(h);
	    headerNextIterator(hi, &tag, &type, (void *) &p, &c);
	    p = headerFreeData(p, type))
	{
	    /* could be dupes */
	    headerRemoveEntry(hdr->h, tag);
	    headerAddEntry(hdr->h, tag, type, p, c);
	}

	headerFreeIterator(hi);
	h = headerFree(h);

	Py_BEGIN_ALLOW_THREADS
	h = headerRead(fd, HEADER_MAGIC_YES);
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
    h = headerRead(fd, HEADER_MAGIC_YES);
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

/**
 */
static int compare_values(const char *str1, const char *str2)
{
    if (!str1 && !str2)
	return 0;
    else if (str1 && !str2)
	return 1;
    else if (!str1 && str2)
	return -1;
    return rpmvercmp(str1, str2);
}

PyObject * labelCompare (PyObject * self, PyObject * args)
{
    char *v1, *r1, *e1, *v2, *r2, *e2;
    int rc;

    if (!PyArg_ParseTuple(args, "(zzz)(zzz)",
			&e1, &v1, &r1, &e2, &v2, &r2))
	return NULL;

    if (e1 == NULL)	e1 = "0";
    if (e2 == NULL)	e2 = "0";

    rc = compare_values(e1, e2);
    if (!rc) {
	rc = compare_values(v1, v2);
	if (!rc)
	    rc = compare_values(r1, r2);
    }
    return Py_BuildValue("i", rc);
}
