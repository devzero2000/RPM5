/** \ingroup py_c
 * \file python/header-py.c
 */

#include "system-py.h"

#include <rpmio_internal.h>
#include <rpmcb.h>

#include <legacy.h>
#define	_RPMTAG_INTERNAL
#include <rpmtypes.h>
#include <header_internal.h>	/* XXX HEADERFLAG_ALLOCATED */
#define	_RPMEVR_INTERNAL
#include <rpmevr.h>
#include <pkgio.h>		/* XXX rpmpkgRead */

#include <rpmts.h>	/* XXX rpmtsCreate/rpmtsFree */

#include <rpmcli.h>

#include "header-py.h"
#include "rpmds-py.h"
#include "rpmfd-py.h"
#include "rpmfi-py.h"
#define	_RPMTD_INTERNAL
#include <rpmtd-py.h>

#include "debug.h"

#define	RPM_CHAR_TYPE	1

extern int _hdr_debug;
#define	SPEW(_list)	if (_hdr_debug) fprintf _list

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
 *	ts = rpm.TransactionSet()
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
 * This method of access is slower because the name must be translated
 * into the tag number dynamically. You also must make sure the strings
 * in header lookups don't get translated, or the lookups will fail.
 */

struct hdrObject_s {
    PyObject_HEAD
    Header h;
    HeaderIterator hi;
    struct _HE_s he;
};

enum hMagic {
    HEADER_MAGIC_NO             = 0,
    HEADER_MAGIC_YES            = 1
};

static Header headerRead(FD_t fd, int magic)
{
    Header h = NULL;
    const char item[] = "Header";
    const char * msg = NULL;
    rpmRC rc;

    Py_BEGIN_ALLOW_THREADS
    rc = rpmpkgRead(item, fd, &h, &msg);
    Py_END_ALLOW_THREADS

    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOTFOUND:
	break;
    default:
	rpmlog(RPMLOG_ERR, "%s: %s: %s : error code: %d\n", "rpmpkgRead",
		item, msg, rc);
	break;
    }
    msg = _free(msg);
    return h;
}

/** \ingroup python
 * \name Class: Rpmhdr
 */
/*@{*/

static PyObject * hdrKeyList(hdrObject * s)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    PyObject * keys = PyList_New(0);
    HeaderIterator hi;

SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));

    for (hi = headerInit(s->h);
	headerNext(hi, he, 0);
	he->p.ptr = _free(he->p.ptr))
    {
	PyObject * to = PyInt_FromLong(he->tag);
	PyList_Append(keys, to);
	Py_DECREF(to);
    }
    hi = headerFini(hi);

    return keys;
}

static PyObject * hdrUnload(hdrObject * s, PyObject * args, PyObject *keywords)
{
    char * buf;
    PyObject * rc;
    int legacy = 0;
    int nb;
    Header h;
    static char *kwlist[] = { "legacyHeader", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|i", kwlist, &legacy))
	return NULL;

SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));
    h = headerLink(s->h);
    /* XXX this legacy switch is a hack, needs to be removed. */
    if (legacy) {
	h = headerCopy(s->h);	/* XXX strip region tags, etc */
	(void)headerFree(s->h);
	s->h = NULL;
    }
    {	size_t len;
	buf = headerUnload(h, &len);
	nb = len;
	nb -= 8;	/* XXX HEADER_MAGIC_NO */
    }
    (void)headerFree(h);
    h = NULL;

    if (buf == NULL || nb == 0) {
	PyErr_SetString(pyrpmError, "can't unload bad header\n");
	return NULL;
    }

    rc = PyString_FromStringAndSize(buf, nb);
    buf = _free(buf);

    return rc;
}

static PyObject * hdrGetOrigin(hdrObject * s)
{
    const char * origin = NULL;
SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));
    if (s->h != NULL)
	origin = headerGetOrigin(s->h);
    if (origin != NULL)
	return Py_BuildValue("s", origin);
    Py_RETURN_NONE;
}

