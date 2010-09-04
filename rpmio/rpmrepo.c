/** \ingroup rpmio
 * \file rpmio/rpmrepo.c
 */

#include "system.h"

#if defined(WITH_SQLITE)
#include <sqlite3.h>
#ifdef	__LCLINT__
/*@-incondefs -redecl @*/
extern const char *sqlite3_errmsg(sqlite3 *db)
	/*@*/;
extern int sqlite3_open(
  const char *filename,		   /* Database filename (UTF-8) */
  /*@out@*/ sqlite3 **ppDb	   /* OUT: SQLite db handle */
)
	/*@modifies *ppDb @*/;
extern int sqlite3_exec(
  sqlite3 *db,			   /* An open database */
  const char *sql,		   /* SQL to be evaluted */
  int (*callback)(void*,int,char**,char**),  /* Callback function */
  void *,			   /* 1st argument to callback */
  /*@out@*/ char **errmsg	   /* Error msg written here */
)
	/*@modifies db, *errmsg @*/;
extern int sqlite3_prepare(
  sqlite3 *db,			   /* Database handle */
  const char *zSql,		   /* SQL statement, UTF-8 encoded */
  int nByte,			   /* Maximum length of zSql in bytes. */
  /*@out@*/ sqlite3_stmt **ppStmt, /* OUT: Statement handle */
  /*@out@*/ const char **pzTail	   /* OUT: Pointer to unused portion of zSql */
)
	/*@modifies *ppStmt, *pzTail @*/;
