#ifndef H_RPMFD_PY
#define H_RPMFD_PY

#include <rpmio.h>

typedef struct rpmfdObject_s rpmfdObject;

extern PyTypeObject rpmfd_Type;

#define rpmfdObject_Check(v)	((v)->ob_type == &rpmfd_Type)

FD_t rpmfdGetFd(rpmfdObject *fdo)
	RPM_GNUC_PURE;

int rpmfdFromPyObject(PyObject *obj, rpmfdObject **fdop);

#endif
