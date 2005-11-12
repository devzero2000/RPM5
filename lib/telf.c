#include "system.h"

#if HAVE_GELF_H
#include <gelf.h>
#endif

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

typedef struct rpmMergePR_s {
    rpmds * Pdsp;
    rpmds * Rdsp;
} * rpmMergePR;

static rpmds P = NULL;
static rpmds R = NULL;

static struct rpmMergePR_s mergePR = { &P, &R };

/**
 * Merge single provides/requires dependency.
 * @param context	merge dependency set(s) container
 * @param ds		dependency set to merge
 * @return		0 always
 */
static int rpmdsMergePR(void * context, rpmds ds)
{
    rpmMergePR PR = context;
    int xx;

    switch(rpmdsTagN(ds)) {
    default:
	break;
    case RPMTAG_PROVIDENAME:
	if (PR->Pdsp != NULL)
	    xx = rpmdsMerge(PR->Pdsp, ds);
	break;
    case RPMTAG_REQUIRENAME:
	if (PR->Rdsp != NULL)
	    xx = rpmdsMerge(PR->Rdsp, ds);
	break;
    }
    return 0;
}

/**
 * Return a soname dependency constructed from an elf string.
 * @retval t		soname dependency
 * @param s		elf string
 * @param isElf64	is this an ELF64 symbol?
 * @		soname dependency
 */
static char * sonameDep(/*@returned@*/ char * t, const char * s, int isElf64)
	/*@modifies t @*/
{
    *t = '\0';
#if !defined(__alpha__)
    if (isElf64)
	(void) stpcpy( stpcpy(t, s), "()(64bit)");
    else
#endif
	(void) stpcpy(t, s);
    return t;
}

#define	RPMELF_FLAG_SKIPPROVIDES	0x1
#define	RPMELF_FLAG_SKIPREQUIRES	0x2

/**
 * Extract Elf dependencies from a file.
 * @param fn		file name
 * @param flags		1: skipP 2: skipR
 * @param *add		add(arg, ds) saves next provide/require dependency.
 * @param arg		add() arg
 * @return		0 on success
 */
static int rpmdsELF(const char * fn, int flags,
		int (*add) (void * context, rpmds ds), void * context)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
#if HAVE_GELF_H && HAVE_LIBELF
    Elf * elf;
    Elf_Scn * scn;
    Elf_Data * data;
    GElf_Ehdr ehdr_mem, * ehdr;
    GElf_Shdr shdr_mem, * shdr;
    GElf_Verdef def_mem, * def;
    GElf_Verneed need_mem, * need;
    GElf_Dyn dyn_mem, * dyn;
    unsigned int auxoffset;
    unsigned int offset;
    int fdno;
    int cnt2;
    int cnt;
    char buf[BUFSIZ];
    const char * s;
    int is_executable;
    const char * soname = NULL;
    rpmds ds;
    char * t;
    int xx;
    int isElf64;
    int isDSO;
    int gotSONAME = 0;
    int gotDEBUG = 0;
    int skipP = (flags & RPMELF_FLAG_SKIPPROVIDES);
    int skipR = (flags & RPMELF_FLAG_SKIPREQUIRES);
    static int filter_GLIBC_PRIVATE = 0;
    static int oneshot = 0;

    if (oneshot == 0) {
	oneshot = 1;
	filter_GLIBC_PRIVATE = rpmExpandNumeric("%{?_filter_GLIBC_PRIVATE}");
    }

    /* Extract dependencies only from files with executable bit set. */
    {	struct stat sb, * st = &sb;
	if (stat(fn, st) != 0)
	    return -1;
	is_executable = (st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));
    }

    fdno = open(fn, O_RDONLY);
    if (fdno < 0)
	return fdno;

    (void) elf_version(EV_CURRENT);

/*@-evalorder@*/
    elf = NULL;
    if ((elf = elf_begin (fdno, ELF_C_READ, NULL)) == NULL
     || elf_kind(elf) != ELF_K_ELF
     || (ehdr = gelf_getehdr(elf, &ehdr_mem)) == NULL
     || !(ehdr->e_type == ET_DYN || ehdr->e_type == ET_EXEC))
	goto exit;
