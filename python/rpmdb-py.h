#ifndef H_RPMDB_PY
#define H_RPMDB_PY

#include "rpmdb.h"

/** \ingroup py_c
 * \file python/rpmdb-py.h
 */

/** \name Type: _rpm.db */
/*@{*/

typedef struct rpmdbObject_s rpmdbObject;

extern PyTypeObject rpmdb_Type;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef  _LEGACY_BINDINGS_TOO
rpmdb dbFromDb(rpmdbObject * db);

rpmdbObject * rpmOpenDB(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * rebuildDB (PyObject * self, PyObject * args, PyObject * kwds);
#endif

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