static PyObject * hdrSetOrigin(hdrObject * s, PyObject * args, PyObject * kwds)
{
    char * kwlist[] = {"origin", NULL};
    const char * origin = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:SetOrigin", kwlist, &origin))
        return NULL;

    if (s->h != NULL && origin != NULL)
	headerSetOrigin(s->h, origin);

    Py_RETURN_NONE;
}

static PyObject * hdrFormat(hdrObject * s, PyObject * args, PyObject * kwds)
{
    char * fmt;
    char * r;
    errmsg_t err;
    PyObject * result;
    char * kwlist[] = {"format", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &fmt))
	return NULL;
SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));

    r = headerSprintf(s->h, fmt, NULL, rpmHeaderFormats, &err);
    if (!r) {
	PyErr_SetString(pyrpmError, err);
	return NULL;
    }

    result = Py_BuildValue("s", r);
    r = _free(r);

    return result;
}

static int headerIsSource(Header h)
{
    return (!headerIsEntry(h, RPMTAG_SOURCERPM));
}

static PyObject *hdrIsSource(hdrObject *s)
{
    return PyBool_FromLong(headerIsSource(s->h));
}

static int hdrContains(hdrObject *s, PyObject *pytag)
{
    rpmTag tag;
    if (!tagNumFromPyObject(pytag, &tag)) return -1;

    return headerIsEntry(s->h, tag);
}

typedef enum headerConvOps_e {
    HEADERCONV_EXPANDFILELIST   = 0,
    HEADERCONV_COMPRESSFILELIST = 1,
    HEADERCONV_RETROFIT_V3      = 2,
} headerConvOps;

static int headerConvert(Header h, headerConvOps op)
{
    int rc = 1;	/* XXX assume success */
    return rc;
}

static PyObject *hdrConvert(hdrObject *self, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = {"op", NULL};
    headerConvOps op = -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &op)) {
	return NULL;
    }
SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, self, self->h));
    return PyBool_FromLong(headerConvert(self->h, op));
}

static int headerWrite(FD_t fd, Header h, enum hMagic magicp)
{
    ssize_t nb;
    size_t length = 0;
    void * uh = headerUnload(h, &length);

    if (uh == NULL)
	return 1;
    switch (magicp) {
    case HEADER_MAGIC_YES:
    {	unsigned char * magic = NULL;
	size_t nmagic = 0;
	(void) headerGetMagic(h, &magic, &nmagic);
	nb = Fwrite(magic, sizeof(*magic), nmagic, fd);
	if (nb != (ssize_t)nmagic)
	    goto exit;
    }	break;
    case HEADER_MAGIC_NO:
	break;
    }

    nb = Fwrite(uh, sizeof(char), length, fd);

exit:
    uh = _free(uh);
    return (nb == (ssize_t)length ? 0 : 1);
}

static PyObject * hdrWrite(hdrObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "file", "magic", NULL };
    int magic = 1;
    rpmfdObject *fdo = NULL;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|i", kwlist,
				     rpmfdFromPyObject, &fdo, &magic))
	return NULL;
SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));

    Py_BEGIN_ALLOW_THREADS;
    rc = headerWrite(rpmfdGetFd(fdo), s->h,
		     magic ? HEADER_MAGIC_YES : HEADER_MAGIC_NO);
    Py_END_ALLOW_THREADS;

    if (rc) PyErr_SetFromErrno(PyExc_IOError);
    Py_XDECREF(fdo); /* avoid messing up errno with file close  */
    if (rc) return NULL;

    Py_RETURN_NONE;
}

/*@}*/

#ifdef	NOTYET
/*
 * Just a backwards-compatibility dummy, the arguments are not looked at
 * or used. TODO: push this over to python side...
 */
static PyObject * hdr_fiFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));
    return PyObject_Call((PyObject *) &rpmfi_Type,
			 Py_BuildValue("(O)", s), NULL);
}

