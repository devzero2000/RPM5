#ifndef H_RPMTE_PY
#define H_RPMTE_PY

#include "rpmte.h"

/** \ingroup py_c
 * \file python/rpmte-py.h
 */

/** \name Type: _rpm.te */
/*@{*/

/** \ingroup py_c
 */
typedef struct rpmteObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmte	te;
} rpmteObject;

/** \ingroup py_c
 */
/*@unchecked@*/
extern PyTypeObject rpmte_Type;

__BEGIN_DECLS
/** \ingroup py_c
 */
/*@null@*/
rpmteObject * rpmte_Wrap(rpmte te)
	/*@*/;

__END_DECLS
/*@}*/

#endif
