#ifndef H_RPMTE_PY
#define H_RPMTE_PY

#include "rpmte.h"

/** \ingroup py_c
 * \file python/rpmte-py.h
 */

/** \name Type: _rpm.te */
/*@{*/

typedef struct rpmteObject_s rpmteObject;

extern PyTypeObject rpmte_Type;
#define rpmteObject_Check(v)    ((v)->ob_type == &rpmte_Type)

#ifdef __cplusplus
extern "C" {
#endif

PyObject * rpmte_Wrap(PyTypeObject *subtype, rpmte te);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
