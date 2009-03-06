#include "system.h"

#include <rpmiotypes.h>

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

/*==============================================================*/

/*@-mustmod@*/
int rpmbzCompressBlock(void * _bz, rpmzJob job)
	/*@globals fileSystem @*/
	/*@modifies job, fileSystem @*/
{
    rpmbz bz = _bz;
    int rc;
    rc = BZ2_bzBuffToBuffCompress(job->out->buf, &job->out->len,
		job->in->buf, job->in->len, bz->B, bz->V, bz->W);
    if (rc != BZ_OK)
	fprintf(stderr, "==> %s(%p,%p) rc %d\n", __FUNCTION__, bz, job, rc);
    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int rpmbzDecompressBlock(rpmbz bz, rpmzJob job)
	/*@globals fileSystem @*/
	/*@modifies job, fileSystem @*/
{
    int rc;
    rc = BZ2_bzBuffToBuffDecompress(job->out->buf, &job->out->len,
		job->in->buf, job->in->len, bz->S, bz->V);
    if (rc != BZ_OK)
	fprintf(stderr, "==> %s(%p,%p) rc %d\n", __FUNCTION__, bz, job, rc);
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

#if !defined(_RPMZQ_WHITEOUT)
/* initialize a pool (pool structure itself provided, not allocated) -- the
   limit is the maximum number of spaces in the pool, or -1 to indicate no
   limit, i.e., to never wait for a buffer to return to the pool */
rpmzPool rpmzNewPool(size_t size, int limit)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmzPool pool = xcalloc(1, sizeof(*pool));
    pool->have = yarnNewLock(0);
/*@=mustfreeonly@*/
    pool->head = NULL;
/*@=mustfreeonly@*/
    pool->size = size;
    pool->limit = limit;
    pool->made = 0;
fprintf(stderr, "    ++ pool %p[%u,%d]\n", pool, (unsigned)size, limit);
    return pool;
}

/* get a space from a pool -- the use count is initially set to one, so there
   is no need to call rpmzUseSpace() for the first use */
rpmzSpace rpmzNewSpace(rpmzPool pool)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, fileSystem, internalState @*/
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
fprintf(stderr, "    ++ space %p use %d buf %p[%u]\n", space, 1, space->buf, space->len);
/*@-nullret@*/
    return space;
/*@=nullret@*/
}

/* increment the use count to require one more drop before returning this space
   to the pool */
