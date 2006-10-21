#ifndef H_SPEC_PY
#define H_SPEC_PY

#include <rpmbuild.h>

/** \ingroup py_c
 * \file python/spec-py.h
 */

/** \name Type: _rpm.spec */
/*@{*/

/** \ingroup py_c
 */
typedef struct specObject_s {
    PyObject_HEAD
    /*type specific fields */
/*@null@*/
    Spec spec;
} specObject;

/** \ingroup py_c
 */
/*@unchecked@*/
extern PyTypeObject spec_Type;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup _rpm.spec
 */
/*@null@*/
Spec specFromSpec(specObject * spec)
/*@*/;

/** \ingroup _rpm.spec
 */
/*@null@*/
specObject * spec_Wrap(Spec spec)
/*@*/;

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif /* H_SPEC_PY */
