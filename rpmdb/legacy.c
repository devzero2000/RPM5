/**
 * \file rpmdb/legacy.c
 */

#include "system.h"

#if defined(HAVE_GELF_H)
#if LIBELF_H_LFS_CONFLICT
/* some gelf.h/libelf.h implementations (Solaris) are
 * incompatible with the Large File API
 */
# undef _LARGEFILE64_SOURCE
# undef _LARGEFILE_SOURCE
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
#endif
#if defined(__LCLINT__)
/*@-incondefs@*/
typedef long long loff_t;
/*@=incondefs@*/
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
/*@-compdef -moduncon -noeffectuncon @*/
static int open_dso(const char * path, /*@null@*/ pid_t * pidp, /*@null@*/ size_t *fsizep)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *pidp, *fsizep, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
/*@only@*/
    static const char * cmd = NULL;
    static int oneshot = 0;
    int fdno;

    if (!oneshot) {
	cmd = rpmExpand("%{?__prelink_undo_cmd}", NULL);
	oneshot++;
    }

    if (pidp) *pidp = 0;

    if (fsizep) {
	struct stat sb, * st = &sb;
	if (stat(path, st) < 0)
	    return -1;
	*fsizep = (size_t)st->st_size;
    }

    fdno = open(path, O_RDONLY);
    if (fdno < 0)
	return fdno;

    if (!(cmd && *cmd))
	return fdno;

#if defined(HAVE_GELF_H) && defined(HAVE_LIBELF)
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
    while (!bingo && (scn = elf_nextscn(elf, scn)) != NULL) {
	(void) gelf_getshdr(scn, &shdr);
	if (shdr.sh_type != SHT_DYNAMIC)
	    continue;
	while (!bingo && (data = elf_getdata (scn, data)) != NULL) {
	    unsigned maxndx = (unsigned) (data->d_size / shdr.sh_entsize);
	    unsigned ndx;

            for (ndx = 0; ndx < maxndx; ++ndx) {
/*@-uniondef@*/
		(void) gelf_getdyn (data, ndx, &dyn);
/*@=uniondef@*/
		if (!(dyn.d_tag == DT_GNU_PRELINKED || dyn.d_tag == DT_GNU_LIBLIST))
		    /*@innercontinue@*/ continue;
		bingo = 1;
		/*@innerbreak@*/ break;
	    }
	}
    }

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

exit:
    if (elf) (void) elf_end(elf);
 }
#endif

    return fdno;
}
/*@=compdef =moduncon =noeffectuncon @*/

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
#if defined(HAVE_MMAP)
    int xx;
#endif

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
#if defined(HAVE_MMAP)
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

#if defined(HAVE_MADVISE) && defined(MADV_SEQUENTIAL)
	    xx = madvise(mapped, fsize, MADV_SEQUENTIAL);
#endif
	}

	ctx = rpmDigestInit(digestalgo, RPMDIGEST_NONE);
	if (fsize)
	    xx = rpmDigestUpdate(ctx, mapped, fsize);
	xx = rpmDigestFinal(ctx, &dsum, &dlen, asAscii);
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
	fd = (pid != 0) ? fdDup(fdno) : Fopen(fn, "r.fdio");
	(void) close(fdno);
	if (fd == NULL || Ferror(fd)) {
	    rc = 1;
	    if (fd != NULL)
		(void) Fclose(fd);
	    break;
	}
	
	fdInitDigest(fd, digestalgo, 0);
	fsize = 0;
	while ((rc = (int) Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    fsize += rc;
	fdFiniDigest(fd, digestalgo, &dsum, &dlen, asAscii);
	if (Ferror(fd))
	    rc = 1;

	(void) Fclose(fd);
	break;
    }

    /* Reap the prelink -y helper. */
    if (pid) {
	int status;
/*@+longunsignedintegral@*/
	(void) waitpid(pid, &status, 0);
/*@=longunsignedintegral@*/
	if (!WIFEXITED(status) || WEXITSTATUS(status))
	    rc = 1;
    }

exit:
    if (fsizep)
	*fsizep = fsize;
    if (!rc)
	memcpy(digest, dsum, dlen);
    dsum = _free(dsum);

    return rc;
}
