/** \ingroup rpmio
 * \file rpmio/rpmsm.c
 */

#include "system.h"

#if defined(WITH_SEMANAGE)
#include <semanage/semanage.h>
#endif

#define	_RPMSM_INTERNAL
#include <rpmsm.h>
#include <rpmlog.h>
#include <rpmmacro.h>
#include <argv.h>
#include <mire.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsm_debug = -1;

/*@unchecked@*/ /*@relnull@*/
rpmsm _rpmsmI = NULL;

#define F_ISSET(_sm, _FLAG) (((_sm)->flags & ((RPMSM_FLAGS_##_FLAG) & ~0x40000000)) != RPMSM_FLAGS_NONE)

/*==============================================================*/
static int rpmsmChk(rpmsm sm, int rc, const char * msg)
{
    if (rc < 0 && msg != NULL) {
	rpmiobAppend(sm->iob, "semanage_", 0);
	rpmiobAppend(sm->iob, msg, 0);
	if (errno)
	    rpmiobAppend(sm->iob, strerror(errno), 0);
    }
    return rc;
}

static int rpmsmSelect(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    /* Select "targeted" or other store. */
    if (sm->fn)
	semanage_select_store(I, (char *)sm->fn, SEMANAGE_CON_DIRECT);
#endif
    return rc;
}

static int rpmsmCreate(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;
    int xx;

    /* if installing base module create store if necessary, for bootstrapping */
    semanage_set_create_store(I, (F_ISSET(sm, CREATE) ? 1 : 0));

    if (!F_ISSET(sm, CREATE)) {
	if (!(xx = semanage_is_managed(I))) {
	    /* XXX FIXME error msg */
	    rc = -1;
	} else if ((xx = semanage_access_check(I)) < SEMANAGE_CAN_READ) {
	    /* XXX FIXME error msg */
	    rc = -1;
	}
    }
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p) I %p rc %d\n", __FUNCTION__, sm, I, rc);
#endif
    return rc;
}

static int rpmsmConnect(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    if (!semanage_is_connected(I))
	rc = rpmsmChk(sm, semanage_connect(I), "connect");
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p) I %p rc %d\n", __FUNCTION__, sm, I, rc);
#endif
    return rc;
}

static int rpmsmDisconnect(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    if (semanage_is_connected(I))
	rc = rpmsmChk(sm, semanage_disconnect(I), "disconnect");
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p) I %p rc %d\n", __FUNCTION__, sm, I, rc);
#endif
    return rc;
}

static int rpmsmReload(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = rpmsmChk(sm, semanage_reload_policy(I), "reload_policy");
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p) I %p rc %d\n", __FUNCTION__, sm, I, rc);
#endif
    return rc;
}

static int rpmsmBegin(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = rpmsmChk(sm, semanage_begin_transaction(I), "begin_transaction");
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p) I %p rc %d\n", __FUNCTION__, sm, I, rc);
#endif
    return rc;
}

static int rpmsmInstall(rpmsm sm, char * arg)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = rpmsmChk(sm, semanage_module_install_file(I, arg), "module_install_file");
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p,%s) I %p rc %d\n", __FUNCTION__, sm, arg, I, rc);
#endif
    return rc;
}

static int rpmsmUpgrade(rpmsm sm, char * arg)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = rpmsmChk(sm, semanage_module_upgrade_file(I, arg), "module_upgrade_file");
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p,%s) I %p rc %d\n", __FUNCTION__, sm, arg, I, rc);
#endif
    return rc;
}

static int rpmsmInstallBase(rpmsm sm, char * arg)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = rpmsmChk(sm, semanage_module_install_base_file(I, arg), "module_install_base_file");
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p,%s) I %p rc %d\n", __FUNCTION__, sm, arg, I, rc);
#endif
    return rc;
}

static int rpmsmRemove(rpmsm sm, char * arg)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = rpmsmChk(sm, semanage_module_remove(I, arg), "module_remove");
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p,%s) I %p rc %d\n", __FUNCTION__, sm, arg, I, rc);
#endif
    return rc;
}

static int rpmsmList(rpmsm sm, char * arg)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;
    semanage_module_info_t *modules = NULL;
    int nmodules = 0;

    rc = rpmsmChk(sm, semanage_module_list(I, &modules, &nmodules), "module_list");
    if (rc >= 0) {
	const char * NV = NULL;
	miRE include = NULL;
	int ninclude = 0;
	int j;

	if (arg && *arg) {
	    include = mireNew(RPMMIRE_REGEX, 0);
	    (void) mireRegcomp(include, arg);
	    ninclude++;
	}

	for (j = 0; j < nmodules; j++) {
	    semanage_module_info_t * m = semanage_module_list_nth(modules, j);
	    NV = rpmExpand(semanage_module_get_name(m), "-",
			   semanage_module_get_version(m), NULL);
	    if (include == NULL
	     || !(mireApply(include, ninclude, NV, 0, +1) < 0))
		rpmiobAppend(sm->iob, NV, 1);
	    NV = _free(NV);
	    semanage_module_info_datum_destroy(m);
	}

	include = mireFree(include);
	modules = _free(modules);
    }
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p,%s) I %p rc %d\n", __FUNCTION__, sm, arg, I, rc);
#endif
    return rc;
}

