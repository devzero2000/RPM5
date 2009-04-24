#ifndef H_RPMDS_JS
#define H_RPMDS_JS

/**
 * \file js/rpmds-js.h
 */

#include "rpm-js.h"

extern JSClass rpmdsClass;

#ifdef __cplusplus
extern "C" {
#endif

extern JSObject *
rpmjs_InitDsClass(JSContext *cx, JSObject* obj);

extern JSObject *
rpmjs_NewDsObject(JSContext *cx, JSObject *o, int _tagN);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMDS_JS */
