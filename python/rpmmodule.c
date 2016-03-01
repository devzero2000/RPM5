/** \ingroup py_c
 * \file python/rpmmodule.c
 */

#include "system-py.h"

#include <rpmio_internal.h>
#include <rpmcb.h>
#include <rpmsq.h>
#include <argv.h>

#include <rpmversion.h>
#define	_RPMTAG_INTERNAL
#include <rpmtag.h>
#define _RPMEVR_INTERNAL
#include <rpmevr.h>
#include <rpmdb.h>
#include <rpmcli.h>	/* XXX for rpmCheckSig */

#include "legacy.h"
#include "misc.h"

#include "header-py.h"
#include "rpmal-py.h"
#include "rpmds-py.h"
#include "rpmfd-py.h"
#include "rpmfts-py.h"
#include "rpmfi-py.h"
#include "rpmkeyring-py.h"
#include "rpmmacro-py.h"
#include "rpmmi-py.h"
#include "rpmps-py.h"
#include "rpmtd-py.h"
#include "rpmte-py.h"
#include "rpmts-py.h"

#include "debug.h"

/* XXX revert RPMVERCMP_DIGITS_BEAT_ALPHA at runtime */
extern int _invert_digits_alphas_comparison;

/* XXX add --noparentdirs --nolinktos to rpmtsCheck() */
extern int global_depFlags;

/** \ingroup python
 * \name Module: rpm
 */
/*@{*/

PyObject * pyrpmError;

static PyObject * archScore(PyObject * self, PyObject * arg)
{
    char * arch;
    char * platform;
    int score;

    if (!PyArg_Parse(arg, "s", &arch))
	return NULL;

#if defined(RPM_VENDOR_WINDRIVER) || defined(RPM_VENDOR_OE)
    platform = rpmExpand(arch, "-%{_host_vendor}", "-%{_host_os}%{?_host_gnu}%{!?_host_gnu:%{?_gnu}}", NULL);
#else
    platform = rpmExpand(arch, "-", "%{_vendor}", "-", "%{_os}", NULL);
#endif
    score = rpmPlatformScore(platform, NULL, 0);
    platform = _free(platform);

    return Py_BuildValue("i", score);
}

static PyObject * platformScore(PyObject * s, PyObject * arg)
{
    char * platform;
    int score;

    if (!PyArg_Parse(arg, "s", &platform))
	return NULL;

    score = rpmPlatformScore(platform, NULL, 0);

    return Py_BuildValue("i", score);
}

static PyObject * signalCaught(PyObject * self, PyObject * o)
{
    int signo;
    if (!PyArg_Parse(o, "i", &signo)) return NULL;

    return PyBool_FromLong(rpmsqIsCaught(signo));
}

static PyObject * checkSignals(PyObject * self)
{
    rpmdbCheckSignals();
    Py_RETURN_NONE;
}

static PyObject * setLogFile (PyObject * self, PyObject *arg)
{
    FILE *fp;
    int fdno = PyObject_AsFileDescriptor(arg);

    if (fdno >= 0) {
	/* XXX we dont know the mode here.. guessing append for now */
	fp = fdopen(fdno, "a");
	if (fp == NULL) {
	    PyErr_SetFromErrno(PyExc_IOError);
	    return NULL;
	}
    } else if (arg == Py_None) {
	fp = NULL;
    } else {
	PyErr_SetString(PyExc_TypeError, "file object or None expected");
	return NULL;
    }

    (void) rpmlogSetFile(fp);
    Py_RETURN_NONE;
}

static PyObject *
setVerbosity (PyObject * self, PyObject * arg)
{
    int level;

    if (!PyArg_Parse(arg, "i", &level))
	return NULL;

    rpmSetVerbosity(level);

    Py_RETURN_NONE;
}

static PyObject *
setEpochPromote (PyObject * self, PyObject * arg)
{
    if (!PyArg_Parse(arg, "i", &_rpmds_nopromote))
	return NULL;

    Py_RETURN_NONE;
}

static PyObject * setStats (PyObject * self, PyObject * arg)
{
    if (!PyArg_Parse(arg, "i", &_rpmts_stats))
	return NULL;

    Py_RETURN_NONE;
}

static PyObject * doLog(PyObject * self, PyObject * args, PyObject *kwds)
{
    int code;
    const char *msg;
    char * kwlist[] = {"code", "msg", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "is", kwlist, &code, &msg))
	return NULL;

    rpmlog(code, "%s", msg);
    Py_RETURN_NONE;
}

