/** \ingroup py_c
 * \file python/spec-py.c
 */

#include "system-py.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include "spec-py.h"

#include "debug.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
/** \ingroup python
 * \name Class: Rpmspec
 * \class Rpmspec
 * \brief A python rpm.spec object represents an RPM spec file set.
 * 
 *  The spec file is at the heart of RPM's packaging building process. Similar
 *  in concept to a makefile, it contains information required by RPM to build
 *  the package, as well as instructions telling RPM how to build it. The spec
 *  file also dictates exactly what files are a part of the package, and where
 *  they should be installed.
 *  
 *  The rpm.spec object represents a parsed specfile to aid extraction of data.
 *  
 *  For example
 * \code
 *  import rpm
 *  rpm.addMacro("_topdir","/path/to/topdir")
 *  s=rpm.spec("foo.spec")
 *  print s.prep()
 * \endcode
 *
 *  Macros set using add macro will be used allowing testing of conditional builds
 *
 */
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

/* Header objects are in another module, some hoop jumping required... */
static PyObject *makeHeader(Header h)
{
    PyObject *rpmmod = PyImport_ImportModuleNoBlock("rpm");
    if (rpmmod == NULL) return NULL;

    PyObject *ptr = PyCObject_FromVoidPtr(h, NULL);
    PyObject *hdr = PyObject_CallMethod(rpmmod, "hdr", "(O)", ptr);
    Py_XDECREF(ptr);
    Py_XDECREF(rpmmod);
    return hdr;
}

struct specPkgObject_s {
    PyObject_HEAD
    /*type specific fields */
    Package pkg;
};

static char specPkg_doc[] =
"";

static PyObject * specpkg_get_header(specPkgObject *s, void *closure)
{
    return makeHeader( (s->pkg ? s->pkg->header : NULL) );
}

static PyGetSetDef specpkg_getseters[] = {
    { "header",	(getter) specpkg_get_header, NULL, NULL },
    { NULL } 	/* sentinel */
};

PyTypeObject specPkg_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.specpkg",			/* tp_name */
	sizeof(specPkgObject),		/* tp_size */
	0,				/* tp_itemsize */
	0, 				/* tp_dealloc */
	0,				/* tp_print */
	0, 				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	specPkg_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	specpkg_getseters,		/* tp_getset */
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
};

struct specObject_s {
    PyObject_HEAD
    /*type specific fields */
    Spec spec;
};

static void 
spec_dealloc(specObject * s) 
{
    if (s->spec)
	s->spec = freeSpec(s->spec);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject * 
spec_get_buildroot(specObject * s, void * closure) 
{
#ifdef	REFERENCE
    Spec spec = specFromSpec(s);
    if (spec->buildRoot)
        return Py_BuildValue("s", spec->buildRoot);
    Py_RETURN_NONE;
#else
    Spec spec = specFromSpec(s);
    PyObject * result = NULL;
    const char * buildRootURL = rpmExpand("%{?buildroot}", NULL);
    if (spec != NULL && *buildRootURL)
	result = Py_BuildValue("s", buildRootURL);
    buildRootURL = _free(buildRootURL);
    return result;
#endif
}

static PyObject *
getSection(Spec spec, int section)
{
    rpmiob iob = NULL;;
    const char * str = NULL;

    if (spec == NULL)
	goto exit;

    switch (section) {
#ifdef	NOTYET
    case RPMBUILD_NONE:		iob = spec->parsed;	break;
#endif
    case RPMBUILD_PREP:		iob = spec->prep;	break;
    case RPMBUILD_BUILD:	iob = spec->build;	break;
    case RPMBUILD_INSTALL:	iob = spec->install;	break;
    case RPMBUILD_CHECK:	iob = spec->check;	break;
    case RPMBUILD_CLEAN:	iob = spec->clean;	break;
    }
    if (iob)
	str = rpmiobStr(iob);
    if (str)
	return Py_BuildValue("s", str);

exit:
    Py_RETURN_NONE;
}

static PyObject * 
spec_get_prep(specObject * s) 
{
    return getSection(specFromSpec(s), RPMBUILD_PREP);
}

static PyObject * 
spec_get_build(specObject * s) 
{
    return getSection(specFromSpec(s), RPMBUILD_BUILD);
}

static PyObject * 
spec_get_install(specObject * s) 
{
    return getSection(specFromSpec(s), RPMBUILD_INSTALL);
}

static PyObject * 
spec_get_check(specObject * s) 
{
    return getSection(specFromSpec(s), RPMBUILD_CHECK);
}

static PyObject * 
spec_get_clean(specObject * s) 
{
    return getSection(specFromSpec(s), RPMBUILD_CLEAN);
}

static PyObject *
spec_get_sources(specObject *s)
{
    PyObject *sourceList = PyList_New(0);
    Spec spec = specFromSpec(s);
    struct Source * source;

    if (sourceList == NULL || spec == NULL)
	return NULL;

    for (source = spec->sources; source != NULL; source = source->next) {
	PyObject *srcUrl = Py_BuildValue("(sii)",
				source->fullSource, source->num, source->flags);
	PyList_Append(sourceList, srcUrl);
	Py_DECREF(srcUrl);
    } 

    return sourceList;
}

static PyObject *
spec_get_packages(specObject *s, void *closure)
{
    PyObject *pkgList = PyList_New(0);
    Spec spec = specFromSpec(s);
    Package pkg;

    if (pkgList == NULL || spec == NULL)
	return NULL;

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
        PyObject *po = specPkg_Wrap(&specPkg_Type, pkg);
        PyList_Append(pkgList, po);
        Py_DECREF(po);
    }
    return pkgList;
}