/*@=evalorder@*/

    isElf64 = ehdr->e_ident[EI_CLASS] == ELFCLASS64;
    isDSO = ehdr->e_type == ET_DYN;

    /*@-branchstate -uniondef @*/
    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
	shdr = gelf_getshdr(scn, &shdr_mem);
	if (shdr == NULL)
	    break;

	soname = _free(soname);
	switch (shdr->sh_type) {
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case SHT_GNU_verdef:
	    data = NULL;
	    if (!skipP)
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		
		    def = gelf_getverdef (data, offset, &def_mem);
		    if (def == NULL)
			/*@innerbreak@*/ break;
		    auxoffset = offset + def->vd_aux;
		    for (cnt2 = def->vd_cnt; --cnt2 >= 0; ) {
			GElf_Verdaux aux_mem, * aux;

			aux = gelf_getverdaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    /*@innerbreak@*/ break;

			s = elf_strptr(elf, shdr->sh_link, aux->vda_name);
			if (s == NULL)
			    /*@innerbreak@*/ break;

			if (def->vd_flags & VER_FLG_BASE) {
			    soname = _free(soname);
			    soname = xstrdup(s);
			} else
			if (soname != NULL
			 && !(filter_GLIBC_PRIVATE != 0
				&& !strcmp(s, "GLIBC_PRIVATE")))
			{
			    buf[0] = '\0';
			    t = buf;
			    t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

			    t++;	/* XXX "foo(bar)" already in buf. */

			    /* Add next provide dependency. */
			    ds = rpmdsSingle(RPMTAG_PROVIDES,
					sonameDep(t, buf, isElf64),
					"", RPMSENSE_FIND_PROVIDES);
			    xx = add(context, ds);
			    ds = rpmdsFree(ds);
			}
			auxoffset += aux->vda_next;
		    }
		    offset += def->vd_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_GNU_verneed:
	    data = NULL;
	    /* Only from files with executable bit set. */
	    if (!skipR && is_executable)
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		    need = gelf_getverneed (data, offset, &need_mem);
		    if (need == NULL)
			/*@innerbreak@*/ break;

		    s = elf_strptr(elf, shdr->sh_link, need->vn_file);
		    if (s == NULL)
			/*@innerbreak@*/ break;
		    soname = _free(soname);
		    soname = xstrdup(s);
		    auxoffset = offset + need->vn_aux;
		    for (cnt2 = need->vn_cnt; --cnt2 >= 0; ) {
			GElf_Vernaux aux_mem, * aux;

			aux = gelf_getvernaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    /*@innerbreak@*/ break;

			s = elf_strptr(elf, shdr->sh_link, aux->vna_name);
			if (s == NULL)
			    /*@innerbreak@*/ break;

			/* Filter dependencies that contain GLIBC_PRIVATE */
			if (soname != NULL
			 && !(filter_GLIBC_PRIVATE != 0
				&& !strcmp(s, "GLIBC_PRIVATE")))
			{
			    buf[0] = '\0';
			    t = buf;
			    t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

			    t++;	/* XXX "foo(bar)" already in buf. */

			    /* Add next require dependency. */
			    ds = rpmdsSingle(RPMTAG_REQUIRENAME,
					sonameDep(t, buf, isElf64),
					"", RPMSENSE_FIND_REQUIRES);
			    xx = add(context, ds);
			    ds = rpmdsFree(ds);
			}
			auxoffset += aux->vna_next;
		    }
		    offset += need->vn_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_DYNAMIC:
	    data = NULL;
	    while ((data = elf_getdata (scn, data)) != NULL) {
/*@-boundswrite@*/
		for (cnt = 0; cnt < (shdr->sh_size / shdr->sh_entsize); ++cnt) {
		    dyn = gelf_getdyn (data, cnt, &dyn_mem);
		    if (dyn == NULL)
			/*@innerbreak@*/ break;
		    s = NULL;
		    switch (dyn->d_tag) {
		    default:
			/*@innercontinue@*/ continue;
			/*@notreached@*/ /*@switchbreak@*/ break;
                    case DT_DEBUG:    
			gotDEBUG = 1;
			/*@innercontinue@*/ continue;
		    case DT_NEEDED:
			/* Only from files with executable bit set. */
			if (skipR || !is_executable)
			    /*@innercontinue@*/ continue;
			/* Add next require dependency. */
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
assert(s != NULL);
			ds = rpmdsSingle(RPMTAG_REQUIRENAME,
				sonameDep(buf, s, isElf64),
				"", RPMSENSE_FIND_REQUIRES);
			xx = add(context, ds);
			ds = rpmdsFree(ds);
			/*@switchbreak@*/ break;
		    case DT_SONAME:
			gotSONAME = 1;
			if (skipP)
			    /*@innercontinue@*/ continue;
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
assert(s != NULL);
			/* Add next provide dependency. */
			ds = rpmdsSingle(RPMTAG_PROVIDENAME,
				sonameDep(buf, s, isElf64),
				"", RPMSENSE_FIND_PROVIDES);
			xx = add(context, ds);
			ds = rpmdsFree(ds);
			/*@switchbreak@*/ break;
		    }
		}
/*@=boundswrite@*/
	    }
	    /*@switchbreak@*/ break;
	}
    }
    /*@=branchstate =uniondef @*/

    /* For DSO's, provide the basename of the file if DT_SONAME not found. */
    if (!skipP && isDSO && !gotDEBUG && !gotSONAME) {
	s = strrchr(fn, '/');
	if (s)
	    s++;
	else
	    s = fn;

	/* Add next provide dependency. */
	ds = rpmdsSingle(RPMTAG_PROVIDENAME,
		sonameDep(buf, s, isElf64), "", RPMSENSE_FIND_PROVIDES);
	xx = add(context, ds);
	ds = rpmdsFree(ds);
    }

exit:
    soname = _free(soname);
    if (elf) (void) elf_end(elf);
    if (fdno > 0)
	xx = close(fdno);
    return 0;
#else
    return -1;
#endif
}

int main(int argc, char *argv[])
{
    int flags = 0;
    int rc = 0;
    int i;

    for (i = 1; i < argc; i++)
	rc = rpmdsELF(argv[i], flags, rpmdsMergePR, &mergePR);
	
    P = rpmdsInit(P);
    while (rpmdsNext(P) >= 0)
	fprintf(stderr, "%d Provides: %s\n", rpmdsIx(P), rpmdsDNEVR(P)+2);
    P = rpmdsFree(P);

    R = rpmdsInit(R);
    while (rpmdsNext(R) >= 0)
	fprintf(stderr, "%d Requires: %s\n", rpmdsIx(R), rpmdsDNEVR(R)+2);
    R = rpmdsFree(R);

    return rc;
}
