#include "system.h"

#include <rpmio.h>
#include <poptIO.h>

#include "debug.h"

/**
 */
static void rpmborgArgCallback(poptContext con,
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

#define	BORG_TO_DEB	(1 << 0)
#define	BORG_TO_RPM	(1 << 1)
#define	BORG_TO_SLP	(1 << 2)
#define	BORG_TO_LSB	(1 << 3)
#define	BORG_TO_TGZ	(1 << 4)
#define	BORG_TO_PKG	(1 << 5)
#define	BORG_TO_XAR	(1 << 6)

static int _targets;
static int _install;
static int _generate;
static int _scripts;
static int _keep_version;
static int _bump;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmborgCommandsPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmborgArgCallback, 0, NULL, NULL },
/*@=type@*/

	/* XXX this bit is (default). */
  { "to-deb", 'd', POPT_BIT_SET, &_targets, BORG_TO_DEB,
        N_("Generate a Debian deb package."), NULL },
  { "to-rpm", 'r', POPT_BIT_SET, &_targets, BORG_TO_RPM,
        N_("Generate a RPM rpm package."), NULL },
  { "to-slp", '\0', POPT_BIT_SET, &_targets, BORG_TO_SLP,
        N_("Generate a Stampede slp package."), NULL },
  { "to-lsb", 'l', POPT_BIT_SET, &_targets, BORG_TO_LSB,
        N_("Generate a LSB package."), NULL },
  { "to-tgz", 't', POPT_BIT_SET, &_targets, BORG_TO_TGZ,
        N_("Generate a Slackware tgz package."), NULL },
  { "to-pkg", 'p', POPT_BIT_SET, &_targets, BORG_TO_PKG,
        N_("Generate a Solaris pkg package."), NULL },
  { "to-xar", 'x', POPT_BIT_SET, &_targets, BORG_TO_XAR,
        N_("Generate a XARv1 package."), NULL },
  { "install", 'i', POPT_ARG_VAL, &_install, -1,
        N_("Install generated package."), NULL },
  { "generate", 'g', POPT_ARG_VAL, &_generate, -1,
        N_("Generate build tree, but do not build package."), NULL },
  { "scripts", 'c', POPT_ARG_VAL, &_scripts, -1,
        N_("Include scripts in package."), NULL },
  { "keep-version", 'k', POPT_ARG_VAL, &_keep_version, -1,
        N_("Do not change version of generated package."), NULL },
  { "bump", '\0', POPT_ARG_VAL, &_bump, -1,
        N_("Increment package version by this number."), N_("number") },

#ifdef	REFERENCE

  -d, --to-deb              Generate a Debian deb package (default).
     Enables these options:
       --patch=<patch>      Specify patch file to use instead of automatically
                            looking for patch in /var/lib/borg.
       --nopatch	    Do not use patches.
       --anypatch           Use even old version os patches.
       -s, --single         Like --generate, but do not create .orig
                            directory.
       --fixperms           Munge/fix permissions and owners.
       --test               Test generated packages with lintian.
  -r, --to-rpm              Generate a RPM rpm package.
      --to-slp              Generate a Stampede slp package.
  -l, --to-lsb              Generate a LSB package.
  -t, --to-tgz              Generate a Slackware tgz package.
     Enables these options:
       --description=<desc> Specify package description.
       --version=<version>  Specify package version.
  -p, --to-pkg              Generate a Solaris pkg package.
  -i, --install             Install generated package.
  -g, --generate            Generate build tree, but do not build package.
  -c, --scripts             Include scripts in package.
  -v, --verbose             Display each command the Borg runs.
      --veryverbose         Be verbose, and also display output of run commands.
  -k, --keep-version        Do not change version of generated package.
      --bump=number         Increment package version by this number.
  -h, --help                Display this help message.
  -V, --version		    Display the Borg version number.

#endif

  POPT_TABLEEND

};

#ifdef	NOTYET
/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmborgOptionsPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmborgArgCallback, 0, NULL, NULL },
/*@=type@*/

  POPT_TABLEEND

};
#endif

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmborgArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmborgCommandsPoptTable, 0,
        N_("Commands:"), NULL },

#ifdef	NOTYET
  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmborgOptionsPoptTable, 0,
        N_("Options:"), NULL },
#endif

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: rpmborg [options] file [...]\n\
  file [...]                Package file or files to convert.\n\
  -d, --to-deb              Generate a Debian deb package (default).\n\
     Enables these options:\n\
       --patch=<patch>      Specify patch file to use instead of automatically\n\
                            looking for patch in /var/lib/borg.\n\
       --nopatch	    Do not use patches.\n\
       --anypatch           Use even old version os patches.\n\
       -s, --single         Like --generate, but do not create .orig\n\
                            directory.\n\
       --fixperms           Munge/fix permissions and owners.\n\
       --test               Test generated packages with lintian.\n\
  -r, --to-rpm              Generate a RPM rpm package.\n\
      --to-slp              Generate a Stampede slp package.\n\
  -l, --to-lsb              Generate a LSB package.\n\
  -t, --to-tgz              Generate a Slackware tgz package.\n\
     Enables these options:\n\
       --description=<desc> Specify package description.\n\
       --version=<version>  Specify package version.\n\
  -p, --to-pkg              Generate a Solaris pkg package.\n\
  -i, --install             Install generated package.\n\
  -g, --generate            Generate build tree, but do not build package.\n\
  -c, --scripts             Include scripts in package.\n\
  -v, --verbose             Display each command the Borg runs.\n\
      --veryverbose         Be verbose, and also display output of run commands.\n\
  -k, --keep-version        Do not change version of generated package.\n\
      --bump=number         Increment package version by this number.\n\
  -h, --help                Display this help message.\n\
  -V, --version		    Display the Borg's version number.\n\
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
    __progname = "rpmborg";
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
