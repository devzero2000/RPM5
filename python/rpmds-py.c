/** \ingroup py_c
 * \file python/rpmds-py.c
 */

#include "system-py.h"

#include <rpmio.h>
#include <rpmiotypes.h>		/* XXX fnpyKey */
#include <rpmtypes.h>
#include <rpmtag.h>

#include "header-py.h"
#include "rpmds-py.h"

#include "debug.h"

struct rpmdsObject_s {
    PyObject_HEAD
    PyObject *md_dict;          /*!< to look like PyModuleObject */
    int		active;
    rpmds	ds;
};

#ifdef	DYING
/**
 * Split EVR into epoch, version, and release components.
 * @param evr		[epoch:]version[-release] string
 * @retval *ep		pointer to epoch
 * @retval *vp		pointer to version
 * @retval *rp		pointer to release
 */
static
void rpmds_ParseEVR(char * evr,
		const char ** ep,
		const char ** vp,
		const char ** rp)
{
    const char *epoch;
    const char *version;		/* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;
    while (*s && xisdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':') {
	epoch = evr;
	*s++ = '\0';
	version = s;
	if (*epoch == '\0') epoch = "0";
    } else {
	epoch = NULL;	/* XXX disable epoch compare if missing */
	version = evr;
    }
    if (se) {
	*se++ = '\0';
	release = se;
    } else {
	release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}

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

static int
rpmds_compare(rpmdsObject * a, rpmdsObject * b)
{
    char *aEVR = xstrdup(rpmdsEVR(a->ds));
    const char *aE, *aV, *aR;
    char *bEVR = xstrdup(rpmdsEVR(b->ds));
    const char *bE, *bV, *bR;
    int rc;

    /* XXX W2DO? should N be compared? */
    rpmds_ParseEVR(aEVR, &aE, &aV, &aR);
    rpmds_ParseEVR(bEVR, &bE, &bV, &bR);

    rc = compare_values(aE, bE);
    if (!rc) {
	rc = compare_values(aV, bV);
	if (!rc)
	    rc = compare_values(aR, bR);
    }

    aEVR = _free(aEVR);
    bEVR = _free(bEVR);

    return rc;
}

static PyObject *
rpmds_richcompare(rpmdsObject * a, rpmdsObject * b, int op)
{
    int rc;

    switch (op) {
    case Py_NE:
	/* XXX map ranges overlap boolean onto '!=' python syntax. */
	rc = rpmdsCompare(a->ds, b->ds);
	rc = (rc < 0 ? -1 : (rc == 0 ? 1 : 0));
	break;
    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
    case Py_EQ:
	/*@fallthrough@*/
    default:
	rc = -1;
	break;
    }
    return Py_BuildValue("i", rc);
}
#endif

/** \ingroup python
 * \name Class: Rpmds
 */
/*@{*/

static PyObject *
rpmds_Debug(rpmdsObject * s, PyObject * args,
		PyObject * kwds)
{
    char * kwlist[] = {"debugLevel", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &_rpmds_debug))
	return NULL;

    Py_RETURN_NONE;
}

static PyObject *
rpmds_Count(rpmdsObject * s)
{
    DEPRECATED_METHOD("use len(ds) instead");
    return Py_BuildValue("i", PyMapping_Size((PyObject *)s));
}

static PyObject *
rpmds_Ix(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsIx(s->ds));
}

static PyObject *
rpmds_DNEVR(rpmdsObject * s)
{
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

static PyObject *
rpmds_N(rpmdsObject * s)
{
    return Py_BuildValue("s", rpmdsN(s->ds));
}

static PyObject *
rpmds_EVR(rpmdsObject * s)
{
    return Py_BuildValue("s", rpmdsEVR(s->ds));
}

static PyObject *
rpmds_Flags(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsFlags(s->ds));
}

static PyObject *
rpmds_BT(rpmdsObject * s)
{
    return Py_BuildValue("i", (int) rpmdsBT(s->ds));
}

