/*@-mustmod@*/
/*@-paramuse@*/
/*@-globuse@*/
/*@-moduncon@*/
/*@-noeffectuncon@*/
/*@-compdef@*/
/*@-compmempass@*/
/*@-modfilesystem@*/
/*@-evalorderuncon@*/

/*
 * sqlite.c
 * sqlite interface for rpmdb
 *
 * Author: Mark Hatle <mhatle@mvista.com> or <fray@kernel.crashing.org>
 * Copyright (c) 2004 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * or GNU Library General Public License, at your option,
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and GNU Library Public License along with this program;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "system.h"

#include <rpmio.h>
#include <rpmlog.h>
#include <rpmmacro.h>
#include <rpmurl.h>     /* XXX urlPath proto */

#include <rpmtag.h>
#define	_RPMDB_INTERNAL
#include <rpmdb.h>

#include <sqlite3.h>

#include "debug.h"

/* XXX retrofit the *BSD typedef for the deprived. */
#if defined(__QNXNTO__)
typedef rpmuint32_t       u_int32_t;
#endif

#if defined(__LCLINT__)
#define	UINT32_T	u_int32_t
#else
#define	UINT32_T	uint32_t
#endif

/*@access rpmdb @*/
/*@access dbiIndex @*/

/*@unchecked@*/
int _sqldb_debug = 0;

#define SQLDEBUG(_dbi, _list)	\
    if (((_dbi) && (_dbi)->dbi_debug) || (_sqldb_debug)) fprintf _list

struct _sql_db_s;	typedef struct _sql_db_s	SQL_DB;
struct _sql_db_s {
    sqlite3 * db;		/* Database pointer */
    int transaction;		/* Do we have a transaction open? */
};

/* =================================================================== */
/*@-redef@*/
typedef struct key_s {
    uint32_t	v;
/*@observer@*/
    const char *n;
} KEY;
/*@=redef@*/

/*@observer@*/
static const char * tblName(uint32_t v, KEY * tbl, size_t ntbl)
	/*@*/
{
    const char * n = NULL;
    static char buf[32];
    size_t i;

    for (i = 0; i < ntbl; i++) {
	if (v != tbl[i].v)
	    continue;
	n = tbl[i].n;
	break;
    }
    if (n == NULL) {
	(void) snprintf(buf, sizeof(buf), "0x%x", (unsigned)v);
	n = buf;
    }
    return n;
}

static const char * fmtBits(uint32_t flags, KEY tbl[], size_t ntbl, char *t)
	/*@modifies t @*/
{
    char pre = '<';
    char * te = t;
    int i;

    sprintf(t, "0x%x", (unsigned)flags);
    te = t;
    te += strlen(te);
    for (i = 0; i < 32; i++) {
	uint32_t mask = (1 << i);
	const char * name;

	if (!(flags & mask))
	    continue;

	name = tblName(mask, tbl, ntbl);
	*te++ = pre;
	pre = ',';
	te = stpcpy(te, name);
    }
    if (pre == ',') *te++ = '>';
    *te = '\0';
    return t;
}
#define _DBT_ENTRY(_v)      { DB_DBT_##_v, #_v, }
/*@unchecked@*/ /*@observer@*/
static KEY DBTflags[] = {
    _DBT_ENTRY(MALLOC),
    _DBT_ENTRY(REALLOC),
    _DBT_ENTRY(USERMEM),
    _DBT_ENTRY(PARTIAL),
    _DBT_ENTRY(APPMALLOC),
    _DBT_ENTRY(MULTIPLE),
#if defined(DB_DBT_READONLY)	/* XXX db-5.2.28 */
    _DBT_ENTRY(READONLY),
#endif
};
#undef	_DBT_ENTRY
/*@unchecked@*/
static size_t nDBTflags = sizeof(DBTflags) / sizeof(DBTflags[0]);
/*@observer@*/
static char * fmtDBT(const DBT * K, char * te)
	/*@modifies te @*/
{
    static size_t keymax = 35;
    int unprintable;
    uint32_t i;

    sprintf(te, "%p[%u]\t", K->data, (unsigned)K->size);
    te += strlen(te);
    (void) fmtBits(K->flags, DBTflags, nDBTflags, te);
    te += strlen(te);
    if (K->data && K->size > 0) {
 	uint8_t * _u;
	size_t _nu;

	/* Grab the key data/size. */
	if (K->flags & DB_DBT_MULTIPLE) {
	    DBT * _K = K->data;
	    _u = _K->data;
	    _nu = _K->size;
	} else {
	    _u = K->data;
	    _nu = K->size;
	}
	/* Verify if data is a string. */
	unprintable = 0;
	for (i = 0; i < _nu; i++)
	    unprintable |= !xisprint(_u[i]);

	/* Display the data. */
	if (!unprintable) {
	    size_t nb = (_nu < keymax ? _nu : keymax);
	    char * ellipsis = (_nu < keymax ? "" : "...");
	    sprintf(te, "\t\"%.*s%s\"", (int)nb, (char *)_u, ellipsis);
	} else {
	    switch (_nu) {
	    default: break;
	    case 4:	sprintf(te, "\t0x%08x", (unsigned)*(uint32_t *)_u); break;
	    }
	}

	te += strlen(te);
	*te = '\0';
    }
    return te;
}
/*@observer@*/
static const char * fmtKDR(const DBT * K, const DBT * P, const DBT * D, const DBT * R)
	/*@*/
{
    static char buf[BUFSIZ];
    char * te = buf;

    if (K) {
	te = stpcpy(te, "\n\t  key: ");
	te = fmtDBT(K, te);
    }
    if (P) {
	te = stpcpy(te, "\n\t pkey: ");
	te = fmtDBT(P, te);
    }
    if (D) {
	te = stpcpy(te, "\n\t data: ");
	te = fmtDBT(D, te);
    }
    if (R) {
	te = stpcpy(te, "\n\t  res: ");
	te = fmtDBT(R, te);
    }
    *te = '\0';
    
    return buf;
}
#define	_KEYDATA(_K, _P, _D, _R)	fmtKDR(_K, _P, _D, _R)

/* =================================================================== */
/*@-globuse -mustmod @*/	/* FIX: rpmError not annotated yet. */
static int Xcvtdberr(/*@unused@*/ dbiIndex dbi, const char * msg,
		int error, int printit,
		const char * func, const char * fn, unsigned ln)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int rc = error;

    if (printit && rc)
    switch (rc) {
    case SQLITE_DONE:
	break;		/* Filter out valid returns. */
    default:
      {	const char * errmsg = dbi != NULL
		? sqlite3_errmsg(((SQL_DB *)dbi->dbi_db)->db)
		: "";
/*@-moduncon@*/ /* FIX: annotate db3 methods */
	rpmlog(RPMLOG_ERR, "%s:%s:%u: %s(%d): %s\n",
		func, fn, ln, msg, rc, errmsg);
/*@=moduncon@*/
      }	break;
    }

    return rc;
}
/*@=globuse =mustmod @*/
#define	cvtdberr(_dbi, _msg, _error)	\
    Xcvtdberr(_dbi, _msg, _error, _sqldb_debug, __FUNCTION__, __FILE__, __LINE__)

/* =================================================================== */
struct _sql_dbcursor_s;	typedef struct _sql_dbcursor_s *SCP_t;
struct _sql_dbcursor_s {
    struct rpmioItem_s _item;   /*!< usage mutex and pool identifier. */
/*@shared@*/
    DB *dbp;

/*@only@*/ /*@relnull@*/
    char * cmd;			/* SQL command string */
/*@only@*/ /*@relnull@*/
    sqlite3_stmt *pStmt;	/* SQL byte code */
    char * pzErrmsg;		/* SQL error msg */

  /* Table -- result of query */
/*@only@*/ /*@relnull@*/
    char ** av;			/* item ptrs */
/*@only@*/ /*@relnull@*/
    size_t * avlen;		/* item sizes */
    int nalloc;
    int ac;			/* no. of items */
    int rx;			/* Which row are we on? 1, 2, 3 ... */
    int nr;			/* no. of rows */
    int nc;			/* no. of columns */

