#include "system.h"

#include <rpmio.h>
#include <poptIO.h>

#include "debug.h"

/**
 */
static void rpmdpkgArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

/*==============================================================*/

#define	POPT_XXX	0
#if !defined(POPT_BIT_TOGGLE)
#define	POPT_BIT_TOGGLE	(POPT_ARG_VAL|POPT_ARGFLAG_XOR)
#endif

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmdpkgCommandsPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmdpkgArgCallback, 0, NULL, NULL },
/*@=type@*/

  { "install", 'i', POPT_ARG_STRING, NULL, 'i',
        N_("?install?"), N_("<.deb file name>") },
#define	POPT_DPKG_UNPACK	1
  { "unpack", '\0', POPT_ARG_STRING, NULL, POPT_DPKG_UNPACK,
        N_("?unpack?"), N_("<.deb file name>") },
  { "record-avail", 'A', POPT_ARG_STRING, NULL, 'A',
        N_("?record-avail?"), N_("<.deb file name>") },
#define	POPT_DPKG_CONFIGURE	2
  { "configure", '\0', POPT_ARG_STRING, NULL, POPT_DPKG_CONFIGURE,
        N_("?configure?"), N_("<package>") },
  { "remove", 'r', POPT_ARG_STRING, NULL, 'r',
        N_("?remove?"), N_("<package>") },
  { "purge", 'p', POPT_ARG_STRING, NULL, 'p',
        N_("?purge?"), N_("<package>") },
#define	POPT_DPKG_TRIGGERS_ONLY	3
  { "triggers-only", '\0', POPT_ARG_STRING, NULL, POPT_DPKG_TRIGGERS_ONLY,
        N_("?triggers-only?"), N_("<package>") },

  { "listfiles", 'L', POPT_ARG_STRING, NULL, 'L',
        N_("List files `owned' by package(s)."), N_("<package>") },
  { "status", 's', POPT_ARG_STRING, NULL, 's',
        N_("Display package status details."), N_("<package>") },

#define	POPT_DPKG_GET_SELECTIONS	4
 	/* XXX optional pattern */
  { "get-selections", '\0', POPT_ARG_STRING, NULL, POPT_DPKG_GET_SELECTIONS,
        N_("Get list of selections to stdout."), N_("<pattern>") },
#define	POPT_DPKG_SET_SELECTIONS	5
  { "set-selections", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_SET_SELECTIONS,
        N_("Set package selections from stdin."), NULL },
#define	POPT_DPKG_CLEAR_SELECTIONS	6
  { "clear-selections", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_CLEAR_SELECTIONS,
        N_("Deselect every non-essential package."), NULL },

  { "print-avail", 'p', POPT_ARG_STRING, NULL, 'p',
        N_("Display available version details."), N_("<package>") },
#define	POPT_DPKG_UPDATE_AVAIL	7
  { "update-avail", '\0', POPT_ARG_STRING, NULL, POPT_DPKG_UPDATE_AVAIL,
        N_("Replace available packages info."), N_("<Packages-file>") },
#define	POPT_DPKG_MERGE_AVAIL	8
  { "merge-avail", '\0', POPT_ARG_STRING, NULL, POPT_DPKG_MERGE_AVAIL,
        N_("Merge with info from file."), N_("<Packages-file>") },
#define	POPT_DPKG_CLEAR_AVAIL	8
  { "clear-avail", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_CLEAR_AVAIL,
        N_("Erase existing available info."), NULL },
#define	POPT_DPKG_FORGET_OLD_UNAVAIL	9
  { "forget-old-unavail", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_FORGET_OLD_UNAVAIL,
        N_("Forget uninstalled unavailable pkgs."), NULL },

  { "audit", 'C', POPT_ARG_NONE, NULL, 'C',
        N_("Check for broken package(s)."), NULL },
#define	POPT_DPKG_YET_TO_UNPACK	10
  { "yet-to-unpack", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_YET_TO_UNPACK,
        N_("?yet-to-unpack?"), NULL },
 	/* XXX optional pattern */
  { "list", 'l', POPT_ARG_STRING, NULL, 'l',
        N_("List packages concisely."), N_("<pattern>") },
  { "search", 'S', POPT_ARG_STRING, NULL, 'S',
        N_("Find package(s) owning file(s)."), N_("<pattern>") },

#define	POPT_DPKG_ASSERT_SUPPORT_PREDEPENDS	11
  { "assert-support-predepends", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_ASSERT_SUPPORT_PREDEPENDS,
        N_("?assert-support-predepends?"), NULL },
#define	POPT_DPKG_ASSERT_WORKING_EPOCH	12
  { "assert-working-epoch", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_ASSERT_WORKING_EPOCH,
        N_("?assert-working-epoch?"), NULL },