static PyObject *
rpmds_TagN(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsTagN(s->ds));
}

static PyObject *
rpmds_Color(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsColor(s->ds));
}

static PyObject *
rpmds_Refs(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsRefs(s->ds));
}

static PyObject *
rpmds_iternext(rpmdsObject * s)
{
    PyObject * result = NULL;

    /* Reset loop indices on 1st entry. */
    if (!s->active) {
	s->ds = rpmdsInit(s->ds);
	s->active = 1;
    }

    /* If more to do, return a (N, EVR, Flags) tuple. */
    if (rpmdsNext(s->ds) >= 0) {
	const char * N = rpmdsN(s->ds);
	const char * EVR = rpmdsEVR(s->ds);
	rpmTag tagN = rpmdsTagN(s->ds);
	rpmsenseFlags Flags = rpmdsFlags(s->ds);

	if (N != NULL) N = xstrdup(N);
	if (EVR != NULL) EVR = xstrdup(EVR);
	result = rpmds_Wrap(Py_TYPE(s), rpmdsSingle(tagN, N, EVR, Flags) );
    } else
	s->active = 0;

    return result;
}

static PyObject *
rpmds_Result(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsResult(s->ds));
}

static PyObject *
rpmds_SetNoPromote(rpmdsObject * s, PyObject * args, PyObject * kwds)
{
    int nopromote;
    char * kwlist[] = {"noPromote", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i:SetNoPromote", kwlist,
	    &nopromote))
	return NULL;

    return Py_BuildValue("i", rpmdsSetNoPromote(s->ds, nopromote));
}

static PyObject *
rpmds_Notify(rpmdsObject * s, PyObject * args, PyObject * kwds)
{
    const char * where;
    int rc;
    char * kwlist[] = {"location", "returnCode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "si:Notify", kwlist,
	    &where, &rc))
	return NULL;

    rpmdsNotify(s->ds, where, rc);
    Py_RETURN_NONE;
}

/* XXX rpmdsFind uses bsearch on s->ds, so a sort is needed. */
static PyObject *
rpmds_Sort(rpmdsObject * s)
{
    rpmds nds = NULL;

    if (rpmdsMerge(&nds, s->ds) >= 0) {
	(void)rpmdsFree(s->ds);
	s->ds = NULL;
	s->ds = nds;
    }
    Py_RETURN_NONE;
}

static PyObject *
rpmds_Find(rpmdsObject * s, PyObject * arg)
{
    rpmdsObject * o;

    if (!PyArg_Parse(arg, "O!:Find", &rpmds_Type, &o))
	return NULL;

    /* XXX make sure ods index is valid, real fix in lib/rpmds.c. */
    if (rpmdsIx(o->ds) == -1)	rpmdsSetIx(o->ds, 0);

    return Py_BuildValue("i", rpmdsFind(s->ds, o->ds));
}

static PyObject *
rpmds_Merge(rpmdsObject * s, PyObject * arg)
{
    rpmdsObject * o;

    if (!PyArg_Parse(arg, "O!:Merge", &rpmds_Type, &o))
	return NULL;

    return Py_BuildValue("i", rpmdsMerge(&s->ds, o->ds));
}

static PyObject *
rpmds_Search(rpmdsObject * s, PyObject * arg)
{
    rpmdsObject * o;

    if (!PyArg_Parse(arg, "O!:Merge", &rpmds_Type, &o))
	return NULL;

    return Py_BuildValue("i", rpmdsSearch(s->ds, o->ds));
}

static PyObject * rpmds_Compare(rpmdsObject * s, PyObject * o)
{
    rpmdsObject * ods;

    if (!PyArg_Parse(o, "O!:Compare", &rpmds_Type, &ods))
	return NULL;

    return PyBool_FromLong(rpmdsCompare(s->ds, ods->ds));
}

#ifdef	NOTYET
static PyObject *rpmds_Instance(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsInstance(s->ds));
}

