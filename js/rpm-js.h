#ifndef H_RPM_JS
#define H_RPM_JS

/**
 * \file js/rpm-js.h
 */

#include <rpmiotypes.h> 
#include <rpmio.h>

#include <rpmtypes.h>
#include <rpmtag.h>

#define	XP_UNIX	1
#include "jsapi.h"
#include "jscntxt.h"	/* XXX JSFRAME_CONSTRUCTING */
#include "jsarray.h"	/* XXX js_ArrayClass */

#ifdef	NOTYET
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsutil.h" /* Added by JSIFY */
#endif

#ifndef JS_FS
#define JS_FS(name,call,nargs,flags,extra) \
    {name, call, nargs, flags, extra}
#endif
#ifndef JS_FS_END
#define JS_FS_END JS_FS(NULL,NULL,0,0,0)
#endif

#endif	/* H_RPM_JS */