static PyObject * reloadConfig(PyObject * self, PyObject * args, PyObject *kwds)
{
    char * target = NULL;
    char * kwlist[] = { "target", NULL };
    int rc;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &target))
        return NULL;

    rpmFreeMacros(NULL);
    rpmFreeRpmrc();
    rc = rpmReadConfigFiles(NULL, target) ;

    return PyBool_FromLong(rc == 0);
}

/*@}*/

static PyMethodDef rpmModuleMethods[] = {
#ifdef	DYING
    { "TransactionSet", (PyCFunction) rpmts_Create, METH_VARARGS|METH_KEYWORDS,
"rpm.TransactionSet([rootDir, [db]]) -> ts\n\
- Create a transaction set.\n" },
#endif

    { "addMacro", (PyCFunction) rpmmacro_AddMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "delMacro", (PyCFunction) rpmmacro_DelMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "expandMacro", (PyCFunction)rpmmacro_ExpandMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "getMacros", (PyCFunction) rpmmacro_GetMacros, METH_VARARGS|METH_KEYWORDS,
	NULL },

    { "archscore", (PyCFunction) archScore, METH_O,
        NULL },
    { "platformscore", (PyCFunction) platformScore, METH_O,
        NULL },

    { "signalCaught", (PyCFunction) signalCaught, METH_O,
	NULL },
    { "checkSignals", (PyCFunction) checkSignals, METH_NOARGS,
	NULL },

#ifdef	DYING
    { "headerLoad", (PyCFunction) hdrLoad, METH_VARARGS|METH_KEYWORDS,
	NULL },

    { "readHeaderListFromFD", (PyCFunction) rpmHeaderFromFD, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "readHeaderListFromFile", (PyCFunction) rpmHeaderFromFile, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "readHeaderFromFD", (PyCFunction) rpmSingleHeaderFromFD, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "writeHeaderListToFD", (PyCFunction) rpmHeaderToFD, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "writeHeaderListToFile", (PyCFunction) rpmHeaderToFile, METH_VARARGS|METH_KEYWORDS,
	NULL },
#endif
    { "mergeHeaderListFromFD", (PyCFunction) rpmMergeHeadersFromFD, METH_VARARGS|METH_KEYWORDS,
	NULL },

    { "log",            (PyCFunction) doLog, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "setLogFile", (PyCFunction) setLogFile, METH_O,
	NULL },

    { "versionCompare", (PyCFunction) versionCompare, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "labelCompare", (PyCFunction) labelCompare, METH_VARARGS|METH_KEYWORDS,
	NULL },
#ifndef	DYING	/* XXX compare using PCRE parsing */
    { "evrCompare", (PyCFunction) evrCompare, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "evrSplit", (PyCFunction) evrSplit, METH_VARARGS|METH_KEYWORDS,
	NULL },
#endif
    { "setVerbosity", (PyCFunction) setVerbosity, METH_O,
	NULL },
    { "setEpochPromote", (PyCFunction) setEpochPromote, METH_O,
	NULL },
    { "setStats", (PyCFunction) setStats, METH_O,
	NULL },
    { "reloadConfig", (PyCFunction) reloadConfig, METH_VARARGS|METH_KEYWORDS,
	NULL },

#ifdef	DYING
    { "dsSingle", (PyCFunction) rpmds_Single, METH_VARARGS|METH_KEYWORDS,
"rpm.dsSingle(TagN, N, [EVR, [Flags]] -> ds\n\
- Create a single element dependency set.\n" },
#endif
    { NULL }
} ;

/*
 * Force clean up of open iterators and dbs on exit.
 */
static void rpm_exithook(void)
{
   rpmdbCheckTerminate(1);
}

static char rpm__doc__[] =
"";

/*
 * Add rpm tag dictionaries to the module
 */