static PyObject *
rpmds_Problem(rpmdsObject * s)
{
    if (!PyArg_ParseTuple(args, ":Problem"))
	return NULL;
    Py_RETURN_NONE;
}
#endif

static PyObject * rpmds_Cpuinfo(rpmdsObject * s)
{
    rpmds ds = NULL;
    int xx;

    /* XXX check return code, permit arg (NULL uses system default). */
    xx = rpmdsCpuinfo(&ds, NULL);

    return rpmds_Wrap(&rpmds_Type, ds );
}

static PyObject * rpmds_Rpmlib(rpmdsObject * s)
{
    rpmds ds = NULL;
    int xx;

    /* XXX check return code, permit arg (NULL uses system default). */
    xx = rpmdsRpmlib(&ds, NULL);

    return rpmds_Wrap(&rpmds_Type, ds );
}

static PyObject * rpmds_Sysinfo(rpmdsObject * s)
{
    rpmPRCO PRCO = rpmdsNewPRCO(NULL);
    rpmds P = NULL;
    int xx;

    /* XXX check return code, permit arg (NULL uses system default). */
    xx = rpmdsSysinfo(PRCO, NULL);
    P = rpmdsLink(rpmdsFromPRCO(PRCO, RPMTAG_PROVIDENAME), "rpmds_Sysinfo");
    PRCO = rpmdsFreePRCO(PRCO);

    return rpmds_Wrap(&rpmds_Type, P );
}

static PyObject * rpmds_Getconf(rpmdsObject * s)
{
    rpmds ds = NULL;
    int xx;

    /* XXX check return code, permit arg (NULL uses system default). */
    xx = rpmdsGetconf(&ds, NULL);

    return rpmds_Wrap(&rpmds_Type, ds );
}

static PyObject * rpmds_Ldconfig(rpmdsObject * s)
{
    rpmPRCO PRCO = rpmdsNewPRCO(NULL);
    rpmds P = NULL;
    int xx;

    /* XXX check return code, permit arg (NULL uses system default). */
    xx = rpmdsLdconfig(PRCO, NULL);

    P = rpmdsLink(rpmdsFromPRCO(PRCO, RPMTAG_PROVIDENAME), "rpmds_Ldconfig");
    PRCO = rpmdsFreePRCO(PRCO);
    return rpmds_Wrap(&rpmds_Type, P );
}

static PyObject * rpmds_Uname(rpmdsObject * s)
{
    rpmds ds = NULL;
    int xx;

    /* XXX check return code, permit arg (NULL uses system default). */
    xx = rpmdsUname(&ds, NULL);

    return rpmds_Wrap(&rpmds_Type, ds );
}

/*@}*/

