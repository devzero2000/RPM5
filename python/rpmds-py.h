#ifndef H_RPMDS_PY
#define H_RPMDS_PY

#include "rpmds.h"

/** \ingroup py_c
 * \file python/rpmds-py.h
 */

/** \name Type: _rpm.ds */
/*@{*/

typedef struct rpmdsObject_s rpmdsObject;

extern PyTypeObject rpmds_Type;
#define rpmdsObject_Check(v)    ((v)->ob_type == &rpmds_Type)

#ifdef __cplusplus
extern "C" {
#endif

rpmds dsFromDs(rpmdsObject * ds);

PyObject * rpmds_Wrap(PyTypeObject *subtype, rpmds ds);

rpmdsObject * rpmds_Single(PyObject * s, PyObject * args, PyObject * kwds);

rpmdsObject * hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds);

rpmdsObject * hdr_dsOfHeader(PyObject * s);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