extern int sqlite3_reset(sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_step(sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_finalize(/*@only@*/ sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_close(sqlite3 * db)
	/*@modifies db @*/;
/*@=incondefs =redecl @*/
#endif	/* __LCLINT__ */
#endif	/* WITH_SQLITE */

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>
#include <poptIO.h>

#define	_RPMREPO_INTERNAL
#include <rpmrepo.h>

#include "debug.h"

/*@unchecked@*/
int _rpmrepo_debug = 0;

#define REPODBG(_l) if (_rpmrepo_debug) fprintf _l

/*==============================================================*/

/*@unchecked@*/ /*@observer@*/
static const char primary_xml_init[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<metadata xmlns=\"http://linux.duke.edu/metadata/common\" xmlns:rpm=\"http://linux.duke.edu/metadata/rpm\" packages=\"0\">\n";
/*@unchecked@*/ /*@observer@*/
static const char primary_xml_fini[] = "</metadata>\n";

/*@unchecked@*/ /*@observer@*/
static const char filelists_xml_init[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<filelists xmlns=\"http://linux.duke.edu/metadata/filelists\" packages=\"0\">\n";
/*@unchecked@*/ /*@observer@*/
static const char filelists_xml_fini[] = "</filelists>\n";

/*@unchecked@*/ /*@observer@*/
static const char other_xml_init[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<otherdata xmlns=\"http://linux.duke.edu/metadata/other\" packages=\"0\">\n";
/*@unchecked@*/ /*@observer@*/
static const char other_xml_fini[] = "</otherdata>\n";

/*@unchecked@*/ /*@observer@*/
static const char repomd_xml_init[] = "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<repomd xmlns=\"http://linux.duke.edu/metadata/repo\">\n";
/*@unchecked@*/ /*@observer@*/
static const char repomd_xml_fini[] = "</repomd>\n";

/* XXX todo: wire up popt aliases and bury the --queryformat glop externally. */
/*@unchecked@*/ /*@observer@*/
static const char primary_xml_qfmt[] =
#include "yum_primary_xml"
;

/*@unchecked@*/ /*@observer@*/
static const char filelists_xml_qfmt[] =
#include "yum_filelists_xml"
;

/*@unchecked@*/ /*@observer@*/
static const char other_xml_qfmt[] =
#include "yum_other_xml"
;

/*@unchecked@*/ /*@observer@*/
static const char primary_yaml_qfmt[] =
#include "wnh_primary_yaml"
;

/*@unchecked@*/ /*@observer@*/
static const char filelists_yaml_qfmt[] =
#include "wnh_filelists_yaml"
;

/*@unchecked@*/ /*@observer@*/
static const char other_yaml_qfmt[] =
#include "wnh_other_yaml"
;

/*@unchecked@*/ /*@observer@*/
static const char Packages_qfmt[] =
#include "deb_Packages"
;

/*@unchecked@*/ /*@observer@*/
static const char Sources_qfmt[] =
#include "deb_Sources"
;

/*@-nullassign@*/
/*@unchecked@*/ /*@observer@*/
static const char *primary_sql_init[] = {
"PRAGMA synchronous = \"OFF\";",
"pragma locking_mode = \"EXCLUSIVE\";",
"CREATE TABLE conflicts (  pkgKey INTEGER,  name TEXT,  flags TEXT,  epoch TEXT,  version TEXT,  release TEXT );",
"CREATE TABLE db_info (dbversion INTEGER,  checksum TEXT);",
"CREATE TABLE files (  pkgKey INTEGER,  name TEXT,  type TEXT );",
"CREATE TABLE obsoletes (  pkgKey INTEGER,  name TEXT,  flags TEXT,  epoch TEXT,  version TEXT,  release TEXT );",
"CREATE TABLE packages (  pkgKey INTEGER PRIMARY KEY,  pkgId TEXT,  name TEXT,  arch TEXT,  version TEXT,  epoch TEXT,  release TEXT,  summary TEXT,  description TEXT,  url TEXT,  time_file INTEGER,  time_build INTEGER,  rpm_license TEXT,  rpm_vendor TEXT,  rpm_group TEXT,  rpm_buildhost TEXT,  rpm_sourcerpm TEXT,  rpm_header_start INTEGER,  rpm_header_end INTEGER,  rpm_packager TEXT,  size_package INTEGER,  size_installed INTEGER,  size_archive INTEGER,  location_href TEXT,  location_base TEXT,  checksum_type TEXT);",
"CREATE TABLE provides (  pkgKey INTEGER,  name TEXT,  flags TEXT,  epoch TEXT,  version TEXT,  release TEXT );",
"CREATE TABLE requires (  pkgKey INTEGER,  name TEXT,  flags TEXT,  epoch TEXT,  version TEXT,  release TEXT  );",
"CREATE INDEX filenames ON files (name);",
"CREATE INDEX packageId ON packages (pkgId);",
"CREATE INDEX packagename ON packages (name);",
"CREATE INDEX pkgconflicts on conflicts (pkgKey);",
"CREATE INDEX pkgobsoletes on obsoletes (pkgKey);",
"CREATE INDEX pkgprovides on provides (pkgKey);",
"CREATE INDEX pkgrequires on requires (pkgKey);",
"CREATE INDEX providesname ON provides (name);",
"CREATE INDEX requiresname ON requires (name);",
"CREATE TRIGGER removals AFTER DELETE ON packages\
\n    BEGIN\n\
\n    DELETE FROM files WHERE pkgKey = old.pkgKey;\
\n    DELETE FROM requires WHERE pkgKey = old.pkgKey;\
\n    DELETE FROM provides WHERE pkgKey = old.pkgKey;\
\n    DELETE FROM conflicts WHERE pkgKey = old.pkgKey;\
\n    DELETE FROM obsoletes WHERE pkgKey = old.pkgKey;\
\n    END;",
"INSERT into db_info values (9, 'direct_create');",
    NULL
};
/*XXX todo: DBVERSION needs to be set */

/*@unchecked@*/ /*@observer@*/
static const char *filelists_sql_init[] = {
"PRAGMA synchronous = \"OFF\";",
"pragma locking_mode = \"EXCLUSIVE\";",
"CREATE TABLE db_info (dbversion INTEGER, checksum TEXT);",
"CREATE TABLE filelist (  pkgKey INTEGER,  name TEXT,  type TEXT );",
"CREATE TABLE packages (  pkgKey INTEGER PRIMARY KEY,  pkgId TEXT);",
"CREATE INDEX filelistnames ON filelist (name);",
"CREATE INDEX keyfile ON filelist (pkgKey);",
"CREATE INDEX pkgId ON packages (pkgId);",
"CREATE TRIGGER remove_filelist AFTER DELETE ON packages\
\n    BEGIN\
\n    DELETE FROM filelist WHERE pkgKey = old.pkgKey;\
\n    END;",
"INSERT into db_info values (9, 'direct_create');",
    NULL
};
/*XXX todo: DBVERSION needs to be set */

/*@unchecked@*/ /*@observer@*/
static const char *other_sql_init[] = {
"PRAGMA synchronous = \"OFF\";",
"pragma locking_mode = \"EXCLUSIVE\";",
"CREATE TABLE changelog (  pkgKey INTEGER,  author TEXT,  date INTEGER,  changelog TEXT);",
"CREATE TABLE db_info (dbversion INTEGER, checksum TEXT);",
"CREATE TABLE packages (  pkgKey INTEGER PRIMARY KEY,  pkgId TEXT);",
"CREATE INDEX keychange ON changelog (pkgKey);",
"CREATE INDEX pkgId ON packages (pkgId);",
"CREATE TRIGGER remove_changelogs AFTER DELETE ON packages\
\n    BEGIN\
\n    DELETE FROM changelog WHERE pkgKey = old.pkgKey;\
\n    END;",
"INSERT into db_info values (9, 'direct_create');",
    NULL
};
/*XXX todo: DBVERSION needs to be set */
/*@=nullassign@*/

/* packages   1 pkgKey INTEGER PRIMARY KEY */
/* packages   2 pkgId TEXT */
/* packages   3 name TEXT */
/* packages   4 arch TEXT */
/* packages   5 version TEXT */
/* packages   6 epoch TEXT */
/* packages   7 release TEXT */
/* packages   8 summary TEXT */
/* packages   9 description TEXT */
/* packages  10 url TEXT */
/* packages  11 time_file INTEGER */
/* packages  12 time_build INTEGER */
/* packages  13 rpm_license TEXT */
/* packages  14 rpm_vendor TEXT */
/* packages  15 rpm_group TEXT */
/* packages  16 rpm_buildhost TEXT */
/* packages  17 rpm_sourcerpm TEXT */
/* packages  18 rpm_header_start INTEGER */
/* packages  19 rpm_header_end INTEGER */
/* packages  20 rpm_packager TEXT */
/* packages  21 size_package INTEGER */
/* packages  22 size_installed INTEGER */
/* packages  23 size_archive INTEGER */
/* packages  24 location_href TEXT */
/* packages  25 location_base TEXT */
/* packages  26 checksum_type TEXT */
/* obsoletes  1 pkgKey INTEGER */
/* obsoletes  2 name TEXT */
/* obsoletes  3 flags TEXT */
/* obsoletes  4 epoch TEXT */
/* obsoletes  5 version TEXT */
/* obsoletes  6 release TEXT */
/* provides   1 pkgKey INTEGER */
/* provides   2 name TEXT */
/* provides   3 flags TEXT */
/* provides   4 epoch TEXT */
/* provides   5 version TEXT */
/* provides   6 release TEXT */
/* conflicts  1 pkgKey INTEGER */
/* conflicts  2 name TEXT */
/* conflicts  3 flags TEXT */
/* conflicts  4 epoch TEXT */
/* conflicts  5 version TEXT */
/* conflicts  6 release TEXT */
/* requires   1 pkgKey INTEGER */
/* requires   2 name TEXT */
/* requires   3 flags TEXT */
/* requires   4 epoch TEXT */
/* requires   5 version TEXT */
/* requires   6 release TEXT */
/* files      1 pkgKey INTEGER */
/* files      2 name TEXT */
/* files      3 type TEXT */

/*@unchecked@*/ /*@observer@*/
static const char primary_sql_qfmt[] =
#include "yum_primary_sqlite"
;

/* packages  1 pkgKey INTEGER PRIMARY KEY */
/* packages  2 pkgId TEXT */
/* filelist  1 pkgKey INTEGER */
/* filelist  2 name TEXT */
/* filelist  3 type TEXT */

/*@unchecked@*/ /*@observer@*/
static const char filelists_sql_qfmt[] =
#include "yum_filelists_sqlite"
;

/* packages  1 pkgKey INTEGER PRIMARY KEY */
/* packages  2 pkgId TEXT */
/* changelog 1 pkgKey INTEGER */
/* changelog 2 author TEXT */
/* changelog 3 date INTEGER */
/* changelog 4 changelog TEXT */

/*@unchecked@*/ /*@observer@*/
static const char other_sql_qfmt[] =
#include "yum_other_sqlite"
;

/* XXX static when ready. */
/*@-fullinitblock@*/
/*@unchecked@*/
static struct rpmrepo_s __repo = {
    .flags	= REPO_FLAGS_PRETTY,

    .tempdir	= ".repodata",
    .finaldir	= "repodata",
    .olddir	= ".olddata",
    .markup	= ".xml",
    .pkgalgo	= PGPHASHALGO_SHA1,
    .algo	= PGPHASHALGO_SHA1,
    .primary	= {
	.type	= "primary",
	.xml_init= primary_xml_init,
	.xml_qfmt= primary_xml_qfmt,
	.xml_fini= primary_xml_fini,
	.sql_init= primary_sql_init,
	.sql_qfmt= primary_sql_qfmt,
#ifdef	NOTYET	/* XXX char **?!? */
	.sql_fini= NULL,
#endif
	.yaml_init= NULL,
	.yaml_qfmt= primary_yaml_qfmt,
	.yaml_fini= NULL,
	.Packages_init= NULL,
	.Packages_qfmt= NULL,
	.Packages_fini= NULL,
	.Sources_init= NULL,
	.Sources_qfmt= NULL,
	.Sources_fini= NULL
    },
    .filelists	= {
	.type	= "filelists",
	.xml_init= filelists_xml_init,
	.xml_qfmt= filelists_xml_qfmt,
	.xml_fini= filelists_xml_fini,
	.sql_init= filelists_sql_init,
	.sql_qfmt= filelists_sql_qfmt,
#ifdef	NOTYET	/* XXX char **?!? */
	.sql_fini= NULL,
#endif
	.yaml_init= NULL,
	.yaml_qfmt= filelists_yaml_qfmt,
	.yaml_fini= NULL,
	.Packages_init= NULL,
	.Packages_qfmt= NULL,
	.Packages_fini= NULL,
	.Sources_init= NULL,
	.Sources_qfmt= NULL,
	.Sources_fini= NULL
    },
    .other	= {
	.type	= "other",
	.xml_init= other_xml_init,
	.xml_qfmt= other_xml_qfmt,
	.xml_fini= other_xml_fini,
	.sql_init= other_sql_init,
	.sql_qfmt= other_sql_qfmt,
#ifdef	NOTYET	/* XXX char **?!? */
	.sql_fini= NULL,
#endif
	.yaml_init= NULL,
	.yaml_qfmt= other_yaml_qfmt,
	.yaml_fini= NULL,
	.Packages_init= NULL,
	.Packages_qfmt= NULL,
	.Packages_fini= NULL,
	.Sources_init= NULL,
	.Sources_qfmt= NULL,
	.Sources_fini= NULL
    },
    .repomd	= {
	.type	= "repomd",
	.xml_init= repomd_xml_init,
	.xml_qfmt= NULL,
	.xml_fini= repomd_xml_fini,
	.sql_init= NULL,
	.sql_qfmt= NULL,
#ifdef	NOTYET	/* XXX char **?!? */
	.sql_fini= NULL,
#endif
	.yaml_init= NULL,
	.yaml_qfmt= NULL,
	.yaml_fini= NULL,
	.Packages_init= NULL,
	.Packages_qfmt= Packages_qfmt,
	.Packages_fini= NULL,
	.Sources_init= NULL,
	.Sources_qfmt= Sources_qfmt,
	.Sources_fini= NULL
    }
};
/*@=fullinitblock@*/

/*@unchecked@*/
static rpmrepo _repo = &__repo;

/*==============================================================*/
void
rpmrepoError(int lvl, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    (void) fflush(NULL);
    (void) fprintf(stderr, "%s: ", __progname);
    (void) vfprintf(stderr, fmt, ap);
    va_end (ap);
    (void) fprintf(stderr, "\n");
    if (lvl)
	exit(EXIT_FAILURE);
}

/*==============================================================*/

/*@unchecked@*/
static int compression = -1;

/*@unchecked@*/ /*@observer@*/
static struct poptOption repoCompressionPoptTable[] = {
 { "uncompressed", '\0', POPT_ARG_VAL,		&compression, 0,
	N_("don't compress"), NULL },
 { "gzip", 'Z', POPT_ARG_VAL,			&compression, 1,
	N_("use gzip compression"), NULL },
 { "bzip2", '\0', POPT_ARG_VAL,			&compression, 2,
	N_("use bzip2 compression"), NULL },
 { "lzma", '\0', POPT_ARG_VAL,			&compression, 3,
	N_("use lzma compression"), NULL },
 { "xz", '\0', POPT_ARG_VAL,			&compression, 4,
	N_("use xz compression"), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
struct poptOption _rpmrepoOptions[] = {

 { "quiet", 'q', POPT_ARG_VAL,			&__repo.quiet, 0,
	N_("output nothing except for serious errors"), NULL },
 { "verbose", 'v', 0,				NULL, (int)'v',
	N_("output more debugging info."), NULL },
 { "dryrun", '\0', POPT_BIT_SET,	&__repo.flags, REPO_FLAGS_DRYRUN,
	N_("sanity check arguments, don't create metadata"), NULL },
 { "excludes", 'x', POPT_ARG_ARGV,		&__repo.exclude_patterns, 0,
	N_("glob PATTERN(s) to exclude"), N_("PATTERN") },
 { "includes", 'i', POPT_ARG_ARGV,		&__repo.include_patterns, 0,
	N_("glob PATTERN(s) to include"), N_("PATTERN") },
 { "basedir", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&__repo.basedir, 0,
	N_("top level directory"), N_("DIR") },
 { "baseurl", 'u', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&__repo.baseurl, 0,
	N_("baseurl to append on all files"), N_("BASEURL") },
#ifdef	NOTYET
 { "groupfile", 'g', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&__repo.groupfile, 0,
	N_("path to groupfile to include in metadata"), N_("FILE") },
#endif
 { "pretty", 'p', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,		&__repo.flags, REPO_FLAGS_PRETTY,
	N_("make sure all xml generated is formatted"), NULL },
 { "checkts", 'C', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__repo.flags, REPO_FLAGS_CHECKTS,
	N_("check timestamps on files vs the metadata to see if we need to update"), NULL },
 { "database", 'd', POPT_BIT_SET,		&__repo.flags, REPO_FLAGS_DATABASE,
	N_("create sqlite3 database files"), NULL },
 { "split", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,		&__repo.flags, REPO_FLAGS_SPLIT,
	N_("generate split media"), NULL },
 { "pkglist", 'l', POPT_ARG_ARGV|POPT_ARGFLAG_DOC_HIDDEN,	&__repo.manifests, 0,
	N_("use only the files listed in this file from the directory specified"), N_("FILE") },
 { "outputdir", 'o', POPT_ARG_STRING,		&__repo.outputdir, 0,
	N_("<dir> = optional directory to output to"), N_("DIR") },
 { "skip-symlinks", 'S', POPT_BIT_SET,		&__repo.flags, REPO_FLAGS_NOFOLLOW,
	N_("ignore symlinks of packages"), NULL },
 { "unique-md-filenames", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &__repo.flags, REPO_FLAGS_UNIQUEMDFN,
	N_("include the file's checksum in the filename, helps with proxies"), NULL },

  POPT_TABLEEND

};

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmrepoOptionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, _rpmrepoOptions, 0,
	N_("Repository options:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
	N_("Fts(3) traversal options:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, repoCompressionPoptTable, 0,
	N_("Available compressions:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioDigestPoptTable, 0,
	N_("Available digests:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

static int rpmrepoInitPopt(rpmrepo repo, char ** av)
	/*@modifies repo @*/
{
    void *use =  repo->_item.use;
    void *pool = repo->_item.pool;
    int ac = argvCount((ARGV_t)av);
    poptContext con = rpmioInit(ac, av, rpmrepoOptionsTable);
    int rc = 0;

    *repo = *_repo;	/* structure assignment */
    repo->_item.use = use;
    repo->_item.pool = pool;

    repo->con = con;

    /* XXX Impedanace match against poptIO common code. */
    if (rpmIsVerbose())
	repo->verbose++;
    if (rpmIsDebug())
	repo->verbose++;

    repo->ftsoptions = (rpmioFtsOpts ? rpmioFtsOpts : FTS_PHYSICAL);
    switch (repo->ftsoptions & (FTS_LOGICAL|FTS_PHYSICAL)) {
    case (FTS_LOGICAL|FTS_PHYSICAL):
	rpmrepoError(1, "FTS_LOGICAL and FTS_PYSICAL are mutually exclusive");
        /*@notreached@*/ break;
    case 0:
        repo->ftsoptions |= FTS_PHYSICAL;
        break;
    }

    repo->algo = (rpmioDigestHashAlgo >= 0
		? (rpmioDigestHashAlgo & 0xff)  : PGPHASHALGO_SHA1);

    repo->compression = (compression >= 0 ? compression : 1);
    switch (repo->compression) {
    case 0:
	repo->suffix = NULL;
	repo->wmode = "w.ufdio";
	break;
    default:
	/*@fallthrough@*/
    case 1:
	repo->suffix = ".gz";
	repo->wmode = "w9.gzdio";
	break;
    case 2:
	repo->suffix = ".bz2";
	repo->wmode = "w9.bzdio";
	break;
    case 3:
	repo->suffix = ".lzma";
	repo->wmode = "w.lzdio";
	break;
    case 4:
	repo->suffix = ".xz";
	repo->wmode = "w.xzdio";
	break;
    }

    repo->av = poptGetArgs(repo->con);

    return rc;
}

static void rpmrepoFini(void * _repo)
	/*@globals fileSystem @*/
	/*@modifies *_repo, fileSystem @*/
{
    rpmrepo repo = _repo;

    repo->primary.digest = _free(repo->primary.digest);
    repo->primary.Zdigest = _free(repo->primary.Zdigest);
    repo->filelists.digest = _free(repo->filelists.digest);
    repo->filelists.Zdigest = _free(repo->filelists.Zdigest);
    repo->other.digest = _free(repo->other.digest);
    repo->other.Zdigest = _free(repo->other.Zdigest);
    repo->repomd.digest = _free(repo->repomd.digest);
    repo->repomd.Zdigest = _free(repo->repomd.Zdigest);
    repo->outputdir = _free(repo->outputdir);
    repo->pkglist = argvFree(repo->pkglist);
    repo->directories = argvFree(repo->directories);
    repo->manifests = argvFree(repo->manifests);
/*@-onlytrans -refcounttrans @*/
    repo->excludeMire = mireFreeAll(repo->excludeMire, repo->nexcludes);
    repo->includeMire = mireFreeAll(repo->includeMire, repo->nincludes);
/*@=onlytrans =refcounttrans @*/
    repo->exclude_patterns = argvFree(repo->exclude_patterns);
    repo->include_patterns = argvFree(repo->include_patterns);

    repo->con = poptFreeContext(repo->con);

}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmrepoPool = NULL;

static rpmrepo rpmrepoGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmrepoPool, fileSystem @*/
	/*@modifies pool, _rpmrepoPool, fileSystem @*/
{
    rpmrepo repo;

    if (_rpmrepoPool == NULL) {
	_rpmrepoPool = rpmioNewPool("repo", sizeof(*repo), -1, _rpmrepo_debug,
			NULL, NULL, rpmrepoFini);
	pool = _rpmrepoPool;
    }
    repo = (rpmrepo) rpmioGetPool(pool, sizeof(*repo));
    memset(((char *)repo)+sizeof(repo->_item), 0, sizeof(*repo)-sizeof(repo->_item));
    return repo;
}

rpmrepo rpmrepoNew(char ** av, int flags)
{
    rpmrepo repo = rpmrepoGetPool(_rpmrepoPool);
    int xx;

    xx = rpmrepoInitPopt(repo, av);

    return rpmrepoLink(repo);
}
