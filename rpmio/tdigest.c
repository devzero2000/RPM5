#include "system.h"
#include <gcrypt.h>
#include "rpmio_internal.h"
#include "popt.h"
#include "debug.h"

static rpmDigestFlags flags = RPMDIGEST_NONE;
extern int _rpmio_debug;

static int fips = 0;
static int crctest = 0;
static int gcrypt = 0;

const char * FIPSAdigest = "a9993e364706816aba3e25717850c26c9cd0d89d";
const char * FIPSBdigest = "84983e441c3bd26ebaae4aa1f95129e5e54670f1";
const char * FIPSCdigest = "34aa973cd4c4daa4f61eeb2bdbad27316534016f";

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmDigestPoptTable, 0,
	N_("Digest options:"),
	NULL },

 { "fipsa",'\0', POPT_ARG_VAL, &fips, 1,	NULL, NULL },
 { "fipsb",'\0', POPT_ARG_VAL, &fips, 2,	NULL, NULL },
 { "fipsc",'\0', POPT_ARG_VAL, &fips, 3,	NULL, NULL },
 { "crccheck",'\0', POPT_ARG_VAL, &crctest, 1,	NULL, NULL },
 { "gcrypt",'\0', POPT_ARG_VAL, &gcrypt, 1,	NULL, NULL },
 { "debug",'d', POPT_ARG_VAL, &_rpmio_debug, -1,	NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

#define	RMD160_CMD	"/usr/bin/openssl rmd160"
#define	SHA1_CMD	"/usr/bin/openssl sha1"
#define	MD2_CMD		"/usr/bin/openssl md2"
#define	MD4_CMD		"/usr/bin/openssl md4"
#define	MD5_CMD		"/usr/bin/openssl md5"

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    const char ** args;
    const char * ifn;
    const char * ofn = "/dev/null";
    DIGEST_CTX ctx = NULL;
    gcry_md_hd_t h = NULL;
    gcry_error_t gcry = NULL;
    const char * scmd;
    const char * idigest;
    const char * odigest;
    const char * sdigest;
    const char * digest;
    size_t digestlen;
    int asAscii = 1;
    int reverse = 0;
    int rc;
    char appendix;
    int i;

    optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    while ((rc = poptGetNextOpt(optCon)) > 0)
	;

    if (fips) {
	struct rpmsw_s begin, end;
	if (gcrypt)
	    gcry = gcry_md_open(h, GCRY_MD_SHA1, 0);

	(void) rpmswNow(&begin);

	if (gcrypt)
	    gcry_md_reset(gcry);
	else
	    ctx = rpmDigestInit(PGPHASHALGO_SHA1, flags);
	ifn = NULL;
	appendix = ' ';
	sdigest = NULL;
	switch (fips) {
	case 1:
	    ifn = "abc";
	    if (gcrypt)
		gcry_md_write (gcry, ifn, strlen(ifn));
	    else
		rpmDigestUpdate(ctx, ifn, strlen(ifn));
	    sdigest = FIPSAdigest;
	    appendix = 'A';
	    break;
	case 2:
	    ifn = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	    if (gcrypt)
		gcry_md_write (gcry, ifn, strlen(ifn));
	    else
		rpmDigestUpdate(ctx, ifn, strlen(ifn));
	    sdigest = FIPSBdigest;
	    appendix = 'B';
	    break;
	case 3:
	    ifn = "aaaaaaaaaaa ...";
	    for (i = 0; i < 1000000; i++) {
		if (gcrypt)
		    gcry_md_write (gcry, ifn, 1);
		else
		    rpmDigestUpdate(ctx, ifn, 1);
	    }
	    sdigest = FIPSCdigest;
	    appendix = 'C';
	    break;
	}
	if (ifn == NULL)
	    return 1;
	if (gcrypt) {
	    const unsigned char * s = gcry_md_read (gcry, 0);
	    char * t;

	    gcry_md_close(gcry);
	    digestlen = 2*20;
	    digest = t = xcalloc(1, digestlen+1);
	    for (i = 0; i < digestlen; i += 2) {
		static const char hex[] = "0123456789abcdef";
		*t++ = hex[ (unsigned)((*s >> 4) & 0x0f) ];
		*t++ = hex[ (unsigned)((*s++   ) & 0x0f) ];
	    }
	    *t = '\0';
	} else
	    rpmDigestFinal(ctx, (void **)&digest, &digestlen, asAscii);
	(void) rpmswNow(&end);

	if (digest) {
	    fprintf(stdout, "%s     %s\n", digest, ifn);
	    fflush(stdout);
	    free((void *)digest);
	}
	if (sdigest) {
	    fprintf(stdout, "%s     FIPS PUB 180-1 Appendix %c\n", sdigest,
		appendix);
	    fflush(stdout);
	}
fprintf(stderr, "*** time %lu usecs\n", (unsigned long)rpmswDiff(&end, &begin));
	return 0;
    } else
    if (crctest) {
	const char * s = "123456789";
	ifn = "cbf43926";
	ctx = rpmDigestInit(PGPHASHALGO_CRC32, flags);
	rpmDigestUpdate(ctx, s, strlen(s));
	rpmDigestFinal(ctx, (void **)&digest, &digestlen, asAscii);
	if (digest) {
	    fprintf(stdout, "%s     %s\n", digest, ifn);
	    fflush(stdout);
	    free((void *)digest);
	}
	return 0;
    }

    args = poptGetArgs(optCon);
    rc = 0;
    if (args)
    while ((ifn = *args++) != NULL) {
	FD_t ifd;
	FD_t ofd;
	unsigned char buf[BUFSIZ];
	ssize_t nb;

	switch (rpmDigestHashAlgo) {
	case PGPHASHALGO_MD2:		scmd = MD2_CMD;		break;
	case PGPHASHALGO_MD4:		scmd = MD4_CMD;		break;
	case PGPHASHALGO_MD5:		scmd = MD5_CMD;		break;
	case PGPHASHALGO_SHA1:		scmd = SHA1_CMD;	break;
	case PGPHASHALGO_RIPEMD160:	scmd = RMD160_CMD;	break;
	case PGPHASHALGO_TIGER192:
	case PGPHASHALGO_HAVAL_5_160:
	default:			scmd = NULL;		break;
	}

	sdigest = NULL;
	if (scmd != NULL) {
	    char *se;
	    FILE * sfp;

	    se = buf;
	    *se = '\0';
	    se = stpcpy(se, scmd);
	    *se++ = ' ';
	    se = stpcpy(se, ifn);
	    if ((sfp = popen(buf, "r")) != NULL) {
		fgets(buf, sizeof(buf), sfp);
		buf[strlen(buf)-1] = '\0';
		if ((se = strchr(buf, ' ')) != NULL)
		    *se++ = '\0';
		sdigest = xstrdup(se);
		pclose(sfp);
	    }
	}

	ifd = Fopen(ifn, "r.ufdio");
	if (ifd == NULL || Ferror(ifd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), ifn, Fstrerror(ifd));
	    if (ifd) Fclose(ifd);
	    rc++;
	    continue;
	}
	idigest = NULL;
	fdInitDigest(ifd, rpmDigestHashAlgo, reverse);

	ofd = Fopen(ofn, "w.ufdio");
	if (ofd == NULL || Ferror(ofd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), ofn, Fstrerror(ofd));
	    if (ifd) Fclose(ifd);
	    if (ofd) Fclose(ofd);
	    rc++;
	    continue;
	}
	odigest = NULL;
	fdInitDigest(ofd, rpmDigestHashAlgo, reverse);

	ctx = rpmDigestInit(rpmDigestHashAlgo, flags);

	while ((nb = Fread(buf, 1, sizeof(buf), ifd)) > 0) {
	    rpmDigestUpdate(ctx, buf, nb);
	    (void) Fwrite(buf, 1, nb, ofd);
	}

	fdFiniDigest(ifd, rpmDigestHashAlgo, (void **)&idigest, NULL, asAscii);
	Fclose(ifd);

	Fflush(ofd);
	fdFiniDigest(ofd, rpmDigestHashAlgo, (void **)&odigest, NULL, asAscii);
	Fclose(ofd);

	rpmDigestFinal(ctx, (void **)&digest, &digestlen, asAscii);

	if (digest) {
	    fprintf(stdout, "%s     %s\n", digest, ifn);
	    fflush(stdout);
	    free((void *)digest);
	}
	if (idigest) {
	    fprintf(stdout, "%s in  %s\n", idigest, ifn);
	    fflush(stdout);
	    free((void *)idigest);
	}
	if (odigest) {
	    fprintf(stdout, "%s out %s\n", odigest, ofn);
	    fflush(stdout);
	    free((void *)odigest);
	}
	if (sdigest) {
	    fprintf(stdout, "%s cmd %s\n", sdigest, ifn);
	    fflush(stdout);
	    free((void *)sdigest);
	}
    }
    return rc;
}
