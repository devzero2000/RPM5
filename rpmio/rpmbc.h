#ifndef	H_RPMBC
#define	H_RPMBC

/** \ingroup rpmpgp
 * \file rpmio/rpmbc.h
 */

#include <api.h>

#include <rpmiotypes.h>
#include <rpmpgp.h>
#include <rpmsw.h>

/* Drag in the beecrypt includes. */
#include <beecrypt.h>
#include <base64.h>
#include <dsa.h>
#include <endianness.h>
#include <md4.h>
#include <md5.h>
#include <mp.h>
#include <rsa.h>
#include <rsapk.h>
#include <elgamal.h>
#include <ripemd128.h>
#include <ripemd160.h>
#include <ripemd256.h>
#include <ripemd320.h>
#include <sha1.h>
#include <sha224.h>
#include <sha256.h>
#include <sha384.h>
#include <sha512.h>

/**
 */
typedef	/*abstract@*/ struct rpmbc_s * rpmbc;

/**
 */
#if defined(_RPMBC_INTERNAL)
struct rpmbc_s {
    int in_fips_mode;	/* XXX trsa */
    int nbits;		/* XXX trsa */
    int qbits;		/* XXX trsa */
    int badok;		/* XXX trsa */
    int err;

    void * digest;
    size_t digestlen;

    randomGeneratorContext rngc;

    rsakp rsa_keypair;

    dsakp dsa_keypair;

    dlkp_p elg_keypair;
#ifdef	DYING
dldp_p elg_params;
#endif

    /* DSA parameters. */
    mpnumber r;
    mpnumber s;

    /* RSA parameters. */
    mpnumber hm;
    mpnumber m;
    mpnumber c;
};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmbcImplVecs;

#endif	/* H_RPMBC */
