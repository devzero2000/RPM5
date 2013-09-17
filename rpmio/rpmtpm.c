/** \ingroup rpmio
 * \file rpmio/rpmtpm.c
 */

#include "system.h"

#if defined(WITH_TPM)

#define	TPM_POSIX		1	/* XXX FIXME: move to tpm-sw */
#define	TPM_V12			1
#define	TPM_NV_DISK		1
#define	TPM_MAXIMUM_KEY_SIZE	4096
#define	TPM_AES			1

#include <tpm.h>
#include <tpmutil.h>
#include <tpmfunc.h>

#endif	/* WITH_TPM */

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#define	_RPMTPM_INTERNAL
#include <rpmtpm.h>

#include "debug.h"

/*@unchecked@*/
int _rpmtpm_debug = 0;
#define TPMDBG(_l) if (_rpmtpm_debug) fprintf _l

struct rpmtpm_s __tpm = {
};
rpmtpm _tpm = &__tpm;

int rpmtpmErr(rpmtpm tpm, const char * msg, uint32_t mask, uint32_t rc)
{
    uint32_t err = rc & (mask ? mask : 0xffffffff);
    (void)tpm;
    (void)err;
#if defined(WITH_TPM)
    if (err || _rpmtpm_debug)
	fprintf (stderr, "*** TPM_%s rc %u: %s\n", msg, rc,
                (err ? TPM_GetErrMsg(rc) : "Success"));
#endif	/* WITH_TPM */
    return rc;
}

void rpmtpmDump(rpmtpm tpm, const char * msg, unsigned char * b, size_t nb)
{
    FILE * fp = stdout;
    size_t i;
    tpm = tpm;
    if (msg)
        fprintf(fp, "%s: ", msg);
    if (b)
    for (i = 0; i < nb; i++)
        fprintf(fp, "%02x", b[i]);
    fprintf(fp, "\n");
}

/*==============================================================*/

static int rpmtpmGetPhysicalCMDEnable(rpmtpm tpm)
{
    int xx = -1;

#if defined(WITH_TPM)
    STACK_TPM_BUFFER( subcap );
    STACK_TPM_BUFFER( resp );
    STACK_TPM_BUFFER( tb );
    TPM_PERMANENT_FLAGS permanentFlags;

    STORE32(subcap.buffer, 0, TPM_CAP_FLAG_PERMANENT);

    subcap.used = 4;
    xx = rpmtpmErr(tpm, "GetCapability", 0,
	TPM_GetCapability(TPM_CAP_FLAG, &subcap, &resp));
    if (xx)
	goto exit;

    TSS_SetTPMBuffer(&tb, resp.buffer, resp.used);

    xx = rpmtpmErr(tpm, "ReadPermanentFlags", 0,
	TPM_ReadPermanentFlags(&tb, 0, &permanentFlags, resp.used));
    if (xx)
	goto exit;

    tpm->enabled = permanentFlags.physicalPresenceCMDEnable;

exit:
#endif	/* WITH_TPM */

    return xx;
}

/*==============================================================*/

#if defined(WITH_TPM)
extern int _rpmio_popt_context_flags;	/* XXX POPT_CONTEXT_POSIXMEHARDER */

/**
 * Process object OPTIONS and ARGS.
 * @param tpm		tpm object
 */
static void rpmtpmInitPopt(rpmtpm tpm, int ac, char ** av, poptOption tbl)
	/*@modifies tpm @*/
{
    poptContext con;
    int rc;

    if (av == NULL || av[0] == NULL || av[1] == NULL)
	goto exit;

    con = poptGetContext(av[0], ac, (const char **)av, tbl,
			_rpmio_popt_context_flags);

    /* Process all options into _tpm, whine if unknown options. */
    while ((rc = poptGetNextOpt(con)) > 0) {
	const char * arg = poptGetOptArg(con);
	arg = _free(arg);
	switch (rc) {
	default:
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
			__FUNCTION__, rc);
	    break;
	}
    }
    /* XXX FIXME: arrange error return iff rc < -1. */
if (rc < -1) {
fprintf(stderr, "%s: poptGetNextOpt rc(%d): %s\n", __FUNCTION__, rc, poptStrerror(rc));
argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
}

    /* Move the POPT parsed values into the current rpmtpm object. */
#ifdef	NOTYET
    tpm->av = argvFree(tpm->av);
    rc = argvAppend(&tpm->av, poptGetArgs(con));

    con = poptFreeContext(con);