static void addRpmTags(PyObject *module)
{
    PyObject *dict = PyDict_New();
#ifdef	NOTYET
    PyObject *pyval, *pyname;
    rpmtd names = rpmtdNew();
    rpmTagGetNames(names, 1);
    const char *tagname, *shortname;
    rpmTag tagval;

    while ((tagname = rpmtdNextString(names))) {
	shortname = tagname + strlen("RPMTAG_");
	tagval = rpmTagGetValue(shortname);

	PyModule_AddIntConstant(module, tagname, tagval);
	pyval = PyInt_FromLong(tagval);
	pyname = Py_BuildValue("s", shortname);
	PyDict_SetItem(dict, pyval, pyname);
	Py_DECREF(pyval);
	Py_DECREF(pyname);
    }
    PyModule_AddObject(module, "tagnames", dict);
    rpmtdFreeData(names);
    rpmtdFree(names);
#else
    PyObject * d = PyModule_GetDict(module);
    PyObject * o;

 {  const struct headerTagTableEntry_s * t;
    PyObject * to;
    for (t = rpmTagTable; t && t->name; t++) {
	PyDict_SetItemString(d, (char *) t->name, to=PyInt_FromLong(t->val));
	Py_XDECREF(to);
        PyDict_SetItem(dict, to, o=PyString_FromString(t->name + 7));
	Py_XDECREF(o);
    }
 }

 {  headerSprintfExtension exts = rpmHeaderFormats;
    headerSprintfExtension ext;
    PyObject * to;
    int extNum;
    for (ext = exts, extNum = 0; ext != NULL && ext->type != HEADER_EXT_LAST;
        ext = (ext->type == HEADER_EXT_MORE ? *ext->u.more : ext+1), extNum++)
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_TAG)
	    continue;
        PyDict_SetItemString(d, (char *) ext->name, to=PyInt_FromLong(tagValue(ext->name)));
	Py_XDECREF(to);
        PyDict_SetItem(dict, to, o=PyString_FromString(ext->name + 7));
	Py_XDECREF(o);
    }
 }

    PyDict_SetItemString(d, "tagnames", dict);
    Py_XDECREF(dict);
#endif
}

/*
 * Do any common preliminary work before python 2 vs python 3 module creation:
 */
static int prepareInitModule(void)
{
    if (PyType_Ready(&hdr_Type) < 0) return 0;
#ifdef	NOTYET
    if (PyType_Ready(&rpmarchive_Type) < 0) return 0;
#endif
    if (PyType_Ready(&rpmds_Type) < 0) return 0;
    if (PyType_Ready(&rpmfd_Type) < 0) return 0;
    if (PyType_Ready(&rpmfi_Type) < 0) return 0;
#ifdef	NOTYET
    if (PyType_Ready(&rpmfile_Type) < 0) return 0;
    if (PyType_Ready(&rpmfiles_Type) < 0) return 0;
#else
    if (PyType_Ready(&rpmfts_Type) < 0) return 0;
#endif
    if (PyType_Ready(&rpmKeyring_Type) < 0) return 0;
    if (PyType_Ready(&rpmmi_Type) < 0) return 0;
#ifdef	NOTYET
    if (PyType_Ready(&rpmii_Type) < 0) return 0;
#endif
    if (PyType_Ready(&rpmProblem_Type) < 0) return 0;
    if (PyType_Ready(&rpmPubkey_Type) < 0) return 0;
#ifdef	NOTYET
    if (PyType_Ready(&rpmstrPool_Type) < 0) return 0;
#endif
#if 0
    if (PyType_Ready(&rpmtd_Type) < 0) return 0;
#endif
    if (PyType_Ready(&rpmte_Type) < 0) return 0;
    if (PyType_Ready(&rpmts_Type) < 0) return 0;

    return 1;
}

