
#include "system.h"

#include <rpmio.h>
#include <poptIO.h>

#include "db.h"
#include "db_int.h"
#include "dbinc/db_swap.h"

#include "logio.h"

#include "debug.h"

/*
 * Sample application dispatch function to handle user-specified log record
 * types.
 */
static int
apprec_dispatch(DB_ENV * dbenv, DBT * dbt, DB_LSN * lsn, db_recops op)
{
    uint32_t rectype;

    /* Pull the record type out of the log record. */
    LOGCOPY_32(dbenv->env, &rectype, dbt->data);

    switch (rectype) {
    case DB_logio_Mkdir:
	return logio_Mkdir_recover(dbenv, dbt, lsn, op);
    case DB_logio_Rmdir:
	return logio_Rmdir_recover(dbenv, dbt, lsn, op);
    default:
	/* We've hit an unexpected, allegedly user-defined record type.  */
	dbenv->errx(dbenv, "Unexpected log record type encountered");
	return EINVAL;
    }
}

static int
open_env(const char * home, FILE * fp, DB_ENV ** dbenvp)
{
    DB_ENV *dbenv;
    uint32_t eflags;
    int ret;

    if (fp == NULL) fp = stderr;

    /* Create an environment initialized for error reporting. */
    if ((ret = db_env_create(&dbenv, 0)) != 0) {
	fprintf(fp, "%s: db_env_create: %s\n", __progname, db_strerror(ret));
	return ret;
    }
    dbenv->set_errfile(dbenv, fp);
    dbenv->set_errpfx(dbenv, __progname);

    /* Set up our custom recovery dispatch function. */
    if ((ret = dbenv->set_app_dispatch(dbenv, apprec_dispatch)) != 0) {
	dbenv->err(dbenv, ret, "DB_ENV->set_app_dispatch");
	return ret;
    }

   /* Open the environment with full transactional support, running recovery. */
    eflags = DB_CREATE | DB_RECOVER |
	DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN;
    if ((ret = dbenv->open(dbenv, home, eflags, 0)) != 0) {
	dbenv->err(dbenv, ret, "DB_ENV->open: %s", home);
	dbenv->close(dbenv, 0);
	return ret;
    }

    *dbenvp = dbenv;
    return 0;
}

static int
verify(const char * DN, mode_t mode)
{
    struct stat sb;
    if (Lstat(DN, &sb))
	return errno;
    return (sb.st_mode == mode ? 0 : -1);
}

static const char *home = "HOME";
static const char * dir = "DIR";
static mode_t mode = 0755;

static struct poptOption _optionsTable[] = {

  { "home", 'h', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&home, 0,
	N_("Specify DIR for database environment"), N_("DIR") },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

static struct poptOption *optionsTable = &_optionsTable[0];

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    const char * DN = rpmGetPath(home, "/", dir, NULL);
    DB_ENV *dbenv;
    DB_TXN *txn;
    DB_LSN lsn;
    DBT DNdbt = {0};
    int ret;
    int ec = EXIT_FAILURE;	/* assume failure */

    if (verify(home, S_IFDIR|mode) == ENOENT)
	(void) Mkdir(home, mode);

    if ((ret = open_env(home, NULL, &dbenv)) != 0)
	goto exit;

