#include "system.h"

#include <rpmcli.h>

#include <rpmlead.h>
#include <signature.h>
#include "header_internal.h"
#include "misc.h"
#include "debug.h"

const char *__progname;
static int _debug = 0;

static const char * yamlfmt = "[%{*:yaml}\n]";
static const char * xmlfmt = "[%{*:xml}\n]";

/*@observer@*/ /*@unchecked@*/
static const struct headerTagTableEntry_s rpmSTagTbl[] = {
	{ "RPMTAG_HEADERIMAGE", HEADER_IMAGE, RPM_BIN_TYPE },
	{ "RPMTAG_HEADERSIGNATURES", HEADER_SIGNATURES, RPM_BIN_TYPE },
	{ "RPMTAG_HEADERIMMUTABLE", HEADER_IMMUTABLE, RPM_BIN_TYPE },
	{ "RPMTAG_HEADERREGIONS", HEADER_REGIONS, RPM_BIN_TYPE },
	{ "RPMTAG_SIGSIZE", 999+1, RPM_INT32_TYPE },
	{ "RPMTAG_SIGLEMD5_1", 999+2, RPM_BIN_TYPE },
	{ "RPMTAG_SIGPGP", 999+3, RPM_BIN_TYPE },
	{ "RPMTAG_SIGLEMD5_2", 999+4, RPM_BIN_TYPE },
	{ "RPMTAG_SIGMD5", 999+5, RPM_BIN_TYPE },
	{ "RPMTAG_SIGGPG", 999+6, RPM_BIN_TYPE },
	{ "RPMTAG_SIGPGP5", 999+7, RPM_BIN_TYPE },
	{ "RPMTAG_SIGPAYLOADSIZE", 999+8, RPM_INT32_TYPE },
	{ "RPMTAG_BADSHA1_1", HEADER_SIGBASE+8, RPM_STRING_TYPE },
	{ "RPMTAG_BADSHA1_2", HEADER_SIGBASE+9, RPM_STRING_TYPE },
	{ "RPMTAG_PUBKEYS", HEADER_SIGBASE+10, RPM_STRING_ARRAY_TYPE },
	{ "RPMTAG_DSAHEADER", HEADER_SIGBASE+11, RPM_BIN_TYPE },
	{ "RPMTAG_RSAHEADER", HEADER_SIGBASE+12, RPM_BIN_TYPE },
	{ "RPMTAG_SHA1HEADER", HEADER_SIGBASE+13, RPM_STRING_TYPE },
	{ NULL, 0 }
};

/*@observer@*/ /*@unchecked@*/
const struct headerTagTableEntry_s * rpmSTagTable = rpmSTagTbl;
typedef enum rpmtoolIOBits_e {
    RPMIOBITS_NONE	= 0,
    RPMIOBITS_LEAD	= (1 <<  0),
    RPMIOBITS_SHEADER	= (1 <<  1),
    RPMIOBITS_HEADER	= (1 <<  2),
    RPMIOBITS_PAYLOAD	= (1 <<  3),
    RPMIOBITS_FDIO	= (1 <<  4),
    RPMIOBITS_UFDIO	= (1 <<  5),
    RPMIOBITS_GZDIO	= (1 <<  6),
    RPMIOBITS_BZDIO	= (1 <<  7),
    RPMIOBITS_UNCOMPRESS= (1 <<  8),
    RPMIOBITS_BINARY	= (1 <<  9),
    RPMIOBITS_DUMP	= (1 << 10),
    RPMIOBITS_XML	= (1 << 11)
} rpmtoolIOBits;

#define _RPMIOBITS_PKGMASK \
    (RPMIOBITS_LEAD|RPMIOBITS_SHEADER|RPMIOBITS_HEADER|RPMIOBITS_PAYLOAD)

static const char * iav[] = { "-", NULL };
static const char * imode = NULL;
static rpmtoolIOBits ibits = RPMIOBITS_NONE;

static const char * queryHeader(Header h, const char * qfmt,
		const struct headerTagTableEntry_s * tags,
		const struct headerSprintfExtension_s * exts)
{
    const char * errstr = "(unkown error)";
    const char * str;

    str = headerSprintf(h, qfmt, tags, exts, &errstr);
    if (str == NULL)
	fprintf(stderr, _("%s: headerSprintf: incorrect format: %s\n"),
			__progname, errstr);
    return str;
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&_debug, -1,
	NULL, NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    const char * ifn;
    struct rpmlead lead;
    Header sigh = NULL;
    Header h = NULL;
    FD_t fdi = NULL;
    const char * s;
    int ec = 0;
    int xx;

    setprogname(argv[0]);
    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }

    if (argc > 1)
	iav[0] = argv[1];
    imode = xstrdup("r.ufdio");
    ibits = _RPMIOBITS_PKGMASK;

    optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    while ((xx = poptGetNextOpt(optCon)) > 0)
	;

    ifn = iav[0];

	/* Open input file. */
	fdi = Fopen(ifn, imode);
	if (fdi == NULL || Ferror(fdi)) {
	    fprintf(stderr, _("%s: input  Fopen(%s, \"%s\"): %s\n"),
			__progname, ifn, imode, Fstrerror(fdi));
	    ec++;
	    goto bottom;
	}


	/* Read package components. */
	if (ibits & RPMIOBITS_LEAD) {
	    rc = readLead(fdi, &lead);
	    if (rc != RPMRC_OK) {
		fprintf(stderr, _("%s: readLead(%s) failed(%d)\n"),
			__progname, ifn, rc);
		ec++;
		goto bottom;
	    }
	}

	if (ibits & RPMIOBITS_SHEADER) {
	    const char * msg = NULL;
	    rc = rpmReadSignature(fdi, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
	    if (rc != RPMRC_OK) {
		fprintf(stderr, _("%s: rpmReadSignature(%s) failed(%d): %s\n"),
			__progname, ifn, rc, msg);
		msg = _free(msg);
		ec++;
		goto bottom;
	    }

	    s = queryHeader(sigh, yamlfmt, rpmSTagTable, rpmHeaderFormats);
	    if (s) {
		fprintf(stdout, "---\n%s\n", s);
		s = _free(s);
	    }
	}

	if (ibits & RPMIOBITS_HEADER) {
	    h = headerRead(fdi, HEADER_MAGIC_YES);
	    if (h == NULL) {
		fprintf(stderr, _("%s: headerRead(%s) failed\n"),
			__progname, ifn);
		ec++;
		goto bottom;
	    }

	    s = queryHeader(h, yamlfmt, rpmTagTable, rpmHeaderFormats);
	    if (s) {
		fprintf(stdout, "---\n%s\n", s);
		s = _free(s);
	    }
	}

bottom:
	sigh = headerFree(sigh);
	h = headerFree(h);
	if (fdi != NULL) {
	    xx = Fclose(fdi);
	    fdi = NULL;
	}

    optCon = rpmcliFini(optCon);

    imode = _free(imode);

    return ec;
}
