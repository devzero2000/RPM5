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

#include "debug.h"

/*@unchecked@*/
int _rpmsm_debug = -1;

/*@unchecked@*/ /*@relnull@*/
rpmsm _rpmsmI = NULL;

#define F_ISSET(_sm, _FLAG) (((_sm)->flags & ((RPMSM_FLAGS_##_FLAG) & ~0x40000000)) != RPMSM_FLAGS_NONE)

/*==============================================================*/
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
if (_rpmsm_debug)
fprintf(stderr, "<-- %s: semanage_is_managed(%p): rc %d\n", __FUNCTION__, I, xx);
	    rc = -1;
	} else if ((xx = semanage_access_check(I)) < SEMANAGE_CAN_READ) {
if (_rpmsm_debug)
fprintf(stderr, "<-- %s: semanage_access_check(%p): rc %d\n", __FUNCTION__, I, xx);
	    rc = -1;
	}
    }
#endif
    return rc;
}

static int rpmsmConnect(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    if (!semanage_is_connected(I)) {
	rc = semanage_connect(I);
if (_rpmsm_debug && rc < 0)
fprintf(stderr, "<-- %s: semanage_connect(%p): %s\n", __FUNCTION__, I, strerror(errno));
    }
#endif
    return rc;
}

static int rpmsmDisconnect(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    if (semanage_is_connected(I)) {
	rc = semanage_disconnect(I);
if (_rpmsm_debug && rc < 0)
fprintf(stderr, "<-- %s: semanage_disconnect(%p): %s\n", __FUNCTION__, I, strerror(errno));
    }
#endif
    return rc;
}

static int rpmsmReload(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = semanage_reload_policy(I);
if (_rpmsm_debug && rc < 0)
fprintf(stderr, "<-- %s: semanage_reload_policy(%p): %s\n", __FUNCTION__, I, strerror(errno));
#endif
    return rc;
}

static int rpmsmBegin(rpmsm sm)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

    rc = semanage_begin_transaction(I);
if (_rpmsm_debug && rc < 0)
fprintf(stderr, "<-- %s: semanage_begin_transaction(%p): %s\n", __FUNCTION__, I, strerror(errno));
#endif
    return rc;
}

static int rpmsmInstall(rpmsm sm, char * arg, const char ** resultp)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

if (_rpmsm_debug)
fprintf(stderr, "--> %s(%p,\"%s\",%p)\n", __FUNCTION__, sm, arg, resultp);
    rc = semanage_module_install_file(I, arg);
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
else if (_rpmsm_debug)
fprintf(stderr, "<-- %s: semanage_module_install_file(%p,%s): %s\n", __FUNCTION__, I, arg, strerror(errno));
#endif
    return rc;
}

static int rpmsmUpgrade(rpmsm sm, char * arg, const char ** resultp)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

if (_rpmsm_debug)
fprintf(stderr, "--> %s(%p,\"%s\",%p)\n", __FUNCTION__, sm, arg, resultp);
    rc = semanage_module_upgrade_file(I, arg);
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
else if (_rpmsm_debug)
fprintf(stderr, "<-- %s: semanage_module_upgrade_file(%p,%s): %s\n", __FUNCTION__, I, arg, strerror(errno));
#endif
    return rc;
}

static int rpmsmInstallBase(rpmsm sm, char * arg, const char ** resultp)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

if (_rpmsm_debug)
fprintf(stderr, "--> %s(%p,\"%s\",%p)\n", __FUNCTION__, sm, arg, resultp);
    rc = semanage_module_install_base_file(I, arg);
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
else if (_rpmsm_debug)
fprintf(stderr, "<-- %s: semanage_module_install_base_file(%p,%s): %s\n", __FUNCTION__, I, arg, strerror(errno));
#endif
    return rc;
}

static int rpmsmRemove(rpmsm sm, char * arg, const char ** resultp)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;

if (_rpmsm_debug)
fprintf(stderr, "--> %s(%p,\"%s\",%p)\n", __FUNCTION__, sm, arg, resultp);
    rc = semanage_module_remove(I, arg);
    if (rc >= 0)
	sm->flags |= RPMSM_FLAGS_COMMIT;
