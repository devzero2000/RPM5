/**
 * \file rpmdb/fprint.c
 */

#include "system.h"

#include <rpmiotypes.h>	/* XXX rpmRC codes. */
#include <rpmio.h>	/* XXX Realpath(). */
#include <rpmmacro.h>	/* XXX for rpmCleanPath */

#include <rpmtag.h>
#include <rpmdb.h>

#define	_FPRINT_INTERNAL
#include "fprint.h"

#include "debug.h"

/*@access hashTable @*/

fingerPrintCache fpCacheCreate(int sizeHint)
{
    fingerPrintCache fpc;

    fpc = xmalloc(sizeof(*fpc));
    fpc->ht = htCreate(sizeHint * 2, 0, 1, NULL, NULL);
assert(fpc->ht != NULL);
    return fpc;
}

fingerPrintCache fpCacheFree(fingerPrintCache cache)
{
    cache->ht = htFree(cache->ht);
    free(cache);
    return NULL;
}

/**
 * Find directory name entry in cache.
 * @param cache		pointer to fingerprint cache
 * @param dirName	string to locate in cache
 * @return pointer to directory name entry (or NULL if not found).
 */
static /*@null@*/ const struct fprintCacheEntry_s * cacheContainsDirectory(
			    fingerPrintCache cache,
			    const char * dirName)
	/*@*/
{
    const void ** data;

    if (htGetEntry(cache->ht, dirName, &data, NULL, NULL))
	return NULL;
    return data[0];
}

/**
 * Return finger print of a file path.
 * @param cache		pointer to fingerprint cache
 * @param dirName	leading directory name of path
 * @param baseName	file name of path
 * @param scareMem
 * @return pointer to the finger print associated with a file path.
 */
static fingerPrint doLookup(fingerPrintCache cache,
		const char * dirName, const char * baseName, int scareMem)
	/*@globals fileSystem, internalState @*/
	/*@modifies cache, fileSystem, internalState @*/
{
    char dir[PATH_MAX];
    const char * cleanDirName;
    size_t cdnl;
    char * end;		    /* points to the '\0' at the end of "buf" */
    fingerPrint fp;
    struct stat sb;
    char * buf;
    const struct fprintCacheEntry_s * cacheHit;

    /* assert(*dirName == '/' || !scareMem); */

    /* XXX WATCHOUT: fp.subDir is set below from relocated dirName arg */
    cleanDirName = dirName;
    cdnl = strlen(cleanDirName);

    if (*cleanDirName == '/') {
	if (!scareMem)
	    cleanDirName =
		rpmCleanPath(strcpy(alloca(cdnl+1), dirName));
    } else {
	scareMem = 0;	/* XXX causes memory leak */

	/* Using realpath on the arg isn't correct if the arg is a symlink,
	 * especially if the symlink is a dangling link.  What we 
	 * do instead is use realpath() on `.' and then append arg to
	 * the result.
	 */

	/* if the current directory doesn't exist, we might fail. 
	   oh well. likewise if it's too long.  */
	dir[0] = '\0';
	if (Realpath(".", dir) != NULL) {
	    end = dir + strlen(dir);
	    if (end[-1] != '/')	*end++ = '/';
	    end = stpncpy(end, cleanDirName, sizeof(dir) - (end - dir));
	    *end = '\0';
	    (void)rpmCleanPath(dir); /* XXX possible /../ from concatenation */
	    end = dir + strlen(dir);
	    if (end[-1] != '/')	*end++ = '/';
	    *end = '\0';
	    cleanDirName = dir;
	    cdnl = end - dir;
	}
    }
    fp.entry = NULL;
    fp.subDir = NULL;
    fp.baseName = NULL;
    /*@-nullret@*/
    if (cleanDirName == NULL) return fp;	/* XXX can't happen */
    /*@=nullret@*/

    buf = strcpy(alloca(cdnl + 1), cleanDirName);
    end = buf + cdnl;

    /* no need to pay attention to that extra little / at the end of dirName */
    if (buf[1] && end[-1] == '/') {
	end--;
	*end = '\0';
    }

    while (1) {

	/* as we're stating paths here, we want to follow symlinks */

	cacheHit = cacheContainsDirectory(cache, (*buf != '\0' ? buf : "/"));
	if (cacheHit != NULL) {
	    fp.entry = cacheHit;
	} else if (!stat((*buf != '\0' ? buf : "/"), &sb)) {
	    size_t nb = sizeof(*fp.entry) + (*buf != '\0' ? (end-buf) : 1) + 1;
	    char * dn = xmalloc(nb);
	    struct fprintCacheEntry_s * newEntry = (void *)dn;

	    /*@-usereleased@*/	/* LCL: contiguous malloc confusion */
	    dn += sizeof(*newEntry);
	    strcpy(dn, (*buf != '\0' ? buf : "/"));
	    newEntry->ino = (ino_t)sb.st_ino;
	    newEntry->dev = (dev_t)sb.st_dev;
	    newEntry->dirName = dn;
	    fp.entry = newEntry;

	    /*@-kepttrans -dependenttrans @*/
	    htAddEntry(cache->ht, dn, fp.entry);
	    /*@=kepttrans =dependenttrans @*/
	    /*@=usereleased@*/
	}

        if (fp.entry) {
	    fp.subDir = cleanDirName + (end - buf);
	    if (fp.subDir[0] == '/' && fp.subDir[1] != '\0')
		fp.subDir++;
	    if (fp.subDir[0] == '\0' ||
	    /* XXX don't bother saving '/' as subdir */
	      (fp.subDir[0] == '/' && fp.subDir[1] == '\0'))
		fp.subDir = NULL;
	    fp.baseName = baseName;
	    if (!scareMem && fp.subDir != NULL)
		fp.subDir = xstrdup(fp.subDir);
	/*@-compdef@*/ /* FIX: fp.entry.{dirName,dev,ino} undef @*/
	    return fp;
	/*@=compdef@*/
	}

        /* stat of '/' just failed! */
	if (end == buf + 1)
	    abort();

	end--;
	while ((end > buf) && *end != '/') end--;
	if (end == buf)	    /* back to stat'ing just '/' */
	    end++;

	*end = '\0';
    }

    /*@notreached@*/

    /*@-compdef@*/ /* FIX: fp.entry.{dirName,dev,ino} undef @*/
    /*@-nullret@*/ return fp; /*@=nullret@*/	/* LCL: can't happen. */
    /*@=compdef@*/
}

