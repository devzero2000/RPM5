#ifndef RPMLUA_H
#define RPMLUA_H

typedef enum rpmluavType_e {
    RPMLUAV_NIL		= 0,
    RPMLUAV_STRING	= 1,
    RPMLUAV_NUMBER	= 2
} rpmluavType;

#if defined(_RPMLUA_INTERNAL)

#include <stdarg.h>
#include <lua.h>

struct rpmlua_s {
    lua_State *L;
    int pushsize;
    int storeprint;
    int printbufsize;
    int printbufused;
/*@relnull@*/
    char *printbuf;
};

struct rpmluav_s {
    rpmluavType keyType;
    rpmluavType valueType;
    union {
	const char *str;
	const void *ptr;
	double num;
    } key;
    union {
	const char *str;
	const void *ptr;
	double num;
    } value;
    int listmode;
};

#endif /* _RPMLUA_INTERNAL */

typedef /*@abstract@*/ struct rpmlua_s * rpmlua;
typedef /*@abstract@*/ struct rpmluav_s * rpmluav;

/*@-exportlocal@*/
/*@only@*/
rpmlua rpmluaNew(void)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;
/*@=exportlocal@*/
void *rpmluaFree(/*@only@*/ rpmlua lua)
	/*@globals internalState @*/
	/*@modifies lua, internalState @*/;

int rpmluaCheckScript(/*@null@*/ rpmlua lua, const char *script,
		      /*@null@*/ const char *name)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
int rpmluaRunScript(/*@null@*/ rpmlua lua, const char *script,
		    /*@null@*/ const char *name)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
/*@-exportlocal@*/
int rpmluaRunScriptFile(/*@null@*/ rpmlua lua, const char *filename)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
/*@=exportlocal@*/
void rpmluaInteractive(/*@null@*/ rpmlua lua)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;

void *rpmluaGetData(/*@null@*/ rpmlua lua, const char *key)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
/*@-exportlocal@*/
void rpmluaSetData(/*@null@*/ rpmlua lua, const char *key, const void *data)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
/*@=exportlocal@*/

/*@exposed@*/
const char *rpmluaGetPrintBuffer(/*@null@*/ rpmlua lua)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
void rpmluaSetPrintBuffer(/*@null@*/ rpmlua lua, int flag)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;

void rpmluaGetVar(/*@null@*/ rpmlua lua, rpmluav var)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, var, fileSystem, internalState @*/;
void rpmluaSetVar(/*@null@*/ rpmlua lua, rpmluav var)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, var, fileSystem, internalState @*/;
void rpmluaDelVar(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
int rpmluaVarExists(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
void rpmluaPushTable(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;
void rpmluaPop(/*@null@*/ rpmlua lua)
	/*@globals fileSystem, internalState @*/
	/*@modifies lua, fileSystem, internalState @*/;

/*@only@*/
rpmluav rpmluavNew(void)
	/*@*/;
void * rpmluavFree(/*@only@*/ rpmluav var)
	/*@modifes var @*/;
void rpmluavSetListMode(rpmluav var, int flag)
	/*@modifies var @*/;
/*@-exportlocal@*/
void rpmluavSetKey(rpmluav var, rpmluavType type, const void *value)
	/*@modifies var @*/;
/*@=exportlocal@*/
/*@-exportlocal@*/
void rpmluavSetValue(rpmluav var, rpmluavType type, const void *value)
	/*@modifies var @*/;
/*@=exportlocal@*/
/*@-exportlocal@*/
void rpmluavGetKey(rpmluav var, /*@out@*/ rpmluavType *type, /*@out@*/ void **value)
	/*@modifies *type, *value @*/;
/*@=exportlocal@*/
/*@-exportlocal@*/
void rpmluavGetValue(rpmluav var, /*@out@*/ rpmluavType *type, /*@out@*/ void **value)
	/*@modifies *type, *value @*/;
/*@=exportlocal@*/

/* Optional helpers for numbers. */
void rpmluavSetKeyNum(rpmluav var, double value)
	/*@modifies var @*/;
void rpmluavSetValueNum(rpmluav var, double value)
	/*@modifies var @*/;
double rpmluavGetKeyNum(rpmluav var)
	/*@*/;
double rpmluavGetValueNum(rpmluav var)
	/*@*/;
int rpmluavKeyIsNum(rpmluav var)
	/*@*/;
int rpmluavValueIsNum(rpmluav var)
	/*@*/;

#endif /* RPMLUA_H */
