/** \ingroup rpmio
 * \file rpmio/rpmzq.c
 * Job queue and buffer management (originally swiped from PIGZ).
 */

/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007, 2008 Mark Adler
 * Version 2.1.4  9 Nov 2008  Mark Adler
 */

/*
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu

  Mark accepts donations for providing this software.  Donations are not
  required or expected.  Any amount that you feel is appropriate would be
  appreciated.  You can use this link:

  https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=536055

 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmlog.h>

#define	_RPMBZ_INTERNAL
#include "rpmbz.h"

/*@access rpmbz @*/

#include "yarn.h"

#define	_RPMZLOG_INTERNAL
#include "rpmzlog.h"

/*@access rpmzMsg @*/
/*@access rpmzLog @*/

#define Trace(x) \
    do { \
	if (zq->verbosity > 2) { \
	    rpmzLogAdd x; \
	} \
    } while (0)

#define	_RPMZQ_INTERNAL
#include "rpmzq.h"

/*@access rpmzSpace @*/
/*@access rpmzPool @*/
/*@access rpmzQueue @*/
/*@access rpmzJob @*/

#include "debug.h"

/*@unchecked@*/
int _rpmzq_debug = 0;

/*@unchecked@*/
static struct rpmzQueue_s __zq;

/*@unchecked@*/
rpmzQueue _rpmzq = &__zq;

/*==============================================================*/
/**
 */