static int initModule(PyObject *m)
{
    PyObject * d;

    /* 
     * treat error to register rpm cleanup hook as fatal, tracebacks
     * can and will leave stale locks around if we can't clean up
     */
    if (Py_AtExit(rpm_exithook) == -1)
	return 0;

    /* XXX revert RPMVERCMP_DIGITS_BEAT_ALPHA at runtime */
    _invert_digits_alphas_comparison = -1;

    /* XXX add --noparentdirs --nolinktos to rpmtsCheck() */
    global_depFlags = (RPMDEPS_FLAG_NOPARENTDIRS | RPMDEPS_FLAG_NOLINKTOS);

    /* failure to initialize rpm (crypto and all) is rather fatal too... */
    if (rpmReadConfigFiles(NULL, NULL) == -1)
	return 0;

    d = PyModule_GetDict(m);

    pyrpmError = PyErr_NewException("_rpm.error", NULL, NULL);
    if (pyrpmError != NULL)
	PyDict_SetItemString(d, "error", pyrpmError);

#if Py_TPFLAGS_HAVE_ITER        /* XXX backport to python-1.5.2 */
    Py_INCREF(&hdr_Type);
    PyModule_AddObject(m, "hdr", (PyObject *) &hdr_Type);

#ifdef	NOTYET
    Py_INCREF(&rpmarchive_Type);
    PyModule_AddObject(m, "archive", (PyObject *) &rpmarchive_Type);
#else
    Py_INCREF(&rpmal_Type);
    PyModule_AddObject(m, "al", (PyObject *) &rpmal_Type);
#endif

    Py_INCREF(&rpmds_Type);
    PyModule_AddObject(m, "ds", (PyObject *) &rpmds_Type);

    Py_INCREF(&rpmfd_Type);
    PyModule_AddObject(m, "fd", (PyObject *) &rpmfd_Type);

    Py_INCREF(&rpmfts_Type);
    PyModule_AddObject(m, "fts", (PyObject *) &rpmfts_Type);

    Py_INCREF(&rpmfi_Type);
    PyModule_AddObject(m, "fi", (PyObject *) &rpmfi_Type);

#ifdef	NOTYET
    Py_INCREF(&rpmfile_Type);
    PyModule_AddObject(m, "file", (PyObject *) &rpmfile_Type);

    Py_INCREF(&rpmfiles_Type);
    PyModule_AddObject(m, "files", (PyObject *) &rpmfiles_Type);
#endif

    Py_INCREF(&rpmKeyring_Type);
    PyModule_AddObject(m, "keyring", (PyObject *) &rpmKeyring_Type);

    Py_INCREF(&rpmmi_Type);
    PyModule_AddObject(m, "mi", (PyObject *) &rpmmi_Type);

#ifdef	NOTYET
    Py_INCREF(&rpmmi_Type);
    PyModule_AddObject(m, "ii", (PyObject *) &rpmii_Type);
#endif

    Py_INCREF(&rpmProblem_Type);
    PyModule_AddObject(m, "prob", (PyObject *) &rpmProblem_Type);

    Py_INCREF(&rpmPubkey_Type);
    PyModule_AddObject(m, "pubkey", (PyObject *) &rpmPubkey_Type);

#ifdef	NOTYET
    Py_INCREF(&rpmstrPool_Type);
    PyModule_AddObject(m, "strpool", (PyObject *) &rpmstrPool_Type);
#endif

#if 0
    Py_INCREF(&rpmtd_Type);
    PyModule_AddObject(m, "td", (PyObject *) &rpmtd_Type);
#endif

    Py_INCREF(&rpmte_Type);
    PyModule_AddObject(m, "te", (PyObject *) &rpmte_Type);

    Py_INCREF(&rpmts_Type);
    PyModule_AddObject(m, "ts", (PyObject *) &rpmts_Type);

#else
    hdr_Type.ob_type = &PyType_Type;
    rpmal_Type.ob_type = &PyType_Type;
    rpmds_Type.ob_type = &PyType_Type;
    rpmfd_Type.ob_type = &PyType_Type;
    rpmfts_Type.ob_type = &PyType_Type;
    rpmfi_Type.ob_type = &PyType_Type;
    rpmmi_Type.ob_type = &PyType_Type;
    rpmps_Type.ob_type = &PyType_Type;
    rpmte_Type.ob_type = &PyType_Type;
    rpmts_Type.ob_type = &PyType_Type;
#endif

    addRpmTags(m);

    PyModule_AddStringConstant(m, "__version__", RPMVERSION);

#ifdef	NOTYET
    PyModule_AddObject(m, "header_magic",
	PyBytes_FromStringAndSize((const char *)rpm_header_magic, 8));
#endif

#define REGISTER_ENUM(val) PyModule_AddIntConstant(m, #val, val)

#ifdef	NOTYET
    REGISTER_ENUM(RPMTAG_NOT_FOUND);
#endif

    REGISTER_ENUM(RPMRC_OK);
    REGISTER_ENUM(RPMRC_NOTFOUND);
    REGISTER_ENUM(RPMRC_FAIL);
    REGISTER_ENUM(RPMRC_NOTTRUSTED);
    REGISTER_ENUM(RPMRC_NOKEY);

    REGISTER_ENUM(RPMFILE_STATE_NORMAL);
    REGISTER_ENUM(RPMFILE_STATE_REPLACED);
    REGISTER_ENUM(RPMFILE_STATE_NOTINSTALLED);
    REGISTER_ENUM(RPMFILE_STATE_NETSHARED);
    REGISTER_ENUM(RPMFILE_STATE_WRONGCOLOR);

    REGISTER_ENUM(RPMFILE_CONFIG);
    REGISTER_ENUM(RPMFILE_DOC);
    REGISTER_ENUM(RPMFILE_ICON);
    REGISTER_ENUM(RPMFILE_MISSINGOK);
    REGISTER_ENUM(RPMFILE_NOREPLACE);
    REGISTER_ENUM(RPMFILE_SPECFILE);
    REGISTER_ENUM(RPMFILE_GHOST);
    REGISTER_ENUM(RPMFILE_LICENSE);
    REGISTER_ENUM(RPMFILE_README);
#ifdef	DYING
    REGISTER_ENUM(RPMFILE_EXCLUDE);
    REGISTER_ENUM(RPMFILE_UNPATCHED);
#endif
    REGISTER_ENUM(RPMFILE_PUBKEY);

    REGISTER_ENUM(RPMDEP_SENSE_REQUIRES);
    REGISTER_ENUM(RPMDEP_SENSE_CONFLICTS);

#ifdef	NOTYET
    REGISTER_ENUM(RPMSENSE_ANY);
#endif
    REGISTER_ENUM(RPMSENSE_LESS);
    REGISTER_ENUM(RPMSENSE_GREATER);
    REGISTER_ENUM(RPMSENSE_EQUAL);
#ifdef	NOTYET
    REGISTER_ENUM(RPMSENSE_POSTTRANS);
    REGISTER_ENUM(RPMSENSE_PREREQ);
    REGISTER_ENUM(RPMSENSE_PRETRANS);
    REGISTER_ENUM(RPMSENSE_INTERP);
#else
 #if defined(RPM_VENDOR_WINDRIVER) || defined(RPM_VENDOR_OE)
    REGISTER_ENUM(RPMSENSE_SCRIPT_PRE);
    REGISTER_ENUM(RPMSENSE_SCRIPT_POST);
    REGISTER_ENUM(RPMSENSE_SCRIPT_PREUN);
    REGISTER_ENUM(RPMSENSE_SCRIPT_POSTUN);
    REGISTER_ENUM(RPMSENSE_SCRIPT_VERIFY);
    REGISTER_ENUM(RPMSENSE_MISSINGOK);
 #endif
    REGISTER_ENUM(RPMSENSE_NOTEQUAL);
#endif
    REGISTER_ENUM(RPMSENSE_FIND_REQUIRES);
    REGISTER_ENUM(RPMSENSE_FIND_PROVIDES);
#ifdef	NOTYET
    REGISTER_ENUM(RPMSENSE_TRIGGERIN);
    REGISTER_ENUM(RPMSENSE_TRIGGERUN);
    REGISTER_ENUM(RPMSENSE_TRIGGERPOSTUN);
    REGISTER_ENUM(RPMSENSE_RPMLIB);
    REGISTER_ENUM(RPMSENSE_TRIGGERPREIN);
    REGISTER_ENUM(RPMSENSE_KEYRING);
    REGISTER_ENUM(RPMSENSE_CONFIG);
#endif
    REGISTER_ENUM(RPMFILE_MISSINGOK);

    REGISTER_ENUM(RPMDEPS_FLAG_NOUPGRADE);
    REGISTER_ENUM(RPMDEPS_FLAG_NOREQUIRES);
    REGISTER_ENUM(RPMDEPS_FLAG_NOCONFLICTS);
    REGISTER_ENUM(RPMDEPS_FLAG_NOOBSOLETES);
    REGISTER_ENUM(RPMDEPS_FLAG_NOPARENTDIRS);
    REGISTER_ENUM(RPMDEPS_FLAG_NOLINKTOS);
    REGISTER_ENUM(RPMDEPS_FLAG_ANACONDA);
    REGISTER_ENUM(RPMDEPS_FLAG_NOSUGGEST);
    REGISTER_ENUM(RPMDEPS_FLAG_DEPLOOPS);

    REGISTER_ENUM(RPMTRANS_FLAG_TEST);
    REGISTER_ENUM(RPMTRANS_FLAG_BUILD_PROBS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSCRIPTS);
    REGISTER_ENUM(RPMTRANS_FLAG_JUSTDB);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERS);
    REGISTER_ENUM(RPMTRANS_FLAG_NODOCS);
    REGISTER_ENUM(RPMTRANS_FLAG_ALLFILES);
