#ifndef rpmsql_h
#define RPMSQL_H

/** \ingroup rpmio
 * \file rpmio/rpmsql.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>
#include <argv.h>

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmsql_s * rpmsql;

/*@unchecked@*/
extern int _rpmsql_debug;

/*@unchecked@*/
extern rpmsql _rpmsqlI;

#if defined(_RPMSQL_INTERNAL)

#define F_ISSET(_sql, _FLAG) ((_sql)->flags & (RPMSQL_FLAGS_##_FLAG))
#define SQLDBG(_l) if (_rpmsql_debug) fprintf _l

/**
 * Interpreter flags.
 */
enum rpmsqlFlags_e {
    RPMSQL_FLAGS_NONE		= 0,
    RPMSQL_FLAGS_INTERACTIVE	= (1 <<  0),	/*    -interactive */
    RPMSQL_FLAGS_BAIL		= (1 <<  1),	/*    -bail */

    RPMSQL_FLAGS_ECHO		= (1 << 16),	/*    -echo */
    RPMSQL_FLAGS_SHOWHDR	= (1 << 17),	/*    -[no]header */
    RPMSQL_FLAGS_WRITABLE	= (1 << 18),	/* PRAGMA writable_schema */
};

/**
 * Operational modes.
 */
enum rpmsqlModes_e {
    RPMSQL_MODE_LINE	= 0,	/* One column per line.  Blank line between records */
    RPMSQL_MODE_COLUMN	= 1,	/* One record per line in neat columns */
    RPMSQL_MODE_LIST	= 2,	/* One record per line with a separator */
    RPMSQL_MODE_SEMI	= 3,	/* Same as MODE_LIST but append ";" to each line */
    RPMSQL_MODE_HTML	= 4,	/* Generate an XHTML table */
    RPMSQL_MODE_INSERT	= 5,	/* Generate SQL "insert" statements */
    RPMSQL_MODE_TCL	= 6,	/* Generate ANSI-C or TCL quoted elements */
    RPMSQL_MODE_CSV	= 7,	/* Quote strings, numbers are plain */
    RPMSQL_MODE_EXPLAIN	= 8,	/* Like MODE_COLUMN, but do not truncate data */
};

struct previous_mode {
    int valid;			/* Is there legit data in here? */
    uint32_t mode;
    uint32_t flags;
    int colWidth[100];
};

struct rpmsql_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    uint32_t flags;		/*!< control bits */
    const char ** av;		/*!< arguments */

    void * I;			/* The database (sqlite *) */
/*@null@*/
    void * S;			/* Current statement if any (sqlite3_stmt *) */

    const char * zInitFile;	/*    -init FILE */

    const char * zDbFilename;	/* Name of the database file */
    char * zDestTable;		/* Name of destination table iff MODE_INSERT */

    uint32_t mode;		/* Operational mode. */

    int cnt;			/* Number of records displayed so far */

    rpmiob iob;			/* Output I/O buffer */
    FILE * out;			/* Write results here */
    FILE * pLog;		/* Write log output here */

	/* Holds the mode information just before .explain ON */
    struct previous_mode explainPrev;
    char separator[20];		/* Separator character for MODE_LIST */
    int colWidth[100];		/* Requested width of each column when in column mode */
    int actualWidth[100];	/* Actual width of each column */
    char nullvalue[20];		/* Text to print for NULL from the database */
    char outfile[FILENAME_MAX];	/* Filename for *out */

    /* XXX INTERACTIVE cruft. */
    const char * zHome;		/* HOME */
    const char * zInitrc;	/* ~/.sqliterc */
    const char * zHistory;	/* ~/.sqlite_history */
    const char * zPrompt;	/* "dbsql> " */
    const char * zContinue;	/* "   ...> " */

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

/*@unchecked@*/
extern struct rpmsql_s _sql;

/*@unchecked@*/
extern struct poptOption _rpmsqlOptions[];

#endif /* _RPMSQL_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a sql interpreter instance.
 * @param sql		sql interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmsql rpmsqlUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmsql sql)
	/*@modifies sql @*/;
#define	rpmsqlUnlink(_ds)	\
    ((rpmsql)rpmioUnlinkPoolItem((rpmioItem)(_sql), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a sql interpreter instance.
 * @param sql		sql interpreter
 * @return		new sql interpreter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmsql rpmsqlLink (/*@null@*/ rpmsql sql)
	/*@modifies sql @*/;
#define	rpmsqlLink(_sql)	\
    ((rpmsql)rpmioLinkPoolItem((rpmioItem)(_sql), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a sql interpreter.
 * @param sql		sql interpreter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmsql rpmsqlFree(/*@killref@*/ /*@null@*/rpmsql sql)
	/*@globals fileSystem @*/
	/*@modifies sql, fileSystem @*/;
#define	rpmsqlFree(_sql)	\
    ((rpmsql)rpmioFreePoolItem((rpmioItem)(_sql), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a sql interpreter.
 * @param av		sql interpreter args (or NULL)
 * @param flags		sql interpreter flags
 * @return		new sql interpreter
 */
/*@newref@*/ /*@null@*/
rpmsql rpmsqlNew(/*@null@*/ char ** av, uint32_t flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Return arguments from a sql interpreter.
 * @param sql		sql interpreter
 * @retval *argcp	no. of arguments
 * @return		sql interpreter args
 */
/*@null@*/
const char ** rpmsqlArgv(/*@null@*/ rpmsql sql, /*@null@*/ int * argcp)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef	NOTYET
/**
 * Execute sql from a file.
 * @param sql		sql interpreter (NULL uses global interpreter)
 * @param fn		sql file to run (NULL returns RPMRC_FAIL)
 * @param *resultp	sql exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmsqlRunFile(rpmsql sql, /*@null@*/ const char * fn,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies sql, fileSystem, internalState @*/;
#endif

/**
 * Execute sql string.
 * @param sql		sql interpreter (NULL uses global interpreter)
 * @param str		sql string to execute (NULL returns RPMRC_FAIL)
 * @param *resultp	sql interpreter result
 * @return		RPMRC_OK on success
 */
rpmRC rpmsqlRun(rpmsql sql, /*@null@*/ const char * str,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies sql, *resultp, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMSQL_H */
