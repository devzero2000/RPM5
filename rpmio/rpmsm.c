/** \ingroup rpmio
 * \file rpmio/rpmsm.c
 */

#include "system.h"

#if defined(WITH_SELINUX)
#include <selinux/selinux.h>
#endif
#if defined(WITH_SEMANAGE)
#include <semanage/semanage.h>
#endif

#define	_RPMSM_INTERNAL
#include <rpmsm.h>
#include <rpmsx.h>
#include <rpmlog.h>
#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsm_debug = 0;

static void rpmsmFini(void * _sm)
	/*@globals fileSystem @*/
	/*@modifies *_sm, fileSystem @*/
{
    rpmsm sm = _sm;

#if defined(WITH_SEMANAGE)
    if (sm->I) {
	if (semanage_is_connected(sm->I))
	    semanage_disconnect(sm->I);
	semanage_handle_destroy(sm->I);
    }
#endif
    sm->fn = _free(sm->fn);
    sm->flags = 0;
    sm->I = NULL;
    sm->access = 0;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsmPool = NULL;

static rpmsm rpmsmGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmsmPool, fileSystem @*/
	/*@modifies pool, _rpmsmPool, fileSystem @*/
{
    rpmsm sm;

    if (_rpmsmPool == NULL) {
	_rpmsmPool = rpmioNewPool("sm", sizeof(*sm), -1, _rpmsm_debug,
			NULL, NULL, rpmsmFini);
	pool = _rpmsmPool;
    }
    return (rpmsm) rpmioGetPool(pool, sizeof(*sm));
}

rpmsm rpmsmNew(const char * fn, unsigned int flags)
{
    rpmsm sm = rpmsmGetPool(_rpmsmPool);

#if defined(WITH_SEMANAGE)
    semanage_handle_t *I = sm->I = semanage_handle_create();
    int xx;

    if (I == NULL) {
if (_rpmsm_debug)
fprintf(stderr, "--> %s(%s,0x%x): semanage_handle_create() failed\n", __FUNCTION__, fn, flags);
	return NULL;
    }

    if ((xx = semanage_is_managed(I)) <= 0) {
if (xx < 0 && _rpmsm_debug)
fprintf(stderr, "--> %s(%s,0x%x): semanage_is_managed(%p): %s\n", __FUNCTION__, fn, flags, I, strerror(errno));
	(void)rpmsmFree(sm);
	return NULL;
    }

    if ((xx = semanage_access_check(I)) < 0) {
if (_rpmsm_debug)
fprintf(stderr, "--> %s(%s,0x%x): semanage_access_check(%p): %s\n", __FUNCTION__, fn, flags, I, strerror(errno));
	(void)rpmsmFree(sm);
	return NULL;
    }
    sm->access = xx;
    if ((xx = semanage_mls_enabled(I)) > 0)
	sm->access |= 0x4;

#ifdef	NOTYET
    sm->fn = NULL;
    sm->flags = flags;
    if ((xx = semanage_select_store(I, fn, SEMANAGE_CON_DIRECT)) < 0) {
if (_rpmsm_debug)
fprintf(stderr, "--> %s(%s,0x%x): semanage_select_store(%p): %s\n", __FUNCTION__, fn, flags, I, strerror(errno));
	(void)rpmsmFree(sm);
	return NULL;
    }
#endif

    if ((xx = semanage_connect(I)) < 0) {
if (_rpmsm_debug)
fprintf(stderr, "--> %s(%s,0x%x): semanage_connect(%p): %s\n", __FUNCTION__, fn, flags, I, strerror(errno));
	(void)rpmsmFree(sm);
	return NULL;
    }
#endif

    return rpmsmLink(sm);
}

/*@unchecked@*/ /*@null@*/
static const char * _rpmsmI_fn;
/*@unchecked@*/
static int _rpmsmI_flags;

static rpmsm rpmsmI(void)
	/*@globals _rpmsmI @*/
	/*@modifies _rpmsmI @*/
{
    if (_rpmsmI == NULL)
	_rpmsmI = rpmsmNew(_rpmsmI_fn, _rpmsmI_flags);
    return _rpmsmI;
}

#ifdef	REFERENCE	/* <semanage/handle.h> */
/* Just reload the policy */
int semanage_reload_policy(semanage_handle_t * handle);

/* Set whether to reload the policy or not after a commit,
 * 1 for yes (default), 0 for no */
void semanage_set_reload(semanage_handle_t * handle, int do_reload);

/* Set whether to rebuild the policy on commit, even if no
 * changes were performed.
 * 1 for yes, 0 for no (default) */
void semanage_set_rebuild(semanage_handle_t * handle, int do_rebuild);

/* Create the store if it does not exist, this only has an effect on 
 * direct connections and must be called before semanage_connect 
 * 1 for yes, 0 for no (default) */
void semanage_set_create_store(semanage_handle_t * handle, int create_store);

/* Get whether or not dontaudits will be disabled upon commit */
int semanage_get_disable_dontaudit(semanage_handle_t * handle);

/* Set whether or not to disable dontaudits upon commit */
void semanage_set_disable_dontaudit(semanage_handle_t * handle, int disable_dontaudit);

/* Attempt to obtain a transaction lock on the manager.  If another
 * process has the lock then this function may block, depending upon
 * the timeout value in the handle.
 *
 * Note that if the semanage_handle has not yet obtained a transaction
 * lock whenever a writer function is called, there will be an
 * implicit call to this function. */
int semanage_begin_transaction(semanage_handle_t *);
#endif
