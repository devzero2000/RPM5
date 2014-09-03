#ifndef H_RPMTD_PY
#define H_RPMTD_PY

#include <rpmtd.h>

/** \ingroup py_c
 * \file python/rpmtd-py.h
 */

typedef struct rpmtdObject_s rpmtdObject;

extern PyTypeObject rpmtd_Type;
#define rpmtdObject_Check(v)    ((v)->ob_type == &rpmtd_Type)

PyObject * rpmtd_AsPyobj(rpmtd td);

PyObject * rpmtd_Wrap(PyTypeObject *subtype, rpmtd td);
int rpmtdFromPyObject(PyObject *obj, rpmtd *td);

#endif	/* H_RPMTD_PY */
