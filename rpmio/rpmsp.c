/** \ingroup rpmio
 * \file rpmio/rpmsp.c
 */

#include "system.h"

#if defined(WITH_SEPOL)
#include <sepol/sepol.h>
#endif

#define	_RPMSP_INTERNAL
#include <rpmsp.h>
#include <rpmlog.h>
#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsp_debug = -1;

static void rpmspFini(void * _sp)
	/*@globals fileSystem @*/
	/*@modifies *_sp, fileSystem @*/
{
    rpmsp sp = _sp;

#if defined(WITH_SEPOL)
    if (sp->F)
	sepol_policy_file_free(sp->F);
    if (sp->DB)
	sepol_policydb_free(sp->DB);
    if (sp->I)
        sepol_handle_destroy(sp->I);
#endif
    sp->fn = _free(sp->fn);
    sp->flags = 0;
    sp->I = NULL;
    sp->DB = NULL;
    sp->F = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmspPool = NULL;

static rpmsp rpmspGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmspPool, fileSystem @*/
	/*@modifies pool, _rpmspPool, fileSystem @*/
{
    rpmsp sp;

    if (_rpmspPool == NULL) {
	_rpmspPool = rpmioNewPool("sp", sizeof(*sp), -1, _rpmsp_debug,
			NULL, NULL, rpmspFini);
	pool = _rpmspPool;
    }
    return (rpmsp) rpmioGetPool(pool, sizeof(*sp));
}

rpmsp rpmspNew(const char * fn, unsigned int flags)
{
    rpmsp sp = rpmspGetPool(_rpmspPool);

#if defined(WITH_SEPOL)
    sepol_handle_t *I = sp->I = sepol_handle_create();
    int xx;

    if (I == NULL) {
if (_rpmsp_debug)
fprintf(stderr, "--> %s(%s,0x%x): sepol_handle_create() failed\n", __FUNCTION__, fn, flags);
	(void)rpmspFree(sp);
	return NULL;
    }

    sp->DB = NULL;
    sp->F = NULL;

    if (fn != NULL) {
	FILE * fp = fopen(fn, "r");

	if (fp == NULL || ferror(fp)) {
if (_rpmsp_debug)
fprintf(stderr, "--> %s: fopen(%s)\n", __FUNCTION__, fn);
	    (void)rpmsmFree(sm);
	    return NULL;
	}

	if ((xx = sepol_policydb_create(&sp->DB)) < 0) {
if (_rpmsp_debug)
fprintf(stderr, "--> %s: sepol_policydb_create(): %s\n", __FUNCTION__, strerror(errno));	/* XXX errno? */
	    (void)rpmsmFree(sm);
	    return NULL;
	}

	if ((xx = sepol_policy_file_create(&sp->F)) < 0) {
if (_rpmsp_debug)
fprintf(stderr, "--> %s: sepol_policy_file_create(): %s\n", __FUNCTION__, strerror(errno));	/* XXX errno? */
	    (void)rpmsmFree(sm);
	    return NULL;
	}

	sepol_policy_file_set_handle(sp->F, sp->I);
	sepol_policy_file_set_fp(sp->F, fp);

	if ((xx = sepol_policydb_read(sp->DB, sp->F)) < 0) {
if (_rpmsp_debug)
fprintf(stderr, "--> %s: sepol_policydb_read(%p,%p): %s\n", __FUNCTION__, sp->DB, sp->F, strerror(errno));	/* XXX errno? */
	}

	(void) fclose(fp);

    }
#endif

    return rpmspLink(sp);
}

/*@unchecked@*/ /*@null@*/
static const char * _rpmspI_fn;
/*@unchecked@*/
static int _rpmspI_flags;

static rpmsp rpmspI(void)
	/*@globals _rpmspI @*/
	/*@modifies _rpmspI @*/
{
    if (_rpmspI == NULL)
	_rpmspI = rpmspNew(_rpmspI_fn, _rpmspI_flags);
    return _rpmspI;
}
