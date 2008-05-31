/* Support for LZMA library.
 */

#include "system.h"
#include "rpmio_internal.h"
#include <rpmmacro.h>
#include <rpmcb.h>

#include "LzmaDecode.h"

#include "debug.h"

#include "LzmaDecode.c"

#define kInBufferSize (1 << 15)

typedef struct _CBuffer {
  ILzmaInCallback InCallback;
/*@dependent@*/
  FILE *File;
  unsigned char Buffer[kInBufferSize];
} CBuffer;

typedef struct lzfile {
    CBuffer g_InBuffer;
    CLzmaDecoderState state;  /* it's about 24-80 bytes structure, if int is 32-bit */
    unsigned char properties[LZMA_PROPERTIES_SIZE];

#if 0
    FILE *file;
#endif
    pid_t pid;
} LZFILE;

#define	LZDONLY(fd)	assert(fdGetIo(fd) == lzdio)

/*@access FD_t @*/

/* =============================================================== */

static size_t MyReadFile(FILE *file, void *data, size_t size)
	/*@globals fileSystem @*/
	/*@modifies *file, *data, fileSystem @*/
{ 
    if (size == 0) return 0;
    return fread(data, 1, size, file); 
}

static int MyReadFileAndCheck(FILE *file, void *data, size_t size)
	/*@globals fileSystem @*/
	/*@modifies *file, *data, fileSystem @*/
{
    return (MyReadFile(file, data, size) == size);
}

static int LzmaReadCompressed(void *object, const unsigned char **buffer, SizeT *size)
	/*@globals fileSystem @*/
	/*@modifies *buffer, *size, fileSystem @*/
{
    CBuffer *b = object;
    *buffer = b->Buffer;
    *size = MyReadFile(b->File, b->Buffer, kInBufferSize);
    return LZMA_RESULT_OK;
}

static inline /*@dependent@*/ void * lzdFileno(FD_t fd)
	/*@*/
{
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != lzdio)
	    continue;
	rc = fps->fp;
    	break;
    }
    
    return rc;
}

/*@-mods@*/	/* XXX hide rpmGlobalMacroContext mods for now. */
static LZFILE * lzdWriteOpen(int fdno, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    pid_t pid;
    int p[2];
    int xx;
    const char *lzma;
    char l[3] = "-7";	/* XXX same as default */
    const char *level;

    /* revisit use of LZMA_Alone, when lzdRead supports new LZMA Utils */
    char *env[] = { "LZMA_OPT=--format=alone", NULL };

    if (fmode != NULL && fmode[0] == 'w' && xisdigit(fmode[1])) /* "w9" */
	l[1] = fmode[1];
    level = l;

    if (fdno < 0) return NULL;
    if (pipe(p) < 0) {
	xx = close(fdno);
	return NULL;
    }
    pid = fork();
    if (pid < 0) {
	xx = close(fdno);
	return NULL;
    }
    if (pid) {
	LZFILE * lzfile = xcalloc(1, sizeof(*lzfile));

	xx = close(fdno);
	xx = close(p[0]);
	lzfile->pid = pid;
	lzfile->g_InBuffer.File = fdopen(p[1], "wb");
	if (lzfile->g_InBuffer.File == NULL) {
	    xx = close(p[1]);
	    lzfile = _free(lzfile);
	    return NULL;
	}
	return lzfile;
    } else {
	int i;
	/* lzma */
	xx = close(p[1]);
	xx = dup2(p[0], 0);
	xx = dup2(fdno, 1);
	for (i = 3; i < 1024; i++)
	    xx = close(i);
	lzma = rpmGetPath("%{?__lzma}%{!?__lzma:/usr/bin/lzma}", NULL);
	if (execle(lzma, "lzma", level, NULL, env))
	    _exit(1);
	lzma = _free(lzma);
    }
    return NULL; /* warning */
}
/*@=mods@*/

static LZFILE * lzdReadOpen(int fdno)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    LZFILE * lzfile;
    unsigned char ff[8];
    size_t nb;

    if (fdno < 0) return NULL;
    lzfile = xcalloc(1, sizeof(*lzfile));
    if (lzfile == NULL) return NULL;
    lzfile->g_InBuffer.File = fdopen(fdno, "rb");
    if (lzfile->g_InBuffer.File == NULL) goto error2;

    if (!MyReadFileAndCheck(lzfile->g_InBuffer.File, lzfile->properties, sizeof(lzfile->properties)))
	goto error;

    memset(ff, 0, sizeof(ff));
    if (!MyReadFileAndCheck(lzfile->g_InBuffer.File, ff, 8)) goto error;
    if (LzmaDecodeProperties(&lzfile->state.Properties, lzfile->properties, LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK)
	goto error;
    nb = LzmaGetNumProbs(&lzfile->state.Properties) * sizeof(*lzfile->state.Probs);
    lzfile->state.Probs = xmalloc(nb);
    if (lzfile->state.Probs == NULL) goto error;

    if (lzfile->state.Properties.DictionarySize == 0)
	lzfile->state.Dictionary = 0;
    else {
	lzfile->state.Dictionary = xmalloc(lzfile->state.Properties.DictionarySize);
	if (lzfile->state.Dictionary == NULL) {
	    lzfile->state.Probs = _free(lzfile->state.Probs);
	    goto error;
	}
    }
    lzfile->g_InBuffer.InCallback.Read = LzmaReadCompressed;
    LzmaDecoderInit(&lzfile->state);

    return lzfile;

error:
    (void) fclose(lzfile->g_InBuffer.File);
error2:
    lzfile = _free(lzfile);
    return NULL;
}

