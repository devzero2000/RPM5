#include "system.h"

#undef	_	/* XXX everyone gotta be different */

#define _RPMPERL_INTERNAL
#include "rpmperl.h"

#include "debug.h"

#define	my_perl	((PerlInterpreter *)perl->I)

/*@unchecked@*/
int _rpmperl_debug = 0;

static void rpmperlFini(void * _perl)
        /*@globals fileSystem @*/
        /*@modifies *_perl, fileSystem @*/
{
    rpmperl perl = _perl;

    perl->fn = _free(perl->fn);
    perl->flags = 0;
#if defined(WITH_PERLEMBED)
    PERL_SET_CONTEXT(my_perl);
    PL_perl_destruct_level = 1;
    perl_destruct(my_perl);
    perl_free(my_perl);
#endif
    perl->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmperlPool;

static rpmperl rpmperlGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmperlPool, fileSystem @*/
        /*@modifies pool, _rpmperlPool, fileSystem @*/
{
    rpmperl perl;

    if (_rpmperlPool == NULL) {
        _rpmperlPool = rpmioNewPool("perl", sizeof(*perl), -1, _rpmperl_debug,
                        NULL, NULL, rpmperlFini);
        pool = _rpmperlPool;
    }
    return (rpmperl) rpmioGetPool(pool, sizeof(*perl));
}

#if defined(NOTYET)
EXTERN_C void xs_init (pTHX);

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

EXTERN_C void
xs_init(pTHX)
{
	char *file = __FILE__;
	dXSUB_SYS;

	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

/*@unchecked@*/
static const char * rpmperlInitStringIO = "\
use RPM::Constant;\n\
use RPM::Header;\n\
use RPM::Transaction;\n\
use RPM::PackageIterator;\n\
use RPM::Problems;\n\
use RPM::Files;\n\
use RPM::Dependencies;\n\
use RPM::Spec;\n\
use IO::String;\n\
$io = IO::String->new;\n\
select $io;\n\
";
#else
#define	xs_init	NULL
#if defined(WITH_PERLEMBED)
/*@unchecked@*/
static const char * rpmperlInitStringIO = "\
use IO::String;\n\
$io = IO::String->new;\n\
select $io;\n\
";

#endif
#endif

rpmperl rpmperlNew(const char * fn, int flags)
{
#if defined(WITH_PERLEMBED)
    static char *embedding[] = { "", "-e", "0" };
    int xx;
#endif
    rpmperl perl = rpmperlGetPool(_rpmperlPool);

    if (fn)
	perl->fn = xstrdup(fn);
    perl->flags = flags;

#if defined(WITH_PERLEMBED)
    perl->I = perl_alloc();
    PERL_SET_CONTEXT(my_perl);
    PL_perl_destruct_level = 1;
    perl_construct(my_perl);

    PL_origalen = 1; /* don't let $0 assignment update proctitle/embedding[0] */
    xx = perl_parse(my_perl, xs_init, 3, embedding, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    perl_run(my_perl);
    (void) rpmperlRun(perl, rpmperlInitStringIO, NULL);
#endif

    return rpmperlLink(perl);
}

rpmRC rpmperlRun(rpmperl perl, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmperl_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, perl, str);

    if (str != NULL) {
#if defined(WITH_PERLEMBED)
	STRLEN n_a;
	SV * retSV;
	PERL_SET_CONTEXT(my_perl);
	retSV = Perl_eval_pv(my_perl, str, TRUE);
	if (resultp) {
	    retSV = Perl_eval_pv(my_perl, "${$io->string_ref}", TRUE);
	    *resultp = SvPV(retSV, n_a);
	}
#endif
	rc = RPMRC_OK;
    }
    return rc;
}