    int all;			/* sequential iteration cursor */
/*@relnull@*/
    DBT ** keys;		/* array of package keys */
    int nkeys;

    int count;

/*@null@*/
    void * lkey;		/* Last key returned */
/*@null@*/
    void * ldata;		/* Last data returned */

    int used;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

/*@-redef@*/
union _dbswap {
    uint32_t ui;
    unsigned char uc[4];
};
/*@=redef@*/

#define _DBSWAP(_a) \
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
  }

/*@unchecked@*/ /*@only@*/ /*@null@*/
static const char * sqlCwd = NULL;
/*@unchecked@*/
static int sqlInRoot = 0;

static void enterChroot(dbiIndex dbi)
	/*@globals sqlCwd, sqlInRoot, internalState @*/
	/*@modifies sqlCwd, sqlInRoot, internalState @*/
{
    char * currDir = NULL;
    int xx;

    if ((dbi->dbi_root[0] == '/' && dbi->dbi_root[1] == '\0') || dbi->dbi_rpmdb->db_chrootDone || sqlInRoot)
	/* Nothing to do, was not already in chroot */
	return;

SQLDEBUG(dbi, (stderr, "==> %s(%s)\n", __FUNCTION__, dbi->dbi_root));

    {	int currDirLen = 0;

	do {
	    currDirLen += 128;
	    currDir = xrealloc(currDir, currDirLen);
	    memset(currDir, 0, currDirLen);
	} while (getcwd(currDir, currDirLen) == NULL && errno == ERANGE);
    }

    sqlCwd = currDir;
/*@-globs@*/
    xx = Chdir("/");
/*@=globs@*/
/*@-modobserver@*/
    xx = Chroot(dbi->dbi_root);
/*@=modobserver@*/
assert(xx == 0);
    sqlInRoot=1;
}

static void leaveChroot(dbiIndex dbi)
	/*@globals sqlCwd, sqlInRoot, internalState @*/
	/*@modifies sqlCwd, sqlInRoot, internalState @*/
{
    int xx;

    if ((dbi->dbi_root[0] == '/' && dbi->dbi_root[1] == '\0') || dbi->dbi_rpmdb->db_chrootDone || !sqlInRoot)
	/* Nothing to do, not in chroot */
	return;

SQLDEBUG(dbi, (stderr, "==> %s(%s)\n", __FUNCTION__, dbi->dbi_root));

/*@-modobserver@*/
    xx = Chroot(".");
/*@=modobserver@*/
assert(xx == 0);
    if (sqlCwd != NULL) {
/*@-globs@*/
	xx = Chdir(sqlCwd);
/*@=globs@*/
	sqlCwd = _free(sqlCwd);
    }

    sqlInRoot = 0;
}

/*==============================================================*/
int _scp_debug = 0;

#define SCPDEBUG(_dbi, _list)   if (_scp_debug) fprintf _list

/**
 * Unreference a SCP wrapper instance.
 * @param scp		SCP wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
SCP_t scpUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ SCP_t scp)
	/*@modifies scp @*/;
