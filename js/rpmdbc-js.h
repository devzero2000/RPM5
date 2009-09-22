#ifndef H_RPMDBC_JS
#define H_RPMDBC_JS

/**
 * \file js/rpmdbc-js.h
 */

#include "rpm-js.h"

extern JSClass rpmdbcClass;

#ifdef __cplusplus
extern "C" {
#endif

extern JSObject *
rpmjs_InitDbcClass(JSContext *cx, JSObject* obj);

extern JSObject *
rpmjs_NewDbcObject(JSContext *cx);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMDBC_JS */
