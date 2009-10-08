/* This file is modified from the template file logio_template.  */

#include "system.h"

#include <rpmio.h>

#include <db.h>
#include "logio.h"

#include "debug.h"

int
logio_Mkdir_recover(DB_ENV * dbenv, DBT * dbtp, DB_LSN * lsnp, db_recops op)
{
    logio_Mkdir_args * argp = NULL;
    int ret = EINVAL;

#ifdef DEBUG_RECOVER
    logio_Mkdir_print(dbenv, dbtp, lsnp, op);
#endif
    if ((ret = logio_Mkdir_read(dbenv, dbtp->data, &argp)) != 0)
	goto exit;

    switch (op) {
    case DB_TXN_ABORT:
    case DB_TXN_BACKWARD_ROLL:
	/*
	 * If we're aborting, we need to remove the directory if it
	 * exists.  We log the trailing zero in pathnames, so we can
	 * simply pass the data part of the DBT into Rmdir as a string.
	 * (Note that we don't have any alignment guarantees, but for
	 * a char * this doesn't matter.)
	 *
	 * Ignore all errors other than ENOENT;  DB may attempt to undo
	 * or redo operations without knowing whether they have already
	 * been done or undone, so we should never assume in a recovery
	 * function that the task definitely needs doing or undoing.
	 */
	ret = Rmdir(argp->path.data);
	if (ret != 0 && errno != ENOENT)
	    dbenv->err(dbenv, ret, "Mkdir: DB_TXN_BACKWARD_ROLL");
	else
	    ret = 0;
	break;
    case DB_TXN_APPLY:
    case DB_TXN_FORWARD_ROLL:
	/*
	 * The forward direction is just the opposite;  here, we ignore
	 * EEXIST, because the directory may already exist.
	 */
	ret = Mkdir(argp->path.data, argp->mode);
	if (ret != 0 && errno != EEXIST)
	    dbenv->err(dbenv, ret, "Mkdir: DB_TXN_FORWARD_ROLL");
	else
	    ret = 0;
	break;
    case DB_TXN_PRINT:
	if (ret != 0)
	    dbenv->err(dbenv, ret, "Mkdir: DB_TXN_PRINT");
	else
	    ret = 0;
	break;
    default:
	if (ret != 0)
	    dbenv->err(dbenv, ret, "Mkdir: UNKNOWN");
	else
	    ret = 0;
	break;
    }

   /*
    * The recovery function is responsible for returning the LSN of the
    * previous log record in this transaction, so that transaction aborts
    * can follow the chain backwards.
    *
    * (If we'd wanted the LSN of this record earlier, we could have
    * read it from lsnp, as well--but because we weren't working with
    * pages or other objects that store their LSN and base recovery
    * decisions on it, we didn't need to.)
    */
    *lsnp = argp->prev_lsn;

exit:
    if (argp != NULL)
	free(argp);
    return ret;
}

/*
 * logio_Rmdir_recover --
 *	Recovery function for Rmdir.
 *
 * PUBLIC: int logio_Rmdir_recover
 * PUBLIC:   __P((dbenv *, DBT *, DB_LSN *, db_recops));
 */
int
logio_Rmdir_recover(DB_ENV * dbenv, DBT * dbtp, DB_LSN * lsnp, db_recops op)
{
    logio_Rmdir_args *argp = NULL;
    int ret = EINVAL;

#ifdef DEBUG_RECOVER
    (void)logio_Rmdir_print(dbenv, dbtp, lsnp, op);
#endif
    if ((ret = logio_Rmdir_read(dbenv, dbtp->data, &argp)) != 0)
	goto exit;

    switch (op) {
    case DB_TXN_ABORT:
    case DB_TXN_BACKWARD_ROLL:
	ret = Mkdir(argp->path.data, argp->mode);
	if (ret != 0 && errno != EEXIST)
	    dbenv->err(dbenv, ret, "Rmdir: DB_TXN_BACKWARD_ROLL");
	else
	    ret = 0;
	break;
    case DB_TXN_APPLY:
    case DB_TXN_FORWARD_ROLL:
	ret = Rmdir(argp->path.data);
	if (ret != 0 && errno != ENOENT)
	    dbenv->err(dbenv, ret, "Rmdir: DB_TXN_FORWARD_ROLL");
	else
	    ret = 0;
	break;
    case DB_TXN_PRINT:
	if (ret != 0)
	    dbenv->err(dbenv, ret, "Rmdir: DB_TXN_PRINT");
	else
	    ret = 0;
	break;
    default:
	if (ret != 0)
	    dbenv->err(dbenv, ret, "Rmdir: UNKNOWN");
	else
	    ret = 0;
	break;
    }

    /* Allow for following LSN pointers through a transaction. */
    *lsnp = argp->prev_lsn;

exit:
    if (argp != NULL)
	free(argp);
    return ret;
}
