#define _RPMGI_INTERNAL
#define _RPMBF_INTERNAL
/* avoid warning */
#define	xrealloc(ptr, size) realloc(ptr, size) 

#include <stdint.h>
#include <rpmio.h>
#include <rpmtag.h>
#include <rpmgi.h>
#include <rpmbf.h>

int main(int argc, char *argv[]) {
    FD_t fd;
    rpmts ts = NULL;
    rpmgi gi = NULL;
    rpmRC rc = RPMRC_NOTFOUND;
    rpmbf bf = NULL;
    size_t n = 0;
    static double e = 1.0e-6;
    size_t m = 0;
    size_t k = 0;
    HE_t he = (HE_t)memset(alloca(sizeof(*he)), 0, sizeof(*he));



    rc = RPMRC_NOTFOUND;
    ts = rpmtsCreate();
    rpmtsSetRootDir(ts, NULL);
    gi = rpmgiNew(ts, RPMDBI_HDLIST, NULL, 0);

    rpmtsSetVSFlags(ts, _RPMVSF_NOSIGNATURES | RPMVSF_NOHDRCHK | _RPMVSF_NOPAYLOAD | _RPMVSF_NOHEADER);
    gi->active = 1;
    gi->fd = Fopen(argv[1], "r.fdio");
    while ((rc = rpmgiNext(gi)) == RPMRC_OK) {
	int rc;
	
	he->tag = RPMTAG_FILEPATHS;
	if(headerGet(gi->h, he, 0)) {
	    n += he->c;
	}
    }

    rpmbfParams(n, e, &m, &k);

    bf = rpmbfNew(m, k, 0);
    
    gi->active = 1;
    gi->fd = Fopen(argv[1], "r.fdio");

    while ((rc = rpmgiNext(gi)) == RPMRC_OK) {
	int rc;
	
	he->tag = RPMTAG_FILEPATHS;
	if(headerGet(gi->h, he, 0)) {
	    for(he->ix = 0; he->ix < (int) he->c; he->ix++)
		rpmbfAdd(bf, he->p.argv[he->ix], 0);
	}
    }

    fd = Fopen("bloom.dump", "w.fdio");
    Fwrite(bf->bits, 1, bf->m/8, fd);
    Fclose(fd);

    printf("n: %zu m: %zu k: %zu\nsize: %zu\n", n, m, k, bf->m/8);

}