   /*
    * Remember, always log actions before you execute them!
    * Since this log record is describing a file system operation and
    * we have no control over when file system operations go to disk,
    * we need to flush the log record immediately to ensure that the
    * log record is on disk before the operation it describes.  The
    * flush would not be necessary were we doing an operation into the
    * BDB mpool and using LSNs that mpool knew about.
    */
    printf("INIT: Mkdir transaction logged and directory created: ");
    if ((ret = dbenv->txn_begin(dbenv, NULL, &txn, 0)) != 0) {
	dbenv->err(dbenv, ret, "txn_begin");
	goto exit;
    }
    DNdbt.data = (void *)DN;
    DNdbt.size = strlen(DN) + 1;	/* trailing NUL too */
    memset(&lsn, 0, sizeof(lsn));
    if ((ret = logio_Mkdir_log(dbenv, txn, &lsn, DB_FLUSH, &DNdbt, mode)) != 0) {
	dbenv->err(dbenv, ret, "Mkdir_log");
	goto exit;
    }
    if (Mkdir(DN, mode) != 0) {
	dbenv->err(dbenv, errno, "Mkdir");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == 0 ? "ok" : "FAIL"));

    printf("TEST: directory is removed by transaction abort: ");
    if ((ret = txn->abort(txn)) != 0) {
	dbenv->err(dbenv, ret, "TXN->abort");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == ENOENT ? "ok" : "FAIL"));

    /* Repeat, with a commit this time. */
    printf("INIT: Mkdir transaction logged and directory created: ");
    DNdbt.data = (void *)DN;
    DNdbt.size = strlen(DN) + 1;
    if ((ret = dbenv->txn_begin(dbenv, NULL, &txn, 0)) != 0) {
	dbenv->err(dbenv, ret, "DB_ENV->txn_begin");
	goto exit;
    }
    memset(&lsn, 0, sizeof(lsn));
    if ((ret = logio_Mkdir_log(dbenv, txn, &lsn, 0, &DNdbt, mode)) != 0) {
	dbenv->err(dbenv, ret, "Mkdir_log");
	goto exit;
    }
    if (Mkdir(DN, mode) != 0) {
	dbenv->err(dbenv, errno, "Mkdir");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == 0 ? "ok" : "FAIL"));

    /* Commit the transaction and verify that the creation persists. */
    printf("TEST: creation persists after transaction commit: ");
    if ((ret = txn->commit(txn, 0)) != 0) {
	dbenv->err(dbenv, ret, "TXN->commit");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == 0 ? "ok" : "FAIL"));

    printf("INIT: Close dbenv and remove directory: ");
    if ((ret = dbenv->close(dbenv, 0)) != 0) {
	fprintf(stderr, "DB_ENV->close: %s\n", db_strerror(ret));
	goto exit;
    }
    if (Rmdir(DN) != 0) {
	fprintf(stderr, "%s: Rmdir failed with error %s", __progname,
	    strerror(errno));
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == ENOENT ? "ok" : "FAIL"));

    printf("TEST: directory is re-created opening dbenv with DB_RECOVER: ");
    if ((ret = open_env(home, stderr, &dbenv)) != 0)
	goto exit;
    printf("%s\n", (verify(DN, S_IFDIR|mode) == 0 ? "ok" : "FAIL"));

    printf("INIT: Rmdir transaction logged and directory removed: ");
    if ((ret = dbenv->txn_begin(dbenv, NULL, &txn, 0)) != 0) {
	dbenv->err(dbenv, ret, "txn_begin");
	goto exit;
    }
    DNdbt.data = (void *)DN;
    DNdbt.size = strlen(DN) + 1;	/* trailing NUL too */
    memset(&lsn, 0, sizeof(lsn));
    if ((ret = logio_Rmdir_log(dbenv, txn, &lsn, DB_FLUSH, &DNdbt, mode)) != 0) {
	dbenv->err(dbenv, ret, "Rmdir_log");
	goto exit;
    }
    if (Rmdir(DN) != 0) {
	dbenv->err(dbenv, errno, "Rmdir");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == ENOENT ? "ok" : "FAIL"));

    printf("TEST: directory is re-created by transaction abort: ");
    if ((ret = txn->abort(txn)) != 0) {
	dbenv->err(dbenv, ret, "TXN->abort");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == 0 ? "ok" : "FAIL"));

    /* Repeat, with a commit this time. */
    printf("INIT: Rmdir transaction logged and directory removed: ");
    DNdbt.data = (void *)DN;
    DNdbt.size = strlen(DN) + 1;
    if ((ret = dbenv->txn_begin(dbenv, NULL, &txn, 0)) != 0) {
	dbenv->err(dbenv, ret, "DB_ENV->txn_begin");
	goto exit;
    }
    memset(&lsn, 0, sizeof(lsn));
    if ((ret = logio_Rmdir_log(dbenv, txn, &lsn, 0, &DNdbt, mode)) != 0) {
	dbenv->err(dbenv, ret, "Rmdir_log");
	goto exit;
    }
    if (Rmdir(DN) != 0) {
	dbenv->err(dbenv, errno, "Rmdir");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == ENOENT ? "ok" : "FAIL"));

    /* Commit the transaction and verify that the removal persists. */
    printf("TEST: removal persists after transaction commit: ");
    if ((ret = txn->commit(txn, 0)) != 0) {
	dbenv->err(dbenv, ret, "TXN->commit");
	goto exit;
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == ENOENT ? "ok" : "FAIL"));

    printf("INIT: Close dbenv and create directory: ");
    if ((ret = dbenv->close(dbenv, 0)) != 0) {
	fprintf(stderr, "DB_ENV->close: %s\n", db_strerror(ret));
	goto exit;
    }
    if (Mkdir(DN, mode) != 0) {
	fprintf(stderr, "%s: Mkdir failed with error %s", __progname,
	    strerror(errno));
    }
    printf("%s\n", (verify(DN, S_IFDIR|mode) == 0 ? "ok" : "FAIL"));

    printf("TEST: directory is removed opening dbenv with DB_RECOVER: ");
    if ((ret = open_env(home, stderr, &dbenv)) != 0)
	goto exit;
    printf("%s\n", (verify(DN, S_IFDIR|mode) == ENOENT ? "ok" : "FAIL"));

    if ((ret = dbenv->close(dbenv, 0)) != 0) {
	fprintf(stderr, "DB_ENV->close: %s\n", db_strerror(ret));
	goto exit;
    }
    ec = EXIT_SUCCESS;

exit:
    DN = _free(DN);
    optCon = rpmioFini(optCon);
    return ec;
}