/* Backwards compatibility. Flags argument is just a dummy and discarded. */
static PyObject * hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
    rpmTag tag = RPMTAG_REQUIRENAME;
    rpmsenseFlags flags = 0;
    char * kwlist[] = {"to", "flags", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O&i:dsFromHeader", kwlist,
            tagNumFromPyObject, &tag, &flags))
        return NULL;
SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));

    return PyObject_Call((PyObject *) &rpmds_Type,
			 Py_BuildValue("(Oi)", s, tag), NULL);
}

static PyObject * hdr_dsOfHeader(PyObject * s)
{
SPEW((stderr, "*** %s(%p) h %p\n", __FUNCTION__, s, s->h));
    return PyObject_Call((PyObject *) &rpmds_Type,
			Py_BuildValue("(Oi)", s, RPMTAG_NEVR), NULL);
}
#endif

static long hdr_hash(PyObject * h)
{
    return (long) h;
}

static struct PyMethodDef hdr_methods[] = {
    {"keys",		(PyCFunction) hdrKeyList,	METH_NOARGS,
	NULL },
    {"unload",		(PyCFunction) hdrUnload,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"getorigin",	(PyCFunction) hdrGetOrigin,	METH_NOARGS,
	NULL },
    {"setorigin",	(PyCFunction) hdrSetOrigin,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"convert",		(PyCFunction) hdrConvert,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"format",		(PyCFunction) hdrFormat,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"sprintf",		(PyCFunction) hdrFormat,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"isSource",	(PyCFunction)hdrIsSource,	METH_NOARGS, 
	NULL },
    {"write",		(PyCFunction)hdrWrite,		METH_VARARGS|METH_KEYWORDS,
	NULL },

    {"dsOfHeader",	(PyCFunction)hdr_dsOfHeader,	METH_NOARGS,
	NULL},
    {"dsFromHeader",	(PyCFunction)hdr_dsFromHeader,	METH_VARARGS|METH_KEYWORDS,
	NULL},
    {"fiFromHeader",	(PyCFunction)hdr_fiFromHeader,	METH_VARARGS|METH_KEYWORDS,
	NULL},

    {NULL,		NULL}		/* sentinel */
};

/* TODO: permit keyring check + retrofits on copy/load */
static PyObject *hdr_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
    PyObject *obj = NULL;
    rpmfdObject *fdo = NULL;
    Header h = NULL;
    char *kwlist[] = { "obj", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &obj)) {
	return NULL;
    }
#ifdef	DYING
_hdr_debug = -1;
#endif

    if (obj == NULL) {
	h = headerNew();
    } else if (PyCObject_Check(obj)) {
	h = PyCObject_AsVoidPtr(obj);
    } else if (hdrObject_Check(obj)) {
	h = headerCopy(((hdrObject*) obj)->h);
    } else if (PyBytes_Check(obj)) {
	h = headerCopyLoad(PyBytes_AsString(obj));
    } else if (rpmfdFromPyObject(obj, &fdo)) {
	Py_BEGIN_ALLOW_THREADS;
	h = headerRead(rpmfdGetFd(fdo), HEADER_MAGIC_YES);
	Py_END_ALLOW_THREADS;
	Py_XDECREF(fdo);
    } else {
    	PyErr_SetString(PyExc_TypeError, "header, blob or file expected");
	return NULL;
    }

    if (h == NULL) {
	PyErr_SetString(pyrpmError, "bad header");
	return NULL;
    }
    
    return hdr_Wrap(subtype, h);
}