#define	scpUnlink(_scp)	\
    ((SCP_t)rpmioUnlinkPoolItem((rpmioItem)(_scp), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a SCP wrapper instance.
 * @param scp		SCP wrapper
 * @return		new SCP wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
SCP_t scpLink (/*@null@*/ SCP_t scp)
	/*@modifies scp @*/;
#define	scpLink(_scp)	\
    ((SCP_t)rpmioLinkPoolItem((rpmioItem)(_scp), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a SCP wrapper.
 * @param scp		SCP wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
SCP_t scpFree(/*@killref@*/ /*@null@*/SCP_t scp)
	/*@globals fileSystem @*/
	/*@modifies scp, fileSystem @*/;
#define	scpFree(_scp)	\
    ((SCP_t)rpmioFreePoolItem((rpmioItem)(_scp), __FUNCTION__, __FILE__, __LINE__))

static void dbg_scp(void *ptr)
	/*@globals stderr, fileSystem @*/
	/*@modifies stderr, fileSystem @*/
{
    SCP_t scp = ptr;

if (_scp_debug)
fprintf(stderr, "\tscp %p [%d:%d] av %p avlen %p nr [%d:%d] nc %d all %d\n", scp, scp->ac, scp->nalloc, scp->av, scp->avlen, scp->rx, scp->nr, scp->nc, scp->all);

}

static void dbg_keyval(const char * msg, dbiIndex dbi, /*@null@*/ DBC * dbcursor,
		DBT * key, DBT * data, unsigned int flags)
	/*@globals stderr, fileSystem @*/
	/*@modifies stderr, fileSystem @*/
{

if (!_scp_debug) return;

    fprintf(stderr, "%s on %s (%p,%p,%p,0x%x)", msg, dbi->dbi_subfile, dbcursor, key, data, flags);

    /* XXX FIXME: ptr alignment is fubar here. */
    if (key != NULL && key->data != NULL) {
	fprintf(stderr, "  key 0x%x[%d]", *(unsigned int *)key->data, key->size);
	if (dbi->dbi_rpmtag == RPMTAG_NAME)
	    fprintf(stderr, " \"%s\"", (const char *)key->data);
    }
    if (data != NULL && data->data != NULL)
	fprintf(stderr, " data 0x%x[%d]", *(unsigned int *)data->data, data->size);

    fprintf(stderr, "\n");
    if (dbcursor != NULL)
	dbg_scp(dbcursor);
}

/*@only@*/
static SCP_t scpResetKeys(/*@only@*/ SCP_t scp)
	/*@modifies scp @*/
{
    int ix;

#if 0
SCPDEBUG(NULL, (stderr, "--> %s(%p)\n", __FUNCTION__, scp));
dbg_scp(scp);
#endif

    for ( ix =0 ; ix < scp->nkeys ; ix++ ) {
	scp->keys[ix]->data = _free(scp->keys[ix]->data);
/*@-unqualifiedtrans@*/
	scp->keys[ix] = _free(scp->keys[ix]);
/*@=unqualifiedtrans@*/
    }
    scp->keys = _free(scp->keys);
    scp->nkeys = 0;

    return scp;
}

/*@only@*/
static SCP_t scpResetAv(/*@only@*/ SCP_t scp)
	/*@modifies scp @*/
{
    int xx;

#if 0
SCPDEBUG(NULL, (stderr, "--> %s(%p)\n", __FUNCTION__, scp));
dbg_scp(scp);
#endif

    if (scp->av != NULL) {
	if (scp->nalloc <= 0) {
	    /* Clean up SCP_t used by sqlite3_get_table(). */
	    sqlite3_free_table(scp->av);
	    scp->av = NULL;
	    scp->nalloc = 0;
	} else {
	    /* Clean up SCP_t used by sql_step(). */
/*@-unqualifiedtrans@*/
	    for (xx = 0; xx < scp->ac; xx++)
		scp->av[xx] = _free(scp->av[xx]);
/*@=unqualifiedtrans@*/
	    if (scp->av != NULL)
		memset(scp->av, 0, scp->nalloc * sizeof(*scp->av));
	    if (scp->avlen != NULL)
		memset(scp->avlen, 0, scp->nalloc * sizeof(*scp->avlen));
	    scp->av = _free(scp->av);
	    scp->avlen = _free(scp->avlen);
	    scp->nalloc = 0;
	}
    } else
	scp->nalloc = 0;
    scp->ac = 0;
    scp->nr = 0;
    scp->nc = 0;

    return scp;
}

/*@only@*/
static SCP_t scpReset(/*@only@*/ SCP_t scp)
	/*@modifies scp @*/
{
    int xx;

#if 0
SCPDEBUG(NULL, (stderr, "--> %s(%p)\n", __FUNCTION__, scp));
dbg_scp(scp);
#endif

    if (scp->cmd) {
	sqlite3_free(scp->cmd);
	scp->cmd = NULL;
    }
    if (scp->pStmt) {
	xx = cvtdberr(NULL, "sqlite3_reset",
		sqlite3_reset(scp->pStmt));
	xx = cvtdberr(NULL, "sqlite3_finalize",
		sqlite3_finalize(scp->pStmt));
	scp->pStmt = NULL;
    }

    scp = scpResetAv(scp);

    scp->rx = 0;
    return scp;
}

static void scpFini(void * _scp)
	/*@globals fileSystem @*/
	/*@modifies *_scp, fileSystem @*/
{
    SCP_t scp = (SCP_t) _scp;

    scp = scpReset(scp);
    scp = scpResetKeys(scp);
    scp->av = _free(scp->av);
    scp->avlen = _free(scp->avlen);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _scpPool = NULL;

static SCP_t scpGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmscpPool, fileSystem @*/
	/*@modifies pool, _scpPool, fileSystem @*/
{
    SCP_t scp;

    if (_scpPool == NULL) {
	_scpPool = rpmioNewPool("scp", sizeof(*scp), -1, _scp_debug,
			NULL, NULL, scpFini);
	pool = _scpPool;
    }
    scp = (SCP_t) rpmioGetPool(pool, sizeof(*scp));
    memset(((char *)scp)+sizeof(scp->_item), 0, sizeof(*scp)-sizeof(scp->_item));
    return scp;
}

static SCP_t scpNew(DB * dbp)
{
    SCP_t scp = scpGetPool(_scpPool);

/*@-temptrans@*/
    scp->dbp = dbp;
/*@=temptrans@*/

    scp->used = 0;
    scp->lkey = NULL;
    scp->ldata = NULL;

    return scpLink(scp);
}

/* ============================================================== */
static int sql_step(dbiIndex dbi, SCP_t scp)
	/*@modifies dbi, scp @*/
{
    int swapped = dbiByteSwapped(dbi);
    const char * cname;
    const char * vtype;
    size_t nb;
    int loop;
    int need;
    int rc;
    int i;

    scp->nc = sqlite3_column_count(scp->pStmt);	/* XXX cvtdberr? */

    if (scp->nr == 0 && scp->av != NULL)
	need = 2 * scp->nc;
    else
	need = scp->nc;

    /* XXX scp->nc = need = scp->nalloc = 0 case forces + 1 here */
    if (!scp->ac && !need && !scp->nalloc)
	need++;

    if (scp->ac + need >= scp->nalloc) {
	/* XXX +4 is bogus, was +1 */
	scp->nalloc = 2 * scp->nalloc + need + 4;
	scp->av = xrealloc(scp->av, scp->nalloc * sizeof(*scp->av));
	scp->avlen = xrealloc(scp->avlen, scp->nalloc * sizeof(*scp->avlen));
    }

    if (scp->av != NULL && scp->nr == 0) {
	for (i = 0; i < scp->nc; i++) {
	    scp->av[scp->ac] = xstrdup(sqlite3_column_name(scp->pStmt, i));
	    if (scp->avlen) scp->avlen[scp->ac] = strlen(scp->av[scp->ac]) + 1;
	    scp->ac++;
assert(scp->ac <= scp->nalloc);
	}
    }

/*@-infloopsuncon@*/
    loop = 1;
    while (loop) {
	rc = cvtdberr(dbi, "sqlite3_step",
		sqlite3_step(scp->pStmt));
	switch (rc) {
	case SQLITE_DONE:
SQLDEBUG(dbi, (stderr, "%s(%p,%p): DONE [%d:%d] av %p avlen %p\n", __FUNCTION__, dbi, scp, scp->ac, scp->nalloc, scp->av, scp->avlen));
	    loop = 0;
	    /*@switchbreak@*/ break;
	case SQLITE_ROW:
	    if (scp->av != NULL)
	    for (i = 0; i < scp->nc; i++) {
		/* Expand the row array for new elements */
    		if (scp->ac + need >= scp->nalloc) {
		    /* XXX +4 is bogus, was +1 */
		    scp->nalloc = 2 * scp->nalloc + need + 4;
		    scp->av = xrealloc(scp->av, scp->nalloc * sizeof(*scp->av));
		    scp->avlen = xrealloc(scp->avlen, scp->nalloc * sizeof(*scp->avlen));
	        }
assert(scp->av != NULL);
assert(scp->avlen != NULL);

		cname = sqlite3_column_name(scp->pStmt, i);
		vtype = sqlite3_column_decltype(scp->pStmt, i);
		nb = 0;

		if (!strcmp(vtype, "blob")) {
		    const void * v = sqlite3_column_blob(scp->pStmt, i);
		    nb = sqlite3_column_bytes(scp->pStmt, i);
SQLDEBUG(dbi, (stderr, "\t%d %s %s %p[%d]\n", i, cname, vtype, v, (int)nb));
		    if (nb > 0) {
			void * t = (void *) xmalloc(nb);
			scp->av[scp->ac] = (char *) memcpy(t, v, nb);
			scp->avlen[scp->ac] = nb;
			scp->ac++;
		    }
		} else
		if (!strcmp(vtype, "double")) {
		    double v = sqlite3_column_double(scp->pStmt, i);
		    nb = sizeof(v);
SQLDEBUG(dbi, (stderr, "\t%d %s %s %g\n", i, cname, vtype, v));
		    if (nb > 0) {
			scp->av[scp->ac] = (char *) memcpy(xmalloc(nb), &v, nb);
			scp->avlen[scp->ac] = nb;
assert(swapped == 0); /* Byte swap?! */
			scp->ac++;
		    }
		} else
		if (!strcmp(vtype, "int")) {
		    rpmint32_t v = sqlite3_column_int(scp->pStmt, i);
		    nb = sizeof(v);
SQLDEBUG(dbi, (stderr, "\t%d %s %s %d\n", i, cname, vtype, (int) v));
		    if (nb > 0) {
			scp->av[scp->ac] = (char *) memcpy(xmalloc(nb), &v, nb);
			scp->avlen[scp->ac] = nb;
if (swapped == 1) {
  union _dbswap dbswap;
  memcpy(&dbswap.ui, scp->av[scp->ac], sizeof(dbswap.ui));
  _DBSWAP(dbswap);
  memcpy(scp->av[scp->ac], &dbswap.ui, sizeof(dbswap.ui));
}
			scp->ac++;
		    }
		} else
		if (!strcmp(vtype, "int64")) {
		    int64_t v = sqlite3_column_int64(scp->pStmt, i);
		    nb = sizeof(v);
SQLDEBUG(dbi, (stderr, "\t%d %s %s %ld\n", i, cname, vtype, (long)v));
		    if (nb > 0) {
			scp->av[scp->ac] = (char *) memcpy(xmalloc(nb), &v, nb);
			scp->avlen[scp->ac] = nb;
assert(swapped == 0); /* Byte swap?! */
			scp->ac++;
		    }
		} else
		if (!strcmp(vtype, "text")) {
		    const char * v = (const char *)sqlite3_column_text(scp->pStmt, i);
		    nb = strlen(v) + 1;
SQLDEBUG(dbi, (stderr, "\t%d %s %s \"%s\"\n", i, cname, vtype, v));
		    if (nb > 0) {
			scp->av[scp->ac] = (char *) memcpy(xmalloc(nb), v, nb);
			scp->avlen[scp->ac] = nb;
			scp->ac++;
		    }
		}
assert(scp->ac <= scp->nalloc);
	    }
	    scp->nr++;
	    /*@switchbreak@*/ break;
	case SQLITE_BUSY:
	    fprintf(stderr, "sqlite3_step: BUSY %d\n", rc);
	    /*@switchbreak@*/ break;
	case SQLITE_ERROR:
	    fprintf(stderr, "sqlite3_step: ERROR %d -- %s\n", rc, scp->cmd);
	    fprintf(stderr, "              %s (%d)\n",
			sqlite3_errmsg(((SQL_DB*)dbi->dbi_db)->db), sqlite3_errcode(((SQL_DB*)dbi->dbi_db)->db));
/*@-nullpass@*/
	    fprintf(stderr, "              cwd '%s'\n", getcwd(NULL,0));
/*@=nullpass@*/
	    loop = 0;
	    /*@switchbreak@*/ break;
	case SQLITE_MISUSE:
	    fprintf(stderr, "sqlite3_step: MISUSE %d\n", rc);
	    loop = 0;
	    /*@switchbreak@*/ break;
	default:
	    fprintf(stderr, "sqlite3_step: rc %d\n", rc);
	    loop = 0;
	    /*@switchbreak@*/ break;
	}
    }
/*@=infloopsuncon@*/

    if (rc == SQLITE_DONE)
	rc = SQLITE_OK;

    return rc;
}

static int sql_bind_key(dbiIndex dbi, SCP_t scp, int pos, DBT * key)
	/*@modifies dbi, scp @*/
{
    int swapped = dbiByteSwapped(dbi);
    int rc = 0;

assert(key->data != NULL);
    switch (dbi->dbi_rpmtag) {
    case RPMDBI_PACKAGES:
	{   unsigned int hnum;
/*@i@*/ assert(key->size == sizeof(rpmuint32_t));
	    memcpy(&hnum, key->data, sizeof(hnum));

	    rc = cvtdberr(dbi, "sqlite3_bind_int",
			sqlite3_bind_int(scp->pStmt, pos, hnum));
	} break;
    default:
	switch (tagType(dbi->dbi_rpmtag) & RPM_MASK_TYPE) {
	case RPM_BIN_TYPE:
/*@-castfcnptr -nullpass@*/ /* FIX: annotate sqlite. */
	    rc = cvtdberr(dbi, "sqlite3_bind_blob",
			sqlite3_bind_blob(scp->pStmt, pos,
				key->data, key->size, SQLITE_STATIC));
/*@=castfcnptr =nullpass@*/
	    /*@innerbreak@*/ break;
	case RPM_UINT8_TYPE:
	{   unsigned char i;
/*@i@*/ assert(key->size == sizeof(unsigned char));
assert(swapped == 0); /* Byte swap?! */
	    memcpy(&i, key->data, sizeof(i));
	    rc = cvtdberr(dbi, "sqlite3_bind_int",
	    		sqlite3_bind_int(scp->pStmt, pos, (int) i));
	} /*@innerbreak@*/ break;
	case RPM_UINT16_TYPE:
	{	unsigned short i;
/*@i@*/ assert(key->size == sizeof(rpmuint16_t));
assert(swapped == 0); /* Byte swap?! */
	    memcpy(&i, key->data, sizeof(i));
	    rc = cvtdberr(dbi, "sqlite3_bind_int",
			sqlite3_bind_int(scp->pStmt, pos, (int) i));
	} /*@innerbreak@*/ break;
	case RPM_UINT64_TYPE:
assert(0);	/* borken */
	/*@innerbreak@*/ break;
	case RPM_UINT32_TYPE:
	default:
	{   unsigned int i;
/*@i@*/ assert(key->size == sizeof(rpmuint32_t));
	    memcpy(&i, key->data, sizeof(i));

	    rc = cvtdberr(dbi, "sqlite3_bind_int",
			sqlite3_bind_int(scp->pStmt, pos, i));
	}   /*@innerbreak@*/ break;
	case RPM_STRING_TYPE:
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
/*@-castfcnptr -nullpass@*/ /* FIX: annotate sqlite. */
	    rc = cvtdberr(dbi, "sqlite3_bind_text",
			sqlite3_bind_text(scp->pStmt, pos,
				key->data, key->size, SQLITE_STATIC));
/*@=castfcnptr =nullpass@*/
	    /*@innerbreak@*/ break;
	}
    }

    return rc;
}

static int sql_bind_data(/*@unused@*/ dbiIndex dbi, SCP_t scp,
		int pos, DBT * data)
	/*@modifies scp @*/
{
    int rc;

assert(data->data != NULL);
/*@-castfcnptr -nullpass@*/ /* FIX: annotate sqlite */
    rc = cvtdberr(dbi, "sqlite3_bind_blob",
		sqlite3_bind_blob(scp->pStmt, pos,
			data->data, data->size, SQLITE_STATIC));
/*@=castfcnptr =nullpass@*/

    return rc;
}

/* =================================================================== */
static int sql_exec(dbiIndex dbi, SCP_t scp, const char * cmd)
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    char * pzErrmsg = NULL;
    char ** ppzErrmsg = (scp ? &scp->pzErrmsg : &pzErrmsg);
    int rc = cvtdberr(dbi, "sqlite3_exec",
		sqlite3_exec(sqldb->db, cmd, NULL, NULL, ppzErrmsg));

SQLDEBUG(dbi, (stderr, "\t%s\n<-- %s(%p,%p) rc %d\n", cmd, __FUNCTION__, dbi, scp, rc));

    return rc;
}

static int sql_startTransaction(dbiIndex dbi)
	/*@*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = 0;

    if (!sqldb->transaction) {
	static char _cmd[] = "BEGIN TRANSACTION;";
	rc = sql_exec(dbi, NULL, _cmd);

	if (rc == 0)
	    sqldb->transaction = 1;
    }

    return rc;
}

static int sql_endTransaction(dbiIndex dbi)
	/*@*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = 0;

    if (sqldb->transaction) {
	static char _cmd[] = "END TRANSACTION;";
	rc = sql_exec(dbi, NULL, _cmd);

	if (rc == 0)
	    sqldb->transaction = 0;
    }

    return rc;
}

static int sql_commitTransaction(dbiIndex dbi, int flag)
	/*@*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = 0;

    if (sqldb->transaction) {
	static char _cmd[] = "COMMIT;";
	rc = sql_exec(dbi, NULL, _cmd);

	sqldb->transaction = 0;

	/* Start a new transaction if we were in the middle of one */
	if (flag == 0)
	    rc = sql_startTransaction(dbi);
    }

    return rc;
}

static int sql_busy_handler(void * dbi_void, int time)
	/*@*/
{
/*@-castexpose@*/
    dbiIndex dbi = (dbiIndex) dbi_void;
/*@=castexpose@*/

    rpmlog(RPMLOG_WARNING, _("Unable to get lock on db %s, retrying... (%d)\n"),
		dbi->dbi_file, time);

    (void) sleep(1);

    return 1;
}

/* =================================================================== */
static int sql_get_table(dbiIndex dbi, SCP_t scp, const char * cmd)
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = cvtdberr(dbi, "sqlite3_get_table",
		sqlite3_get_table(sqldb->db, cmd,
			&scp->av, &scp->nr, &scp->nc, &scp->pzErrmsg));

SQLDEBUG(dbi, (stderr, "\t%s\n<-- %s(%p,%p) rc %d av %p nr %d nc %d %s\n", cmd, __FUNCTION__, dbi, scp, rc, scp->av, scp->nr, scp->nc, scp->pzErrmsg));

    return rc;
}

/**
 * Verify the DB is setup.. if not initialize it
 *
 * Create the table.. create the db_info
 */
static int sql_initDB(dbiIndex dbi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies internalState @*/
{
    SCP_t scp = scpNew(dbi->dbi_db);
    char cmd[BUFSIZ];
    int rc = 0;
int xx;

    if (dbi->dbi_tmpdir) {
	const char *root;
	const char *tmpdir;
	root = (dbi->dbi_root ? dbi->dbi_root : dbi->dbi_rpmdb->db_root);
	if ((root[0] == '/' && root[1] == '\0') || dbi->dbi_rpmdb->db_chrootDone)
	    root = NULL;
	/*@-mods@*/
	tmpdir = rpmGenPath(root, dbi->dbi_tmpdir, NULL);
	/*@=mods@*/
	sprintf(cmd, "PRAGMA temp_store_directory = '%s';", tmpdir);
	xx = sql_exec(dbi, scp, cmd);
	tmpdir = _free(tmpdir);
    }

    if (dbi->dbi_eflags & DB_EXCL) {
	sprintf(cmd, "PRAGMA locking_mode = EXCLUSIVE;");
	xx = sql_exec(dbi, scp, cmd);
    }

#ifdef	DYING
    if (dbi->dbi_pagesize > 0) {
	sprintf(cmd, "PRAGMA cache_size = %d;", dbi->dbi_cachesize);
	xx = sql_exec(dbi, scp, cmd);
    }
    if (dbi->dbi_cachesize > 0) {
	sprintf(cmd, "PRAGMA page_size = %d;", dbi->dbi_pagesize);
	xx = sql_exec(dbi, scp, cmd);
    }
#endif

    /* Check if the table exists... */
    /* XXX add dbi->exists? to avoid endless repetition. */
    sprintf(cmd,
	"SELECT name FROM 'sqlite_master' WHERE type='table' and name='%s';",
		dbi->dbi_subfile);
/*@-nullstate@*/
    rc = sql_get_table(dbi, scp, cmd);
/*@=nullstate@*/
    if (rc)
	goto exit;

    if (scp->nr < 1) {
	const char * valtype = "blob";
	const char * keytype;

	switch (dbi->dbi_rpmtag) {
	case RPMDBI_PACKAGES:
	    keytype = "int UNIQUE PRIMARY KEY";
	    valtype = "blob";
	    break;
	default:
	    switch (tagType(dbi->dbi_rpmtag) & RPM_MASK_TYPE) {
	    case RPM_BIN_TYPE:
	    default:
		keytype = "blob UNIQUE";
		/*@innerbreak@*/ break;
	    case RPM_UINT8_TYPE:
	    case RPM_UINT16_TYPE:
	    case RPM_UINT32_TYPE:
	    case RPM_UINT64_TYPE:
		keytype = "int UNIQUE";
		/*@innerbreak@*/ break;
	    case RPM_STRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		keytype = "text UNIQUE";
		/*@innerbreak@*/ break;
	    }
	}
SQLDEBUG(dbi, (stderr, "\t%s(%d) type(%d) keytype %s\n", tagName(dbi->dbi_rpmtag), dbi->dbi_rpmtag, (tagType(dbi->dbi_rpmtag) & RPM_MASK_TYPE), keytype));
	sprintf(cmd, "CREATE %sTABLE '%s' (key %s, value %s)",
			dbi->dbi_temporary ? "TEMPORARY " : "",
			dbi->dbi_subfile, keytype, valtype);
	rc = sql_exec(dbi, scp, cmd);
	if (rc)
	    goto exit;
    }

    if (dbi->dbi_no_fsync) {
	static const char _cmd[] = "PRAGMA synchronous = OFF;";
	xx = sql_exec(dbi, scp, _cmd);
    }

exit:
    scp = scpFree(scp);

SQLDEBUG(dbi, (stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, dbi, rc));

    return rc;
}

/**
 * Close database cursor.
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_cclose (dbiIndex dbi, /*@only@*/ DBC * dbcursor,
		unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies dbi, *dbcursor, fileSystem, internalState @*/
{
    SCP_t scp = (SCP_t)dbcursor;
    int rc;

SQLDEBUG(dbi, (stderr, "==> sql_cclose(%p)\n", scp));

    if (scp->lkey)
	scp->lkey = _free(scp->lkey);

    if (scp->ldata)
	scp->ldata = _free(scp->ldata);

enterChroot(dbi);

    if (flags == DB_WRITECURSOR)
	rc = sql_commitTransaction(dbi, 1);
    else
	rc = sql_endTransaction(dbi);

/*@-kepttrans -nullstate@*/
    scp = scpFree(scp);
/*@=kepttrans =nullstate@*/

leaveChroot(dbi);

SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, dbcursor, flags, rc));

    return rc;
}