fingerPrint fpLookup(fingerPrintCache cache, const char * dirName, 
			const char * baseName, int scareMem)
{
    return doLookup(cache, dirName, baseName, scareMem);
}

rpmuint32_t fpHashFunction(rpmuint32_t h, const void * data,
		/*@unused@*/ size_t size)
{
    const fingerPrint * fp = data;
    const char * chptr = fp->baseName;
    unsigned char ch = '\0';

    while (*chptr != '\0') ch ^= *chptr++;

    h |= ((unsigned)ch) << 24;
    h |= (((((unsigned)fp->entry->dev) >> 8) ^ fp->entry->dev) & 0xFF) << 16;
    h |= fp->entry->ino & 0xFFFF;
    
    return h;
}

int fpEqual(const void * key1, const void * key2)
{
    const fingerPrint *k1 = key1;
    const fingerPrint *k2 = key2;

    /* If the addresses are the same, so are the values. */
    if (k1 == k2)
	return 0;

    /* Otherwise, compare fingerprints by value. */
    /*@-nullpass@*/	/* LCL: whines about (*k2).subdir */
    if (FP_EQUAL(*k1, *k2))
	return 0;
    /*@=nullpass@*/
    return 1;

}

void fpLookupList(fingerPrintCache cache, const char ** dirNames, 
		  const char ** baseNames, const rpmuint32_t * dirIndexes, 
		  rpmuint32_t fileCount, fingerPrint * fpList)
{
    unsigned i;

    for (i = 0; i < (unsigned) fileCount; i++) {
	/* If this is in the same directory as the last file, don't bother
	   redoing all of this work */
	if (i > 0 && dirIndexes[i - 1] == dirIndexes[i]) {
	    fpList[i].entry = fpList[i - 1].entry;
	    fpList[i].subDir = fpList[i - 1].subDir;
	    fpList[i].baseName = baseNames[i];
	} else {
	    fpList[i] = doLookup(cache, dirNames[dirIndexes[i]], baseNames[i],
				 1);
	}
    }
}

#ifdef	NOTUSED
/**
 * Return finger prints of all file names in header.
 * @warning: scareMem is assumed!
 * @param cache		pointer to fingerprint cache
 * @param h		package header
 * @retval fpList	pointer to array of finger prints
 */