static void hdr_dealloc(hdrObject * s)
{
    if (s->h) headerFree(s->h);
    s->h = NULL;
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject * hdr_iternext(hdrObject *s)
{
    PyObject *res = NULL;

    if (s->hi == NULL) {
	memset(&s->he, 0, sizeof(s->he));
	s->hi = headerInit(s->h);
    }

    if (headerNext(s->hi, &s->he, 0)) {
	res = PyInt_FromLong(s->he.tag);
	s->he.p.ptr = _free(s->he.p.ptr);	/* XXX return as rpmtd? */
    } else {
	s->hi = headerFini(s->hi);
	memset(&s->he, 0, sizeof(s->he));
    }
    return res;
}

int utf8FromPyObject(PyObject *item, PyObject **str)
{
    PyObject *res = NULL;
    if (PyBytes_Check(item)) {
	Py_XINCREF(item);
	res = item;
    } else if (PyUnicode_Check(item)) {
	res = PyUnicode_AsUTF8String(item);
    }
    if (res == NULL) return 0;

    *str = res;
    return 1;
}

int tagNumFromPyObject (PyObject *item, rpmTag *tagp)
{
    rpmTag tag = RPMTAG_NOT_FOUND;
    PyObject *str = NULL;

    if (PyInt_Check(item)) {
	/* XXX we should probably validate tag numbers too */
	tag = PyInt_AsLong(item);
    } else if (utf8FromPyObject(item, &str)) {
	tag = tagValue(PyBytes_AsString(str));
	Py_DECREF(str);
    } else {
	PyErr_SetString(PyExc_TypeError, "expected a string or integer");
	return 0;
    }
    if (tag == RPMTAG_NOT_FOUND) {
	PyErr_SetString(PyExc_ValueError, "unknown header tag");
	return 0;
    }

    *tagp = tag;
    return 1;
}

static PyObject * hdrGetTag(Header h, rpmTag tag)
{
    PyObject *res = NULL;
    struct rpmtd_s td;

#ifdef	REFERENCE
    (void) headerGet(h, tag, &td, HEADERGET_EXT);
#else
    {	HE_t he = (HE_t) memset(alloca(sizeof(*he)), 0, sizeof(*he));
	int flags = 0;
	(void) rpmtdReset(&td);
	he->tag = tag;
	if (headerGet(h, he, flags))
	    rpmtdSet(&td, he->tag, he->t, he->p.ptr, he->c);
	else	/* XXX W2DO? best effort with avail info */
	    rpmtdSetTag(&td, tag);
    }
#endif

    /* rpmtd_AsPyObj() knows how to handle empty containers and all */
    res = rpmtd_AsPyobj(&td);
    rpmtdFreeData(&td);
    return res;
}

static int validItem(rpmTagClass class, PyObject *item)
{
    int rc;

    switch (class) {
    case RPM_NUMERIC_CLASS:
	rc = (PyLong_Check(item) || PyInt_Check(item));
	break;
    case RPM_STRING_CLASS:
	rc = (PyBytes_Check(item) || PyUnicode_Check(item));
	break;
    case RPM_BINARY_CLASS:
	rc = PyBytes_Check(item);
	break;
    default:
	rc = 0;
	break;
    }
    return rc;
}

static int validData(rpmTag tag, rpmTagType type, PyObject *value)
{
    rpmTagClass class = rpmTagGetClass(tag);
    rpmTagReturnType retype = (type & RPM_MASK_RETURN_TYPE);
    int valid = 1;
    
    if (retype == RPM_SCALAR_RETURN_TYPE) {
	valid = validItem(class, value);
    } else if (retype == RPM_ARRAY_RETURN_TYPE && PyList_Check(value)) {
	/* python lists can contain arbitrary objects, validate each item */
	Py_ssize_t len = PyList_Size(value);
	Py_ssize_t i;
	for (i = 0; i < len; i++) {
	    PyObject *item = PyList_GetItem(value, i);
	    if (!validItem(class, item)) {
		valid = 0;
		break;
	    }
	}
    } else {
	valid = 0;
    }
    return valid;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
static int hdrAppendItem(Header h, rpmTag tag, rpmTagType type, PyObject *item)
{
    HE_t he = (HE_t) memset(alloca(sizeof(*he)), 0, sizeof(*he));
    int rc = 0;

    he->tag = tag;
    he->t = (type & RPM_MASK_TYPE);
    he->append = 1;
    switch (he->t) {
    case RPM_I18NSTRING_TYPE: /* XXX this needs to be handled separately */
#if !defined(SUPPORT_I18NSTRING_TYPE)
#else
assert(0);
#endif
	break;
    case RPM_STRING_TYPE:
    {	PyObject *str = NULL;
	if (utf8FromPyObject(item, &str)) {
	    const char *s = PyBytes_AsString(str);
	    he->p.str = s;
	    he->c = 1;
	    rc = headerPut(h, he, 0);
	}
	Py_XDECREF(str);
    }	break;
    case RPM_STRING_ARRAY_TYPE:
    {	PyObject *str = NULL;
	if (utf8FromPyObject(item, &str)) {
	    const char *s = PyBytes_AsString(str);
	    he->p.argv = &s;
	    he->c = 1;
	    rc = headerPut(h, he, 0);
	}
	Py_XDECREF(str);
    }	break;
    case RPM_BIN_TYPE:
	he->p.blob = (unsigned char *) PyBytes_AsString(item);
	he->c = PyBytes_Size(item);
	rc = headerPut(h, he, 0);
	break;
    case RPM_UINT64_TYPE:
    {	rpmuint64_t val = PyInt_AsUnsignedLongLongMask(item);
	he->p.ui64p = &val;
	he->c = 1;
	rc = headerPut(h, he, 0);
    }	break;
    case RPM_UINT32_TYPE:
    {	rpmuint32_t val = PyInt_AsUnsignedLongMask(item);
	he->p.ui32p = &val;
	he->c = 1;
	rc = headerPut(h, he, 0);
    }	break;
    case RPM_UINT16_TYPE:
    {	rpmuint16_t val = PyInt_AsUnsignedLongMask(item);
	he->p.ui16p = &val;
	he->c = 1;
	rc = headerPut(h, he, 0);
    }	break;
    case RPM_CHAR_TYPE:
    case RPM_UINT8_TYPE:
    {	rpmuint8_t val = PyInt_AsUnsignedLongMask(item);
	he->p.ui8p = &val;
	he->c = 1;
	rc = headerPut(h, he, 0);
    }	break;
    default:
	PyErr_SetString(PyExc_TypeError, "unhandled datatype");
	break;
    }
    return rc;
}
#pragma GCC diagnostic pop

static int hdrPutTag(Header h, rpmTag tag, PyObject *value)
{
    rpmTagType type = rpmTagGetType(tag);
    rpmTagReturnType retype = (type & RPM_MASK_RETURN_TYPE);
    int rc = 0;

    /* XXX this isn't really right (i18n strings etc) but for now ... */
    if (headerIsEntry(h, tag)) {
	PyErr_SetString(PyExc_TypeError, "tag already exists");
	return rc;
    }

    /* validate all data before trying to insert */
    if (!validData(tag, type, value)) { 
	PyErr_SetString(PyExc_TypeError, "invalid type for tag");
	return 0;
    }

    if (retype == RPM_SCALAR_RETURN_TYPE) {
	rc = hdrAppendItem(h, tag, type, value);
    } else if (retype == RPM_ARRAY_RETURN_TYPE && PyList_Check(value)) {
	Py_ssize_t len = PyList_Size(value);
	Py_ssize_t i;
	for (i = 0; i < len; i++) {
	    PyObject *item = PyList_GetItem(value, i);
	    rc = hdrAppendItem(h, tag, type, item);
	}
    } else {
	PyErr_SetString(PyExc_RuntimeError, "cant happen, right?");
    }

    return rc;
}

static PyObject * hdr_subscript(hdrObject * s, PyObject * item)
{
    rpmTag tag;

    if (!tagNumFromPyObject(item, &tag)) return NULL;
    return hdrGetTag(s->h, tag);
}

static int hdr_ass_subscript(hdrObject *s, PyObject *key, PyObject *value)
{
    HE_t he = (HE_t) memset(alloca(sizeof(*he)), 0, sizeof(*he));
    rpmTag tag;
    if (!tagNumFromPyObject(key, &tag)) return -1;

    if (value == NULL) {
	he->tag = tag;
	/* XXX should failure raise key error? */
	headerDel(s->h, he, 0);
    } else if (!hdrPutTag(s->h, tag, value)) {
	return -1;
    }
    return 0;
}

static PyObject * hdr_getattro(hdrObject * s, PyObject * n)
{
    PyObject *res = PyObject_GenericGetAttr((PyObject *) s, n);
    if (res == NULL) {
	rpmTag tag;
	if (tagNumFromPyObject(n, &tag)) {
	    PyErr_Clear();
	    res = hdrGetTag(s->h, tag);
	}
    }
    return res;
}

static int hdr_setattro(PyObject * o, PyObject * n, PyObject * v)
{
    return PyObject_GenericSetAttr(o, n, v);
}

static PyMappingMethods hdr_as_mapping = {
	(lenfunc) 0,			/* mp_length */
	(binaryfunc) hdr_subscript,	/* mp_subscript */
	(objobjargproc)hdr_ass_subscript,/* mp_ass_subscript */
};

static PySequenceMethods hdr_as_sequence = {
    0,				/* sq_length */
    0,				/* sq_concat */
    0,				/* sq_repeat */
    0,				/* sq_item */
    0,				/* sq_slice */
    0,				/* sq_ass_item */
    0,				/* sq_ass_slice */
    (objobjproc)hdrContains,	/* sq_contains */
    0,				/* sq_inplace_concat */
    0,				/* sq_inplace_repeat */
};

static char hdr_doc[] =
"";

PyTypeObject hdr_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.hdr",			/* tp_name */
	sizeof(hdrObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) hdr_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) 0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	&hdr_as_sequence,		/* tp_as_sequence */
	&hdr_as_mapping,		/* tp_as_mapping */
	hdr_hash,			/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	(getattrofunc) hdr_getattro,	/* tp_getattro */
	(setattrofunc) hdr_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	hdr_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc)hdr_iternext,	/* tp_iternext */
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
	hdr_new,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
