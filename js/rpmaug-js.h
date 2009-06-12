#ifndef H_RPMAUG_JS
#define H_RPMAUG_JS

/**
 * \file js/rpmaug-js.h
 */

#include "rpm-js.h"

extern JSClass rpmaugClass;

#ifdef __cplusplus
extern "C" {
#endif

extern JSObject *
rpmjs_InitAugClass(JSContext *cx, JSObject* obj);

extern JSObject *
rpmjs_NewAugObject(JSContext *cx);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMAUG_JS */