#define	POPT_DPKG_ASSERT_LONG_FILENAMES	13
  { "assert-long-filenames", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_ASSERT_LONG_FILENAMES,
        N_("?assert-long-filenames?"), NULL },
#define	POPT_DPKG_ASSERT_MULTI_CONREP	14
  { "assert-multi-conrep", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_ASSERT_MULTI_CONREP,
        N_("?assert-multi-conrep?"), NULL },
#define	POPT_DPKG_PRINT_ARCH	15
  { "print-architecture", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_PRINT_ARCH,
        N_("Print dpkg architecture."), NULL },
#define	POPT_DPKG_PRINT_INSTALL_ARCH	15
  { "print-installation-architecture", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_PRINT_INSTALL_ARCH,
        N_("Print dpkg installation architecture."), NULL },
#define	POPT_DPKG_PREDEP_PACKAGE	15
  { "predep-package", '\0', POPT_ARG_NONE, NULL, POPT_DPKG_PREDEP_PACKAGE,
        N_("?predep-package?"), NULL },

#define	POPT_DPKG_COMPARE_VERSIONS	16
  { "compare-versions", '\0', POPT_ARG_STRING, NULL, POPT_DPKG_COMPARE_VERSIONS,
	N_("Compare version numbers - see below."), N_("<a> <op> <b>") },

#ifdef	NOTYET
  { "help",              'h', 0, NULL,          NULL,      helponly,      0 },
  { "version",           0,   0, NULL,          NULL,      versiononly,   0 },
  /* UK spelling. */
  { "licence",           0,   0, NULL,          NULL,      showcopyright, 0 },
  /* US spelling. */
  { "license",           0,   0, NULL,          NULL,      showcopyright, 0 },
  ACTIONBACKEND( "build",               'b', BACKEND),
  ACTIONBACKEND( "contents",            'c', BACKEND),
  ACTIONBACKEND( "control",             'e', BACKEND),
  ACTIONBACKEND( "info",                'I', BACKEND),
  ACTIONBACKEND( "field",               'f', BACKEND),
  ACTIONBACKEND( "extract",             'x', BACKEND),
  ACTIONBACKEND( "new",                 0,  BACKEND),
  ACTIONBACKEND( "old",                 0,  BACKEND),
  ACTIONBACKEND( "vextract",            'X', BACKEND),
  ACTIONBACKEND( "fsys-tarfile",        0,   BACKEND),
#endif

#ifdef	REFERENCE

  --compare-versions <a> <op> <b>  Compare version numbers - see below.\n\
  --force-help                     Show help on forcing.\n\
  -Dh|--debug=help                 Show help on debugging.\n\
\n\
  -h|--help                        Show this help message.\n\
  --version                        Show the version.\n\
  --license|--licence              Show the copyright licensing terms.\n\

#endif

  POPT_TABLEEND

};

#define	_ADMINDIR	"?ADMINDIR?"
static const char * _admindir = _ADMINDIR;
static const char * _rootdir;
static const char * _instdir;
static int _selected_only;
static int _skip_same_version;
static int _refuse_downgrade;
static int _auto_deconfigure;
static int _status_fd;
static int _notriggers;
static int _nodebsig;
static int _dryrun;
static int _debug;
static const char * _log;
static const char ** _ignore_depends;
static int _force;
static int _abort_after;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmdpkgOptionsPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmdpkgArgCallback, 0, NULL, NULL },
/*@=type@*/

#ifdef	NOTYET
/*
  ACTION( "command-fd",                   'c', act_commandfd,   commandfd     ),
*/

  { "status-fd",         0,   1, NULL,          NULL,      setpipe, 0, &status_pipes },
  { "log",               0,   1, NULL,          &log_file, NULL,    0 },
  { "pending",           'a', 0, &f_pending,    NULL,      NULL,    1 },
  { "recursive",         'R', 0, &f_recursive,  NULL,      NULL,    1 },
  { "no-act",            0,   0, &f_noact,      NULL,      NULL,    1 },
  { "dry-run",           0,   0, &f_noact,      NULL,      NULL,    1 },
  { "simulate",          0,   0, &f_noact,      NULL,      NULL,    1 },
  { "no-debsig",         0,   0, &f_nodebsig,   NULL,      NULL,    1 },
  /* Alias ('G') for --refuse. */
  {  NULL,               'G', 0, &fc_downgrade, NULL,      NULL,    0 },
  { "selected-only",     'O', 0, &f_alsoselect, NULL,      NULL,    0 },
  { "triggers",           0,  0, &f_triggers,   NULL,      NULL,    1 },
  { "no-triggers",        0,  0, &f_triggers,   NULL,      NULL,   -1 },
  /* FIXME: Remove ('N') sometime. */
  { "no-also-select",    'N', 0, &f_alsoselect, NULL,      NULL,    0 },
  { "skip-same-version", 'E', 0, &f_skipsame,   NULL,      NULL,    1 },
  { "auto-deconfigure",  'B', 0, &f_autodeconf, NULL,      NULL,    1 },
  OBSOLETE( "largemem", 0 ),
  OBSOLETE( "smallmem", 0 ),
  { "root",              0,   1, NULL,          NULL,      setroot,       0 },
  { "abort-after",       0,   1, &errabort,     NULL,      setinteger,    0 },
  { "admindir",          0,   1, NULL,          &admindir, NULL,          0 },
  { "instdir",           0,   1, NULL,          &instdir,  NULL,          0 },
  { "ignore-depends",    0,   1, NULL,          NULL,      ignoredepends, 0 },
  { "force",             0,   2, NULL,          NULL,      setforce,      1 },
  { "refuse",            0,   2, NULL,          NULL,      setforce,      0 },
  { "no-force",          0,   2, NULL,          NULL,      setforce,      0 },
  { "debug",             'D', 1, NULL,          NULL,      setdebug,      0 },