#ifdef NOYET
    REGISTER_ENUM(RPMTRANS_FLAG_NOPLUGINS);
    REGISTER_ENUM(RPMTRANS_FLAG_KEEPOBSOLETE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOCONTEXTS);
    REGISTER_ENUM(RPMTRANS_FLAG_REPACKAGE);
    REGISTER_ENUM(RPMTRANS_FLAG_REVERSE);
#else
    REGISTER_ENUM(RPMTRANS_FLAG_NORPMDB);
    REGISTER_ENUM(RPMTRANS_FLAG_REPACKAGE);
#endif
    REGISTER_ENUM(RPMTRANS_FLAG_NOPRE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOST);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERPREIN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERIN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPREUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOSTUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERPOSTUN);
#ifdef	NOTYET
    REGISTER_ENUM(RPMTRANS_FLAG_NOPRETRANS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOSTTRANS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOMD5);
    REGISTER_ENUM(RPMTRANS_FLAG_NOFILEDIGEST);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSUGGEST);
    REGISTER_ENUM(RPMTRANS_FLAG_ADDINDEPS);
#else
    REGISTER_ENUM(RPMTRANS_FLAG_NOFDIGESTS);
    REGISTER_ENUM(RPMDEPS_FLAG_ADDINDEPS);