#endif
};

Header hdrGetHeader(hdrObject * s)
{
    return s->h;
}

PyObject * hdr_Wrap(PyTypeObject *subtype, Header h)
{
    hdrObject * hdr = (hdrObject *) subtype->tp_alloc(subtype, 0);
    if (hdr == NULL) return NULL;
    hdr->h = headerLink(h);
    return (PyObject *) hdr;
}

int hdrFromPyObject(PyObject *item, Header *hptr)
{
    if (hdrObject_Check(item)) {
	*hptr = ((hdrObject *) item)->h;
	return 1;
    } else {
	PyErr_SetString(PyExc_TypeError, "header object expected");
	return 0;
    }
}

/**
 * This assumes the order of list matches the order of the new headers, and
 * throws an exception if that isn't true.
 */
static int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    Header h = NULL;
    uint32_t count = 0;
    int rc = 1; /* assume failure */

    while ((h = headerRead(fd, HEADER_MAGIC_YES)) != NULL) {
	hdrObject * hdr;
	rpmTag newMatch;
	rpmTag oldMatch;
	HeaderIterator hi;

	he->tag = matchTag;
	if (!headerGet(h, he, 0)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    goto exit;
	}
	newMatch = he->tag;
	he->p.ptr = _free(he->p.ptr);

	hdr = (hdrObject *) PyList_GetItem(list, count++);
	if (!hdr) goto exit;

	he->tag = matchTag;
	if (!headerGet(hdr->h, he, 0)) {	/* XXX ... in list header? */
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    goto exit;
	}
	oldMatch = he->tag;
	he->p.ptr = _free(he->p.ptr);

	if (newMatch != oldMatch) {
	    PyErr_SetString(pyrpmError, "match tag mismatch");
	    goto exit;
	}

	for (hi = headerInit(h);
	    headerNext(hi, he, 0);
	    he->p.ptr = _free(he->p.ptr))
	{
	    /* could be dupes */
	    headerDel(hdr->h, he, 0);
	    headerPut(hdr->h, he, 0);
	}
	hi = headerFini(hi);

	h = headerFree(h);
    }
    rc = 0;

