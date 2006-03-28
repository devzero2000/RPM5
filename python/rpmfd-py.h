#ifndef H_RPMFD_PY
#define H_RPMFD_PY

/** \ingroup py_c
 * \file python/rpmfd-py.h
 */

__BEGIN_DECLS
/** \name Type: _rpm.fd */
/*@{*/

/** \ingroup py_c
 */
typedef struct rpmfdObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
/*@relnull@*/
    FD_t	fd;
} rpmfdObject;

/** \ingroup py_c
 */
/*@unchecked@*/
extern PyTypeObject rpmfd_Type;

/** \ingroup py_c
 */
/*@null@*/
rpmfdObject * rpmfd_Wrap(FD_t fd)
	/*@*/;

/*@}*/
__END_DECLS

#endif
