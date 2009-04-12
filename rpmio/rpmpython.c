#include "system.h"

#define _RPMPYTHON_INTERNAL
#include "rpmpython.h"

#if defined(WITH_PYTHON_EMBED)
#include <cStringIO.h>
#endif

#include "debug.h"

/*@unchecked@*/
int _rpmpython_debug = 0;

static void rpmpythonFini(void * _python)
        /*@globals fileSystem @*/
        /*@modifies *_python, fileSystem @*/
{
    rpmpython python = _python;

    python->fn = _free(python->fn);
    python->flags = 0;
#if defined(WITH_PYTHONEMBED)
    Py_Finalize();
#endif
    python->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmpythonPool;

static rpmpython rpmpythonGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmpythonPool, fileSystem @*/
        /*@modifies pool, _rpmpythonPool, fileSystem @*/
{
    rpmpython python;

    if (_rpmpythonPool == NULL) {
        _rpmpythonPool = rpmioNewPool("python", sizeof(*python), -1, _rpmpython_debug,
                        NULL, NULL, rpmpythonFini);
        pool = _rpmpythonPool;
    }
    return (rpmpython) rpmioGetPool(pool, sizeof(*python));
}

/*@unchecked@*/
static const char * rpmpythonInitStringIO = "\
import rpm\n\
import sys\n\
from cStringIO import StringIO\n\
stdout = sys.stdout\n\
sys.stdout = StringIO()\n\
";

rpmpython rpmpythonNew(const char * fn, int flags)
{
    rpmpython python = rpmpythonGetPool(_rpmpythonPool);

    if (fn)
	python->fn = xstrdup(fn);
    python->flags = flags;

#if defined(WITH_PYTHONEMBED)
    Py_Initialize();
    if (PycStringIO == NULL)
	PycStringIO = PyCObject_Import("cStringIO", "cStringIO_CAPI");
    (void) rpmpythonRun(python, rpmpythonInitStringIO, NULL);
#endif

    return rpmpythonLink(python);
}

rpmRC rpmpythonRunFile(rpmpython python, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;


if (_rpmpython_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, python, fn);

    if (fn != NULL) {
#if defined(WITH_PYTHONEMBED)
	const char * pyfn = ((fn == NULL || !strcmp(fn, "-")) ? "<stdin>" : fn);
	FILE * pyfp = (!strcmp(pyfn, "<stdin>") ? stdin : fopen(fn, "rb"));
	int closeit = (pyfp != stdin);
	PyCompilerFlags cf = { .cf_flags = 0 };
	
	if (pyfp != NULL) {
	    PyRun_AnyFileExFlags(pyfp, pyfn, closeit, &cf);
	    rc = RPMRC_OK;
	}
#endif
    }
    return rc;
}

rpmRC rpmpythonRun(rpmpython python, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmpython_debug)
fprintf(stderr, "==> %s(%p,%s,%p)\n", __FUNCTION__, python, str, resultp);

    if (str != NULL) {
#if defined(WITH_PYTHONEMBED)
	PyCompilerFlags cf = { .cf_flags = 0 };
	PyObject * m = PyImport_AddModule("__main__");
	PyObject * d = (m ? PyModule_GetDict(m) : NULL);
	PyObject * v = (m ? PyRun_StringFlags(str, Py_file_input, d, d, &cf) : NULL);

        if (v == NULL) {
	    PyErr_Print();
	} else {
	    if (resultp != NULL) {
		PyObject * sys_stdout = PySys_GetObject("stdout");
		if (sys_stdout != NULL && PycStringIO_OutputCheck(sys_stdout)) {
		    PyObject * o = (*PycStringIO->cgetvalue)(sys_stdout);
		    *resultp = (PyString_Check(o) ? PyString_AsString(o) : "");
		} else
		    *resultp = "";
	    }
	   
	    Py_DECREF(v);
	    if (Py_FlushLine())
		PyErr_Clear();
	    rc = RPMRC_OK;
	}
#endif
    }
    return rc;
}