exit:
    if (rc)
	h = headerFree(h);
    he->p.ptr = _free(he->p.ptr);
    return rc;
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

    Py_RETURN_NONE;
}

static int hdr_compare(hdrObject * a, hdrObject * b)
{
    return rpmVersionCompare(a->h, b->h);
}

PyObject * versionCompare (PyObject * self, PyObject * args,
		PyObject * kwds)
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
    EVR_t a = rpmEVRnew(RPMSENSE_EQUAL, 1);
    EVR_t b = rpmEVRnew(RPMSENSE_EQUAL, 1);
    int rc;
    PyObject *aTuple, *bTuple;

    if (!PyArg_ParseTuple(args, "OO", &aTuple, &bTuple) ||
	    !PyArg_ParseTuple(aTuple, "zzz|z",
		&a->F[RPMEVR_E], &a->F[RPMEVR_V], &a->F[RPMEVR_R], &a->F[RPMEVR_D]) ||
	    !PyArg_ParseTuple(bTuple, "zzz|z",
		&b->F[RPMEVR_E], &b->F[RPMEVR_V], &b->F[RPMEVR_R], &b->F[RPMEVR_D]))
    {
	a = rpmEVRfree(a);
	b = rpmEVRfree(b);
	return NULL;
    }

    /* XXX HACK: postpone committing to single "missing" value for now. */
    if (a->F[RPMEVR_E] == NULL)	a->F[RPMEVR_E] = "0";
    if (b->F[RPMEVR_E] == NULL)	b->F[RPMEVR_E] = "0";
    if (a->F[RPMEVR_V] == NULL)	a->F[RPMEVR_V] = "";
    if (b->F[RPMEVR_V] == NULL)	b->F[RPMEVR_V] = "";
    if (a->F[RPMEVR_R] == NULL)	a->F[RPMEVR_R] = "";
    if (b->F[RPMEVR_R] == NULL)	b->F[RPMEVR_R] = "";
    if (a->F[RPMEVR_D] == NULL)	a->F[RPMEVR_D] = "";
    if (b->F[RPMEVR_D] == NULL)	b->F[RPMEVR_D] = "";

    rc = rpmEVRcompare(a, b);

    a = rpmEVRfree(a);
    b = rpmEVRfree(b);

    return Py_BuildValue("i", rc);
}

