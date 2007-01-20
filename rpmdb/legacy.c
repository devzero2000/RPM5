/**
 * \file rpmdb/legacy.c
 */

#include "system.h"

#if HAVE_GELF_H
#if LIBELF_H_LFS_CONFLICT
/* some gelf.h/libelf.h implementations (Solaris) are
 * incompatible with the Large File API
 */
# undef _LARGEFILE64_SOURCE
# undef _LARGEFILE_SOURCE
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif
#include <gelf.h>

#if !defined(DT_GNU_PRELINKED)
#define	DT_GNU_PRELINKED	0x6ffffdf5
#endif
#if !defined(DT_GNU_LIBLIST)
#define	DT_GNU_LIBLIST		0x6ffffef9
#endif

#endif

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>
#include "misc.h"
#include "legacy.h"
#include "debug.h"

#define alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/**
 * Open a file descriptor to verify file MD5 and size.
 * @param path		file path
 * @retval pidp		prelink helper pid or 0
 * @retval fsizep	file size
 * @return		-1 on error, otherwise, an open file descriptor
 */ 
static int open_dso(const char * path, /*@null@*/ pid_t * pidp, /*@null@*/ size_t *fsizep)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *pidp, *fsizep, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
/*@only@*/
    static const char * cmd = NULL;
    static int initted = 0;
    int fdno;

    if (!initted) {
	cmd = rpmExpand("%{?__prelink_undo_cmd}", NULL);
	initted++;
    }

/*@-boundswrite@*/
    if (pidp) *pidp = 0;

    if (fsizep) {
	struct stat sb, * st = &sb;
	if (stat(path, st) < 0)
	    return -1;
	*fsizep = st->st_size;
    }
/*@=boundswrite@*/

    fdno = open(path, O_RDONLY);
    if (fdno < 0)
	return fdno;

/*@-boundsread@*/
    if (!(cmd && *cmd))
	return fdno;
/*@=boundsread@*/

#if HAVE_GELF_H && HAVE_LIBELF
 {  Elf *elf = NULL;
    Elf_Scn *scn = NULL;
    Elf_Data *data = NULL;
    GElf_Ehdr ehdr;
    GElf_Shdr shdr;
    GElf_Dyn dyn;
    int bingo;

    (void) elf_version(EV_CURRENT);

/*@-evalorder@*/
    if ((elf = elf_begin (fdno, ELF_C_READ, NULL)) == NULL
     || elf_kind(elf) != ELF_K_ELF
     || gelf_getehdr(elf, &ehdr) == NULL
     || !(ehdr.e_type == ET_DYN || ehdr.e_type == ET_EXEC))
	goto exit;
/*@=evalorder@*/

    bingo = 0;
    /*@-branchstate -uniondef @*/
    while (!bingo && (scn = elf_nextscn(elf, scn)) != NULL) {
	(void) gelf_getshdr(scn, &shdr);
	if (shdr.sh_type != SHT_DYNAMIC)
	    continue;
	while (!bingo && (data = elf_getdata (scn, data)) != NULL) {
	    int maxndx = data->d_size / shdr.sh_entsize;
	    int ndx;

            for (ndx = 0; ndx < maxndx; ++ndx) {
		(void) gelf_getdyn (data, ndx, &dyn);
		if (!(dyn.d_tag == DT_GNU_PRELINKED || dyn.d_tag == DT_GNU_LIBLIST))
		    /*@innercontinue@*/ continue;
		bingo = 1;
		/*@innerbreak@*/ break;
	    }
	}
    }
    /*@=branchstate =uniondef @*/

/*@-boundswrite@*/
    if (pidp != NULL && bingo) {
	int pipes[2];
	pid_t pid;
	int xx;

	xx = close(fdno);
	pipes[0] = pipes[1] = -1;
	xx = pipe(pipes);
	if (!(pid = fork())) {
	    const char ** av;
	    int ac;
	    xx = close(pipes[0]);
	    xx = dup2(pipes[1], STDOUT_FILENO);
	    xx = close(pipes[1]);
	    if (!poptParseArgvString(cmd, &ac, &av)) {
		av[ac-1] = path;
		av[ac] = NULL;
		unsetenv("MALLOC_CHECK_");
		xx = execve(av[0], (char *const *)av+1, environ);
	    }
	    _exit(127);
	}
	*pidp = pid;
	fdno = pipes[0];
	xx = close(pipes[1]);
    }
/*@=boundswrite@*/

exit:
    if (elf) (void) elf_end(elf);
 }
#endif

    return fdno;
}

