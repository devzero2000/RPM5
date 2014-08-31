#ifndef H_RPMFD_PY
#define H_RPMFD_PY

/** \ingroup py_c
 * \file python/rpmfd-py.h
 */

/** \name Type: _rpm.fd */
/*@{*/

typedef struct rpmfdObject_s rpmfdObject;

extern PyTypeObject rpmfd_Type;
#define rpmfdObject_Check(v)    ((v)->ob_type == &rpmfd_Type)

#ifdef __cplusplus
extern "C" {
#endif

rpmfdObject * rpmfd_Wrap(FD_t fd);

#ifdef __cplusplus      
}
#endif

/*@}*/

#endif