static
void fpLookupHeader(fingerPrintCache cache, Header h, fingerPrint * fpList)
	/*@modifies h, cache, *fpList @*/
{
    rpmTagData he_p = { .ptr = NULL };
    HE_s he_s = { .tag = 0, .t = 0, .p = &he_p, .c = 0, .freeData = 0 };
    HE_t he = &he_s;
    const char ** baseNames;
    const char ** dirNames;
    rpmuint32_t * dirIndexes;
    rpmTagCount fileCount;
    int xx;

    he->tag = RPMTAG_BASENAMES;
    xx = headerGet(h, he->tag, &he->t, he->p, &he->c);
    baseNames = he_p.argv;
    fileCount = he->c;
    if (!xx)
	return;

    he->tag = RPMTAG_DIRNAMES;
    xx = headerGet(h, he, 0);
    dirNames = he_p.argv;
    he->tag = RPMTAG_DIRINDEXES;
    xx = headerGet(h, he, 0);
    dirIndexes = he_p.ui32p;

    fpLookupList(cache, dirNames, baseNames, dirIndexes, fileCount, fpList);

    dirIndexes = _free(dirIndexes);
    dirNames = _free(dirNames);
    baseNames = _free(baseNames);
}
#endif

#define	_RPMFI_INTERNAL
#include "rpmfi.h"
#define	_RPMTE_INTERNAL
#include "rpmte.h"

static inline /*@null@*/
struct fingerPrint_s * __rpmfiFpsIndex(rpmfi fi, int ix)
	/*@*/
{
    struct fingerPrint_s * fps = NULL;
    if (fi != NULL && fi->fps != NULL && ix >= 0 && ix < (int)fi->fc) {
	fps = fi->fps + ix;
    }
    return fps;
}

static inline
int __rpmfiFX(rpmfi fi)
	/*@*/
{
    return (fi != NULL ? fi->i : -1);
}

static inline
int __rpmfiSetFX(rpmfi fi, int fx)
	/*@*/
{
    int i = -1;

    if (fi != NULL && fx >= 0 && fx < (int)fi->fc) {
	i = fi->i;
	fi->i = fx;
	fi->j = fi->dil[fi->i];
    }
    return i;
}

static inline
const char * __rpmfiFLink(rpmfi fi)
	/*@*/
{
    const char * flink = NULL;

    if (fi != NULL && fi->i >= 0 && fi->i < (int)fi->fc) {
	if (fi->flinks != NULL)
	    flink = fi->flinks[fi->i];
    }
    return flink;
}

static inline
rpmfi __rpmteFI(rpmte te, rpmTag tag)
	/*@*/
{
    /*@-compdef -refcounttrans -retalias -retexpose -usereleased @*/
    if (te == NULL)
	return NULL;

    if (tag == RPMTAG_BASENAMES)
	return te->fi;
    else
	return NULL;
    /*@=compdef =refcounttrans =retalias =retexpose =usereleased @*/
}

/*
 * Concatenate strings with dynamically (re)allocated
 * memory what prevents static buffer overflows by design.
 * *dest is reallocated to the size of strings to concatenate.
 * List of strings has to be NULL terminated.
 *
 * Note:
 * 1) char *buf = rstrscat(NULL,"string",NULL); is the same like rstrscat(&buf,"string",NULL);
 * 2) rstrscat(&buf,NULL) returns buf
 * 3) rstrscat(NULL,NULL) returns NULL
 * 4) *dest and argument strings can overlap
 */
static
char * rstrscat(char **dest, const char *arg, ...)
	/*@modifies *dest @*/
{
    va_list ap;
    size_t arg_size, dst_size;
    const char *s;
    char *dst, *p;

    dst = dest ? *dest : NULL;

    if ( arg == NULL ) {
        return dst;
    }

    va_start(ap, arg);
    for (arg_size=0, s=arg; s; s = va_arg(ap, const char *))
        arg_size += strlen(s);
    va_end(ap);

    dst_size = dst ? strlen(dst) : 0;
    dst = xrealloc(dst, dst_size+arg_size+1);    /* include '\0' */
    p = &dst[dst_size];

    va_start(ap, arg);
    for (s = arg; s; s = va_arg(ap, const char *)) {
        size_t size = strlen(s);
        memmove(p, s, size);
        p += size;
    }
    *p = '\0';

    if ( dest ) {
        *dest = dst;
    }

    return dst;
}

