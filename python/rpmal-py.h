#ifndef H_RPMAL_PY
#define H_RPMAL_PY

#include "rpmal.h"

/** \ingroup py_c
 * \file python/rpmal-py.h
 */

/** \name Type: _rpm.al */
/*@{*/

typedef struct rpmalObject_s rpmalObject;

extern PyTypeObject rpmal_Type;

#ifdef __cplusplus
extern "C" {
#endif

PyObject * rpmal_Wrap(PyTypeObject *subtype, rpmal al);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
