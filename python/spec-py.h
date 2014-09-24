#ifndef H_SPEC_PY
#define H_SPEC_PY

#include <rpmbuild.h>

/** \ingroup py_c
 * \file python/spec-py.h
 */

typedef struct specObject_s specObject;
typedef struct specPkgObject_s specPkgObject;

extern PyTypeObject spec_Type;
extern PyTypeObject specPkg_Type;

#ifdef __cplusplus
extern "C" {
#endif

Spec specFromSpec(specObject * spec)
	RPM_GNUC_PURE;

PyObject * spec_Wrap(PyTypeObject *subtype, Spec spec);

PyObject * specPkg_Wrap(PyTypeObject *subtype, Package pkg);

#ifdef __cplusplus      
}
#endif

#endif /* H_SPEC_PY */
