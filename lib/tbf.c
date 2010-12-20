#include "system.h"

#include <rpmio.h>
#include <rpmbf.h>
#include <argv.h>
#include <poptIO.h>

#include <rpmcli.h>
#include <rpmgi.h>

#include "debug.h"

static const char * gitagstr;
static const char * gikeystr;

static rpmtransFlags transFlags;
static rpmdepFlags depFlags;
static rpmVSFlags vsFlags =
	_RPMVSF_NOSIGNATURES | RPMVSF_NOHDRCHK | _RPMVSF_NOPAYLOAD | _RPMVSF_NOHEADER;

#ifdef	NOTYET
static const char * queryFormat;
static const char * defaultQueryFormat =
	"%{name}-%{version}-%{release}.%|SOURCERPM?{%{arch}.rpm}:{%|ARCH?{src.rpm}:{pubkey}|}|";
#endif

static struct poptOption optionsTable[] = {

 { "tag", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &gitagstr, 0,
	N_("iterate tag index"), NULL },
 { "key", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &gikeystr, 0,
	N_("tag value key"), NULL },

#ifdef	NOTYET
 { "transaction", 'T', POPT_BIT_SET, &giFlags, (RPMGI_TSADD|RPMGI_TSORDER),
	N_("create transaction set"), NULL},
 { "noorder", '\0', POPT_BIT_CLR, &giFlags, RPMGI_TSORDER,
	N_("do not order transaction set"), NULL},
 { "noglob", '\0', POPT_BIT_SET, &giFlags, RPMGI_NOGLOB,
	N_("do not glob arguments"), NULL},
 { "nomanifest", '\0', POPT_BIT_SET, &giFlags, RPMGI_NOMANIFEST,
	N_("do not process non-package files as manifests"), NULL},
 { "noheader", '\0', POPT_BIT_SET, &giFlags, RPMGI_NOHEADER,
	N_("do not read headers"), NULL},

 { "qf", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },
 { "queryformat", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
        N_("File tree walk options for fts(3):"),
        NULL },
#endif

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options for all rpm modes and executables:"),
        NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));
    poptContext optCon;
    rpmts ts = NULL;
    rpmgi gi = NULL;
    rpmbf bf = NULL;
    size_t n = 0;
    static double e = 1.0e-4;
    size_t m = 0;
    size_t k = 0;
    rpmTag gitag = RPMDBI_PACKAGES;
    rpmTag hetag = RPMTAG_FILEPATHS;
    ARGV_t av;
    rpmRC rc;
    int ec = EXIT_FAILURE;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    if (rpmioFtsOpts == 0)
	rpmioFtsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    if (gitagstr != NULL)
	gitag = tagValue(gitagstr);

    av = poptGetArgs(optCon);

    ts = rpmtsCreate();
    (void) rpmtsSetFlags(ts, transFlags);
    (void) rpmtsSetDFlags(ts, depFlags);
    (void) rpmtsSetVSFlags(ts, vsFlags);

    gi = rpmgiNew(ts, gitag, gikeystr, 0);
    (void) rpmgiSetArgs(gi, av, rpmioFtsOpts, giFlags);

    while ((rc = rpmgiNext(gi)) == RPMRC_OK) {
	Header h = rpmgiHeader(gi);
	he->tag = hetag;
	if (headerGet(h, he, 0))
	    n += he->c;
    }

    rpmbfParams(n, e, &m, &k);
    bf = rpmbfNew(m, k, 0);
    
    while ((rc = rpmgiNext(gi)) == RPMRC_OK) {
	Header h = rpmgiHeader(gi);
	he->tag = hetag;
	if (headerGet(h, he, 0))
	for (he->ix = 0; he->ix < (int) he->c; he->ix++)
	    rpmbfAdd(bf, he->p.argv[he->ix], 0);
    }

#ifdef	NOTYET
    {	FD_t fd = Fopen("bloom.dump", "w");
	Fwrite(bf->bits, 1, m/8, fd);
	Fclose(fd);
    }
#endif

    printf("n: %u m: %u k: %u\nsize: %u\n", (unsigned)n, (unsigned)m, (unsigned)k, (unsigned)m/8);
    ec = 0;

exit:
    bf = rpmbfFree(bf);
    gi = rpmgiFree(gi);
    (void) rpmtsFree(ts); 
    ts = NULL;
    optCon = rpmcliFini(optCon);

    return ec;
}
