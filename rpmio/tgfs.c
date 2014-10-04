#include "system.h"

#include <rpmio.h>
#include <poptIO.h>

#define	_RPMGFS_INTERNAL
#include <rpmgfs.h>

#include "debug.h"

#define	SPEW(_list)	if (_rpmgfs_debug) fprintf _list

static char * _gfs_fn =		"/foo/bar/baz";
static int _gfs_flags =		0;

static char * _gfs_scheme =	"mongodb";
static char * __gfs_u =		"luser";
static char * __gfs_pw =	"jasnl";
static char * _gfs_user =	"%{__gfs_u}%{?__gfs_pw::%{__gfs_pw}}";
static char * __gfs_h =		"localhost";
static char * __gfs_p =		"27017";
static char * _gfs_host =
	"%{?_gfs_user:%{_gfs_user}@}%{__gfs_h}%{?__gfs_p::%{__gfs_p}}";
static char * _gfs_db =		"gfs";
static char * _gfs_coll =	"fs";
static char * _gfs_opts =	"?ssl=false";
static char * _gfs_uri =
	"%{_gfs_scheme}://%{_gfs_host}/%{_gfs_db}";

/*==============================================================*/
static int doit(rpmgfs gfs, int ac, ARGV_t av)
{
    int rc = 1;		/* assume failure */

    if (ac < 1 || av[0] == NULL) {
	/* XXX usage */
	goto exit;
    } else if (strcmp(av[0], "get") == 0) {
	if (ac != 2) {
	    fprintf(stderr, "usage - %s get filename\n", __progname);
	    goto exit;
	}
	rc = rpmgfsGet(gfs, NULL, av[1]);
    } else if (strcmp(av[0], "put") == 0) {
	if (ac != 3) {
	    fprintf(stderr, "usage - %s put filename input_file\n", __progname);
	    goto exit;
	}
	rc = rpmgfsPut(gfs, av[1], av[2]);
    } else if (strcmp(av[0], "del") == 0) {
	if (ac != 2) {
	    fprintf(stderr, "usage - %s del filename\n", __progname);
	    goto exit;
	}
	rc = rpmgfsDel(gfs, av[1]);
    } else if (strcmp(av[0], "list") == 0) {
	rc = rpmgfsList(gfs);
    } else if (strcmp(av[0], "dump") == 0) {
	rc = rpmgfsDump(gfs);
    } else {
	fprintf(stderr, "Unknown command: %s\n", av[0]);
	goto exit;
    }

exit:
    return rc;
}

/*==============================================================*/

static struct poptOption rpmgfsOptionsTable[] = {

 { "scheme", 'S', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&_gfs_scheme, 0,
	N_("Set the GridFS SCHEME"),	N_("SCHEME") },

 { "u", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&__gfs_u, 0,
	N_("Set the GridFS U"),	N_("U") },
 { "pw", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&__gfs_pw, 0,
	N_("Set the GridFS PW"),	N_("PW") },
 { "user", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&_gfs_user, 0,
	N_("Set the GridFS USER"),	N_("USER") },

 { "h", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&__gfs_h, 0,
	N_("Set the GridFS H"),	N_("H") },
 { "p", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&__gfs_p, 0,
	N_("Set the GridFS P"),	N_("P") },
 { "host", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&_gfs_host, 0,
	N_("Set the GridFS HOST"),	N_("HOST") },

 { "db", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&_gfs_db, 0,
	N_("Set the GridFS DB"),	N_("DB") },
 { "coll", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&_gfs_coll, 0,
	N_("Set the GridFS COLL"),	N_("COLL") },
 { "opts", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&_gfs_opts, 0,
	N_("Set the GridFS OPTS"),	N_("OPTS") },
 { "uri", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&_gfs_uri, 0,
	N_("Set the GridFS URI"),	N_("URI") },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_(" Common options for all rpmio executables:"), NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        N_("\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, rpmgfsOptionsTable);
    rpmgfs gfs;
    ARGV_t av = poptGetArgs(con);
    int ac = argvCount(av);;
    int ec = -1;	/* assume failure */

    /* XXX no macros are loaded if using poptIO. */
    addMacro(NULL, "_gfs_scheme",	NULL, _gfs_scheme, -1);
    addMacro(NULL, "__gfs_u",		NULL, __gfs_u, -1);
    addMacro(NULL, "__gfs_pw",		NULL, __gfs_pw, -1);
    addMacro(NULL, "_gfs_user",		NULL, _gfs_user, -1);

    addMacro(NULL, "__gfs_h",		NULL, __gfs_h, -1);
    addMacro(NULL, "__gfs_p",		NULL, __gfs_p, -1);
    addMacro(NULL, "_gfs_host",		NULL, _gfs_host, -1);

    if (_gfs_db)
	addMacro(NULL, "_gfs_db",		NULL, _gfs_db, -1);
    if (_gfs_coll)
	addMacro(NULL, "_gfs_coll",		NULL, _gfs_coll, -1);
    if (_gfs_opts)
	addMacro(NULL, "_gfs_opts",		NULL, _gfs_opts, -1);
    addMacro(NULL, "_gfs_uri",		NULL, _gfs_uri, -1);

    gfs = rpmgfsNew(_gfs_fn, _gfs_flags);

    ec = doit(gfs, ac, av);

    gfs = rpmgfsFree(gfs);

    con = rpmioFini(con);

    rpmioClean();

    return ec;
}
