#ifndef H_RPMDBE_JS
#define H_RPMDBE_JS

/**
 * \file js/rpmdbe-js.h
 */

#include "rpm-js.h"

extern JSClass rpmdbeClass;

#ifdef __cplusplus
extern "C" {
#endif

extern JSObject *
rpmjs_InitDbeClass(JSContext *cx, JSObject* obj);

extern JSObject *
rpmjs_NewDbeObject(JSContext *cx);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMDBE_JS */
