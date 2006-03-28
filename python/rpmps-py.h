#ifndef H_RPMPS_PY
#define H_RPMPS_PY

#include "rpmps.h"

/** \ingroup py_c
 * \file python/rpmps-py.h
 */

/** \name Type: _rpm.ps */
/*@{*/

/** \ingroup py_c
 */
typedef struct rpmpsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int		active;
    int		ix;
/*@relnull@*/
    rpmps	ps;
} rpmpsObject;

/** \ingroup py_c
 */
/*@unchecked@*/
extern PyTypeObject rpmps_Type;

__BEGIN_DECLS
/** \ingroup py_c
 */
/*@null@*/
rpmps psFromPs(rpmpsObject * ps)
	/*@*/;

/** \ingroup py_c
 */
/*@null@*/
rpmpsObject * rpmps_Wrap(rpmps ps)
	/*@*/;

__END_DECLS
/*@}*/

#endif