/*@-globuse@*/
static /*@null@*/ FD_t lzdOpen(const char * path, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd;
    mode_t mode = (fmode && fmode[0] == 'w' ? O_WRONLY : O_RDONLY);
    int fdno = open(path, mode);
    LZFILE * lzfile = (mode == O_WRONLY)
	? lzdWriteOpen(fdno, fmode)
	: lzdReadOpen(fdno);

    if (lzfile == NULL) {
	if (fdno >= 0) (void) close(fdno);
	return NULL;
    }

    fd = fdNew("open (lzdOpen)");
    fdPop(fd); fdPush(fd, lzdio, lzfile, -1);
    fdSetOpen(fd, path, fdno, mode);

DBGIO(fd, (stderr, "==>\tlzdOpen(\"%s\", \"%s\") fd %p %s\n", path, fmode, (fd ? fd : NULL), fdbg(fd)));

    return fdLink(fd, "lzdOpen");
}
/*@=globuse@*/

/*@-globuse@*/
static /*@null@*/ FD_t lzdFdopen(void * cookie, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    int fdno = fdFileno(fd);
    LZFILE * lzfile;

assert(fmode != NULL);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    lzfile = (fmode[0] == 'w')
	? lzdWriteOpen(fdno, fmode)
	: lzdReadOpen(fdno);
    if (lzfile == NULL) return NULL;
    fdPush(fd, lzdio, lzfile, fdno);
    return fdLink(fd, "lzdFdopen");
}
/*@=globuse@*/

#ifdef	UNUSED
/*@-globuse@*/
static int lzdFlush(FD_t fd)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    LZFILE *lzfile = lzdFileno(fd);

    if (lzfile == NULL || lzfile->g_InBuffer.File == NULL) return -2;
    return fflush(lzfile->g_InBuffer.File);
}
/*@=globuse@*/
#endif

/* =============================================================== */
/*@-globuse@*/
/*@-mustmod@*/		/* LCL: *buf is modified */
static ssize_t lzdRead(void * cookie, /*@out@*/ char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies buf, fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    LZFILE *lzfile;
    ssize_t rc = -1;
    size_t out;
    int res = 0;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    lzfile = lzdFileno(fd);
    fdstat_enter(fd, FDSTAT_READ);
    if (lzfile->g_InBuffer.File) {
/*@-compdef@*/
	res = LzmaDecode(&lzfile->state, &lzfile->g_InBuffer.InCallback, (unsigned char *)buf, count, &out);
	rc = (ssize_t)out;
    }
/*@=compdef@*/
DBGIO(fd, (stderr, "==>\tlzdRead(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (res) {
	if (lzfile)
	    fd->errcookie = "Lzma: decoding error";
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_READ, rc);
	/*@-compdef@*/
	if (fd->ndigests && rc > 0) fdUpdateDigests(fd, (void *)buf, rc);
	/*@=compdef@*/
    }
    return rc;
}
/*@=mustmod@*/
/*@=globuse@*/

/*@-globuse@*/
static ssize_t lzdWrite(void * cookie, const char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    LZFILE *lzfile;
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    lzfile = lzdFileno(fd);
    fdstat_enter(fd, FDSTAT_WRITE);
    rc = fwrite((void *)buf, 1, count, lzfile->g_InBuffer.File);
DBGIO(fd, (stderr, "==>\tlzdWrite(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (rc == -1) {
	fd->errcookie = strerror(ferror(lzfile->g_InBuffer.File));
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}
/*@=globuse@*/

static inline int lzdSeek(void * cookie, /*@unused@*/ _libio_pos_t pos,
			/*@unused@*/ int whence)
	/*@*/
{
    FD_t fd = c2f(cookie);

    LZDONLY(fd);
    return -2;
}

static int lzdClose( /*@only@*/ void * cookie)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    LZFILE * lzfile = lzdFileno(fd);
    int rc;

    if (lzfile == NULL) return -2;
    fdstat_enter(fd, FDSTAT_CLOSE);
/*@-noeffectuncon@*/ /* FIX: check rc */
    rc = fclose(lzfile->g_InBuffer.File);
    if (lzfile->pid)
	rc = (int) wait4(lzfile->pid, NULL, 0, NULL);
    else { /* reading */
	lzfile->state.Probs = _free(lzfile->state.Probs);
	lzfile->state.Dictionary = _free(lzfile->state.Dictionary);
    }
/*@-dependenttrans@*/
    lzfile = _free(lzfile);
/*@=dependenttrans@*/
/*@=noeffectuncon@*/
    rc = 0;	/* XXX FIXME */

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
assert(lzfile != NULL);	/* XXX FIXME, lzfile is always NULL here. */
	    fd->errcookie = strerror(ferror(lzfile->g_InBuffer.File));
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tlzdClose(%p) rc %lx %s\n", cookie, (unsigned long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "LZDIO", stderr);
    if (rc == 0)
	fd = fdFree(fd, "open (lzdClose)");
    return rc;
}

/*@-type@*/ /* LCL: function typedefs */
static struct FDIO_s lzdio_s = {
  lzdRead, lzdWrite, lzdSeek, lzdClose,	lzdOpen, lzdFdopen,
};
/*@=type@*/

FDIO_t lzdio = /*@-compmempass@*/ &lzdio_s /*@=compmempass@*/ ;
