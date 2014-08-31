#ifndef H_RPMPS_PY
#define H_RPMPS_PY

#include "rpmps.h"

/** \ingroup py_c
 * \file python/rpmps-py.h
 */

/** \name Type: _rpm.ps */
/*@{*/

typedef struct rpmpsObject_s rpmpsObject;

extern PyTypeObject rpmps_Type;

#ifdef __cplusplus
extern "C" {
#endif

rpmps psFromPs(rpmpsObject * ps);

PyObject * rpmps_Wrap(PyTypeObject *subtype, rpmps ps);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
