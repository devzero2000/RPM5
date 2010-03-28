#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

static void nixCopyClosureArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    rpmnix nix = &_nix;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case NIX_COPY_CLOSURE_FROM_HOST:
	nix->op = opt->val;
	nix->sshHost = xstrdup(arg);
	break;
    case NIX_COPY_CLOSURE_TO_HOST:
	nix->op = opt->val;
	nix->sshHost = xstrdup(arg);
	break;
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption nixCopyClosureOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixCopyClosureArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "from", '\0', POPT_ARG_STRING,	0, NIX_COPY_CLOSURE_FROM_HOST,
	N_("FIXME"), NULL },
 { "to", '\0', POPT_ARG_STRING,		0, NIX_COPY_CLOSURE_TO_HOST,
	N_("FIXME"), NULL },
 { "gzip", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_GZIP,
	N_("FIXME"), NULL },
 { "sign", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_SIGN,
	N_("FIXME"), NULL },

#ifndef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: nix-copy-closure [--from | --to] HOSTNAME [--sign] [--gzip] PATHS...\n\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, nixCopyClosureOptions);
    int ec = rpmnixCopyClosire(nix);

    nix = rpmnixFree(nix);

    return ec;
}