/**
 * Close index database, and destroy database handle.
 * @param dbi           index database handle
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_close(/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies dbi, fileSystem, internalState @*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = 0;

    if (sqldb) {
	int xx;

enterChroot(dbi);

	/* Commit, don't open a new one */
	rc = sql_commitTransaction(dbi, 1);

	xx = cvtdberr(dbi, "sqlite3_close",
		sqlite3_close(sqldb->db));

	rpmlog(RPMLOG_DEBUG, D_("closed   sql db         %s\n"),
		dbi->dbi_subfile);

#if defined(MAYBE) /* XXX should SQLite and BDB have different semantics? */
	if (dbi->dbi_temporary && !(dbi->dbi_eflags & DB_PRIVATE)) {
	    const char * dbhome = NULL;
	    urltype ut = urlPath(dbi->dbi_home, &dbhome);
	    const char * dbfname = rpmGenPath(dbhome, dbi->dbi_file, NULL);
	    int xx = (dbfname ? Unlink(dbfname) : 0);
	    (void)ut; (void)xx;	/* XXX tell gcc to be quiet. */
	    dbfname = _free(dbfname);
	}
#endif

	dbi->dbi_stats = _free(dbi->dbi_stats);
	dbi->dbi_file = _free(dbi->dbi_file);
	dbi->dbi_db = _free(dbi->dbi_db);

