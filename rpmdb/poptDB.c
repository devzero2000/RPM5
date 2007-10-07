/** \ingroup rpmcli
 * \file rpmdb/poptDB.c
 *  Popt tables for database modes.
 */

#include "system.h"

#include <rpmio.h>
#include <rpmcli.h>

#include "debug.h"

struct rpmQVKArguments_s rpmDBArgs;

/**
 */
struct poptOption rpmDatabasePoptTable[] = {
#if defined(SUPPORT_INITDB)
 { "initdb", '\0', POPT_ARG_VAL, &rpmDBArgs.init, 1,
	N_("initialize database"), NULL},
#endif
 { "rebuilddb", '\0', POPT_ARG_VAL, &rpmDBArgs.rebuild, 1,
	N_("rebuild database inverted lists from installed package headers"),
	NULL},
#if defined(SUPPORT_VERIFYDB)
 { "verifydb", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &rpmDBArgs.verify, 1,
	N_("verify database files"), NULL},
#endif

   POPT_TABLEEND
};
