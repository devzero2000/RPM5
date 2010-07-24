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
int _rpmtpm_debug = -1;

static
int rpmtpmErr(rpmtpm tpm, const char * msg, uint32_t rc)
        /*@*/
{
    /* XXX Don't spew on expected failures ... */
    if (rc || _rpmtpm_debug)
        fprintf (stderr, "*** TPM_%s rc %u: %s\n", msg, rc,
		(rc ? TPM_GetErrMsg(rc) : "Success"));
    return rc;
}


/*@-mustmod@*/	/* XXX splint on crack */
static void rpmtpmFini(void * _tpm)
	/*@globals fileSystem @*/
	/*@modifies *_tpm, fileSystem @*/
{
    rpmtpm tpm = _tpm;

    tpm->digest = _free(tpm->digest);
    tpm->digestlen = 0;

}
/*@=mustmod@*/

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

static int rpmtpmGetPhysicalCMDEnable(rpmtpm tpm)
{
    int xx;
    STACK_TPM_BUFFER( subcap );
    STACK_TPM_BUFFER( resp );
    STACK_TPM_BUFFER( tb );
    TPM_PERMANENT_FLAGS permanentFlags;

    STORE32(subcap.buffer, 0, TPM_CAP_FLAG_PERMANENT);

    subcap.used = 4;
    xx = rpmtpmErr(tpm, "GetCapability",
	TPM_GetCapability(TPM_CAP_FLAG, &subcap, &resp));
    if (xx)
	goto exit;

    TSS_SetTPMBuffer(&tb, resp.buffer, resp.used);

    xx = rpmtpmErr(tpm, "ReadPermanentFlags",
	TPM_ReadPermanentFlags(&tb, 0, &permanentFlags, resp.used));
    if (xx)
	goto exit;

    tpm->enabled = permanentFlags.physicalPresenceCMDEnable;

exit:
    return xx;
}

rpmtpm rpmtpmNew(const char * fn, int flags)
{
    rpmtpm tpm = rpmtpmGetPool(_rpmtpmPool);

#if defined(WITH_TPM)
    unsigned char startupparm = 0x1;	/* startup_clear: non-volatile state */
    int xx;

    TPM_setlog(0);	/* turn off verbose output */

    xx = rpmtpmErr(tpm, "Startup",
	TPM_Startup(startupparm));

   /* Enable TPM (if not already done). */
    xx = rpmtpmGetPhysicalCMDEnable(tpm);
    if (!xx && !tpm->enabled) {
	/* TSC_PhysicalPresence to turn on physicalPresenceCMDEnable */
	xx = rpmtpmErr(tpm, "PhysicalPresence(0x20)",
		TSC_PhysicalPresence(0x20));
	/* TSC_PhysicalPresence to turn on physicalPresence */
	if (!xx)
	    xx = rpmtpmErr(tpm, "PhysicalPresence(0x08)",
		TSC_PhysicalPresence(0x08));
	/* TPM_Process_PhysicalEnable to clear disabled */
	if (!xx)
	    xx = rpmtpmErr(tpm, "PhysicalEnable()",
		TPM_PhysicalEnable());
	/* TPM_Process_PhysicalSetDeactivated to clear deactivated */
	if (!xx)
	    xx = rpmtpmErr(tpm, "PhysicalSetDeactivated(FALSE)",
		TPM_PhysicalSetDeactivated(FALSE));
	if (!xx)
	    tpm->enabled = 1;
    }
#endif

    return rpmtpmLink(tpm);
}