leaveChroot(dbi);
    }

SQLDEBUG(dbi, (stderr, "<-- %s(%p,0x%x) rc %d\n", __FUNCTION__, dbi, flags, rc));
    return rc;
}

/**
 * Return handle for an index database.
 * @param rpmdb         rpm database
 * @param rpmtag        rpm tag
 * @retval *dbip	index database handle
 * @return              0 on success
 */
static int sql_open(rpmdb rpmdb, rpmTag rpmtag, /*@out@*/ dbiIndex * dbip)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *dbip, rpmGlobalMacroContext, fileSystem, internalState @*/
{
/*@-nestedextern -shadow @*/
    extern struct _dbiVec sqlitevec;
/*@=nestedextern -shadow @*/

    const char * urlfn = NULL;
    const char * root;
    const char * home;
    const char * dbhome;
    const char * dbfile;
    const char * dbfname;
    const char * sql_errcode;
    mode_t umask_safed = 0002;
    dbiIndex dbi;
    SQL_DB * sqldb;
    size_t len;
    int rc = 0;
    int xx;

    if (dbip)
	*dbip = NULL;

    /* Parse db configuration parameters. */
    /*@-mods@*/
    if ((dbi = db3New(rpmdb, rpmtag)) == NULL)
	/*@-nullstate@*/
	return 1;
	/*@=nullstate@*/
    /*@=mods@*/

   /* Get the prefix/root component and directory path */
    root = rpmdb->db_root;
    home = rpmdb->db_home;

    dbi->dbi_root = root;
    dbi->dbi_home = home;

    dbfile = tagName(dbi->dbi_rpmtag);

enterChroot(dbi);

    /* USe a copy of tagName for the file/table name(s). */
    {	
	char * t;
	len = strlen(dbfile);
	t = (char *) xcalloc(len + 1, sizeof(*t));
	(void) stpcpy( t, dbfile );
	dbi->dbi_file = t;
/*@-kepttrans@*/ /* WRONG */
	dbi->dbi_subfile = t;
/*@=kepttrans@*/
    }

    dbi->dbi_mode = O_RDWR;

    /*
     * Either the root or directory components may be a URL. Concatenate,
     * convert the URL to a path, and add the name of the file.
     */
    /*@-mods@*/
    urlfn = rpmGenPath(NULL, home, NULL);
    /*@=mods@*/
    (void) urlPath(urlfn, &dbhome);

    /* Create the %{sqldb} directory if it doesn't exist (root only). */
    (void) rpmioMkpath(dbhome, 0755, getuid(), getgid());

    if (dbi->dbi_eflags & DB_PRIVATE)
	dbfname = xstrdup(":memory:");
    else
	dbfname = rpmGenPath(dbhome, dbi->dbi_file, NULL);

    rpmlog(RPMLOG_DEBUG, D_("opening  sql db         %s (%s) mode=0x%x\n"),
		dbfname, dbi->dbi_subfile, dbi->dbi_mode);

    /* Open the Database */
    sqldb = (SQL_DB *) xcalloc(1, sizeof(*sqldb));

    sql_errcode = NULL;
/*@+longunsignedintegral@*/
    if (dbi->dbi_perms)
	/* mask-out permission bits which are not requested (security) */
	umask_safed = umask(~((mode_t)(dbi->dbi_perms)));
/*@=longunsignedintegral@*/
    xx = cvtdberr(dbi, "sqlite3_open",
		sqlite3_open(dbfname, &sqldb->db));
    if (dbi->dbi_perms) {
	if ((0644 /* = SQLite hard-coded default */ & dbi->dbi_perms) != dbi->dbi_perms) {
	    /* add requested permission bits which are still missing (semantic) */
	    (void) Chmod(dbfname, dbi->dbi_perms);
	}
/*@+longunsignedintegral@*/
	(void) umask(umask_safed);
/*@=longunsignedintegral@*/
    }
    if (xx != SQLITE_OK)
	sql_errcode = sqlite3_errmsg(sqldb->db);

    if (sqldb->db)
	xx = cvtdberr(dbi, "sqlite3_busy_handler",
		sqlite3_busy_handler(sqldb->db, &sql_busy_handler, dbi));

    sqldb->transaction = 0;	/* Initialize no current transactions */

    dbi->dbi_db = (DB *) sqldb;

    if (sql_errcode != NULL) {
	rpmlog(RPMLOG_DEBUG, D_("Unable to open database: %s\n"), sql_errcode);
	rc = EINVAL;
    }

    /* initialize table */
    if (rc == 0)
	rc = sql_initDB(dbi);

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	dbi->dbi_vec = &sqlitevec;
	*dbip = dbi;
    } else {
	dbi->dbi_db = _free(dbi->dbi_db);
	(void) sql_close(dbi, 0);
	dbi = db3Free(dbi);
    }

    urlfn = _free(urlfn);
    dbfname = _free(dbfname);

