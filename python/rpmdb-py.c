/** \ingroup py_c
 * \file python/rpmdb-py.c
 */

#include "system-py.h"

#include <rpmio.h>
#include <rpmcb.h>		/* XXX fnpyKey */
#include <rpmtypes.h>
#include <rpmtag.h>

#include "rpmdb-py.h"
#include "rpmmi-py.h"
#include "header-py.h"

#include "debug.h"

/** \ingroup python
 * \class Rpmdb
 * \brief A python rpm.db object represents an RPM database.
 *
 * Instances of the rpmdb object provide access to the records of a
 * RPM database.  The records are accessed by index number.  To
 * retrieve the header data in the RPM database, the rpmdb object is
 * subscripted as you would access members of a list.
 *
 * The rpmdb class contains the following methods:
 *
 * - firstkey()	Returns the index of the first record in the database.
 * deprecated	Use mi = ts.dbMatch() (or db.match()) instead.
 *
 * - nextkey(index) Returns the index of the next record after "index" in the
 * 		database.
 * param index	current rpmdb location
 * deprecated	Use hdr = mi.next() instead.
 *
 * - findbyfile(file) Returns a list of the indexes to records that own file
 * 		"file".
 * param file	absolute path to file
 * deprecated	Use mi = ts.dbMatch('basename') instead.
 *
 * - findbyname(name) Returns a list of the indexes to records for packages
 *		named "name".
 * param name	package name
 * deprecated	Use mi = ts.dbMatch('name') instead.
 *
 * - findbyprovides(dep) Returns a list of the indexes to records for packages
 *		that provide "dep".
 * param dep	provided dependency string
 * deprecated	Use mi = ts.dbMmatch('providename') instead.
 *
 * To obtain a db object explicitly, the opendb function in the rpm module
 * can be called. The opendb function takes two optional arguments.
 * The first optional argument is a boolean flag that specifies if the
 * database is to be opened for read/write access or read-only access.
 * The second argument specifies an alternate root directory for RPM
 * to use.
 *
 * Note that in most cases, one is interested in querying the default
 * database in /var/lib/rpm and an rpm.mi match iterator derived from
 * an implicit open of the database from an rpm.ts transaction set object:
 * \code
 * 	import rpm
 * 	ts = rpm.TransactionSet()
 *	mi = ts.dbMatch()
 *	...
 * \endcode
 * is simpler than explicitly opening a database object:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match()
 * \endcode
 *
 * An example of opening a database and retrieving the first header in
 * the database, then printing the name of the package that the header
 * represents:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match()
 *	if mi:
 *	    h = mi.next()
 *	    if h:
 * 		print h['name']
 * \endcode
 *
 * To print all of the packages in the database that match a package
 * name, the code will look like this:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match('name', "foo")
 * 	while mi:
 *	    h = mi.next()
 *	    if not h:
 *		break
 * 	    print "%s-%s-%s" % (h['name'], h['version'], h['release'])
 * \endcode
 */

struct rpmdbObject_s {
    PyObject_HEAD
    PyObject *md_dict;          /*!< to look like PyModuleObject */
    rpmdb db;
};

/** \ingroup python
 * \name Class: Rpmdb
 */
/*@{*/

static PyObject *
rpmdb_Match (rpmdbObject * s, PyObject * args, PyObject * kwds)
{
    PyObject * mio;
    char *key = NULL;
    int len = 0;
    int tag = RPMDBI_PACKAGES;
    char * kwlist[] = {"tag", "key", "len", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O&zi", kwlist,
	    tagNumFromPyObject, &tag, &key, &len))
	return NULL;

    mio = rpmmi_Wrap(&rpmmi_Type, rpmmiInit(s->db, tag, key, len), (PyObject *)s);
    return mio;
}

/*@}*/

static struct PyMethodDef rpmdb_methods[] = {
    {"match",	    (PyCFunction) rpmdb_Match,	METH_VARARGS|METH_KEYWORDS,
"db.match([TagN, [key, [len]]]) -> mi\n\
- Create an rpm db match iterator.\n" },
    {NULL,		NULL}		/* sentinel */
};

/**
 */
static int
rpmdb_length(rpmdbObject * s)
{
    rpmmi mi;
    int count = 0;

    mi = rpmmiInit(s->db, RPMDBI_PACKAGES, NULL, 0);
    while (rpmmiNext(mi) != NULL)
	count++;
    mi = rpmmiFree(mi);

    return count;
}

static hdrObject *
rpmdb_subscript(rpmdbObject * s, PyObject * key)
{
    int offset;
    hdrObject * ho;
    Header h;
    rpmmi mi;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    offset = (int) PyInt_AsLong(key);

    mi = rpmmiInit(s->db, RPMDBI_PACKAGES, &offset, sizeof(offset));
    if (!(h = rpmmiNext(mi))) {
	mi = rpmmiFree(mi);
	PyErr_SetString(pyrpmError, "cannot read rpmdb entry");
	return NULL;
    }

    ho = (hdrObject *) hdr_Wrap(&hdr_Type, h);
    (void)headerFree(h);
    h = NULL;

    return ho;
}

static PyMappingMethods rpmdb_as_mapping = {
	(lenfunc) rpmdb_length,		/* mp_length */
	(binaryfunc) rpmdb_subscript,	/* mp_subscript */
	(objobjargproc)0,		/* mp_ass_subscript */
};

static void rpmdb_dealloc(rpmdbObject * s)
{
    if (s->db != NULL)
	rpmdbClose(s->db);
    s->db = NULL;
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static char rpmdb_doc[] =
"";

PyTypeObject rpmdb_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.db",			/* tp_name */
	sizeof(rpmdbObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmdb_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmdb_as_mapping,		/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmdb_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmdb_methods,			/* tp_methods */
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

#ifdef  _LEGACY_BINDINGS_TOO
rpmdb dbFromDb(rpmdbObject * db)
{
    return db->db;
}

rpmdbObject *
rpmOpenDB(PyObject * self, PyObject * args, PyObject * kwds) {
    rpmdbObject * o;
    char * root = "";
    int forWrite = 0;
    char * kwlist[] = {"forWrite", "rootdir", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|is", kwlist,
	    &forWrite, &root))
	return NULL;

    o = (rpmdbObject *) Py_TYPE(s)->tp_alloc(subtype, 0);
    o->db = NULL;

    if (rpmdbOpen(root, &o->db, forWrite ? O_RDWR | O_CREAT: O_RDONLY, (mode_t)0644)) {
	char * errmsg = "cannot open database in %s";
	char * errstr = NULL;
	int errsize;

	Py_XDECREF(o);
	/* PyErr_SetString should take varargs... */
	errsize = strlen(errmsg) + *root == '\0' ? 15 /* "/var/lib/rpm" */ : strlen(root);
	errstr = alloca(errsize);
	snprintf(errstr, errsize, errmsg, *root == '\0' ? "/var/lib/rpm" : root);
	PyErr_SetString(pyrpmError, errstr);
	return NULL;
    }

    return o;
}
#endif