static int rpmsmCommit(rpmsm sm)	/* XXX add arg */
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    if (F_ISSET(sm, COMMIT)) {
	if (!F_ISSET(sm, RELOAD))
	    semanage_set_reload(I, 0);
	if (F_ISSET(sm, BUILD))
	    semanage_set_rebuild(I, 1);
	if (F_ISSET(sm, NOAUDIT))
	    semanage_set_disable_dontaudit(I, 1);
	else if (F_ISSET(sm, BUILD))
	    semanage_set_disable_dontaudit(I, 0);

	rc = rpmsmChk(sm, semanage_commit(I), "commit");
	sm->flags &= ~RPMSM_FLAGS_COMMIT;
    }
if (_rpmsm_debug)
fprintf(stderr, "<-- %s(%p) I %p rc %d\n", __FUNCTION__, sm, I, rc);
#endif
    return rc;
}

/*==============================================================*/

static void rpmsmFini(void * _sm)
	/*@globals fileSystem @*/
	/*@modifies *_sm, fileSystem @*/
{
    rpmsm sm = _sm;

#if defined(WITH_SEMANAGE)
    if (sm->I) {
	rpmsmDisconnect(sm);
	semanage_handle_destroy(sm->I);
    }
#endif
    (void) rpmiobFree(sm->iob);
    sm->iob = NULL;
    sm->fn = _free(sm->fn);
    sm->flags = 0;
    sm->access = 0;
    sm->av = argvFree(sm->av);
    sm->I = NULL;
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

    if (I == NULL) {
if (_rpmsm_debug)
fprintf(stderr, "--> %s: semanage_handle_create() failed\n", __FUNCTION__);
	(void)rpmsmFree(sm);
	return NULL;
    }

    sm->fn = (fn ? xstrdup(fn) : NULL);
    sm->flags = flags;
    sm->iob = rpmiobNew(0);

    /* Select "targeted", "minimum", or other store. */
    if (rpmsmSelect(sm)) {
	(void)rpmsmFree(sm);
	return NULL;
    }

    /* Connect to the policy store immediately (if requested). */
    if (F_ISSET(sm, CONNECT)) {
	sm->flags |= RPMSM_FLAGS_CREATE;
	if (rpmsmCreate(sm) < 0 || rpmsmConnect(sm) < 0 || rpmsmBegin(sm) < 0) {
	    (void)rpmsmFree(sm);
	    return NULL;
	}
    }
#endif

    return rpmsmLink(sm);
}

/*@unchecked@*/ /*@null@*/
static char * _rpmsmI_fn = "minimum";
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

/*==============================================================*/

int rpmsmRun(rpmsm sm, const char ** av, const char ** resultp)
{
    int ncmds = argvCount(av);
    int rc = 0;
    int i;

    if (sm == NULL) sm = rpmsmI();

    if (!F_ISSET(sm, CONNECT)) {

	/* Select "targeted", "minimum", or other store. */
	rc = rpmsmSelect(sm);

	/* Create store if bootstrapping base policy. */
	if ((rc = rpmsmCreate(sm)) < 0) {
	    rpmiobAppend(sm->iob, "Policy store access failed", 0);
	    goto exit;
	}

	if ((rc = rpmsmConnect(sm)) < 0)
	    goto exit;

	if (F_ISSET(sm, RELOAD) && (rc = rpmsmReload(sm)) < 0)
	    goto exit;

	if (F_ISSET(sm, BUILD) && (rc = rpmsmBegin(sm)) < 0)
	    goto exit;
    }

    for (i = 0; i < ncmds; i++) {
	char * cmd = (char *)av[i];
	char * arg = strchr(cmd+1, ' ');

	if (arg == NULL) arg = "";
	while (xisspace(*arg)) arg++;

	switch (*cmd) {
	case 'i':
	    rc = rpmsmInstall(sm, arg);
	    break;
	case 'u':
	    rc = rpmsmUpgrade(sm, arg);
	    break;
	case 'b':
	    rc = rpmsmInstallBase(sm, arg);
	    break;
	case 'r':
	    rc = rpmsmRemove(sm, arg);
	    if (rc == -2) {	/* XXX WTF? */
		rc = 0;
		continue;
	    }
	    break;
	case 'l':
	    rc = rpmsmList(sm, arg);
	    break;
	default:
	    rpmiobAppend(sm->iob, "Unknown cmd: \"", 0);
	    rpmiobAppend(sm->iob, cmd, 0);
	    rpmiobAppend(sm->iob, "\"", 0);
	    goto exit;
	    /*@notreached@*/ break;
	}
    }

    if (F_ISSET(sm, COMMIT)) {
	rc = rpmsmCommit(sm);
	if (rc < 0) {
	    rpmiobAppend(sm->iob, "Commit failed", 0);
	    goto exit;
	}
    }

exit:
    /* Return any spewage to caller. */
    if (resultp)
	*resultp = (rpmiobLen(sm->iob) > 0
		? xstrdup(rpmiobStr(rpmiobRTrim(sm->iob))) : NULL);
    (void) rpmiobEmpty(sm->iob);
    
    return rc;
}
