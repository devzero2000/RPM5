#ifndef H_RPMCUDF_JS
#define H_RPMCUDF_JS

/**
 * \file js/rpmcudf-js.h
 */

#include "rpm-js.h"

extern JSClass rpmcudfClass;

#ifdef __cplusplus
extern "C" {
#endif

extern JSObject *
rpmjs_InitCudfClass(JSContext *cx, JSObject* obj);

extern JSObject *
rpmjs_NewCudfObject(JSContext *cx);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMCUDF_JS */