/*@-usereleased@*/
leaveChroot(dbi);
/*@=usereleased@*/

SQLDEBUG(dbi, (stderr, "<-- %s(%p,%s(%u),%p) rc %d dbi %p\n", __FUNCTION__, rpmdb, tagName(rpmtag), rpmtag, dbip, rc, (dbip ? *dbip : NULL)));
    return rc;
}

/**
 * Flush pending operations to disk.
 * @param dbi           index database handle
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_sync (dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int rc = 0;

enterChroot(dbi);
    rc = sql_commitTransaction(dbi, 0);
leaveChroot(dbi);

SQLDEBUG(dbi, (stderr, "<-- %s(%p,0x%x) rc %d\n", __FUNCTION__, dbi, flags, rc));
    return rc;
}

/** \ingroup dbi
 * Return whether key exists in a database.
 * @param dbi		index database handle
 * @param key		retrieve key value/length/flags
 * @param flags		usually 0
 * @return		0 if key exists, DB_NOTFOUND if not, else error
 */
static int sql_exists(dbiIndex dbi, DBT * key, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,0x%x) rc %d %s\n", __FUNCTION__, dbi, key, flags, rc, _KEYDATA(key, NULL, NULL, NULL)));
    return rc;
}

/** \ingroup dbi
 * Return next sequence number.
 * @param dbi		index database handle (with attached sequence)
 * @retval *seqnop	IN: delta (0 does seqno++) OUT: returned 64bit seqno
 * @param flags		usually 0
 * @return		0 on success
 */
static int sql_seqno(dbiIndex dbi, int64_t * seqnop, unsigned int flags)
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, seqnop, flags, rc));
    return rc;
}

/**
 * Open database cursor.
 * @param dbi		index database handle
 * @param txnid		database transaction handle
 * @retval dbcp		address of new database cursor
 * @param flags		DB_WRITECURSOR or 0
 * @return		0 on success
 */