static struct PyMethodDef rpmds_methods[] = {
 {"Debug",	(PyCFunction)rpmds_Debug,	METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Count",	(PyCFunction)rpmds_Count,	METH_NOARGS,
	"ds.Count -> Count	- Return no. of elements.\n" },
 {"Ix",		(PyCFunction)rpmds_Ix,		METH_NOARGS,
	"ds.Ix -> Ix		- Return current element index.\n" },
 {"DNEVR",	(PyCFunction)rpmds_DNEVR,	METH_NOARGS,
	"ds.DNEVR -> DNEVR	- Return current DNEVR.\n" },
 {"N",		(PyCFunction)rpmds_N,		METH_NOARGS,
	"ds.N -> N		- Return current N.\n" },
 {"EVR",	(PyCFunction)rpmds_EVR,		METH_NOARGS,
	"ds.EVR -> EVR		- Return current EVR.\n" },
 {"Flags",	(PyCFunction)rpmds_Flags,	METH_NOARGS,
	"ds.Flags -> Flags	- Return current Flags.\n" },
 {"BT",		(PyCFunction)rpmds_BT,		METH_NOARGS,
	"ds.BT -> BT	- Return build time.\n" },
 {"TagN",	(PyCFunction)rpmds_TagN,	METH_NOARGS,
	"ds.TagN -> TagN	- Return current TagN.\n" },
 {"Color",	(PyCFunction)rpmds_Color,	METH_NOARGS,
	"ds.Color -> Color	- Return current Color.\n" },
 {"Refs",	(PyCFunction)rpmds_Refs,	METH_NOARGS,
	"ds.Refs -> Refs	- Return current Refs.\n" },
 {"Result",	(PyCFunction)rpmds_Result,	METH_NOARGS,
	"ds.Result -> Result	- Return current Result.\n" },
 {"SetNoPromote",(PyCFunction)rpmds_SetNoPromote, METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Notify",	(PyCFunction)rpmds_Notify,	METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Sort",	(PyCFunction)rpmds_Sort,	METH_NOARGS,
"ds.Sort() -> None\n\
- Sort the (N,EVR,Flags) elements in ds\n" },
 {"Find",	(PyCFunction)rpmds_Find,	METH_O,
"ds.Find(element) -> matching ds index (-1 on failure)\n\
- Check for an exactly matching element in ds.\n\
The current index in ds is positioned at matching member upon success.\n" },
 {"Merge",	(PyCFunction)rpmds_Merge,	METH_O,
"ds.Merge(elements) -> 0 on success\n\
- Merge elements into ds, maintaining (N,EVR,Flags) sort order.\n" },
 {"Search",	(PyCFunction)rpmds_Search,	METH_O,
"ds.Search(element) -> matching ds index (-1 on failure)\n\
- Check that element dependency range overlaps some member of ds.\n\
The current index in ds is positioned at overlapping member upon success.\n" },
 {"Rpmlib",	(PyCFunction)rpmds_Rpmlib,	METH_NOARGS|METH_STATIC,
	"ds.Rpmlib -> nds	- Return internal rpmlib dependency set.\n"},
 {"Compare",	(PyCFunction)rpmds_Compare,	METH_O,
	NULL},
#ifdef	NOTYET
 {"Instance",   (PyCFunction)rpmds_Instance,    METH_NOARGS,
	NULL},
 {"Problem",	(PyCFunction)rpmds_Problem,	METH_NOARGS,
	NULL},
#endif
 {"Cpuinfo",	(PyCFunction)rpmds_Cpuinfo,	METH_NOARGS|METH_STATIC,
	"ds.Cpuinfo -> nds	- Return /proc/cpuinfo dependency set.\n"},
 {"Sysinfo",	(PyCFunction)rpmds_Sysinfo,	METH_NOARGS|METH_STATIC,
	"ds.Sysinfo -> nds	- Return /etc/rpm/sysinfo dependency set.\n"},
 {"Getconf",	(PyCFunction)rpmds_Getconf,	METH_NOARGS|METH_STATIC,
	"ds.Getconf -> nds	- Return getconf(1) dependency set.\n"},
 {"Ldconfig",	(PyCFunction)rpmds_Ldconfig,	METH_NOARGS|METH_STATIC,
	"ds.Ldconfig -> nds	- Return /etc/ld.so.cache dependency set.\n"},
 {"Uname",	(PyCFunction)rpmds_Uname,	METH_NOARGS|METH_STATIC,
	"ds.Uname -> nds	- Return uname(2) dependency set.\n"},
 {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static void
rpmds_dealloc(rpmdsObject * s)
{
    (void)rpmdsFree(s->ds);
    s->ds = NULL;
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static Py_ssize_t rpmds_length(rpmdsObject * s)
{
    return rpmdsCount(s->ds);
}

static PyObject *
rpmds_subscript(rpmdsObject * s, PyObject * key)
{
    int ix;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    ix = (int) PyInt_AsLong(key);
    /* XXX make sure that DNEVR exists. */
    rpmdsSetIx(s->ds, ix-1);
    (void) rpmdsNext(s->ds);
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

static PyMappingMethods rpmds_as_mapping = {
        (lenfunc) rpmds_length,		/* mp_length */
        (binaryfunc) rpmds_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

static int rpmds_init(rpmdsObject * s, PyObject *args, PyObject *kwds)
{
    s->active = 0;

    return 0;
}

static PyObject * rpmds_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
{
    rpmdsObject *s;
    PyObject *obj;
    rpmTag tagN = RPMTAG_REQUIRENAME;
    rpmds ds = NULL;
    Header h = NULL;
    char * kwlist[] = {"obj", "tag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO&:rpmds_new", kwlist, 
	    	 &obj, tagNumFromPyObject, &tagN))
	return NULL;

    if (PyTuple_Check(obj)) {
	const char *name = NULL;
	const char *evr = NULL;
	rpmsenseFlags flags = 0;	/* XXX RPMSENSE_ANY */
	if (PyArg_ParseTuple(obj, "s|is", &name, &flags, &evr))
	    ds = rpmdsSingle(tagN, name, evr, flags);
    } else if (hdrFromPyObject(obj, &h)) {
	if (tagN == RPMTAG_NEVR)
	    ds = rpmdsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
	else
	    ds = rpmdsNew(h, tagN, 0);
    } else {
	PyErr_SetString(PyExc_TypeError, "header or tuple expected");
	return NULL;
    }
    
    s = (rpmdsObject *) rpmds_Wrap(subtype, ds);

if (_rpmds_debug)
fprintf(stderr, "%p ++ ds %p\n", s, s->ds);

    return (PyObject *) s;
}

static char rpmds_doc[] =
"";

PyTypeObject rpmds_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.ds",			/* tp_name */
	sizeof(rpmdsObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmds_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	0,				/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmds_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /* tp_flags */
	rpmds_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc) rpmds_iternext,	/* tp_iternext */
	rpmds_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmds_init,		/* tp_init */
	0,				/* tp_alloc */
	(newfunc) rpmds_new,		/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
#endif
};

/* ---------- */

rpmds dsFromDs(rpmdsObject * s)
{
    return s->ds;
}

PyObject * rpmds_Wrap(PyTypeObject *subtype, rpmds ds)
{
    rpmdsObject * s = (rpmdsObject *) subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->ds = ds;
    s->active = 0;
    return (PyObject *) s;
}

#ifndef	DYING
PyObject *
rpmds_Single(PyObject * s, PyObject * args, PyObject * kwds)
{
    PyObject * to = NULL;
    rpmTag tagN = RPMTAG_PROVIDENAME;
    const char * N;
    const char * EVR = NULL;
    rpmsenseFlags Flags = 0;
    char * kwlist[] = {"to", "name", "evr", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os|si:Single", kwlist,
	    &to, &N, &EVR, &Flags))
	return NULL;

    if (to != NULL) {
	if (!tagNumFromPyObject(to, &tagN)) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return NULL;
	}
    }
    if (N != NULL) N = xstrdup(N);
    if (EVR != NULL) EVR = xstrdup(EVR);
    return rpmds_Wrap(&rpmds_Type, rpmdsSingle(tagN, N, EVR, Flags) );
}

PyObject *
hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
    hdrObject * ho = (hdrObject *)s;
    PyObject * to = NULL;
    rpmTag tagN = RPMTAG_REQUIRENAME;
    int flags = 0;
    char * kwlist[] = {"to", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oi:dsFromHeader", kwlist,
	    &to, &flags))
	return NULL;

    if (to != NULL) {
	if (!tagNumFromPyObject(to, &tagN)) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return NULL;
	}
    }
    return rpmds_Wrap(&rpmds_Type, rpmdsNew(hdrGetHeader(ho), tagN, flags) );
}

PyObject *
hdr_dsOfHeader(PyObject * s)
{
    hdrObject * ho = (hdrObject *)s;
    int tagN = RPMTAG_PROVIDENAME;
    int Flags = RPMSENSE_EQUAL;

    return rpmds_Wrap(&rpmds_Type, rpmdsThis(hdrGetHeader(ho), tagN, Flags) );
}
#endif
