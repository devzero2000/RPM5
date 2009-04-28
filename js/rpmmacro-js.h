#ifndef H_RPMMACRO_JS
#define H_RPMMACRO_JS

/**
 * \file js/rpmmacro-js.h
 */

#include "rpm-js.h"

extern JSClass rpmmacroClass;

#ifdef __cplusplus
extern "C" {
#endif

extern JSObject *
rpmjs_InitMacroClass(JSContext *cx, JSObject* obj);

extern JSObject *
rpmjs_NewMacroObject(JSContext *cx, JSObject *o);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMMACRO_JS */