#endif
    REGISTER_ENUM(RPMTRANS_FLAG_NOCONFIGS);

    REGISTER_ENUM(RPMPROB_FILTER_IGNOREOS);
    REGISTER_ENUM(RPMPROB_FILTER_IGNOREARCH);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEPKG);
    REGISTER_ENUM(RPMPROB_FILTER_FORCERELOCATE);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACENEWFILES);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEOLDFILES);
    REGISTER_ENUM(RPMPROB_FILTER_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKSPACE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKNODES);

    REGISTER_ENUM(RPMCALLBACK_UNKNOWN);
    REGISTER_ENUM(RPMCALLBACK_INST_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_INST_START);
    REGISTER_ENUM(RPMCALLBACK_INST_OPEN_FILE);
    REGISTER_ENUM(RPMCALLBACK_INST_CLOSE_FILE);
    REGISTER_ENUM(RPMCALLBACK_TRANS_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_TRANS_START);
    REGISTER_ENUM(RPMCALLBACK_TRANS_STOP);
    REGISTER_ENUM(RPMCALLBACK_UNINST_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_UNINST_START);
    REGISTER_ENUM(RPMCALLBACK_UNINST_STOP);
    REGISTER_ENUM(RPMCALLBACK_REPACKAGE_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_REPACKAGE_START);
    REGISTER_ENUM(RPMCALLBACK_REPACKAGE_STOP);
    REGISTER_ENUM(RPMCALLBACK_UNPACK_ERROR);
    REGISTER_ENUM(RPMCALLBACK_CPIO_ERROR);
    REGISTER_ENUM(RPMCALLBACK_SCRIPT_ERROR);
    REGISTER_ENUM(RPMCALLBACK_SCRIPT_START);
    REGISTER_ENUM(RPMCALLBACK_SCRIPT_STOP);
    REGISTER_ENUM(RPMCALLBACK_INST_STOP);

    REGISTER_ENUM(RPMPROB_BADARCH);
    REGISTER_ENUM(RPMPROB_BADOS);
    REGISTER_ENUM(RPMPROB_PKG_INSTALLED);
    REGISTER_ENUM(RPMPROB_BADRELOCATE);
    REGISTER_ENUM(RPMPROB_REQUIRES);
    REGISTER_ENUM(RPMPROB_CONFLICT);
    REGISTER_ENUM(RPMPROB_NEW_FILE_CONFLICT);
    REGISTER_ENUM(RPMPROB_FILE_CONFLICT);
    REGISTER_ENUM(RPMPROB_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_DISKSPACE);
    REGISTER_ENUM(RPMPROB_DISKNODES);
#ifdef	NOTYET
    REGISTER_ENUM(RPMPROB_OBSOLETES);
#else
    REGISTER_ENUM(RPMPROB_BADPRETRANS);
