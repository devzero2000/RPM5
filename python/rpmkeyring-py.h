#ifndef H_RPMKEYRING_PY
#define H_RPMKEYRING_PY

#include "rpmkeyring.h"

/** \ingroup py_c
 * \file python/rpmkeyring-py.h
 */

typedef struct rpmPubkeyObject_s rpmPubkeyObject;

typedef struct rpmKeyringObject_s rpmKeyringObject;

extern PyTypeObject rpmKeyring_Type;

extern PyTypeObject rpmPubkey_Type;

PyObject * rpmKeyring_Wrap(PyTypeObject *subtype, rpmKeyring keyring);

#endif	/* H_RPMKEYRING_PY */
