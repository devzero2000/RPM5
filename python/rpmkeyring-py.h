#ifndef H_RPMKEYRING_PY
#define H_RPMKEYRING_PY

#include <rpmkeyring.h>

/** \ingroup py_c
 * \file python/rpmkeyring-py.h
 */

typedef struct rpmPubkeyObject_s rpmPubkeyObject;
typedef struct rpmKeyringObject_s rpmKeyringObject;

extern PyTypeObject rpmPubkey_Type;
extern PyTypeObject rpmKeyring_Type;

PyObject * rpmPubkey_Wrap(PyTypeObject *subtype, rpmPubkey pubkey);
PyObject * rpmKeyring_Wrap(PyTypeObject *subtype, rpmKeyring keyring);

int rpmKeyringFromPyObject(PyObject *item, rpmKeyring *keyring);
#endif	/* H_RPMKEYRING_PY */