static int sql_copen (dbiIndex dbi,
		/*@unused@*/ /*@null@*/ DB_TXN * txnid,
		/*@out@*/ DBC ** dbcp, unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies dbi, *txnid, *dbcp, fileSystem, internalState @*/
{
    SCP_t scp = scpNew(dbi->dbi_db);
    DBC * dbcursor = (DBC *)scp;
    int rc = 0;

SQLDEBUG(dbi, (stderr, "==> %s(%s) tag %d type %d scp %p\n", __FUNCTION__, tagName(dbi->dbi_rpmtag), dbi->dbi_rpmtag, (tagType(dbi->dbi_rpmtag) & RPM_MASK_TYPE), scp));

enterChroot(dbi);

    /* If we're going to write, start a transaction (lock the DB) */
    if (flags == DB_WRITECURSOR)
	rc = sql_startTransaction(dbi);

    if (dbcp)
	/*@-onlytrans@*/ *dbcp = dbcursor; /*@=onlytrans@*/
    else
	/*@-kepttrans -nullstate @*/ (void) sql_cclose(dbi, dbcursor, 0); /*@=kepttrans =nullstate @*/

leaveChroot(dbi);

SQLDEBUG(dbi, (stderr, "<== %s(%p,%p,%p,0x%x) rc %d subfile %s\n", __FUNCTION__, dbi, txnid, dbcp, flags, rc, dbi->dbi_subfile));
    return rc;
}

/**
 * Delete (key,data) pair(s) using db->del or dbcursor->c_del.
 * @param dbi           index database handle
 * @param dbcursor      database cursor (NULL will use db->del)
 * @param key           delete key value/length/flags
 * @param data          delete data value/length/flags
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_cdel (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key,
		DBT * data, unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies dbi, *dbcursor, fileSystem, internalState @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = scpNew(dbi->dbi_db);
    int rc = 0;

dbg_keyval(__FUNCTION__, dbi, dbcursor, key, data, flags);
enterChroot(dbi);

    scp->cmd = sqlite3_mprintf("DELETE FROM '%q' WHERE key=? AND value=?;",
	dbi->dbi_subfile);

    rc = cvtdberr(dbi, "sqlite3_prepare",
		sqlite3_prepare(sqldb->db, scp->cmd, (int)strlen(scp->cmd),
		&scp->pStmt, (const char **) &scp->pzErrmsg));
    if (rc) rpmlog(RPMLOG_WARNING, "cdel(%s) prepare %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
    rc = cvtdberr(dbi, "sql_bind_key",
		sql_bind_key(dbi, scp, 1, key));
    if (rc) rpmlog(RPMLOG_WARNING, "cdel(%s) bind key %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
    rc = cvtdberr(dbi, "sql_bind_data",
		sql_bind_data(dbi, scp, 2, data));
    if (rc) rpmlog(RPMLOG_WARNING, "cdel(%s) bind data %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

    rc = cvtdberr(dbi, "sql_step",
		sql_step(dbi, scp));
    if (rc) rpmlog(RPMLOG_WARNING, "cdel(%s) sql_step rc %d\n", dbi->dbi_subfile, rc);

    scp = scpFree(scp);

leaveChroot(dbi);

SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,0x%x) rc %d subfile %s %s\n", __FUNCTION__, dbi, dbcursor, key, data, flags, rc, dbi->dbi_subfile, _KEYDATA(key, NULL, data, NULL)));
    return rc;
}

/**
 * Retrieve (key,data) pair using db->get or dbcursor->c_get.
 * @param dbi           index database handle
 * @param dbcursor      database cursor (NULL will use db->get)
 * @param key           retrieve key value/length/flags
 * @param data          retrieve data value/length/flags
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_cget (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key,
		DBT * data, unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies dbi, dbcursor, *key, *data, fileSystem, internalState @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = (SCP_t)dbcursor;
    int rc = 0;
    int ix;

assert(dbcursor != NULL);
dbg_keyval(__FUNCTION__, dbi, dbcursor, key, data, flags);

enterChroot(dbi);

    /* First determine if this is a new scan or existing scan */

SQLDEBUG(dbi, (stderr, "\tcget(%s) scp %p rc %d flags %d av %p\n",
		dbi->dbi_subfile, scp, rc, flags, scp->av));
    if (flags == DB_SET || scp->used == 0) {
	scp->used = 1; /* Signal this scp as now in use... */
/*@i@*/	scp = scpReset(scp);	/* Free av and avlen, reset counters.*/

/* XXX: Should we also reset the key table here?  Can you re-use a cursor? */

	/* If we're scanning everything, load the iterator key table */
	if (key->size == 0) {
	    scp->all = 1;

/*
 * The only condition not dealt with is if there are multiple identical keys.  This can lead
 * to later iteration confusion.  (It may return the same value for the multiple keys.)
 */

#ifdef	DYING
/* Only RPMDBI_PACKAGES is supposed to be iterating, and this is guarenteed to be unique */
assert(dbi->dbi_rpmtag == RPMDBI_PACKAGES);
#endif

	    switch (dbi->dbi_rpmtag) {
	    case RPMDBI_PACKAGES:
	        scp->cmd = sqlite3_mprintf("SELECT key FROM '%q' ORDER BY key;",
		    dbi->dbi_subfile);
	        break;
	    default:
	        scp->cmd = sqlite3_mprintf("SELECT key FROM '%q';",
		    dbi->dbi_subfile);
	        break;
	    }
	    rc = cvtdberr(dbi, "sqlite3_prepare",
			sqlite3_prepare(sqldb->db, scp->cmd,
				(int)strlen(scp->cmd), &scp->pStmt,
				(const char **) &scp->pzErrmsg));
	    if (rc) rpmlog(RPMLOG_WARNING, "cget(%s) sequential prepare %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

	    rc = sql_step(dbi, scp);
	    if (rc) rpmlog(RPMLOG_WARNING, "cget(%s) sequential sql_step rc %d\n", dbi->dbi_subfile, rc);

	    scp = scpResetKeys(scp);
	    scp->nkeys = scp->nr;
	    scp->keys = (DBT **) xcalloc(scp->nkeys, sizeof(*scp->keys));
	    for (ix = 0; ix < scp->nkeys; ix++) {
		scp->keys[ix] = (DBT *) xmalloc(sizeof(*scp->keys[0]));
		scp->keys[ix]->size = (UINT32_T) scp->avlen[ix+1];
		scp->keys[ix]->data = (void *) xmalloc(scp->keys[ix]->size);
		memcpy(scp->keys[ix]->data, scp->av[ix+1], scp->avlen[ix+1]);
	    }
	} else {
	    /* We're only scanning ONE element */
	    scp = scpResetKeys(scp);
	    scp->nkeys = 1;
	    scp->keys = (DBT **) xcalloc(scp->nkeys, sizeof(*scp->keys));
	    scp->keys[0] = (DBT *) xmalloc(sizeof(*scp->keys[0]));
	    scp->keys[0]->size = key->size;
	    scp->keys[0]->data = (void *) xmalloc(scp->keys[0]->size);
	    memcpy(scp->keys[0]->data, key->data, key->size);
	}

/*@i@*/	scp = scpReset(scp);	/* reset */

	/* Prepare SQL statement to retrieve the value for the current key */
	scp->cmd = sqlite3_mprintf("SELECT value FROM '%q' WHERE key=?;", dbi->dbi_subfile);
	rc = cvtdberr(dbi, "sqlite3_prepare",
		sqlite3_prepare(sqldb->db, scp->cmd, (int)strlen(scp->cmd),
			&scp->pStmt, (const char **) &scp->pzErrmsg));

	if (rc) rpmlog(RPMLOG_WARNING, "cget(%s) prepare %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
    }

/*@i@*/ scp = scpResetAv(scp);	/* Free av and avlen, reset counters.*/

    /* Now continue with a normal retrive based on key */
    if ((scp->rx + 1) > scp->nkeys )
	rc = DB_NOTFOUND; /* At the end of the list */

    if (rc != 0)
	goto exit;

    /* Bind key to prepared statement */
    rc = cvtdberr(dbi, "sql_bind_key",
		sql_bind_key(dbi, scp, 1, scp->keys[scp->rx]));
    if (rc) rpmlog(RPMLOG_WARNING, "cget(%s)  key bind %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

    rc = cvtdberr(dbi, "sql_step",
		sql_step(dbi, scp));
    if (rc) rpmlog(RPMLOG_WARNING, "cget(%s) sql_step rc %d\n", dbi->dbi_subfile, rc);

    rc = cvtdberr(dbi, "sqlite3_reset",
		sqlite3_reset(scp->pStmt));
    if (rc) rpmlog(RPMLOG_WARNING, "reset %d\n", rc);

/* 1 key should return 0 or 1 row/value */
assert(scp->nr < 2);

    if (scp->nr == 0 && scp->all == 0)
	rc = DB_NOTFOUND; /* No data for that key found! */

    if (rc != 0)
	goto exit;

    /* If we're looking at the whole db, return the key */
    if (scp->all) {

/* To get this far there has to be _1_ key returned! (protect against dup keys) */
assert(scp->nr == 1);

	if (scp->lkey)
	    scp->lkey = _free(scp->lkey);

	key->size = scp->keys[scp->rx]->size;
	key->data = (void *) xmalloc(key->size);
	if (! (key->flags & DB_DBT_MALLOC))
	    scp->lkey = key->data;

	(void) memcpy(key->data, scp->keys[scp->rx]->data, key->size);
    }

    /* Construct and return the data element (element 0 is "value", 1 is _THE_ value)*/
    switch (dbi->dbi_rpmtag) {
    default:
	if (scp->ldata)
	    scp->ldata = _free(scp->ldata);

	data->size = (UINT32_T) scp->avlen[1];
	data->data = (void *) xmalloc(data->size);
	if (! (data->flags & DB_DBT_MALLOC) )
	    scp->ldata = data->data;

	(void) memcpy(data->data, scp->av[1], data->size);
    }

    scp->rx++;

    /* XXX FIXME: ptr alignment is fubar here. */
SQLDEBUG(dbi, (stderr, "\tcget(%s) found  key 0x%x (%d)\n", dbi->dbi_subfile,
		key->data == NULL ? 0 : *(unsigned int *)key->data, key->size));
SQLDEBUG(dbi, (stderr, "\tcget(%s) found data 0x%x (%d)\n", dbi->dbi_subfile,
		key->data == NULL ? 0 : *(unsigned int *)data->data, data->size));

exit:
    if (rc == DB_NOTFOUND) {
SQLDEBUG(dbi, (stderr, "\tcget(%s) not found\n", dbi->dbi_subfile));
    }

leaveChroot(dbi);

SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,0x%x) rc %d subfile %s %s\n", __FUNCTION__, dbi, dbcursor, key, data, flags, rc, dbi->dbi_subfile, _KEYDATA(key, NULL, data, NULL)));
    return rc;
}

/**
 * Store (key,data) pair using db->put or dbcursor->c_put.
 * @param dbi           index database handle
 * @param dbcursor      database cursor (NULL will use db->put)
 * @param key           store key value/length/flags
 * @param data          store data value/length/flags
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_cput (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key,
			DBT * data, unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies dbi, *dbcursor, fileSystem, internalState @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = scpNew(dbi->dbi_db);
    int rc = 0;

dbg_keyval("sql_cput", dbi, dbcursor, key, data, flags);

enterChroot(dbi);

    switch (dbi->dbi_rpmtag) {
    default:
	scp->cmd = sqlite3_mprintf("INSERT OR REPLACE INTO '%q' VALUES(?, ?);",
		dbi->dbi_subfile);
	rc = cvtdberr(dbi, "sqlite3_prepare",
		sqlite3_prepare(sqldb->db, scp->cmd, (int)strlen(scp->cmd),
			&scp->pStmt, (const char **) &scp->pzErrmsg));
	if (rc) rpmlog(RPMLOG_WARNING, "cput(%s) prepare %s (%d)\n",dbi->dbi_subfile,  sqlite3_errmsg(sqldb->db), rc);
	rc = cvtdberr(dbi, "sql_bind_key",
		sql_bind_key(dbi, scp, 1, key));
	if (rc) rpmlog(RPMLOG_WARNING, "cput(%s)  key bind %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
	rc = cvtdberr(dbi, "sql_bind_data",
		sql_bind_data(dbi, scp, 2, data));
	if (rc) rpmlog(RPMLOG_WARNING, "cput(%s) data bind %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

	rc = sql_step(dbi, scp);
	if (rc) rpmlog(RPMLOG_WARNING, "cput(%s) sql_step rc %d\n", dbi->dbi_subfile, rc);

	break;
    }

    scp = scpFree(scp);

leaveChroot(dbi);

SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,0x%x) rc %d subfile %s %s\n", __FUNCTION__, dbi, dbcursor, key, data, flags, rc, dbi->dbi_subfile, _KEYDATA(key, NULL, data, NULL)));
    return rc;
}

/**
 * Is database byte swapped?
 * @param dbi           index database handle
 * @return              0 no
 */
static int sql_byteswapped (dbiIndex dbi)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int rc = 0;
SQLDEBUG(dbi, (stderr, "<-- %s(%p) rc %d subfile %s\n", __FUNCTION__, dbi, rc, dbi->dbi_subfile));
    return rc;
}

/**
 * Associate secondary database with primary.
 * @param dbi           index database handle
 * @param dbisecondary  secondary index database handle
 * @param callback      create secondary key from primary (NULL if DB_RDONLY)
 * @param flags         DB_CREATE or 0
 * @return              0 on success
 */
static int sql_associate (/*@unused@*/ dbiIndex dbi,
		/*@unused@*/ dbiIndex dbisecondary,
    /*@unused@*/int (*callback) (DB *, const DBT *, const DBT *, DBT *),
		/*@unused@*/ unsigned int flags)
	/*@*/
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) rc %d subfile %s\n", __FUNCTION__, dbi, dbisecondary, callback, flags, rc, dbi->dbi_subfile));
    return rc;
}