int dodigest(int digestalgo, const char * fn, unsigned char * digest, int asAscii, size_t *fsizep)
{
    const char * path;
    urltype ut = urlPath(fn, &path);
    unsigned char * dsum = NULL;
    size_t dlen;
    unsigned char buf[32*BUFSIZ];
    FD_t fd;
    size_t fsize = 0;
    pid_t pid = 0;
    int use_mmap;
    int rc = 0;
    int fdno;
    int xx;

/*@-globs -internalglobs -mods @*/
    fdno = open_dso(path, &pid, &fsize);
/*@=globs =internalglobs =mods @*/
    if (fdno < 0) {
	rc = 1;
	goto exit;
    }

    /* XXX 128 Mb resource cap for top(1) scrutiny, MADV_SEQUENTIAL better. */
    use_mmap = (pid == 0 && fsize <= 0x07ffffff);

    switch(ut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
#if HAVE_MMAP
      if (use_mmap) {
	DIGEST_CTX ctx;
	void * mapped = NULL;

	if (fsize) {
	    mapped = mmap(NULL, fsize, PROT_READ, MAP_SHARED, fdno, 0);
	    if (mapped == (void *)-1) {
		xx = close(fdno);
		rc = 1;
		break;
	    }

#ifdef	MADV_SEQUENTIAL
	    xx = madvise(mapped, fsize, MADV_SEQUENTIAL);
#endif
	}

	ctx = rpmDigestInit(digestalgo, RPMDIGEST_NONE);
	if (fsize)
	    xx = rpmDigestUpdate(ctx, mapped, fsize);
	xx = rpmDigestFinal(ctx, (void **)&dsum, &dlen, asAscii);
	if (fsize)
	    xx = munmap(mapped, fsize);
	xx = close(fdno);
	break;
      }	/*@fallthrough@*/
#endif
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    case URL_IS_HKP:
    case URL_IS_DASH:
    default:
	/* Either use the pipe to prelink -y or open the URL. */
	fd = (pid != 0) ? fdDup(fdno) : Fopen(fn, "r");
	(void) close(fdno);
	if (fd == NULL || Ferror(fd)) {
	    rc = 1;
	    if (fd != NULL)
		(void) Fclose(fd);
	    break;
	}
	
	fdInitDigest(fd, digestalgo, 0);
	fsize = 0;
	while ((rc = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    fsize += rc;
	fdFiniDigest(fd, digestalgo, (void **)&dsum, &dlen, asAscii);
	if (Ferror(fd))
	    rc = 1;

	(void) Fclose(fd);
	break;
    }

    /* Reap the prelink -y helper. */
    if (pid) {
	int status;
	(void) waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
	    rc = 1;
    }

exit:
/*@-boundswrite@*/
    if (fsizep)
	*fsizep = fsize;
    if (!rc)
	memcpy(digest, dsum, dlen);
/*@=boundswrite@*/
    dsum = _free(dsum);

    return rc;
}

int domd5(const char * fn, unsigned char * digest, int asAscii, size_t *fsizep)
{
    return dodigest(PGPHASHALGO_MD5, fn, digest, asAscii, fsizep);
}

void rpmfiBuildFNames(Header h, rpmTag tagN,
	/*@out@*/ const char *** fnp, /*@out@*/ int * fcp)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char ** baseNames;
    const char ** dirNames;
    int * dirIndexes;
    int count;
    const char ** fileNames;
    int size;
    rpmTag dirNameTag = 0;
    rpmTag dirIndexesTag = 0;
    rpmTagType bnt, dnt;
    char * t;
    int i, xx;

    if (tagN == RPMTAG_BASENAMES) {
	dirNameTag = RPMTAG_DIRNAMES;
	dirIndexesTag = RPMTAG_DIRINDEXES;
    } else if (tagN == RPMTAG_ORIGBASENAMES) {
	dirNameTag = RPMTAG_ORIGDIRNAMES;
	dirIndexesTag = RPMTAG_ORIGDIRINDEXES;
    }

    if (!hge(h, tagN, &bnt, (void **) &baseNames, &count)) {
	if (fnp) *fnp = NULL;
	if (fcp) *fcp = 0;
	return;		/* no file list */
    }

    xx = hge(h, dirNameTag, &dnt, (void **) &dirNames, NULL);
    xx = hge(h, dirIndexesTag, NULL, (void **) &dirIndexes, &count);

    size = sizeof(*fileNames) * count;
    for (i = 0; i < count; i++) {
	const char * dn = NULL;
	(void) urlPath(dirNames[dirIndexes[i]], &dn);
	size += strlen(baseNames[i]) + strlen(dn) + 1;
    }

    fileNames = xmalloc(size);
    t = ((char *) fileNames) + (sizeof(*fileNames) * count);
    /*@-branchstate@*/
    for (i = 0; i < count; i++) {
	const char * dn = NULL;
	fileNames[i] = t;
	(void) urlPath(dirNames[dirIndexes[i]], &dn);
	t = stpcpy( stpcpy(t, dn), baseNames[i]);
	*t++ = '\0';
    }
    /*@=branchstate@*/
    baseNames = hfd(baseNames, bnt);
    dirNames = hfd(dirNames, dnt);

    /*@-branchstate@*/
    if (fnp)
	*fnp = fileNames;
    else
	fileNames = _free(fileNames);
    /*@=branchstate@*/
    if (fcp) *fcp = count;
}