else if (_rpmsm_debug)
fprintf(stderr, "<-- %s: semanage_module_remove(%p,%s): %s\n", __FUNCTION__, I, arg, strerror(errno));
#endif
    return rc;
}

static int rpmsmList(rpmsm sm, char * arg, const char ** resultp)
{
    int rc = 0;
#if defined(WITH_SEMANAGE)
    semanage_handle_t * I = sm->I;
    semanage_module_info_t *modules = NULL;
    int nmodules = 0;

if (_rpmsm_debug)
fprintf(stderr, "--> %s(%p,\"%s\",%p)\n", __FUNCTION__, sm, arg, resultp);
    if (resultp)
	*resultp = NULL;
    rc = semanage_module_list(I, &modules, &nmodules);
    if (rc < 0) {
if (_rpmsm_debug)
fprintf(stderr, "<-- %s: semanage_module_list(%p): %s\n", __FUNCTION__, I, strerror(errno));
    } else {
	rpmiob iob = rpmiobNew(0);
	int j;
	for (j = 0; j < nmodules; j++) {
	    semanage_module_info_t * m = semanage_module_list_nth(modules, j);
	    const char * N = semanage_module_get_name(m);
	    const char * V = semanage_module_get_version(m);
	    rpmiobAppend(iob, N, 0);
	    rpmiobAppend(iob, "-", 0);
	    rpmiobAppend(iob, V, 1);
	    semanage_module_info_datum_destroy(m);
	}
	modules = _free(modules);
	if (resultp)
	    *resultp = xstrdup(rpmiobStr(iob));
	iob = rpmiobFree(iob);
    }
#endif
    return rc;
}

static int rpmsmCommit(rpmsm sm)
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

	rc = semanage_commit(I);
	sm->flags &= ~RPMSM_FLAGS_COMMIT;
    }

if (_rpmsm_debug && rc < 0)
fprintf(stderr, "<-- %s: semanage_commit(%p): %s\n", __FUNCTION__, I, strerror(errno));
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

    if (rpmsmSelect(sm)) {
	(void)rpmsmFree(sm);
	return NULL;
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

    /* Select "targeted", "minimum",  or other store. */
    rc = rpmsmSelect(sm);

    /* Create store if bootstrapping base policy. */
    rc = rpmsmCreate(sm);
    if (rc < 0) {
	fprintf(stderr, "%s: policy is not managed or store cannot be accessed.\n",
			__progname);
	    goto exit;
    }

    if ((rc = rpmsmConnect(sm)) < 0) {
	fprintf(stderr, "%s:  Could not connect to policy handler\n", __progname);
	goto exit;
    }

    if (F_ISSET(sm, RELOAD)) {
	if ((rc = rpmsmReload(sm)) < 0) {
	    fprintf(stderr, "%s:  Could not reload policy\n", __progname);
	    goto exit;
	}
    }

    if (F_ISSET(sm, BUILD)) {
	if ((rc = rpmsmBegin(sm)) < 0) {
	    fprintf(stderr, "%s:  Could not begin transaction:  %s\n",
			__progname, errno ? strerror(errno) : "");
	    goto exit;
	}
    }

    for (i = 0; i < ncmds; i++) {
	char * cmd = (char *)av[i];
	char * arg = strchr(cmd+1, ' ');

	if (arg == NULL) arg = "";
	while (xisspace(*arg)) arg++;

	switch (*cmd) {
	case 'i':
	    rc = rpmsmInstall(sm, arg, resultp);
	    break;
	case 'u':
	    rc = rpmsmUpgrade(sm, arg, resultp);
	    break;
	case 'b':
	    rc = rpmsmInstallBase(sm, arg, resultp);
	    break;
	case 'r':
	    rc = rpmsmRemove(sm, arg, resultp);
	    if (rc == -2)
		continue;
	    break;
	case 'l':
	    rc = rpmsmList(sm, arg, resultp);
	    break;
	default:
	    fprintf(stderr, "%s:  Unknown cmd specified: \"%s\"\n", __progname, cmd);
	    goto exit;
	    /*@notreached@*/ break;
	}
    }

    if (F_ISSET(sm, COMMIT)) {
	rc = rpmsmCommit(sm);
	if (rc < 0) {
	    fprintf(stderr, "%s:  Commit failed!\n", __progname);
	    goto exit;
	}
    }

exit:
    return rc;
}
