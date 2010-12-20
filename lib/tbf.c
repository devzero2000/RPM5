#include "system.h"

#include <rpmio_internal.h>	/* XXX fdGetOPath */
#define	_RPMBF_INTERNAL
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

static unsigned rpmbfWrite(FD_t fd, rpmbf * bfa, size_t npkgs)
{
    const char * fn = xstrdup(fdGetOPath(fd));
    struct stat sb;
    int ix;
    int xx;

    for (ix = 0; ix < (int) npkgs; ix++) {
	rpmbf bf = bfa[ix];
	static const char _bfmagic[] = "BFL3";
	const unsigned char * _bits = (bf ? bf->bits : NULL);
	uint32_t _n = (bf ? bf->n : 0);
	uint32_t _m = (bf ? bf->m : 0);
	uint32_t _k = (bf ? bf->k : 0);

	if (rpmIsVerbose())
	    fprintf(stderr, "\tbf[%d] %p n %u m %u k %u\n", ix, bf, _n, _m, _k);
	xx = Fwrite(_bfmagic, 1, 2, fd);
	xx = Fwrite(&_n, 1, sizeof(_n), fd);
	xx = Fwrite(&_m, 1, sizeof(_m), fd);
	xx = Fwrite(&_k, 1, sizeof(_m), fd);
	if (_m > 0)
	    xx = Fwrite(_bits, 1, (_m+7)/8, fd);
    }
    xx = Fclose(fd);
    xx = Stat(fn, &sb);
    xx = Unlink(fn);

    return sb.st_size;
}

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
    size_t n = 4096;
    static double e = 1.0e-4;
    size_t m = 0;
    size_t k = 0;
    rpmTag gitag = RPMDBI_PACKAGES;
    rpmTag hetag = RPMTAG_DIRNAMES;
    ARGV_t av;
    rpmRC rc;
    int ec = EXIT_FAILURE;
FD_t fd;
struct stat sb;
static const char fn_dirs[] = "/tmp/dirs.xz";
size_t ndirs = 0;
size_t ndirb = 0;
size_t ndirxz = 0;
static const char fn_bf[] = "/tmp/bf.xz";
static const char fmode_w[] = "w9.xzdio";
rpmbf * bfa = NULL;
size_t npkgs = 0;
size_t nb = 0;
int ix = 0;
int jx;
int xx;

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

    npkgs = 0;
    while ((rc = rpmgiNext(gi)) == RPMRC_OK)
	npkgs++;
    bfa = xcalloc(npkgs, sizeof(*bfa));
    rpmbfParams(n, e, &m, &k);

    fd = Fopen(fn_dirs, fmode_w);
    nb = 0;
    ix = 0;
    while ((rc = rpmgiNext(gi)) == RPMRC_OK) {
	static const char newline[] = "\n";
	rpmbf bf;
	Header h;

	bf = NULL;
	h = rpmgiHeader(gi);
	he->tag = hetag;
	if (headerGet(h, he, 0) && he->c > 0) {
	    /* XXX make sure n >= _nmin in rpmbfParams() */
	    n = (he->c > 10 ? he->c : 10);
	    rpmbfParams(n, e, &m, &k);
	    nb += (m+7)/8;
	    bf = rpmbfNew(m, k, 0);
	    for (jx = 0; jx < (int) he->c; jx++) {
		const char * s = he->p.argv[jx];
		size_t ns = strlen(s);
		xx = rpmbfAdd(bf, s, ns);
		xx = Fwrite(s, 1, ns, fd);
		xx = Fwrite(newline, 1, 1, fd);
		ndirs++;
		ndirb += ns + 1;
	    }
	}
	xx = Fwrite(newline, 1, 1, fd);
	ndirb++;
	bfa[ix++] = bf;
    }
    xx = Fclose(fd);
    xx = Stat(fn_dirs, &sb);
    xx = Unlink(fn_dirs);
    ndirxz = sb.st_size;

fprintf(stdout, "       npkgs: %u\n", (unsigned) npkgs);
fprintf(stdout, "    Dirnames: %u bytes (%u items)\n", (unsigned) ndirb, ndirs);
fprintf(stdout, "  with XZDIO: %u bytes\n", (unsigned) ndirxz);

fprintf(stdout, "Bloom filter: false positives: %5.2g\n", e);
fprintf(stdout, "Uncompressed: %u bytes\n", (unsigned)nb);
fprintf(stdout, "  with XZDIO: %u bytes\n", rpmbfWrite(Fopen(fn_bf, fmode_w), bfa, npkgs));
 
    ec = EXIT_SUCCESS;

exit:
    for (ix = 0; ix < (int) npkgs; ix++)
	bfa[ix] = rpmbfFree(bfa[ix]);
    bfa = _free(bfa);
    gi = rpmgiFree(gi);
    (void) rpmtsFree(ts); 
    ts = NULL;
    optCon = rpmcliFini(optCon);

    return ec;
}
