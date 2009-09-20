#ifndef H_RPMDB_JS
#define H_RPMDB_JS

/**
 * \file js/rpmdb-js.h
 */

#include "rpm-js.h"

extern JSClass rpmdbClass;

#ifdef __cplusplus
extern "C" {
#endif

extern JSObject *
rpmjs_InitDbClass(JSContext *cx, JSObject* obj);

extern JSObject *
rpmjs_NewDbObject(JSContext *cx);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMDB_JS */