void fpLookupSubdir(hashTable symlinks, hashTable fphash, fingerPrintCache fpc,
		void * _p, int filenr)
{
    rpmte p = _p;
    rpmfi fi = __rpmteFI(p, RPMTAG_BASENAMES);
    struct fingerPrint_s current_fp;
    char * endsubdir;
    char * endbasename;
    char * currentsubdir;
    size_t lensubDir;

    struct rpmffi_s * recs;
    int numRecs;
    int i, fiFX;
    fingerPrint * fp = __rpmfiFpsIndex(fi, filenr);
    int symlinkcount = 0;

    struct rpmffi_s * ffi = xmalloc(sizeof(*ffi));
    ffi->p = p;
    ffi->fileno = filenr;

    if (fp->subDir == NULL) {
	goto exit;
    }

    lensubDir = strlen(fp->subDir);
    current_fp = *fp;
    currentsubdir = xstrdup(fp->subDir);

    /* Set baseName to the upper most dir */
    current_fp.baseName = endbasename = currentsubdir;
    while (*endbasename != '/' && endbasename < currentsubdir + lensubDir - 1)
	endbasename++;
    *endbasename = '\0';

    current_fp.subDir = endsubdir = NULL; // no subDir for now

    while (endbasename < currentsubdir + lensubDir - 1) {
	int found;
	found = 0;

	recs = NULL;
	numRecs = 0;
	(void) htGetEntry(symlinks, &current_fp, &recs, &numRecs, NULL);

	for (i = 0; i < numRecs; i++) {
	    rpmfi foundfi;
	    int filenr;
	    char const *linktarget;
	    char *link;

	    foundfi =  __rpmteFI(recs[i].p, RPMTAG_BASENAMES);
	    fiFX = __rpmfiFX(foundfi);

	    filenr = recs[i].fileno;
	    (void) __rpmfiSetFX(foundfi, filenr);
	    linktarget = __rpmfiFLink(foundfi);

	    if (linktarget && *linktarget != '\0') {
		/* this "directory" is a symlink */
		link = NULL;
		if (*linktarget != '/') {
		    (void) rstrscat(&link, current_fp.entry->dirName,
				 current_fp.subDir ? "/" : "",
				 current_fp.subDir ? current_fp.subDir : "",
				 "/", NULL);
		}
		(void) rstrscat(&link, linktarget, "/", NULL);
		if (strlen(endbasename+1)) {
		    (void) rstrscat(&link, endbasename+1, "/", NULL);
		}

		*fp = fpLookup(fpc, link, fp->baseName, 0);

		free(link);
		free(currentsubdir);
		symlinkcount++;

		/* setup current_fp for the new path */
		found = 1;
		current_fp = *fp;
		if (fp->subDir == NULL) {
		    /* directory exists - no need to look for symlinks */
		    goto exit;
		}
		lensubDir = strlen(fp->subDir);
		currentsubdir = xstrdup(fp->subDir);
		current_fp.subDir = endsubdir = NULL; // no subDir for now

		/* Set baseName to the upper most dir */
		current_fp.baseName = currentsubdir;
		endbasename = currentsubdir;
		while (*endbasename != '/'
		 && endbasename < currentsubdir + lensubDir - 1)
		    endbasename++;
		*endbasename = '\0';
		/*@innerbreak@*/ break;

	    }
	    (void) __rpmfiSetFX(foundfi, fiFX);
	}

	if (symlinkcount > 50) {
	    // found too many symlinks in the path
	    // most likley a symlink cicle
	    // giving up
	    // TODO warning/error
	    break;
	}

	if (found) {
	    continue; // restart loop after symlink
	}

	if (current_fp.subDir == NULL) {
            /* after first round set former baseName as subDir */
	    current_fp.subDir = currentsubdir;
	} else {
	    *endsubdir = '/'; // rejoin the former baseName with subDir
	}
	endsubdir = endbasename;

	/* set baseName to the next lower dir */
	endbasename++;
	while (*endbasename != '\0' && *endbasename != '/')
	    endbasename++;
	*endbasename = '\0';
	current_fp.baseName = endsubdir + 1;

    }
    free(currentsubdir);
exit:
    htAddEntry(fphash, fp, ffi);
    return;

}