/**
 * Associate secondary database with primary.
 * @param dbi           index database handle
 * @param dbisecondary  secondary index database handle
 * @param callback      create secondary key from primary (NULL if DB_RDONLY)
 * @param flags         DB_CREATE or 0
 * @return              0 on success
 */
static int sql_associate_foreign (/*@unused@*/ dbiIndex dbi,
		/*@unused@*/ dbiIndex dbisecondary,
    /*@unused@*/int (*callback) (DB *, const DBT *, DBT *, const DBT *, int *),
		/*@unused@*/ unsigned int flags)
	/*@*/
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) rc %d subfile %s\n", __FUNCTION__, dbi, dbisecondary, callback, flags, rc, dbi->dbi_subfile));
    return rc;
}

/**
 * Return join cursor for list of cursors.
 * @param dbi           index database handle
 * @param curslist      NULL terminated list of database cursors
 * @retval dbcp         address of join database cursor
 * @param flags         DB_JOIN_NOSORT or 0
 * @return              0 on success
 */
static int sql_join (/*@unused@*/ dbiIndex dbi,
		/*@unused@*/ DBC ** curslist,
		/*@unused@*/ /*@out@*/ DBC ** dbcp,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, curslist, dbcp, flags, rc));
    return rc;
}

/**
 * Duplicate a database cursor.
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @retval dbcp         address of new database cursor
 * @param flags         DB_POSITION for same position, 0 for uninitialized
 * @return              0 on success
 */
static int sql_cdup (/*@unused@*/ dbiIndex dbi,
		/*@unused@*/ DBC * dbcursor,
		/*@unused@*/ /*@out@*/ DBC ** dbcp,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, dbcursor, dbcp, flags, rc));
    return rc;
}

/**
 * Retrieve (key,data) pair using dbcursor->c_pget.
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @param key           secondary retrieve key value/length/flags
 * @param pkey          primary retrieve key value/length/flags
 * @param data          primary retrieve data value/length/flags
 * @param flags         DB_NEXT, DB_SET, or 0
 * @return              0 on success
 */
static int sql_cpget (/*@unused@*/ dbiIndex dbi,
		/*@unused@*/ /*@null@*/ DBC * dbcursor,
		/*@unused@*/ DBT * key,
		/*@unused@*/ DBT * pkey,
		/*@unused@*/ DBT * data,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *pkey, *data, fileSystem @*/
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,%p,0x%x) rc %d %s\n", __FUNCTION__, dbi, dbcursor, key, pkey, data, flags, rc, _KEYDATA(key, pkey, data, NULL)));
    return rc;
}

/**
 * Retrieve count of (possible) duplicate items using dbcursor->c_count.
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @param countp        address of count
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_ccount (/*@unused@*/ dbiIndex dbi,
		/*@unused@*/ DBC * dbcursor,
		/*@unused@*/ /*@out@*/ unsigned int * countp,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
    int rc = EINVAL;
SQLDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, dbcursor, countp, flags, rc));
    return rc;
}

/** \ingroup dbi
 * Save statistics in database handle.
 * @param dbi           index database handle
 * @param flags         retrieve statistics that don't require traversal?
 * @return              0 on success
 */
static int sql_stat (dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies dbi, fileSystem, internalState @*/
{
    SCP_t scp = scpNew(dbi->dbi_db);
    int rc = 0;
    long nkeys = -1;

    dbi->dbi_stats = _free(dbi->dbi_stats);

/*@-sizeoftype@*/
    dbi->dbi_stats = (void *) xcalloc(1, sizeof(DB_HASH_STAT));
/*@=sizeoftype@*/

    scp->cmd = sqlite3_mprintf("SELECT COUNT('key') FROM '%q';", dbi->dbi_subfile);
/*@-nullstate@*/
enterChroot(dbi);
    rc = sql_get_table(dbi, scp, scp->cmd);
leaveChroot(dbi);
/*@=nullstate@*/

    if (rc == 0 && scp->nr > 0) {
assert(scp->av != NULL);
	nkeys = strtol(scp->av[1], NULL, 10);

	rpmlog(RPMLOG_DEBUG, D_("  stat on %s nkeys %ld\n"),
		dbi->dbi_subfile, nkeys);
    } else {
	if (rc) {
	    rpmlog(RPMLOG_DEBUG, D_("stat failed %s (%d)\n"),
		scp->pzErrmsg, rc);
	}
    }

    if (nkeys < 0)
	nkeys = 4096;  /* Good high value */

    ((DB_HASH_STAT *)(dbi->dbi_stats))->hash_nkeys = nkeys;

    scp = scpFree(scp);

    return rc;
}

/* Major, minor, patch version of DB.. we're not using db.. so set to 0 */
/* open, close, sync, associate, asociate_foreign, join */
/* cursor_open, cursor_close, cursor_dup, cursor_delete, cursor_get, */
/* cursor_pget?, cursor_put, cursor_count */
/* db_bytewapped, stat */
/*@observer@*/ /*@unchecked@*/
struct _dbiVec sqlitevec = {
    "Sqlite " SQLITE_VERSION,
    ((SQLITE_VERSION_NUMBER / (1000 * 1000)) % 1000),
    ((SQLITE_VERSION_NUMBER / (       1000)) % 1000),
    ((SQLITE_VERSION_NUMBER                ) % 1000),
    sql_open,
    sql_close,
    sql_sync,
    sql_associate,
    sql_associate_foreign,
    sql_join,
    sql_exists,
    sql_seqno,
    sql_copen,
    sql_cclose,
    sql_cdup,
    sql_cdel,
    sql_cget,
    sql_cpget,
    sql_cput,
    sql_ccount,
    sql_byteswapped,
    sql_stat
};

/*@=evalorderuncon@*/
/*@=modfilesystem@*/
/*@=compmempass@*/
/*@=compdef@*/
/*@=moduncon@*/
/*@=noeffectuncon@*/
/*@=globuse@*/
/*@=paramuse@*/
/*@=mustmod@*/
