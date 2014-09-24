#ifndef RPMPYTHON_HEADER
#define RPMPYTHON_HEADER

/** \ingroup py_c
 * \file python/header-py.h
 */

/** \name Type: _rpm.hdr */
/*@{*/

typedef struct hdrObject_s hdrObject;

extern PyTypeObject hdr_Type;
#define hdrObject_Check(v)      ((v)->ob_type == &hdr_Type)

extern PyObject * pyrpmError;

#ifdef __cplusplus
extern "C" {
#endif

Header hdrGetHeader(hdrObject * h)
	RPM_GNUC_PURE;
PyObject * hdr_Wrap(PyTypeObject *subtype, Header h);

int hdrFromPyObject(PyObject *item, Header *h);
int utf8FromPyObject(PyObject *item, PyObject **str);
int tagNumFromPyObject (PyObject *item, rpmTag *tagp);

PyObject * labelCompare (PyObject * self, PyObject * args);
PyObject * versionCompare (PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * evrSplit (PyObject * self, PyObject * args, PyObject * kwds);
PyObject * evrCompare (PyObject * self, PyObject * args, PyObject * kwds);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