PyObject * evrCompare (PyObject * self, PyObject * args,
		PyObject * kwds)
{
    EVR_t lEVR = NULL;
    EVR_t rEVR = NULL;
    int rc;
    char * evr1, * evr2;
    char * kwlist[] = {"evr0", "evr1", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss", kwlist, &evr1, &evr2))
	return NULL;

    lEVR = rpmEVRnew(RPMSENSE_EQUAL, 0);
    rEVR = rpmEVRnew(RPMSENSE_EQUAL, 0);
    rpmEVRparse(evr1, lEVR);
    rpmEVRparse(evr2, rEVR);
    rc = rpmEVRcompare(lEVR, rEVR);
    lEVR = rpmEVRfree(lEVR);
    rEVR = rpmEVRfree(rEVR);

    return PyLong_FromLong(rc);
}

PyObject * evrSplit (PyObject * self, PyObject * args, PyObject * kwds)
{
    EVR_t EVR = rpmEVRnew(RPMSENSE_EQUAL, 0);
    char * evr;
    char * kwlist[] = {"evr", NULL};
    PyObject * tuple = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &evr))
	goto exit;

    rpmEVRparse(evr, EVR);
    tuple = Py_BuildValue("(Isss)", EVR->F[RPMEVR_E] ? atoi(EVR->F[RPMEVR_E]) : 0, EVR->F[RPMEVR_V], EVR->F[RPMEVR_R], EVR->F[RPMEVR_D]);

exit:
    EVR = rpmEVRfree(EVR);

    return tuple;
}
