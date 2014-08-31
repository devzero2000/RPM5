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

hdrObject * hdr_Wrap(Header h);

Header hdrGetHeader(hdrObject * h);

rpmTag tagNumFromPyObject (PyObject *item);

PyObject * evrSplit (PyObject * self, PyObject * args, PyObject * kwds);

PyObject * evrCompare (PyObject * self, PyObject * args, PyObject * kwds);

PyObject * labelCompare (PyObject * self, PyObject * args);

PyObject * versionCompare (PyObject * self, PyObject * args, PyObject * kwds);

PyObject * rpmHeaderFromFile(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * rpmHeaderFromFD(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * rpmSingleHeaderFromFD(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * rpmHeaderToFile(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * rpmHeaderToFD(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * rpmReadHeaders (FD_t fd);

PyObject * rpmWriteHeaders (PyObject * list, FD_t fd);

PyObject * rhnLoad(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * hdrLoad(PyObject * self, PyObject * args, PyObject * kwds);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
