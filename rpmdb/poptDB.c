/** \ingroup rpmcli
 * \file rpmdb/poptDB.c
 *  Popt tables for database modes.
 */

#include "system.h"

#include <rpmiotypes.h>

#include <rpmtypes.h>
#include <rpmtag.h>
#include <rpmdb.h>
#include <rpmtxn.h>

#include <rpmcli.h>	/* XXX rpmQVKArguments_s, <popt.h> */

#include "debug.h"

/*@unchecked@*/
extern int _dbi_debug;

struct rpmQVKArguments_s rpmDBArgs;

/**
 */
struct poptOption rpmDatabasePoptTable[] = {

 { "rebuilddb", '\0', POPT_ARG_VAL, &rpmDBArgs.rebuild, 1,
	N_("rebuild database inverted lists from installed package headers"),
	NULL},

 { "rpmdbdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmdb_debug, -1,
	N_("Debug rpmdb DataBase"), NULL},
 { "rpmdbidebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dbi_debug, -1,
	N_("Debug dbiIndex DataBase Index"), NULL},
 { "rpmtxndebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmtxn_debug, -1,
	N_("Debug rpmtxn database Transaction"), NULL},

   POPT_TABLEEND
};
