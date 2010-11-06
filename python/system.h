/** \ingroup py_c
 * \file python/system.h
 */

#ifndef H_SYSTEM_PYTHON
#define	H_SYSTEM_PYTHON

#if defined(__APPLE__)
#include <sys/types.h>
#endif

#include "Python.h"

#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 4
typedef inquiry lenfunc;
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#define Py_RETURN_TRUE return Py_INCREF(Py_True), Py_True
#endif
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 5
typedef int Py_ssize_t;
#endif

#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "../system.h"

#endif	/* H_SYSTEM_PYTHON */