#endif

	/* XXX default value */
  { "admindir", '\0', POPT_ARG_STRING, &_admindir, 0,
        N_("Use <directory> instead of ADMINDIR."), N_("<directory>") },
  { "root", '\0', POPT_ARG_STRING, &_rootdir, 0,
        N_("Install on a different root directory."), N_("<directory>") },
  { "instdir", '\0', POPT_ARG_STRING, &_instdir, 0,
        N_("Change installation dir without changing ADMINDIR."), N_("<directory>") },
  { "selected-only", 'O', POPT_ARG_VAL, &_selected_only, -1,
        N_("Skip packages not selected for install/upgrade."), NULL },
  { "skip-same-version", 'E', POPT_ARG_VAL, &_skip_same_version, -1,
        N_("Skip packages whose same version is installed."), NULL },
  { "refuse-downgrade", 'G', POPT_ARG_VAL, &_refuse_downgrade, -1,
        N_("Skip packages with earlier version than installed."), NULL },
  { "auto-deconfigure", 'B', POPT_ARG_VAL, &_auto_deconfigure, -1,
        N_("Skip packages with earlier version than installed."), NULL },
	/* XXX enabler needed too. */
  { "no-triggers", '\0', POPT_ARG_VAL, &_notriggers, -1,
        N_("Skip or force consequential trigger processing."), NULL },
  { "no-debsig", '\0', POPT_ARG_VAL, &_nodebsig, -1,
        N_("Do not try to verify package signatures."), NULL },
  { "dryrun", '\0', POPT_ARG_VAL, &_dryrun, -1,
        N_("ust say what we would do - don't do it."), NULL },
  { "no-act", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dryrun, -1,
        N_("ust say what we would do - don't do it."), NULL },
  { "simulate", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dryrun, -1,
        N_("ust say what we would do - don't do it."), NULL },
  { "debug", 'D', POPT_ARG_INT, &_debug, -1,
        N_("Enable debugging (see -Dhelp or --debug=help)."), N_("<octal>") },
  { "status-fd", '\0', POPT_ARG_INT, &_status_fd, -1,
        N_("Send status change updates to file descriptor <n>."), N_("<n>") },
  { "log", '\0', POPT_ARG_STRING, &_log, 0,
        N_("Log status changes and actions to <filename>."), N_("<filename>") },
	/* XXX dpkg uses comma separated list. */
  { "ignore-depends", '\0', POPT_ARG_ARGV, &_ignore_depends, 0,
        N_("Ignore dependencies involving <package>."), N_("<package>") },
	/* XXX disabler needed too. */
  { "force", '\0', POPT_ARG_VAL, &_force, 1,
        N_("Override problems (see --force-help)."), NULL },
	/* XXX disablers needed too. */
  { "no-force", '\0', POPT_ARG_VAL, &_force, 0,
        N_("Stop when problems encountered."), NULL },
  { "refuse", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_force, 0,
        N_("Stop when problems encountered."), NULL },
  { "abort-after", 'B', POPT_ARG_INT, &_abort_after, -1,
        N_("Send status change updates to file descriptor <n>."), N_("<n>") },

  { "recursive", 'R', POPT_ARG_STRING, NULL, 'R',
        N_("?recursive?"), N_("<directory>") },
  { "pending", 'a', POPT_ARG_NONE, NULL, 'a',
        N_("?pending?"), NULL },

  POPT_TABLEEND

};

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmdpkgArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmdpkgCommandsPoptTable, 0,
        N_("Commands:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmdpkgOptionsPoptTable, 0,
        N_("Options:"), NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Commands:\n\
  -i|--install       <.deb file name> ... | -R|--recursive <directory> ...\n\
  --unpack           <.deb file name> ... | -R|--recursive <directory> ...\n\
  -A|--record-avail  <.deb file name> ... | -R|--recursive <directory> ...\n\
  --configure        <package> ... | -a|--pending\n\
  --triggers-only    <package> ... | -a|--pending\n\
  -r|--remove        <package> ... | -a|--pending\n\
  -P|--purge         <package> ... | -a|--pending\n\
  --get-selections [<pattern> ...] Get list of selections to stdout.\n\
  --set-selections                 Set package selections from stdin.\n\
  --clear-selections               Deselect every non-essential package.\n\
  --update-avail <Packages-file>   Replace available packages info.\n\
  --merge-avail <Packages-file>    Merge with info from file.\n\
  --clear-avail                    Erase existing available info.\n\
  --forget-old-unavail             Forget uninstalled unavailable pkgs.\n\
  -s|--status <package> ...        Display package status details.\n\
  -p|--print-avail <package> ...   Display available version details.\n\
  -L|--listfiles <package> ...     List files `owned' by package(s).\n\
  -l|--list [<pattern> ...]        List packages concisely.\n\
  -S|--search <pattern> ...        Find package(s) owning file(s).\n\
  -C|--audit                       Check for broken package(s).\n\
  --print-architecture             Print dpkg architecture.\n\
  --compare-versions <a> <op> <b>  Compare version numbers - see below.\n\
  --force-help                     Show help on forcing.\n\
  -Dh|--debug=help                 Show help on debugging.\n\
\n\
  -h|--help                        Show this help message.\n\
  --version                        Show the version.\n\
  --license|--licence              Show the copyright licensing terms.\n\
\n\
Use dpkg -b|--build|-c|--contents|-e|--control|-I|--info|-f|--field|\n\
 -x|--extract|-X|--vextract|--fsys-tarfile  on archives (type %s --help).\n\
\n\
For internal use: dpkg --assert-support-predepends | --predep-package |\n\
  --assert-working-epoch | --assert-long-filenames | --assert-multi-conrep.\n\
\n\
Options:\n\
  --admindir=<directory>     Use <directory> instead of %s.\n\
  --root=<directory>         Install on a different root directory.\n\
  --instdir=<directory>      Change installation dir without changing admin dir.\n\
  -O|--selected-only         Skip packages not selected for install/upgrade.\n\
  -E|--skip-same-version     Skip packages whose same version is installed.\n\
  -G|--refuse-downgrade      Skip packages with earlier version than installed.\n\
  -B|--auto-deconfigure      Install even if it would break some other package.\n\
  --[no-]triggers            Skip or force consequential trigger processing.\n\
  --no-debsig                Do not try to verify package signatures.\n\
  --no-act|--dry-run|--simulate\n\
                             Just say what we would do - don't do it.\n\
  -D|--debug=<octal>         Enable debugging (see -Dhelp or --debug=help).\n\
  --status-fd <n>            Send status change updates to file descriptor <n>.\n\
  --log=<filename>           Log status changes and actions to <filename>.\n\
  --ignore-depends=<package>,...\n\
                             Ignore dependencies involving <package>.\n\
  --force-...                Override problems (see --force-help).\n\
  --no-force-...|--refuse-...\n\
                             Stop when problems encountered.\n\
  --abort-after <n>          Abort after encountering <n> errors.\n\
\n\
Comparison operators for --compare-versions are:\n\
  lt le eq ne ge gt       (treat empty version as earlier than any version);\n\
  lt-nl le-nl ge-nl gt-nl (treat empty version as later than any version);\n\
  < << <= = >= >> >       (only for compatibility with control file syntax).\n\
\n\
Use `dselect' or `aptitude' for user-friendly package management.\n\
\n\
Type dpkg --help for help about installing and deinstalling packages [*];\n\
Use `dselect' or `aptitude' for user-friendly package management;\n\
Type dpkg -Dhelp for a list of dpkg debug flag values;\n\
Type dpkg --force-help for a list of forcing options;\n\
Type dpkg-deb --help for help about manipulating *.deb files;\n\
Type dpkg --license for copyright license and lack of warranty (GNU GPL) [*].\n\
\n\
Options marked [*] produce a lot of output - pipe it through `less' or `more' !\n\
"), NULL },

  POPT_TABLEEND

};

int
main(int argc, char *argv[])
	/*@globals __assert_program_name,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies __assert_program_name, _rpmrepo,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    const char ** av = NULL;
    int ac;
    int rc = 1;		/* assume failure. */
    int i;

/*@-observertrans -readonlytrans @*/
    __progname = "rpmdpkg";
/*@=observertrans =readonlytrans @*/

    av = poptGetArgs(optCon);
    if (av == NULL || av[0] == NULL) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }
    ac = argvCount(av);

    if (av != NULL)
    for (i = 0; i < ac; i++) {
    }
    rc = 0;

exit:
    optCon = rpmioFini(optCon);

    return rc;
}