void rpmzUseSpace(rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/
{
    int use;
    yarnPossess(space->use);
    use = yarnPeekLock(space->use);
fprintf(stderr, "    ++ space %p[%d] buf %p[%u]\n", space, use+1, space->buf, space->len);
    yarnTwist(space->use, BY, 1);
}

/* drop a space, returning it to the pool if the use count is zero */
/*@null@*/
rpmzSpace rpmzDropSpace(/*@only@*/ rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/
{
    int use;

    yarnPossess(space->use);
    use = yarnPeekLock(space->use);
fprintf(stderr, "    -- space %p[%u] buf %p[%u]\n", space, use, space->buf, space->len);
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
/*@null@*/
rpmzPool rpmzFreePool(/*@only@*/ rpmzPool pool, /*@null@*/ int *countp)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, *countp, fileSystem, internalState @*/
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
fprintf(stderr, "==> FIXME: %s: count %d pool->made %d\n", __FUNCTION__, count, pool->made);
#endif
fprintf(stderr, "    -- pool %p count %d\n", pool, count);
/*@-compdestroy@*/
    pool = _free(pool);
/*@=compdestroy@*/
    if (countp != NULL)
	*countp = count;
    return NULL;
}
#endif	/* _RPMZQ_WHITEOUT */

/*@null@*/
rpmzJob rpmzFreeJob(/*@only@*/ rpmzJob job)
        /*@globals fileSystem, internalState @*/
        /*@modifies job, fileSystem, internalState @*/
{
fprintf(stderr, "    -- job %p[%ld] %p => %p\n", job, job->seq, job->in, job->out);
    if (job->calc != NULL)
	job->calc = yarnFreeLock(job->calc);
    job = _free(job);
    return NULL;
}

/*@only@*/
rpmzJob rpmzNewJob(long seq)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/
{
    rpmzJob job = xcalloc(1, sizeof(*job));
    job->seq = seq;
    job->calc = yarnNewLock(0);
fprintf(stderr, "    ++ job %p[%ld]\n", job, seq);
    return job;
}

/* compress or write job (passed from compress list to write list) -- if seq is
   equal to -1, rpmzqCompressThread() is instructed to return; if more is false then
   this is the last chunk, which after writing tells write_thread to return */

/* command the compress threads to all return, then join them all (call from
   main thread), free all the thread-related resources */
void rpmzqFini(rpmzQueue zq)
        /*@globals fileSystem, internalState @*/
        /*@modifies zq, fileSystem, internalState @*/
{
    rpmzLog zlog = zq->zlog;

    struct rpmzJob_s job;
    int caught;

fprintf(stderr, "--> %s(%p)\n", __FUNCTION__, zq);
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
fprintf(stderr, "==> FIXME: %s: caught %d z->cthreads %d\n", __FUNCTION__, caught, zq->cthreads);
#endif
    zq->cthreads = 0;

    /* free the resources */
    zq->out_pool = rpmzFreePool(zq->out_pool, &caught);
    Trace((zlog, "-- freed %d output buffers", caught));
    zq->in_pool = rpmzFreePool(zq->in_pool, &caught);
    Trace((zlog, "-- freed %d input buffers", caught));
    zq->write_first = yarnFreeLock(zq->write_first);
    zq->compress_have = yarnFreeLock(zq->compress_have);
}

/* setup job lists (call from main thread) */
void rpmzqInit(rpmzQueue zq)
        /*@globals fileSystem, internalState @*/
        /*@modifies zq, fileSystem, internalState @*/
{
fprintf(stderr, "--> %s(%p)\n", __FUNCTION__, zq);
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

    zq->in_pool = rpmzNewPool(zq->iblocksize, zq->ilimit);
fprintf(stderr, "-->  in_pool: %p[%u] blocksize %u\n", zq->in_pool, (unsigned)zq->ilimit, (unsigned)zq->iblocksize);
    zq->out_pool = rpmzNewPool(zq->oblocksize, zq->olimit);
fprintf(stderr, "--> out_pool: %p[%u] blocksize %u\n", zq->out_pool, (unsigned)zq->olimit, (unsigned)zq->oblocksize);

}

/*@null@*/
rpmzQueue rpmzqFree(/*@only@*/ rpmzQueue zq)
        /*@modifies zq @*/
{
    zq = _free(zq);
    return NULL;
}

/*@only@*/
rpmzQueue rpmzqNew(rpmzLog zlog, int flags,
		int verbosity, unsigned int level, size_t blocksize, int limit)
	/*@*/
{
    rpmzQueue zq = xcalloc(1, sizeof(*zq));
    zq->flags = flags;
    zq->ifn = NULL;
    zq->ifdno = -1;
    zq->ofn = NULL;
    zq->ofdno = -1;
    zq->verbosity = verbosity;
    zq->level = level;
    zq->iblocksize = blocksize;
    zq->ilimit = limit;
    zq->oblocksize = blocksize;
    zq->olimit = limit;
/*@-assignexpose@*/
    zq->zlog = zlog;
/*@=assignexpose@*/
    return zq;
}

/* forward reference */
static void rpmzqCompressThread (void *_zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies _zq, fileSystem, internalState @*/;

/* forward reference */
static void rpmzqDecompressThread (void *_zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies _zq, fileSystem, internalState @*/;

/* start another compress/decompress thread if needed */
void rpmzqLaunch(rpmzQueue zq, long seq, unsigned int threads)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/
{
    if (zq->cthreads < seq && zq->cthreads < (int)threads) {
	switch (zq->omode) {
	case O_WRONLY: (void)yarnLaunch(rpmzqCompressThread, zq);	break;
	case O_RDONLY: (void)yarnLaunch(rpmzqDecompressThread, zq);	break;
	default:	assert(0);	break;
	}
	zq->cthreads++;
    }
}

/* verify no more jobs, prepare for next use */
void rpmzqVerify(rpmzQueue zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/
{
    yarnPossess(zq->compress_have);
assert(zq->compress_head == NULL && yarnPeekLock(zq->compress_have) == 0);
    yarnRelease(zq->compress_have);
    yarnPossess(zq->write_first);
assert(zq->write_head == NULL);
    yarnTwist(zq->write_first, TO, -1);
}

/*@null@*/
rpmzJob rpmzqDelCJob(rpmzQueue zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/
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
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, job, fileSystem, internalState @*/
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
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/
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
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, job, fileSystem, internalState @*/
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
	fprintf(stderr, "       job %p[%ld]:\t%p[%u] => %p[%u]\t(%3.1f%%)\n",
			job, job->seq, job->in->buf, job->in->len,
			job->out->buf, job->out->len, pct);
	Trace((zlog, "-- compressed #%ld %3.1f%%%s", job->seq, pct,
		(job->more ? "" : " (last)")));
	break;
    case O_RDONLY:
	pct = (100.0 * job->in->len) / job->out->len;
	fprintf(stderr, "       job %p[%ld]:\t%p[%u] <= %p[%u]\t(%3.1f%%)\n",
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

static rpmzJob  rpmzqFillOut(rpmzQueue zq, /*@returned@*/rpmzJob job, rpmbz bz)
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
	job->out = rpmzNewSpace(zq->out_pool);
/*@=mustfreeonly@*/
	if (job->out->len < outlen) {
fprintf(stderr, "==> FIXME: %s: job->out %p %p[%u] malloc(%u)\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len, (unsigned)outlen);
	    job->out = rpmzDropSpace(job->out);
	    job->out = xcalloc(1, sizeof(*job->out));
	    job->out->len = outlen;
	    job->out->buf = xmalloc(job->out->len);
	}

	/* compress job->in to job-out */
	ret = rpmbzCompressBlock(bz, job);
	break;
    case O_RDONLY:
	outlen = 6 * job->in->len;
	job->out = rpmzNewSpace(zq->out_pool);
	if (job->out->len < outlen) {
fprintf(stderr, "==> FIXME: %s: job->out %p %p[%u] malloc(%u)\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len, (unsigned)outlen);
	    job->out = rpmzDropSpace(job->out);
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
fprintf(stderr, "==> FIXME: %s: job->out %p %p[%u] realloc(%u)\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len, (unsigned)outlen);
	    if (job->out->use != NULL)
		job->out = rpmzDropSpace(job->out);
	    else {
fprintf(stderr, "==> FIXME: %s: job->out %p %p[%u] free\n", __FUNCTION__, job->out, job->out->buf, (unsigned)job->out->len);
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
{
    rpmzQueue zq = _zq;
    rpmbz bz = rpmbzInit(zq->level, zq->omode);
    rpmzJob job;

fprintf(stderr, "--> %s(%p) bz %p\n", __FUNCTION__, zq, bz);

    /* get job, insert write job in list in sorted order, alert write thread */
/*@-evalorder@*/
    while ((job = rpmzqDelCJob(zq)) != NULL) {
	rpmzqAddWJob(zq, rpmzqFillOut(zq, job, bz));
    }
/*@=evalorder@*/

    bz = rpmbzFini(bz);
}

static void rpmzqDecompressThread(void *_zq)
{
    rpmzQueue zq = _zq;
    rpmbz bz = rpmbzInit(zq->level, zq->omode);
    rpmzJob job;

fprintf(stderr, "--> %s(%p) bz %p\n", __FUNCTION__, zq, bz);

    /* get job, insert write job in list in sorted order, alert write thread */
/*@-evalorder@*/
    while ((job = rpmzqDelCJob(zq)) != NULL) {
	rpmzqAddWJob(zq, rpmzqFillOut(zq, job, bz));
    }
/*@=evalorder@*/

    bz = rpmbzFini(bz);
}