static void _rpmzqArgCallback(poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, /*@unused@*/ const char * arg,
		/*@unused@*/ void * data)
	/*@globals _rpmzq, fileSystem, internalState @*/
	/*@modifies _rpmzq, fileSystem, internalState @*/
{
    rpmzQueue zq = _rpmzq;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'q':	zq->verbosity = 0; break;
    case 'v':	zq->verbosity++; break;
    default:
	/* XXX really need to display longName/shortName instead. */
	fprintf(stderr, _("Unknown option -%c\n"), (char)opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

#ifdef	REFERENCE
Usage: pigz [options] [files ...]
  will compress files in place, adding the suffix '.gz'.  If no files are
  specified, stdin will be compressed to stdout.  pigz does what gzip does,
  but spreads the work over multiple processors and cores when compressing.

Options:
  -0 to -9, --fast, --best   Compression levels, --fast is -1, --best is -9
  -b, --blocksize mmm  Set compression block size to mmmK (default 128K)
  -p, --processes n    Allow up to n compression threads (default 8)
  -i, --independent    Compress blocks independently for damage recovery
  -R, --rsyncable      Input-determined block locations for rsync
  -d, --decompress     Decompress the compressed input
  -t, --test           Test the integrity of the compressed input
  -l, --list           List the contents of the compressed input
  -f, --force          Force overwrite, compress .gz, links, and to terminal
  -r, --recursive      Process the contents of all subdirectories
  -s, --suffix .sss    Use suffix .sss instead of .gz (for compression)
  -z, --zlib           Compress to zlib (.zz) instead of gzip format
  -K, --zip            Compress to PKWare zip (.zip) single entry format
  -k, --keep           Do not delete original file after processing
  -c, --stdout         Write all processed output to stdout (wont delete)
  -N, --name           Store/restore file name and mod time in/from header
  -n, --no-name        Do not store or restore file name in/from header
  -T, --no-time        Do not store or restore mod time in/from header
  -q, --quiet          Print no messages, even on error
  -v, --verbose        Provide more verbose output
#endif

#ifdef	REFERENCE
Parallel BZIP2 v1.0.5 - by: Jeff Gilchrist http://compression.ca
[Jan. 08, 2009]             (uses libbzip2 by Julian Seward)

Usage: ./pbzip2 [-1 .. -9] [-b#cdfhklp#qrtVz] <filename> <filename2> <filenameN>
 -b#      : where # is the file block size in 100k (default 9 = 900k)
 -c       : output to standard out (stdout)
 -d       : decompress file
 -f       : force, overwrite existing output file
 -h       : print this help message
 -k       : keep input file, dont delete
 -l       : load average determines max number processors to use
 -p#      : where # is the number of processors (default 2)
 -q       : quiet mode (default)
 -r       : read entire input file into RAM and split between processors
 -t       : test compressed file integrity
 -v       : verbose mode
 -V       : display version info for pbzip2 then exit
 -z       : compress file (default)
 -1 .. -9 : set BWT block size to 100k .. 900k (default 900k)

Example: pbzip2 -b15vk myfile.tar
Example: pbzip2 -p4 -r -5 myfile.tar second*.txt
Example: tar cf myfile.tar.bz2 --use-compress-prog=pbzip2 dir_to_compress/
Example: pbzip2 -d myfile.tar.bz2

#endif

/*@unchecked@*/ /*@observer@*/
struct poptOption rpmzqOptionsPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	_rpmzqArgCallback, 0, NULL, NULL },
/*@=type@*/

  { "fast", '\0', POPT_ARG_VAL,				&__zq.level,  1,
	N_("fast compression"), NULL },
  { "best", '\0', POPT_ARG_VAL,				&__zq.level,  9,
	N_("best compression"), NULL },
  { "extreme", 'e', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_DOC_HIDDEN,
	&__zq.flags,  RPMZ_FLAGS_EXTREME,
	N_("extreme compression"), NULL },
  { NULL, '0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  0,
	NULL, NULL },
  { NULL, '1', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  1,
	NULL, NULL },
  { NULL, '2', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  2,
	NULL, NULL },
  { NULL, '3', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  3,
	NULL, NULL },
  { NULL, '4', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  4,
	NULL, NULL },
  { NULL, '5', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  5,
	NULL, NULL },
  { NULL, '6', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  6,
	NULL, NULL },
  { NULL, '7', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  7,
	NULL, NULL },
  { NULL, '8', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  8,
	NULL, NULL },
  { NULL, '9', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.level,  9,
	NULL, NULL },

#ifdef	NOTYET	/* XXX --blocksize/--processes callback to validate arg */
  { "blocksize", 'b', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	&__zq.blocksize, 0,
	N_("Set compression block size to mmmK"), N_("mmm") },
  /* XXX same as --threads */
  { "processes", 'p', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	&__zq.threads, 0,
	N_("Allow up to n compression threads"), N_("n") },
#else
  /* XXX show default is bogus with callback, can't find value. */
  { "blocksize", 'b', POPT_ARG_VAL|POPT_ARGFLAG_SHOW_DEFAULT,	NULL, 'b',
	N_("Set compression block size to mmmK"), N_("mmm") },
  /* XXX same as --threads */
  { "processes", 'p', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	NULL, 'p',
	N_("Allow up to n compression threads"), N_("n") },
#endif
  /* XXX display toggle "-i,--[no]indepndent" bustage. */
  { "independent", 'i', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,	&__zq.flags, RPMZ_FLAGS_INDEPENDENT,
	N_("Compress blocks independently for damage recovery"), NULL },
  /* XXX display toggle "-r,--[no]rsyncable" bustage. */
  { "rsyncable", 'R', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,		&__zq.flags, RPMZ_FLAGS_RSYNCABLE,
	N_("Input-determined block locations for rsync"), NULL },
#if defined(NOTYET)
  /* XXX -T collides with pigz -T,--no-time */
  { "threads", '\0', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	&__zq.threads, 0,
	N_("Allow up to n compression threads"), N_("n") },
#endif	/* _RPMZ_INTERNAL_XZ */

  /* ===== Operation modes */
  { "compress", 'z', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.mode, RPMZ_MODE_COMPRESS,
	N_("force compression"), NULL },
  { "uncompress", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.mode, RPMZ_MODE_DECOMPRESS,
	N_("force decompression"), NULL },
  { "decompress", 'd', POPT_ARG_VAL,		&__zq.mode, RPMZ_MODE_DECOMPRESS,
	N_("Decompress the compressed input"), NULL },
  { "test", 't', POPT_ARG_VAL,			&__zq.mode,  RPMZ_MODE_TEST,
	N_("Test the integrity of the compressed input"), NULL },
  { "list", 'l', POPT_BIT_SET,			&__zq.flags,  RPMZ_FLAGS_LIST,
	N_("List the contents of the compressed input"), NULL },
  { "info", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__zq.flags,  RPMZ_FLAGS_LIST,
	N_("list block sizes, total sizes, and possible metadata"), NULL },
  { "force", 'f', POPT_BIT_SET,		&__zq.flags,  RPMZ_FLAGS_FORCE,
	N_("Force: --overwrite --recompress --symlinks --tty"), NULL },
  { "overwrite", '\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,
	&__zq.flags,  RPMZ_FLAGS_OVERWRITE,
	N_("  Permit overwrite of output files"), NULL },
  { "recompress",'\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,
	&__zq.flags,  RPMZ_FLAGS_ALREADY,
	N_("  Permit compress of already compressed files"), NULL },
  { "symlinks",'\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,
	&__zq.flags,  RPMZ_FLAGS_SYMLINKS,
	N_("  Permit symlink input file to be compressed"), NULL },
  { "tty",'\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,
	&__zq.flags,  RPMZ_FLAGS_TTY,
	N_("  Permit compressed output to terminal"), NULL },

  /* ===== Operation modifiers */
  /* XXX display toggle "-r,--[no]recursive" bustage. */
  { "recursive", 'r', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,	&__zq.flags, RPMZ_FLAGS_RECURSE,
	N_("Process the contents of all subdirectories"), NULL },
  { "suffix", 'S', POPT_ARG_STRING,		&__zq.suffix, 0,
	N_("Use suffix .sss instead of .gz (for compression)"), N_(".sss") },
  { "ascii", 'a', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'a',
	N_("Compress to LZW (.Z) instead of gzip format"), NULL },
  { "bits", 'Z', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'Z',
	N_("Compress to LZW (.Z) instead of gzip format"), NULL },
  { "zlib", 'z', POPT_ARG_VAL,		&__zq.format, RPMZ_FORMAT_ZLIB,
	N_("Compress to zlib (.zz) instead of gzip format"), NULL },
  { "zip", 'K', POPT_ARG_VAL,		&__zq.format, RPMZ_FORMAT_ZIP2,
	N_("Compress to PKWare zip (.zip) single entry format"), NULL },
  { "keep", 'k', POPT_BIT_SET,			&__zq.flags, RPMZ_FLAGS_KEEP,
	N_("Do not delete original file after processing"), NULL },
  { "stdout", 'c', POPT_BIT_SET,		&__zq.flags,  RPMZ_FLAGS_STDOUT,
	N_("Write all processed output to stdout (won't delete)"), NULL },
  { "to-stdout", 'c', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &__zq.flags,  RPMZ_FLAGS_STDOUT,
	N_("write to standard output and don't delete input files"), NULL },

  /* ===== Metadata options */
  /* XXX logic is reversed, disablers should clear with toggle. */
  { "name", 'N', POPT_BIT_SET,		&__zq.flags, (RPMZ_FLAGS_HNAME|RPMZ_FLAGS_HTIME),
	N_("Store/restore file name and mod time in/from header"), NULL },
  { "no-name", 'n', POPT_BIT_CLR,	&__zq.flags, RPMZ_FLAGS_HNAME,
	N_("Do not store or restore file name in/from header"), NULL },
  /* XXX -T collides with xz -T,--threads */
  { "no-time", 'T', POPT_BIT_CLR,	&__zq.flags, RPMZ_FLAGS_HTIME,
	N_("Do not store or restore mod time in/from header"), NULL },

  /* ===== Other options */

  POPT_TABLEEND
};

#define	zqFprint	if (_rpmzq_debug) fprintf

/*==============================================================*/

/*@-mustmod@*/
int rpmbzCompressBlock(void * _bz, rpmzJob job)
{
    rpmbz bz = _bz;
    int rc;
    rc = BZ2_bzBuffToBuffCompress((char *)job->out->buf, &job->out->len,
		(char *)job->in->buf, job->in->len, bz->B, bz->V, bz->W);
    if (rc != BZ_OK)
	zqFprint(stderr, "==> %s(%p,%p) rc %d\n", __FUNCTION__, bz, job, rc);
    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int rpmbzDecompressBlock(rpmbz bz, rpmzJob job)
	/*@globals fileSystem @*/
	/*@modifies job, fileSystem @*/
{
    int rc;
    rc = BZ2_bzBuffToBuffDecompress((char *)job->out->buf, &job->out->len,
		(char *)job->in->buf, job->in->len, bz->S, bz->V);
    if (rc != BZ_OK)
	zqFprint(stderr, "==> %s(%p,%p) rc %d\n", __FUNCTION__, bz, job, rc);
    return rc;
}
/*@=mustmod@*/

/*==============================================================*/

/* -- pool of spaces for buffer management -- */

/* These routines manage a pool of spaces.  Each pool specifies a fixed size
   buffer to be contained in each space.  Each space has a use count, which
   when decremented to zero returns the space to the pool.  If a space is
   requested from the pool and the pool is empty, a space is immediately
   created unless a specified limit on the number of spaces has been reached.
   Only if the limit is reached will it wait for a space to be returned to the
   pool.  Each space knows what pool it belongs to, so that it can be returned.
 */

/* initialize a pool (pool structure itself provided, not allocated) -- the
   limit is the maximum number of spaces in the pool, or -1 to indicate no
   limit, i.e., to never wait for a buffer to return to the pool */
rpmzPool rpmzqNewPool(size_t size, int limit)
{
    rpmzPool pool = xcalloc(1, sizeof(*pool));
/*@=mustfreeonly@*/
    pool->have = yarnNewLock(0);
    pool->head = NULL;
/*@=mustfreeonly@*/
    pool->size = size;
    pool->limit = limit;
    pool->made = 0;
zqFprint(stderr, "    ++ pool %p[%u,%d]\n", pool, (unsigned)size, limit);
    return pool;
}

/* get a space from a pool -- the use count is initially set to one, so there
   is no need to call rpmzqUseSpace() for the first use */
rpmzSpace rpmzqNewSpace(rpmzPool pool)
{
    rpmzSpace space;

    /* if can't create any more, wait for a space to show up */
    yarnPossess(pool->have);
    if (pool->limit == 0)
	yarnWaitFor(pool->have, NOT_TO_BE, 0);

    /* if a space is available, pull it from the list and return it */
    if (pool->head != NULL) {
	space = pool->head;
	yarnPossess(space->use);
	pool->head = space->next;
	yarnTwist(pool->have, BY, -1);      /* one less in pool */
	yarnTwist(space->use, TO, 1);       /* initially one user */
	return space;
    }

    /* nothing available, don't want to wait, make a new space */
assert(pool->limit != 0);
    if (pool->limit > 0)
	pool->limit--;
    pool->made++;
    yarnRelease(pool->have);

    space = xmalloc(sizeof(*space));
/*@-mustfreeonly@*/
    space->use = yarnNewLock(1);           /* initially one user */
    space->len = pool->size;		/* XXX initialize to pool->size */
    space->buf = xmalloc(space->len);
/*@-assignexpose@*/
    space->pool = pool;                 /* remember the pool this belongs to */
/*@=assignexpose@*/
/*@=mustfreeonly@*/
zqFprint(stderr, "    ++ space %p use %d buf %p[%u]\n", space, 1, space->buf, space->len);
/*@-nullret@*/
    return space;
/*@=nullret@*/
}

/* increment the use count to require one more drop before returning this space
   to the pool */
void rpmzqUseSpace(rpmzSpace space)
{
    int use;
    yarnPossess(space->use);
    use = yarnPeekLock(space->use);
zqFprint(stderr, "    ++ space %p[%d] buf %p[%u]\n", space, use+1, space->buf, space->len);
    yarnTwist(space->use, BY, 1);
}

/* drop a space, returning it to the pool if the use count is zero */
rpmzSpace rpmzqDropSpace(rpmzSpace space)
{
    int use;

    yarnPossess(space->use);
    use = yarnPeekLock(space->use);
zqFprint(stderr, "    -- space %p[%u] buf %p[%u]\n", space, use, space->buf, space->len);
assert(use != 0);
    if (use == 1) {
	rpmzPool pool = space->pool;
	yarnPossess(pool->have);
	space->len = pool->size;	/* XXX reset to pool->size */
/*@-mustfreeonly@*/
	space->next = pool->head;
/*@=mustfreeonly@*/
	pool->head = space;
	yarnTwist(pool->have, BY, 1);
    }
    yarnTwist(space->use, BY, -1);
    return NULL;
}

/* free the memory and lock resources of a pool -- return number of spaces for
   debugging and resource usage measurement */
rpmzPool rpmzqFreePool(rpmzPool pool, int *countp)
{
    rpmzSpace space;
    int count;

    yarnPossess(pool->have);
    count = 0;
    while ((space = pool->head) != NULL) {
	pool->head = space->next;
	space->buf = _free(space->buf);
	space->use = yarnFreeLock(space->use);
/*@-compdestroy@*/
	space = _free(space);
/*@=compdestroy@*/
	count++;
    }
    yarnRelease(pool->have);
    pool->have = yarnFreeLock(pool->have);
#ifdef	NOTYET
assert(count == pool->made);
#else
if (count != pool->made)
zqFprint(stderr, "==> FIXME: %s: count %d pool->made %d\n", __FUNCTION__, count, pool->made);
#endif
zqFprint(stderr, "    -- pool %p count %d\n", pool, count);
/*@-compdestroy@*/
    pool = _free(pool);
/*@=compdestroy@*/
    if (countp != NULL)
	*countp = count;
    return NULL;
}

rpmzJob rpmzqFreeJob(rpmzJob job)
{
zqFprint(stderr, "    -- job %p[%ld] %p => %p\n", job, job->seq, job->in, job->out);
    if (job->calc != NULL)
	job->calc = yarnFreeLock(job->calc);
    job = _free(job);
    return NULL;
}

rpmzJob rpmzqNewJob(long seq)
{
    rpmzJob job = xcalloc(1, sizeof(*job));
    job->seq = seq;
    job->calc = yarnNewLock(0);
zqFprint(stderr, "    ++ job %p[%ld]\n", job, seq);
    return job;
}

/* compress or write job (passed from compress list to write list) -- if seq is
   equal to -1, rpmzqCompressThread() is instructed to return; if more is false then
   this is the last chunk, which after writing tells write_thread to return */

/* command the compress threads to all return, then join them all (call from
   main thread), free all the thread-related resources */
void rpmzqFini(rpmzQueue zq)
{
    rpmzLog zlog = zq->zlog;

    struct rpmzJob_s job;
    int caught;

zqFprint(stderr, "--> %s(%p)\n", __FUNCTION__, zq);
    /* only do this once */
    if (zq->compress_have == NULL)
	return;

    /* command all of the extant compress threads to return */
    yarnPossess(zq->compress_have);
    job.seq = -1;
    job.next = NULL;
/*@-immediatetrans -mustfreeonly@*/
    zq->compress_head = &job;
/*@=immediatetrans =mustfreeonly@*/
    zq->compress_tail = &job.next;
    yarnTwist(zq->compress_have, BY, 1);		/* will wake them all up */

    /* join all of the compress threads, verify they all came back */
    caught = yarnJoinAll();
    Trace((zlog, "-- joined %d compress threads", caught));
#ifdef	NOTYET
assert(caught == zq->cthreads);
#else
if (caught != zq->cthreads)
zqFprint(stderr, "==> FIXME: %s: caught %d z->cthreads %d\n", __FUNCTION__, caught, zq->cthreads);
#endif
    zq->cthreads = 0;

    /* free the resources */
    zq->out_pool = rpmzqFreePool(zq->out_pool, &caught);
    Trace((zlog, "-- freed %d output buffers", caught));
    zq->in_pool = rpmzqFreePool(zq->in_pool, &caught);
    Trace((zlog, "-- freed %d input buffers", caught));
    zq->write_first = yarnFreeLock(zq->write_first);
    zq->compress_have = yarnFreeLock(zq->compress_have);
}

/* setup job lists (call from main thread) */
void rpmzqInit(rpmzQueue zq)
{
zqFprint(stderr, "--> %s(%p)\n", __FUNCTION__, zq);
    /* set up only if not already set up*/
    if (zq->compress_have != NULL)
	return;

    /* allocate locks and initialize lists */
/*@-mustfreeonly@*/
    zq->compress_have = yarnNewLock(0);
    zq->compress_head = NULL;
    zq->compress_tail = &zq->compress_head;
    zq->write_first = yarnNewLock(-1);
    zq->write_head = NULL;

    zq->in_pool = rpmzqNewPool(zq->iblocksize, zq->ilimit);
zqFprint(stderr, "-->  in_pool: %p[%u] blocksize %u\n", zq->in_pool, (unsigned)zq->ilimit, (unsigned)zq->iblocksize);
    zq->out_pool = rpmzqNewPool(zq->oblocksize, zq->olimit);
zqFprint(stderr, "--> out_pool: %p[%u] blocksize %u\n", zq->out_pool, (unsigned)zq->olimit, (unsigned)zq->oblocksize);

}

rpmzQueue rpmzqFree(rpmzQueue zq)
{
    return NULL;
}

rpmzQueue rpmzqNew(rpmzQueue zq, rpmzLog zlog, int limit)
{
    zq->ifn = NULL;
    zq->ifdno = -1;
    zq->ofn = NULL;
    zq->ofdno = -1;
    zq->iblocksize = zq->blocksize;
    zq->ilimit = limit;
    zq->oblocksize = zq->blocksize;
    zq->olimit = limit;
/*@-assignexpose@*/
    zq->zlog = zlog;
/*@=assignexpose@*/
    return zq;
}

rpmzJob rpmzqDelCJob(rpmzQueue zq)
{
    rpmzJob job;

    /* get job from compress list, let all the compressors know */
    yarnPossess(zq->compress_have);
    yarnWaitFor(zq->compress_have, NOT_TO_BE, 0);
    job = zq->compress_head;
assert(job != NULL);
    if (job->seq == -1) {
	yarnRelease(zq->compress_have);
	return NULL;
    }

/*@-assignexpose -dependenttrans@*/
    zq->compress_head = job->next;
/*@=assignexpose =dependenttrans@*/
    if (job->next == NULL)
	zq->compress_tail = &zq->compress_head;
    yarnTwist(zq->compress_have, BY, -1);

    return job;
}

void rpmzqAddCJob(rpmzQueue zq, rpmzJob job)
{
    /* put job at end of compress list, let all the compressors know */
    yarnPossess(zq->compress_have);
    job->next = NULL;
/*@-assignexpose@*/
    *zq->compress_tail = job;
    zq->compress_tail = &job->next;
/*@=assignexpose@*/
    yarnTwist(zq->compress_have, BY, 1);
}

rpmzJob rpmzqDelWJob(rpmzQueue zq, long seq)
{
    rpmzJob job;

    /* get next write job in order */
    yarnPossess(zq->write_first);
    yarnWaitFor(zq->write_first, TO_BE, seq);
    job = zq->write_head;
assert(job != NULL);
/*@-assignexpose -dependenttrans@*/
    zq->write_head = job->next;
/*@=assignexpose =dependenttrans@*/
    yarnTwist(zq->write_first, TO, zq->write_head == NULL ? -1 : zq->write_head->seq);
    return job;
}

void rpmzqAddWJob(rpmzQueue zq, rpmzJob job)
{
    rpmzLog zlog = zq->zlog;

    rpmzJob here;		/* pointers for inserting in write list */
    rpmzJob * prior;		/* pointers for inserting in write list */
    double pct;

    yarnPossess(zq->write_first);

    switch (zq->omode) {
    default:	assert(0);	break;
    case O_WRONLY:
	pct = (100.0 * job->out->len) / job->in->len;
	zqFprint(stderr, "       job %p[%ld]:\t%p[%u] => %p[%u]\t(%3.1f%%)\n",
			job, job->seq, job->in->buf, job->in->len,
			job->out->buf, job->out->len, pct);
	Trace((zlog, "-- compressed #%ld %3.1f%%%s", job->seq, pct,
		(job->more ? "" : " (last)")));
	break;
    case O_RDONLY:
	pct = (100.0 * job->in->len) / job->out->len;
	zqFprint(stderr, "       job %p[%ld]:\t%p[%u] <= %p[%u]\t(%3.1f%%)\n",
			job, job->seq, job->in->buf, job->in->len,
			job->out->buf, job->out->len, pct);
	Trace((zlog, "-- decompressed #%ld %3.1f%%%s", job->seq, pct,
		(job->more ? "" : " (last)")));
	break;
    }

    /* insert write job in list in sorted order, alert write thread */
    prior = &zq->write_head;
    while ((here = *prior) != NULL) {
	if (here->seq > job->seq)
	    break;
	prior = &here->next;
    }
/*@-onlytrans@*/
    job->next = here;
/*@=onlytrans@*/
    *prior = job;

    yarnTwist(zq->write_first, TO, zq->write_head->seq);
}

static rpmzJob rpmzqFillOut(rpmzQueue zq, /*@returned@*/rpmzJob job, rpmbz bz)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, job, fileSystem, internalState @*/
{
    size_t outlen;
    int ret;

    switch (zq->omode) {
    default:	assert(0);	break;
    case O_WRONLY:
	/* set up input and output (the output size is assured to be big enough
	 * for the worst case expansion of the input buffer size, plus five
	 * bytes for the terminating stored block) */
	outlen = ((job->in->len*1.01)+600);
/*@-mustfreeonly@*/
	job->out = rpmzqNewSpace(zq->out_pool);
/*@=mustfreeonly@*/
	if (job->out->len < outlen) {
zqFprint(stderr, "==> FIXME: %s: job->out %p %p[%u] malloc(%u)\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len, (unsigned)outlen);
	    job->out = rpmzqDropSpace(job->out);
	    job->out = xcalloc(1, sizeof(*job->out));
	    job->out->len = outlen;
	    job->out->buf = xmalloc(job->out->len);
	}

	/* compress job->in to job-out */
	ret = rpmbzCompressBlock(bz, job);
	break;
    case O_RDONLY:
	outlen = 6 * job->in->len;
	job->out = rpmzqNewSpace(zq->out_pool);
	if (job->out->len < outlen) {
zqFprint(stderr, "==> FIXME: %s: job->out %p %p[%u] malloc(%u)\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len, (unsigned)outlen);
	    job->out = rpmzqDropSpace(job->out);
	    job->out = xcalloc(1, sizeof(*job->out));
	    job->out->len = outlen;
	    job->out->buf = xmalloc(job->out->len);
	}

	for (;;) {
	    static int _grow = 2;	/* XXX factor to scale malloc by. */

	    outlen = job->out->len * _grow;
	    ret = rpmbzDecompressBlock(bz, job);
	    if (ret != BZ_OUTBUFF_FULL)
		/*@loopbreak@*/ break;
zqFprint(stderr, "==> FIXME: %s: job->out %p %p[%u] realloc(%u)\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len, (unsigned)outlen);
	    if (job->out->use != NULL)
		job->out = rpmzqDropSpace(job->out);
	    else {
zqFprint(stderr, "==> FIXME: %s: job->out %p %p[%u] free\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len);
		job->out->buf = _free(job->out->buf);
		job->out = _free(job->out);
	    }
	    job->out = xcalloc(1, sizeof(*job->out));
	    job->out->len = outlen;
	    job->out->buf = xmalloc(job->out->len);
	}
assert(ret == BZ_OK);
	break;
    }
    return job;
}

/* get the next compression job from the head of the list, compress and compute
   the check value on the input, and put a job in the write list with the
   results -- keep looking for more jobs, returning when a job is found with a
   sequence number of -1 (leave that job in the list for other incarnations to
   find) */
static void rpmzqCompressThread (void *_zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies _zq, fileSystem, internalState @*/
{
    rpmzQueue zq = _zq;
    rpmbz bz = rpmbzInit(zq->level, zq->omode);
    rpmzJob job;

zqFprint(stderr, "--> %s(%p) bz %p\n", __FUNCTION__, zq, bz);

    /* get job, insert write job in list in sorted order, alert write thread */
/*@-evalorder@*/
    while ((job = rpmzqDelCJob(zq)) != NULL) {
	rpmzqAddWJob(zq, rpmzqFillOut(zq, job, bz));
    }
/*@=evalorder@*/

    bz = rpmbzFini(bz);
}

static void rpmzqDecompressThread(void *_zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies _zq, fileSystem, internalState @*/
{
    rpmzQueue zq = _zq;
    rpmbz bz = rpmbzInit(zq->level, zq->omode);
    rpmzJob job;

zqFprint(stderr, "--> %s(%p) bz %p\n", __FUNCTION__, zq, bz);

    /* get job, insert write job in list in sorted order, alert write thread */
/*@-evalorder@*/
    while ((job = rpmzqDelCJob(zq)) != NULL) {
	rpmzqAddWJob(zq, rpmzqFillOut(zq, job, bz));
    }
/*@=evalorder@*/

    bz = rpmbzFini(bz);
}

/* start another compress/decompress thread if needed */
void rpmzqLaunch(rpmzQueue zq, long seq, unsigned int threads)
{
    if (zq->cthreads < seq && zq->cthreads < (int)threads) {
	switch (zq->omode) {
	default:	assert(0);	break;
	case O_WRONLY: (void)yarnLaunch(rpmzqCompressThread, zq);	break;
	case O_RDONLY: (void)yarnLaunch(rpmzqDecompressThread, zq);	break;
	}
	zq->cthreads++;
    }
}

/* verify no more jobs, prepare for next use */
void rpmzqVerify(rpmzQueue zq)
{
    yarnPossess(zq->compress_have);
assert(zq->compress_head == NULL && yarnPeekLock(zq->compress_have) == 0);
    yarnRelease(zq->compress_have);
    yarnPossess(zq->write_first);
assert(zq->write_head == NULL);
    yarnTwist(zq->write_first, TO, -1);
}