static PyObject *
spec_get_source_header(specObject *s, void *closure)
{
    Spec spec = specFromSpec(s);
    if (spec->sourceHeader == NULL)
        initSourceHeader(spec, NULL);
    return makeHeader(spec->sourceHeader);
}

static char spec_doc[] = "RPM Spec file object";

static PyGetSetDef spec_getseters[] = {
    {"sources",   (getter) spec_get_sources, NULL, NULL },
    {"prep",   (getter) spec_get_prep, NULL, NULL },
    {"build",   (getter) spec_get_build, NULL, NULL },
    {"install",   (getter) spec_get_install, NULL, NULL },
    {"check",   (getter) spec_get_check, NULL, NULL },
    {"clean",   (getter) spec_get_clean, NULL, NULL },
    {"buildRoot",   (getter) spec_get_buildroot, NULL, NULL },
    {"packages", (getter) spec_get_packages, NULL, NULL },
    {"sourceHeader", (getter) spec_get_source_header, NULL, NULL },
    {NULL}  /* Sentinel */
};

static PyObject *
spec_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
    rpmts ts = NULL;
    const char * specfile;
    Spec spec = NULL;
    int recursing = 0;
    char * passPhrase = "";
    char *cookie = NULL;
    int anyarch = 1;
    int verify = 1;
    int force = 1;
    char * kwlist[] = {"specfile", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:spec_new", kwlist,
				     &specfile))
	return NULL;

    ts = rpmtsCreate();
    if (parseSpec(ts, specfile, "/", recursing, passPhrase,
             	  cookie, anyarch, force, verify) == 0)
	spec = rpmtsSpec(ts);
    else
	 PyErr_SetString(PyExc_ValueError, "can't parse specfile\n");
    ts = rpmtsFree(ts);

    return spec ? spec_Wrap(subtype, spec) : NULL;
}

PyTypeObject spec_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "rpm.spec",               /*tp_name*/
    sizeof(specObject),        /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor) spec_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
    spec_doc,                  /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    spec_getseters,            /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    spec_new,                  /* tp_new */
    0,                         /* tp_free */
    0,                         /* tp_is_gc */
};

Spec specFromSpec(specObject *s) 
{
    return s->spec;
}

PyObject *
spec_Wrap(PyTypeObject *subtype, Spec spec) 
{
    specObject * s = (specObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL)
        return NULL;
    s->spec = spec; 
    return (PyObject *)s;
}

PyObject *
specPkg_Wrap(PyTypeObject *subtype, Package pkg) 
{
    specPkgObject * s = (specPkgObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL)
        return NULL;
    s->pkg = pkg; 
    return (PyObject *)s;
}
