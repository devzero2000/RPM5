#ifndef H_RPMMI_PY
#define H_RPMMI_PY

/** \ingroup py_c
 * \file python/rpmmi-py.h
 */

/** \name Type: _rpm.mi */
/*@{*/

typedef struct rpmmiObject_s rpmmiObject;

extern PyTypeObject rpmmi_Type;
#define rpmmiObject_Check(v)    ((v)->ob_type == &rpmmi_Type)

#ifdef __cplusplus
extern "C" {
#endif

PyObject * rpmmi_Wrap(PyTypeObject *subtype, rpmmi mi, PyObject *s);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