#endif

    REGISTER_ENUM(VERIFY_DIGEST);
    REGISTER_ENUM(VERIFY_SIGNATURE);

    REGISTER_ENUM(RPMLOG_EMERG);
    REGISTER_ENUM(RPMLOG_ALERT);
    REGISTER_ENUM(RPMLOG_CRIT);
    REGISTER_ENUM(RPMLOG_ERR);
    REGISTER_ENUM(RPMLOG_WARNING);
    REGISTER_ENUM(RPMLOG_NOTICE);
    REGISTER_ENUM(RPMLOG_INFO);
    REGISTER_ENUM(RPMLOG_DEBUG);

    REGISTER_ENUM(RPMMIRE_DEFAULT);
    REGISTER_ENUM(RPMMIRE_STRCMP);
    REGISTER_ENUM(RPMMIRE_REGEX);
    REGISTER_ENUM(RPMMIRE_GLOB);

    REGISTER_ENUM(RPMVSF_DEFAULT);
    REGISTER_ENUM(RPMVSF_NOHDRCHK);
    REGISTER_ENUM(RPMVSF_NEEDPAYLOAD);
    REGISTER_ENUM(RPMVSF_NOSHA1HEADER);
    REGISTER_ENUM(RPMVSF_NOMD5HEADER);
    REGISTER_ENUM(RPMVSF_NODSAHEADER);
    REGISTER_ENUM(RPMVSF_NORSAHEADER);
    REGISTER_ENUM(RPMVSF_NOECDSAHEADER);
    REGISTER_ENUM(RPMVSF_NOSHA1);
    REGISTER_ENUM(RPMVSF_NOMD5);
    REGISTER_ENUM(RPMVSF_NODSA);
    REGISTER_ENUM(RPMVSF_NORSA);
    REGISTER_ENUM(RPMVSF_NOECDSA);

    REGISTER_ENUM(TR_ADDED);
    REGISTER_ENUM(TR_REMOVED);

    REGISTER_ENUM(RPMDBI_PACKAGES);
#ifdef	NOTYET
    REGISTER_ENUM(RPMDBI_LABEL);
    REGISTER_ENUM(RPMDBI_INSTFILENAMES);
    REGISTER_ENUM(RPMDBI_NAME);
    REGISTER_ENUM(RPMDBI_BASENAMES);
    REGISTER_ENUM(RPMDBI_GROUP);
    REGISTER_ENUM(RPMDBI_REQUIRENAME);
    REGISTER_ENUM(RPMDBI_PROVIDENAME);
    REGISTER_ENUM(RPMDBI_CONFLICTNAME);
    REGISTER_ENUM(RPMDBI_OBSOLETENAME);
    REGISTER_ENUM(RPMDBI_TRIGGERNAME);
    REGISTER_ENUM(RPMDBI_DIRNAMES);
    REGISTER_ENUM(RPMDBI_INSTALLTID);
    REGISTER_ENUM(RPMDBI_SIGMD5);
    REGISTER_ENUM(RPMDBI_SHA1HEADER);

    REGISTER_ENUM(HEADERCONV_EXPANDFILELIST);
    REGISTER_ENUM(HEADERCONV_COMPRESSFILELIST);
    REGISTER_ENUM(HEADERCONV_RETROFIT_V3);
#else
    REGISTER_ENUM((long)RPMAL_NOMATCH);
#endif

#ifdef	DYING
rpmIncreaseVerbosity();
rpmIncreaseVerbosity();
#endif
    return 1;
}

#if PY_MAJOR_VERSION >= 3
static int rpmModuleTraverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(pyrpmError);
    return 0;
}

static int rpmModuleClear(PyObject *m) {
    Py_CLEAR(pyrpmError);
    return 0;
}

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_rpm",            /* m_name */
    rpm__doc__,        /* m_doc */
    0,                 /* m_size */
    rpmModuleMethods,
    NULL,              /* m_reload */
    rpmModuleTraverse,
    rpmModuleClear,
    NULL               /* m_free */
};

PyObject *
PyInit__rpm(void)
{
    PyObject * m;
    if (!prepareInitModule()) return NULL;
    m = PyModule_Create(&moduledef);
    initModule(m);
    return m;
}
#else
void init_rpm(void);
void init_rpm(void)
{
    PyObject * m;

    if (!prepareInitModule()) return;
    m = Py_InitModule3("_rpm", rpmModuleMethods, rpm__doc__);
    if (m == NULL)
        return;
    initModule(m);
}
#endif
