
#include "system.h"

#include <rpmlib.h>

#include "debug.h"

#ifdef	DYING
#define	_PATH	"~/bin:/bin:/usr/bin:/usr/X11R6/bin"
/*@unchecked@*/ /*@observer@*/
static const char * _path = _PATH;

#define alloca_strdup(_s)       strcpy(alloca(strlen(_s)+1), (_s))

static int rpmioAccess(const char * FN, const char * path, int mode)
{
    char fn[4096];
    char * bn;
    char * r, * re;
    char * t, * te;
    int negate = 0;
    int rc = 0;

    /* Empty paths are always accessible. */
    if (FN == NULL || *FN == '\0')
	return 0;

    if (mode == 0)
	mode = X_OK;

    /* Strip filename out of its name space wrapper. */
    bn = alloca_strdup(FN);
    for (t = bn; t && *t; t++) {
	if (*t != '(')
	    continue;
	*t++ = '\0';

	/* Permit negation on name space tests. */
	if (*bn == '!') {
	    negate = 1;
	    bn++;
	}

	/* Set access flags from name space marker. */
	if (strlen(bn) == 3
	 && strchr("Rr_", bn[0]) != NULL
	 && strchr("Ww_", bn[1]) != NULL
	 && strchr("Xx_", bn[2]) != NULL) {
	    mode = 0;
	    if (strchr("Rr", bn[0]) != NULL)
		mode |= R_OK;
	    if (strchr("Ww", bn[1]) != NULL)
		mode |= W_OK;
	    if (strchr("Xx", bn[2]) != NULL)
		mode |= X_OK;
	    if (mode == 0)
		mode = F_OK;
	} else if (!strcmp(bn, "exists"))
	    mode = F_OK;
	else if (!strcmp(bn, "executable"))
	    mode = X_OK;
	else if (!strcmp(bn, "readable"))
	    mode = R_OK;
	else if (!strcmp(bn, "writable"))
	    mode = W_OK;

	bn = t;
	te = bn + strlen(t) - 1;
	if (*te != ')')		/* XXX syntax error, never exists */
	    return 1;
	*te = '\0';
	break;
    }

    /* Empty paths are always accessible. */
    if (*bn == '\0')
	goto exit;

    /* Check absolute path for access. */
    if (*bn == '/') {
	rc = (Access(bn, mode) != 0 ? 1 : 0);
fprintf(stderr, "*** Access(\"%s\", 0x%x) rc %d\n", bn, mode, rc);
	goto exit;
    }

    /* Find path to search. */
    if (path == NULL)
	path = getenv("PATH");
    if (path == NULL)
	path = _path;
    if (path == NULL) {
	rc = 1;
	goto exit;
    }

    /* Look for relative basename on PATH. */
    for (r = alloca_strdup(path); r != NULL && *r != '\0'; r = re) {

	/* Find next element, terminate current element. */
        for (re = r; (re = strchr(re, ':')) != NULL; re++) {
            if (!(re[1] == '/' && re[2] == '/'))
                /*@innerbreak@*/ break;
        }
        if (re && *re == ':')
            *re++ = '\0';
	else
	    re = r + strlen(r);

	/* Expand ~/ to $HOME/ */
	t = fn;
	*t = '\0';
	if (r[0] == '~' && r[1] == '/') {
	    const char * home = getenv("HOME");
	    if (home == NULL)	/* XXX No HOME? */
		continue;
	    if (strlen(home) > (sizeof(fn) - strlen(r))) /* XXX too big */
		continue;
	    t = stpcpy(t, home);
	    r++;	/* skip ~ */
	}
	t = stpcpy(t, r);
	if (t[-1] != '/' && *bn != '/')
	    *t++ = '/';
	t = stpcpy(t, bn);
	t = rpmCleanPath(fn);

	/* Check absolute path for access. */
	rc = (Access(t, mode) != 0 ? 1 : 0);
fprintf(stderr, "*** Access(\"%s\", 0x%x) rc %d\n", t, mode, rc);
	if (rc == 0)
	    goto exit;
    }

    rc = 1;

exit:
    if (negate)
	rc ^= 1;
    return rc;
}
#endif

int main(int argc, char *argv[])
{
    int rc;
    int i;

    for (i = 1; i < argc; i++) {
	rc = rpmioAccess(argv[i], NULL, 0);
	fprintf(stderr, "%s: %d\n", argv[i], rc);
    }

    return 0;
}
