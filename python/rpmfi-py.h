#ifndef H_RPMFI_PY
#define H_RPMFI_PY

#include "rpmfi.h"

/** \ingroup py_c
 * \file python/rpmfi-py.h
 */

/** \name Type: _rpm.fi */
/*@{*/

typedef struct rpmfiObject_s rpmfiObject;

extern PyTypeObject rpmfi_Type;
#define rpmfiObject_Check(v)    ((v)->ob_type == &rpmfi_Type)

#ifdef __cplusplus
extern "C" {
#endif

rpmfi fiFromFi(rpmfiObject * fi)
	RPM_GNUC_PURE;

PyObject * rpmfi_Wrap(PyTypeObject *subtype, rpmfi fi);

PyObject * hdr_fiFromHeader(PyObject * s, PyObject * args, PyObject * kwds);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
