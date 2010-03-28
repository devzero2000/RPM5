#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <ugid.h>
#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

static void rpmnixChannelArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    rpmnix nix = &_nix;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case NIX_CHANNEL_ADD:
    case NIX_CHANNEL_REMOVE:
	nix->url = xstrdup(arg);
	/*@fallthrough@*/
    case NIX_CHANNEL_LIST:
    case NIX_CHANNEL_UPDATE:
	nix->op = opt->val;
	break;
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption rpmnixChannelOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	rpmnixChannelArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "add", '\0', POPT_ARG_STRING,		0, NIX_CHANNEL_ADD,
        N_("subscribe to a Nix channel"), N_("URL") },
 { "remove", '\0', POPT_ARG_STRING,		0, NIX_CHANNEL_REMOVE,
        N_("unsubscribe from a Nix channel"), N_("URL") },
 { "list", '\0', POPT_ARG_NONE,			0, NIX_CHANNEL_LIST,
        N_("list subscribed channels"), NULL },
 { "update", '\0', POPT_ARG_NONE,		0, NIX_CHANNEL_UPDATE,
        N_("download latest Nix expressions"), NULL },

#ifndef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

#ifdef	DYING
  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage:\n\
  nix-channel --add URL\n\
  nix-channel --remove URL\n\
  nix-channel --list\n\
  nix-channel --update\n\
"), NULL },
#endif

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, rpmnixChannelOptions);
    int ec = rpmnixChannel(nix);

    nix = rpmnixFree(nix);

    return ec;
}