#else
    /* XXX move the just parsed options */

    memcpy(((char *) tpm)+sizeof(tpm->_item),
	   ((char *)_tpm)+sizeof(tpm->_item),
	   sizeof(*tpm)-sizeof(tpm->_item));
    memset(((char *)_tpm)+sizeof(tpm->_item),
	   0,
	   sizeof(*tpm)-sizeof(tpm->_item));

    tpm->av = argvFree(tpm->av);
    rc = argvAppend(&tpm->av, poptGetArgs(con));
    tpm->ac = argvCount(tpm->av);

    tpm->con = con;
#endif

exit:
TPMDBG((stderr, "<== %s(%p, %p[%u], %p)\n", __FUNCTION__, tpm, av, (unsigned)ac, tbl));
}
#endif /* defined(WITH_SQLITE) */

static void rpmtpmFini(void * _tpm)
	/*@globals fileSystem @*/
	/*@modifies *_tpm, fileSystem @*/
{
    rpmtpm tpm = (rpmtpm) _tpm;

    tpm->digest = _free(tpm->digest);
    tpm->digestlen = 0;

    tpm->av = argvFree(tpm->av);
    tpm->con = poptFreeContext(tpm->con);	/* XXX FIXME */

    tpm->ifn = _free(tpm->ifn);
    tpm->ofn = _free(tpm->ofn);

    tpm->ownerpass = _free(tpm->ownerpass);
    tpm->keypass = _free(tpm->keypass);
    tpm->parpass = _free(tpm->parpass);
    tpm->certpass = _free(tpm->certpass);
    tpm->newpass = _free(tpm->newpass);
    tpm->areapass = _free(tpm->areapass);
    tpm->sigpass = _free(tpm->sigpass);
    tpm->migpass = _free(tpm->migpass);

}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmtpmPool = NULL;

static rpmtpm rpmtpmGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmtpmPool, fileSystem @*/
	/*@modifies pool, _rpmtpmPool, fileSystem @*/
{
    rpmtpm tpm;

    if (_rpmtpmPool == NULL) {
	_rpmtpmPool = rpmioNewPool("tpm", sizeof(*tpm), -1, _rpmtpm_debug,
			NULL, NULL, rpmtpmFini);
	pool = _rpmtpmPool;
    }
    tpm = (rpmtpm) rpmioGetPool(pool, sizeof(*tpm));
    memset(((char *)tpm)+sizeof(tpm->_item), 0, sizeof(*tpm)-sizeof(tpm->_item));
    return tpm;
}

rpmtpm rpmtpmNew(int ac, char ** av, struct poptOption *tbl, uint32_t flags)
{
    rpmtpm tpm = rpmtpmGetPool(_rpmtpmPool);

    if (tbl)
	rpmtpmInitPopt(tpm, ac, av, tbl);

#if defined(WITH_TPM)
    TPM_setlog(rpmIsVerbose() ? 1 : 0);

    if (tpm->ownerpass) {
	TSS_sha1(tpm->ownerpass, strlen(tpm->ownerpass), tpm->pwdohash);
        tpm->pwdo = tpm->pwdohash;
    }
    if (tpm->keypass) {
	TSS_sha1(tpm->keypass, strlen(tpm->keypass), tpm->pwdkhash);
        tpm->pwdk = tpm->pwdkhash;
    }
    if (tpm->parpass) {
	TSS_sha1(tpm->parpass, strlen(tpm->parpass), tpm->pwdphash);
        tpm->pwdp = tpm->pwdphash;
    }
    if (tpm->certpass) {
	TSS_sha1(tpm->certpass, strlen(tpm->certpass), tpm->pwdchash);
        tpm->pwdc = tpm->pwdchash;
    }
    if (tpm->newpass) {
	TSS_sha1(tpm->newpass, strlen(tpm->newpass), tpm->pwdnhash);
        tpm->pwdn = tpm->pwdnhash;
    }
    if (tpm->areapass) {
	TSS_sha1(tpm->areapass, strlen(tpm->areapass), tpm->pwdahash);
        tpm->pwda = tpm->pwdahash;
    }
    if (tpm->sigpass) {
	TSS_sha1(tpm->sigpass, strlen(tpm->sigpass), tpm->pwdshash);
        tpm->pwds = tpm->pwdshash;
    }
    if (tpm->migpass) {
	TSS_sha1(tpm->migpass, strlen(tpm->migpass), tpm->pwdmhash);
        tpm->pwdm = tpm->pwdmhash;
    }
#endif

    return rpmtpmLink(tpm);
}
