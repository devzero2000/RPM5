#ifndef _RPMKEYRING_PY_H
#define _RPMKEYRING_PY_H

#include <Python.h>
#include <rpm/rpmtypes.h>

/** \ingroup py_c
 * \file python/rpmkeyring-py.h
 */

/** \ingroup py_c
 */
typedef struct rpmPubkeyObject_s rpmPubkeyObject;
typedef struct rpmKeyringObject_s rpmKeyringObject;

/** \ingroup py_c
 */
struct rpmPubkeyObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmPubkey pubkey;
};
struct rpmKeyringObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmKeyring keyring;
};

extern PyTypeObject rpmKeyring_Type;
extern PyTypeObject rpmPubkey_Type;

rpmKeyringObject * rpmKeyring_Wrap(rpmKeyring keyring);
#endif
