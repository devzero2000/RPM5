#ifndef H_SPEC_PY
#define H_SPEC_PY

#include <rpmbuild.h>

/** \ingroup py_c
 * \file python/spec-py.h
 */

typedef struct specObject_s specObject;
typedef struct specPkgObject_s specPkgObject;

/*@unchecked@*/
extern PyTypeObject spec_Type;
/*@unchecked@*/
extern PyTypeObject specPkg_Type;

#ifdef __cplusplus
extern "C" {
#endif

/*@null@*/
Spec specFromSpec(specObject * spec)
	/*@*/;

/*@null@*/
PyObject * spec_Wrap(PyTypeObject *subtype, Spec spec)
	/*@*/;

/*@null@*/
PyObject * specPkg_Wrap(PyTypeObject *subtype, Package pkg)
	/*@*/;

#ifdef __cplusplus      
}
#endif

#endif /* H_SPEC_PY */
