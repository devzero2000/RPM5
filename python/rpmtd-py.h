#ifndef _RPMTD_PY_H
#define _RPMTD_PY_H

#include <Python.h>
#include <rpm/rpmtypes.h>

/** \ingroup py_c
 * \file python/rpmtd-py.h
 */

/** \ingroup py_c
 */
typedef struct rpmtdObject_s rpmtdObject;

/** \ingroup py_c
 */
struct rpmtdObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmtd td;
};

extern PyTypeObject rpmtd_Type;

PyObject * rpmtd_ItemAsPyobj(rpmtd td);
PyObject * rpmtd_AsPyobj(rpmtd td);
rpmtdObject * rpmtd_Wrap(rpmtd td);
#endif
